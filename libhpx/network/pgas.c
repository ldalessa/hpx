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


/// ----------------------------------------------------------------------------
/// Down-and-dirty PGAS implementation.
///
/// Each locality keeps a global hashtable that maps blocks to local virtual
/// addresses.
/// ----------------------------------------------------------------------------
#include <assert.h>
#include <limits.h>
#include <stdlib.h>
#include "hpx/hpx.h"
#include "libsync/locks.h"
#include "libsync/hashtables.h"
#include "libhpx/debug.h"
#include "libhpx/scheduler.h"


// reserved allocations
enum {
  NULL_ID = 0,
  HERE_ID,
  NEXT_ID
};


static tatas_lock_t _lock = SYNC_TATAS_LOCK_INIT;
static int _next_id = -1;
static cuckoo_hashtable_t _map = SYNC_CUCKOO_HASHTABLE_INIT;


/// ----------------------------------------------------------------------------
/// We just get the next allocation ID for this rank.
///
/// If this is a bottleneck then we have more problems than just the fact that
/// I'm using a lock instead of atomic operations.
/// ----------------------------------------------------------------------------
static int _next_allocation_id(void) {
  int id;
  sync_tatas_acquire(&_lock);
  id = _next_id++;
  if (id < NULL_ID) {
    int ranks = hpx_get_num_ranks();
    int rank = hpx_get_my_rank();
    int ids_per_rank = INT_MAX / ranks;
    id = rank * ids_per_rank;

    // we reserve a couple of low allocation IDs for the system
    if (id == NULL_ID)
      id = NEXT_ID;

    _next_id = id + 1;
  }
  sync_tatas_release(&_lock);
  return id;
}


static void *_malloc_local(int id, int bytes) {
  // perform the local allocation
  void *allocation = malloc(bytes);
  if (!allocation) {
    dbg_error("Could not allocate %d bytes.\n", bytes);
    hpx_abort(-1);
  }

  // insert the local mapping for this id
  int success = sync_cuckoo_hashtable_insert(&_map, id, allocation);
  assert(success);
  return allocation;
}


// static void _free_local(int id) {

// }

static hpx_action_t _free_remote = 0;
static hpx_action_t _malloc_remote = 0;


static int _free_remote_action(void *args) {
  return HPX_SUCCESS;
}


/// ----------------------------------------------------------------------------
/// Malloc part of a global allocation.
/// ----------------------------------------------------------------------------
static int _malloc_remote_action(void *args) {
  int *a = args;
  int id = a[0];
  int bytes = a[1];
  _malloc_local(id, bytes);
  return HPX_SUCCESS;
}


/// ----------------------------------------------------------------------------
/// Register the remote actions that we need.
/// ----------------------------------------------------------------------------
static void HPX_CONSTRUCTOR _initialize_actions(void) {
  _free_remote = hpx_register_action("_hpx_free_remote_action",
                                     _free_remote_action);
  _malloc_remote = hpx_register_action("_hpx_malloc_remote_action",
                                       _malloc_remote_action);
}


