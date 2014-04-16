// =============================================================================
//  High Performance ParalleX Library (libhpx)
//
//  Copyright (c) 2013, Trustees of Indiana University,
//  All rights reserved.
//
//  This software may be modified and distributed under the terms of the BSD
//  license.  See the COPYING file for details.
//
//  This software was created at the Indiana University Center for Research in
//  Extreme Scale Technologies (CREST).
// =============================================================================
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <sys/mman.h>
#include "libsync/sync.h"
#include "libhpx/btt.h"
#include "libhpx/debug.h"
#include "libhpx/locality.h"
#include "libhpx/scheduler.h"
#include "addr.h"

typedef struct {
  SYNC_ATOMIC(int64_t count);
  void * SYNC_ATOMIC() base;
} _record_t;

static const uint64_t _TABLE_SIZE = UINT32_MAX * sizeof(_record_t);

static const int64_t _WRITE_LOCK = 1 << sizeof(int64_t);

typedef struct {
  btt_class_t class;
  _record_t  *table;
} agas_btt_t;

static bool _invalid(int64_t count) {
  return (count % 2 == 0);
}


static uint32_t _agas_btt_home(btt_class_t *btt, hpx_addr_t addr) {
  return addr_block_id(addr) % here->ranks;
}


static uint32_t _agas_btt_owner(btt_class_t *btt, hpx_addr_t addr) {
  return _agas_btt_home(btt, addr);
}


static void _agas_btt_delete(btt_class_t *btt) {
  agas_btt_t *agas = (agas_btt_t*)btt;

  if (!agas)
    return;

  if (agas->table)
    munmap((void*)agas->table, _TABLE_SIZE);

  free(agas);
}


static bool _agas_btt_try_pin(btt_class_t *btt, hpx_addr_t addr, void **out) {
  // if invalid
  //   return false
  // else if locked
  //   return false
  // else
  //   bump ref count
  //   output mapping
  //   return true
  agas_btt_t *agas = (agas_btt_t *)btt;
  uint32_t block_id = addr_block_id(addr);
  int64_t count;
  sync_load(count, &agas->table[block_id].count, SYNC_ACQUIRE);

  // if not valid
  if (_invalid(count))
    return false;

  // if locked, return false
  if (count < 0)
    return false;

  // if not ref count success, retry (could have changed arbitrarily)
  if (!sync_cas(&agas->table[block_id].count, count, count + 2, SYNC_RELEASE,
                SYNC_RELAXED))
    return _agas_btt_try_pin(btt, addr, out);

  // if the virtual address is required, return it
  if (out) {
    void *base = agas->table[block_id].base;
    *out = addr_to_local(addr, base);
  }

  return true;
}


static void _agas_btt_unpin(btt_class_t *btt, hpx_addr_t addr) {
  // assume that the programmer knows what they're talking about, just blindly
  // decrement by two
  agas_btt_t *agas = (agas_btt_t *)btt;
  uint32_t block_id = addr_block_id(addr);
  sync_fadd(&agas->table[block_id].count, -2, SYNC_ACQ_REL);
}


static void *_agas_btt_invalidate(btt_class_t *btt, hpx_addr_t addr) {
  agas_btt_t *agas = (agas_btt_t *)btt;
  uint32_t block_id = addr_block_id(addr);
  int64_t count;
  sync_load(count, &agas->table[block_id].count, SYNC_ACQUIRE);

  if (_invalid(count))
    return NULL;

  // if there is an invalidation in process
  if (count < 0)
    return NULL;

  // otherwise acquire the write lock
  if (!sync_cas(&agas->table[block_id].count, count, count | _WRITE_LOCK,
                SYNC_RELEASE, SYNC_RELAXED))
    return _agas_btt_invalidate(btt, addr);

  // wait until the readers drain away
  while (count != _WRITE_LOCK) {
    scheduler_yield();
    sync_load(count, &agas->table[block_id].count, SYNC_ACQUIRE);
  }

  // clear the mapping
  void *base = agas->table[block_id].base;
  agas->table[block_id].base = NULL;

  // mark the row as invalid
  sync_store(&agas->table[block_id].count, 0, SYNC_RELEASE);

  // return the old mapping
  return base;
}


static void _agas_btt_insert(btt_class_t *btt, hpx_addr_t addr, void *base) {
  agas_btt_t *agas = (agas_btt_t *)btt;
  uint32_t block_id = addr_block_id(addr);

  if (DEBUG) {
    int64_t count;
    sync_load(count, &agas->table[block_id].count, SYNC_ACQUIRE);
    assert(_invalid(count));
  }

  // acquire the write lock
  if (!sync_cas(&agas->table[block_id].count, 0, _WRITE_LOCK, SYNC_RELEASE,
                SYNC_RELAXED)) {
    dbg_error("btt insert for existing mapping.\n");
    hpx_abort();
  }

  agas->table[block_id].base = base;
  sync_store(&agas->table[block_id].count, 1, SYNC_RELEASE);
}


static void _agas_btt_remap(btt_class_t *btt, hpx_addr_t src, hpx_addr_t dst,
                            hpx_addr_t lco) {
}


btt_class_t *btt_agas_new(void) {
  // Allocate the object
  agas_btt_t *btt = malloc(sizeof(*btt));
  if (!btt) {
    dbg_error("could not allocate AGAS block-translation-table.\n");
    return NULL;
  }

  // set up class
  btt->class.delete     = _agas_btt_delete;
  btt->class.try_pin    = _agas_btt_try_pin;
  btt->class.unpin      = _agas_btt_unpin;
  btt->class.invalidate = _agas_btt_invalidate;
  btt->class.insert     = _agas_btt_insert;
  btt->class.remap      = _agas_btt_remap;
  btt->class.owner      = _agas_btt_owner;
  btt->class.home       = _agas_btt_home;

  // mmap the table
  int prot = PROT_READ | PROT_WRITE;
  int flags = MAP_ANON | MAP_PRIVATE | MAP_NORESERVE | MAP_NONBLOCK;
  btt->table = mmap(NULL, _TABLE_SIZE, prot, flags, -1, 0);
  if (!btt->table) {
    dbg_error("could not mmap AGAS block stranslation table.\n");
    free(btt);
    return NULL;
  }

  return &btt->class;
}
