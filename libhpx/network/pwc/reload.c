// =============================================================================
//  High Performance ParalleX Library (libhpx)
//
//  Copyright (c) 2013-2015, Trustees of Indiana University,
//  All rights reserved.
//
//  This software may be modified and distributed under the terms of the BSD
//  license.  See the COPYING file for details.
//
//  This software was created at the Indiana University Center for Research in
//  Extreme Scale Technologies (CREST).
// =============================================================================
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <libhpx/config.h>
#include <libhpx/boot.h>
#include <libhpx/libhpx.h>
#include <libhpx/locality.h>
#include <libhpx/memory.h>
#include <libhpx/parcel.h>
#include <libhpx/parcel_block.h>
#include <libhpx/scheduler.h>

#include "parcel_emulation.h"
#include "pwc.h"
#include "send_buffer.h"
#include "xport.h"

#include "../commands.h"

typedef struct {
  size_t              n;
  size_t              i;
  parcel_block_t *block;
  xport_key_t       key;
} buffer_t;

typedef struct {
  void      *addr;
  xport_key_t key;
} remote_t;

typedef struct {
  parcel_emulator_t vtable;
  int                 rank;
  int                ranks;
  buffer_t           *recv;
  xport_key_t     recv_key;
  buffer_t           *send;
  xport_key_t     send_key;
  remote_t        *remotes;
} reload_t;

static COMMAND_DECL(_reload_request);
static COMMAND_DECL(_reload_reply);

static void _buffer_fini(buffer_t *b) {
  if (b) {
    parcel_block_delete(b->block);
  }
}

static void _buffer_reload(buffer_t *b, pwc_xport_t *xport) {
  dbg_assert(1ul << ceil_log2_64(b->n) == b->n);
  b->block = parcel_block_new(b->n, b->n, &b->i);
  int e = xport->key_find(xport, b->block, b->n, &b->key);
  dbg_check(e, "no key for parcel block at (%p, %zu)\n", (void*)b->block, b->n);
}

static void _buffer_init(buffer_t *b, size_t n, pwc_xport_t *xport) {
  b->n = n;
  _buffer_reload(b, xport);
}

static int _recv_parcel_handler(int src, command_t command) {
  hpx_parcel_t *p = (hpx_parcel_t*)command_get_arg(command);
  p->src = src;
  parcel_set_state(p, PARCEL_SERIALIZED | PARCEL_BLOCK_ALLOCATED);
  scheduler_spawn(p);
  return HPX_SUCCESS;
}
COMMAND_DEF(INTERRUPT, _recv_parcel_handler, _recv_parcel);

static int _buffer_send(buffer_t *send, pwc_xport_t *xport, xport_op_t *op) {
  int i = send->i;
  dbg_assert(!(i & 7));
  size_t r = send->n - i;
  if (op->n < r) {
    // make sure i stays 8-byte aligned
    size_t align = (8ul - (op->n & 7ul)) & 7ul;
    send->i += op->n + align;
    log_parcel("allocating %zu bytes in buffer %p (%zu remain)\n",
               op->n + align, (void*)send->block, send->n - send->i);
    op->dest_key = &send->key;
    op->dest = parcel_block_at(send->block, i);
    op->rop = command_pack(_recv_parcel, (uintptr_t)op->dest);
    return xport->pwc(op);
  }

  op->n = 0;
  op->src = NULL;
  op->src_key = NULL;
  op->lop = 0;
  op->rop = command_pack(_reload_request, r);
  int e = xport->command(op);
  if (LIBHPX_OK == e) {
    return LIBHPX_RETRY;
  }

  dbg_error("could not complete send operation\n");
}

static int _reload_send(void *obj, pwc_xport_t *xport, int rank,
                        const hpx_parcel_t *p) {
  size_t n = parcel_size(p);
  xport_op_t op = {
    .rank = rank,
    .n = n,
    .dest = NULL,
    .dest_key = NULL,
    .src = p,
    .src_key = xport->key_find_ref(xport, p, n),
    .lop = command_pack(release_parcel, (uintptr_t)p),
    .rop = 0
  };

  if (!op.src_key) {
    dbg_error("no rdma key for local parcel (%p, %zu)\n", (void*)p, n);
  }

  reload_t *reload = obj;
  buffer_t *send = &reload->send[rank];
  return _buffer_send(send, xport, &op);
}

static hpx_parcel_t *_buffer_recv(buffer_t *recv) {
  const hpx_parcel_t *p = parcel_block_at(recv->block, recv->i);
  recv->i += parcel_size(p);
  if (recv->i >= recv->n) {
    dbg_error("reload buffer recv overload\n");
  }
  return parcel_clone(p);
}