/// ----------------------------------------------------------------------------
/// This is currently trying to have the layout:
///
/// shared [block_size] T[n]; where sizeof(T) == bytes
/// ----------------------------------------------------------------------------
hpx_addr_t
hpx_global_alloc(size_t n, size_t bytes, size_t block_size, size_t alignment) {
  // need to know the number of ranks
  int ranks = hpx_get_num_ranks();

  // compute the block bytes
  int block_bytes = bytes * block_size;

  // figure out the total number of blocks that I need for the
  // allocation---there are n elements, and each block has block_size elements,
  // so there need to be n / block_size + (n % block_size) ? 1 : 0 blocks
  size_t blocks = n / block_size + ((n % block_size) ? 1 : 0);

  // figure out how many blocks we'll allocate on each rank---there are blocks
  // blocks, and ranks ranks, so there need to be (blocks / ranks) + (block %
  // ranks) ? 1 : 0; blocks allocated on each rank
  size_t blocks_per_rank = (blocks / ranks) + ((blocks % ranks) ? 1 : 0);

  // each block has block_size * bytes bytes, so figure out the total allocation
  // at each rank
  size_t bytes_per_rank = blocks_per_rank * block_bytes;

  // get an allocation id
  int id = _next_allocation_id();

  // set up the arguments for the broadcast
  int args[2] = { id, bytes_per_rank };

  // broadcast the allocation request
  hpx_addr_t f[ranks];
  for (int i = 0; i < ranks; ++i) {
    f[i] = hpx_future_new(0);
    hpx_call(HPX_THERE(i), _malloc_remote, args, sizeof(args), f[i]);
  }

  // wait for all of the allocations to complete
  hpx_future_get_all(ranks, f, NULL, NULL);
  for (int i = 0; i < ranks; ++i)
    hpx_future_delete(f[i]);

  // initialize an address structure that points to the base of the allocation
  hpx_addr_t addr = {
    .offset = 0,
    .id = id,
    .block_bytes = block_bytes
  };

  // and return the structure.
  return addr;
}


hpx_addr_t
hpx_alloc(size_t n, size_t bytes, size_t alignment) {
  int block_bytes = n * bytes;
  int id = _next_allocation_id();
  void *local = _malloc_local(id, block_bytes);
  if (!local) {
    dbg_error("failed to allocate %d bytes.\n", block_bytes);
    hpx_abort(-1);
  }

  int rank = hpx_get_my_rank();
  int offset = rank * block_bytes;

  hpx_addr_t addr = {
    .offset = offset,
    .id = id,
    .block_bytes = block_bytes
  };

  return addr;
}

// void
// hpx_free(hpx_addr_t addr) {

// }

bool
hpx_addr_eq(const hpx_addr_t lhs, const hpx_addr_t rhs) {
  // block_bytes are constant per-allocation-id
  return (lhs.offset == rhs.offset) && (lhs.id == rhs.id);
}


bool
hpx_addr_try_pin(const hpx_addr_t addr, void **local) {
  int me = hpx_get_my_rank();
  int rank = HPX_WHERE(addr);
  if (me != rank)
    return false;

  if (!local)
    return true;

  // lookup the mapping
  void *base = sync_cuckoo_hashtable_lookup(&_map, addr.id);
  assert(base);

  // need the local offset
  int ranks = hpx_get_num_ranks();
  int block = (addr.offset / addr.block_bytes); // really?
  int phase = block / ranks;                    // REALLY?
  int block_offset = (addr.offset % addr.block_bytes);
  int o = (phase * addr.block_bytes) + block_offset;
  *local = (char*)base + o;

  return true;
}


void
hpx_addr_unpin(const hpx_addr_t addr) {
}


// HPX_NULL uses allocation ID 0
const hpx_addr_t HPX_NULL = {
  .offset = 0,
  .id = NULL_ID,
  .block_bytes = 0
};


// HPX_HERE is a global address for my scheduler, and needs to be initialized
// (see hpx.c)
hpx_addr_t HPX_HERE = HPX_ADDR_INIT;


/// Generate an address for a rank.
hpx_addr_t HPX_THERE(int i) {
  hpx_addr_t there = {
    .offset = i * sizeof(scheduler_t),
    .id = HERE_ID,
    .block_bytes = sizeof(scheduler_t)
  };
  return there;
}


int HPX_WHERE(hpx_addr_t addr) {
  int block = (addr.offset / addr.block_bytes); // really?
  int ranks = hpx_get_num_ranks();
  return (block % ranks);
}


hpx_addr_t
hpx_addr_add(const hpx_addr_t addr, int bytes) {
  hpx_addr_t a = {
    .offset = addr.offset + bytes,
    .id = addr.id,
    .block_bytes = addr.block_bytes
  };
  return a;
}