static hpx_parcel_t *_reload_recv(void *obj, int rank) {
  reload_t *reload = obj;
  buffer_t *recv = &reload->recv[rank];
  return _buffer_recv(recv);
}

static void _reload_delete(void *obj) {
  if (obj) {
    reload_t *reload = obj;
    for (int i = 0, e = reload->ranks; i < e; ++i) {
      _buffer_fini(&reload->recv[i]);
    }
    registered_free(reload->recv);
    registered_free(reload->send);
    local_free(reload->remotes);
    free(reload);
  }
}

void *parcel_emulator_new_reload(const config_t *cfg, boot_t *boot,
                                 pwc_xport_t *xport) {
  int e;
  int rank = boot_rank(boot);
  int ranks = boot_n_ranks(boot);

  // Allocate the buffer.
  reload_t *reload = local_calloc(1, sizeof(*reload));
  reload->vtable.delete = _reload_delete;
  reload->vtable.send = _reload_send;
  reload->vtable.recv = _reload_recv;
  reload->rank = rank;
  reload->ranks = ranks;

  // Allocate my buffers.
  size_t buffer_row_size = ranks * sizeof(buffer_t);
  size_t remote_table_size = ranks * sizeof(remote_t);
  reload->recv = registered_malloc(buffer_row_size);
  reload->send = registered_malloc(buffer_row_size);
  reload->remotes = local_malloc(remote_table_size);

  // Grab the keys for the recv and send rows
  e = xport->key_find(xport, reload->send, buffer_row_size, &reload->send_key);
  dbg_check(e, "no rdma key for send row (%p)\n", (void*)reload->send);
  e = xport->key_find(xport, reload->recv, buffer_row_size, &reload->recv_key);
  dbg_check(e, "no rdma key for recv row (%p)\n", (void*)reload->recv);

  // Initialize the recv buffers for this rank.
  for (int i = 0, e = ranks; i < e; ++i) {
    buffer_t *recv = &reload->recv[i];
    _buffer_init(recv, cfg->pwc_parcelbuffersize, xport);
  }

  // Initialize a temporary array of remote pointers for this rank's sends.
  remote_t *remotes = local_malloc(remote_table_size);
  for (int i = 0, e = ranks; i < e; ++i) {
    remotes[i].addr = &reload->send[i];
    xport->key_copy(&remotes[i].key, &reload->send_key);
  }

  // exchange all of the recv buffers, and all of the remote send pointers
  size_t buffer_size = sizeof(buffer_t);
  size_t remote_size = sizeof(remote_t);
  boot_alltoall(boot, reload->send, reload->recv, buffer_size, buffer_size);
  boot_alltoall(boot, reload->remotes, remotes, remote_size, remote_size);

  // free the temporary array of remote pointers
  local_free(remotes);

  // just do a sanity check to make sure the alltoalls worked
  if (DEBUG) {
    buffer_t *send = &reload->send[rank];
    buffer_t *recv = &reload->recv[rank];
    dbg_assert(send->n == recv->n);
    dbg_assert(send->i == recv->i);
    dbg_assert(send->block == recv->block);
    dbg_assert(!strncmp(send->key, recv->key, XPORT_KEY_SIZE));
  }

  // Now reload contains:
  //
  // 1) A row of initialized recv buffers, one for each sender.
  // 2) A row of initialized send buffers, one for each target (corresponding to
  //    their recv buffer for me).
  // 3) A table of remote pointers, one for each send buffer targeting me.
  return reload;
}

static int _reload_reply_handler(int src, command_t cmd) {
  pwc_network_t *pwc = (pwc_network_t*)here->network;
  send_buffer_t *sends = &pwc->send_buffers[src];
  return send_buffer_progress(sends);
}
static COMMAND_DEF(DEFAULT, _reload_reply_handler, _reload_reply);

static int _reload_request_handler(int src, command_t cmd) {
  pwc_network_t *pwc = (pwc_network_t*)here->network;
  pwc_xport_t *xport = pwc->xport;
  reload_t *reload = (reload_t*)pwc->parcels;
  buffer_t *recv = &reload->recv[src];
  size_t n = command_get_arg(cmd);
  if (n) {
    parcel_block_deduct(recv->block, n);
  }
  _buffer_reload(recv, xport);

  xport_op_t op = {
    .rank = src,
    .n = sizeof(*recv),
    .dest = reload->remotes[src].addr,
    .dest_key = reload->remotes[src].key,
    .src = recv,
    .src_key = reload->recv_key,
    .lop = 0,
    .rop = command_pack(_reload_reply, 0)
  };

  return xport->pwc(&op);
}
static COMMAND_DEF(INTERRUPT, _reload_request_handler, _reload_request);
