// =============================================================================
//  High Performance ParalleX Library (libhpx)
//
//  Copyright (c) 2013-2016, Trustees of Indiana University,
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

#include "libhpx/action.h"
#include "libhpx/debug.h"
#include "libhpx/gas.h"
#include "libhpx/gpa.h"
#include "libhpx/locality.h"
#include "libhpx/c_network.h"
#include "libhpx/rebalancer.h"
#include "libhpx/worker.h"
#include "hpx/hpx.h"
#include <cinttypes>
#include <cstdlib>

uint64_t HPX_GAS_BLOCK_BYTES_MAX;

hpx_addr_t HPX_HERE = 0;

hpx_addr_t HPX_THERE(uint32_t i) {
  dbg_assert(here && here->gas);
  auto gas = static_cast<gas_t*>(here->gas);
  dbg_assert(gas->there);
  return gas->there(here->gas, i);
}

hpx_addr_t hpx_addr_add(hpx_addr_t addr, hpx_gas_ptrdiff_t bytes,
                        size_t bsize) {
  dbg_assert(here && here->gas);
  auto gas = static_cast<gas_t*>(here->gas);
  dbg_assert(gas->add);
  return gas->add(here->gas, addr, bytes, bsize);
}

hpx_gas_ptrdiff_t hpx_addr_sub(hpx_addr_t lhs, hpx_addr_t rhs, size_t bsize) {
  dbg_assert(here && here->gas);
  auto gas = static_cast<gas_t*>(here->gas);
  dbg_assert(gas->sub);
  return gas->sub(here->gas, lhs, rhs, bsize);
}

bool hpx_gas_try_pin(const hpx_addr_t addr, void **local) {
  dbg_assert(here && here->gas);
  auto gas = static_cast<gas_t*>(here->gas);
  dbg_assert(gas->try_pin);
  return gas->try_pin(here->gas, addr, local);
}

void hpx_gas_unpin(const hpx_addr_t addr) {
  dbg_assert(here && here->gas);
  auto gas = static_cast<gas_t*>(here->gas);
  dbg_assert(gas->unpin);
  gas->unpin(here->gas, addr);
}

hpx_addr_t hpx_gas_alloc(size_t n, size_t bsize, uint32_t boundary,
                         hpx_gas_dist_t dist, uint32_t attr) {

  if (dist == HPX_GAS_DIST_LOCAL) {
    return hpx_gas_alloc_local_attr(n, bsize, boundary, attr);
  }
  else if (dist == HPX_GAS_DIST_CYCLIC) {
    return hpx_gas_alloc_cyclic_attr(n, bsize, boundary, attr);
  }
  else if (dist == HPX_GAS_DIST_BLOCKED) {
    return hpx_gas_alloc_blocked_attr(n, bsize, boundary, attr);
  }
  else {
    return hpx_gas_alloc_user_attr(n, bsize, boundary, dist, attr);
  }
}

hpx_addr_t hpx_gas_calloc(size_t n, size_t bsize, uint32_t boundary,
                          hpx_gas_dist_t dist, uint32_t attr) {
  if (dist ==  HPX_GAS_DIST_LOCAL) {
    return hpx_gas_calloc_local_attr(n, bsize, boundary, attr);
  }
  else if (dist == HPX_GAS_DIST_CYCLIC) {
    return hpx_gas_calloc_cyclic_attr(n, bsize, boundary, attr);
  }
  else if (dist == HPX_GAS_DIST_BLOCKED) {
    return hpx_gas_calloc_blocked_attr(n, bsize, boundary, attr);
  }
  else {
    return hpx_gas_calloc_user_attr(n, bsize, boundary, dist, attr);
  }
}

hpx_addr_t hpx_gas_alloc_cyclic_attr(size_t n, size_t bsize, uint32_t boundary,
                                     uint32_t attr) {
  dbg_assert(here && here->gas);
  auto gas = static_cast<gas_t*>(here->gas);
  dbg_assert(gas->alloc_cyclic);
  return gas->alloc_cyclic(n, bsize, boundary, attr);
}

hpx_addr_t hpx_gas_alloc_cyclic(size_t n, size_t bsize, uint32_t boundary) {
  return hpx_gas_alloc_cyclic_attr(n, bsize, boundary, HPX_GAS_ATTR_NONE);
}

hpx_addr_t hpx_gas_calloc_cyclic_attr(size_t n, size_t bsize, uint32_t boundary,
                                      uint32_t attr) {
  dbg_assert(here && here->gas);
  auto gas = static_cast<gas_t*>(here->gas);
  dbg_assert(gas->calloc_cyclic);
  return gas->calloc_cyclic(n, bsize, boundary, attr);
}

hpx_addr_t hpx_gas_calloc_cyclic(size_t n, size_t bsize, uint32_t boundary) {
  return hpx_gas_calloc_cyclic_attr(n, bsize, boundary, HPX_GAS_ATTR_NONE);
}

hpx_addr_t hpx_gas_alloc_blocked_attr(size_t n, size_t bsize,
                                      uint32_t boundary, uint32_t attr) {
  dbg_assert(here && here->gas);
  auto gas = static_cast<gas_t*>(here->gas);
  dbg_assert(gas->alloc_blocked);
  return gas->alloc_blocked(n, bsize, boundary, attr);
}

hpx_addr_t hpx_gas_alloc_blocked(size_t n, size_t bsize, uint32_t boundary) {
  return hpx_gas_alloc_blocked_attr(n, bsize, boundary, HPX_GAS_ATTR_NONE);
}

hpx_addr_t hpx_gas_calloc_blocked_attr(size_t n, size_t bsize,
                                       uint32_t boundary, uint32_t attr) {
  dbg_assert(here && here->gas);
  auto gas = static_cast<gas_t*>(here->gas);
  dbg_assert(gas->calloc_blocked);
  return gas->calloc_blocked(n, bsize, boundary, attr);
}

hpx_addr_t hpx_gas_calloc_blocked(size_t n, size_t bsize, uint32_t boundary) {
  return hpx_gas_calloc_blocked_attr(n, bsize, boundary, HPX_GAS_ATTR_NONE);
}

hpx_addr_t hpx_gas_alloc_user_attr(size_t n, size_t bsize, uint32_t boundary,
                                   hpx_gas_dist_t dist, uint32_t attr) {
  dbg_assert(here && here->gas);
  auto gas = static_cast<gas_t*>(here->gas);
  dbg_assert(gas->alloc_user);
  return gas->alloc_user(n, bsize, boundary, dist, attr);
}

hpx_addr_t hpx_gas_calloc_user_attr(size_t n, size_t bsize, uint32_t boundary,
                                    hpx_gas_dist_t dist, uint32_t attr) {
  dbg_assert(here && here->gas);
  auto gas = static_cast<gas_t*>(here->gas);
  dbg_assert(gas->calloc_user);
  return gas->calloc_user(n, bsize, boundary, dist, attr);
}

static hpx_addr_t _gas_local_search(size_t n, size_t bsize, uint32_t boundary,
                                    uint32_t attr, hpx_action_t act) {
  hpx_addr_t addr = HPX_NULL;
  unsigned *seed = &self->seed;
  for (int i = 0, e = here->ranks; i < e; ++i) {
    int j = rand_r(seed) % e;
    hpx_addr_t l = HPX_THERE(j);
    dbg_check ( hpx_call_sync(l, act, &addr, sizeof(addr), &n, &bsize,
                              &boundary, &attr),
                "Failed synchronous alloc at\n");
    if (addr) {
      return addr;
    }
    log_mem("out-of-memory detected at locality %d, searching\n", j);
  }

  for (int i = 0, e = here->ranks; i < e; ++i) {
    hpx_addr_t l = HPX_THERE(i);
    dbg_check( hpx_call_sync(l, act, &addr, sizeof(addr), &n, &bsize,
                             &boundary, &attr),
               "Failed synchronous alloc at\n");
    if (addr) {
      return addr;
    }
    log_mem("out-of-memory detected at locality %d, searching\n", i);
  }

  return addr;
}

hpx_addr_t hpx_gas_alloc_local_attr(size_t n, size_t bsize,
                                    uint32_t boundary, uint32_t attr) {
  dbg_assert(here && here->gas);
  auto gas = static_cast<gas_t*>(here->gas);
  dbg_assert(gas->alloc_local);
  hpx_addr_t addr = gas->alloc_local(n, bsize, boundary, attr);
  if (addr) {
    return addr;
  }

  log_mem("out-of-memory detected at locality %d, searching\n", here->rank);
  addr = _gas_local_search(n, bsize, boundary, attr,
                           hpx_gas_alloc_local_at_action);
  if (!addr) {
    dbg_error("out-of-memory detected\n");
  }
  return addr;
}

hpx_addr_t hpx_gas_alloc_local(size_t n, size_t bsize, uint32_t boundary) {
  return hpx_gas_alloc_local_attr(n, bsize, boundary, HPX_GAS_ATTR_NONE);
}

hpx_addr_t hpx_gas_calloc_local_attr(size_t n, size_t bsize,
                                     uint32_t boundary, uint32_t attr) {
  dbg_assert(here && here->gas);
  auto gas = static_cast<gas_t*>(here->gas);
  dbg_assert(gas->calloc_local);
  hpx_addr_t addr = gas->calloc_local(n, bsize, boundary, attr);
  if (addr) {
    return addr;
  }

  log_mem("out-of-memory detected at locality %d, searching\n", here->rank);
  addr = _gas_local_search(n, bsize, boundary, attr,
                           hpx_gas_calloc_local_at_action);
  if (!addr) {
    dbg_error("out-of-memory detected\n");
  }
  return addr;
}

hpx_addr_t hpx_gas_calloc_local(size_t n, size_t bsize, uint32_t boundary) {
  return hpx_gas_calloc_local_attr(n, bsize, boundary, HPX_GAS_ATTR_NONE);
}

void hpx_gas_free(hpx_addr_t addr, hpx_addr_t sync) {
  dbg_assert(here && here->gas);
  auto gas = static_cast<gas_t*>(here->gas);
  dbg_assert(gas->free);
  gas->free(here->gas, addr, sync);
}

void hpx_gas_free_sync(hpx_addr_t addr) {
  hpx_addr_t sync = hpx_lco_future_new(0);
  hpx_gas_free(addr, sync);
  hpx_lco_wait(sync);
  hpx_lco_delete(sync, HPX_NULL);
}

static int _hpx_gas_free_handler(void) {
  hpx_addr_t target = hpx_thread_current_target();
  hpx_gas_free_sync(target);
  return HPX_SUCCESS;
}
LIBHPX_ACTION(HPX_DEFAULT, 0, hpx_gas_free_action, _hpx_gas_free_handler);

void hpx_gas_move(hpx_addr_t src, hpx_addr_t dst, hpx_addr_t lco) {
  if (src == HPX_NULL || dst == HPX_NULL) {
    return;
  }
  dbg_assert(here && here->gas);
  auto gas = static_cast<gas_t*>(here->gas);
  dbg_assert(gas->move);
  gas->move(here->gas, src, dst, lco);
}

int hpx_gas_memget(void *to, hpx_addr_t from, size_t size, hpx_addr_t lsync) {
  dbg_assert(here && here->gas);
  auto gas = static_cast<gas_t*>(here->gas);
  dbg_assert(gas->string.memget_rsync);
  return gas->string.memget(here->gas, to, from, size, lsync, HPX_NULL);
}

int hpx_gas_memget_sync(void *to, hpx_addr_t from, size_t size) {
  dbg_assert(here && here->gas);
  auto gas = static_cast<gas_t*>(here->gas);
  dbg_assert(gas->string.memget_lsync);
  return gas->string.memget_lsync(here->gas, to, from, size);
}

int hpx_gas_memput(hpx_addr_t to, const void *from, size_t size,
                   hpx_addr_t lsync, hpx_addr_t rsync) {
  dbg_assert(here && here->gas);
  auto gas = static_cast<gas_t*>(here->gas);
  dbg_assert(gas->string.memput);
  return gas->string.memput(here->gas, to, from, size, lsync, rsync);
}

int hpx_gas_memput_lsync(hpx_addr_t to, const void *from, size_t size,
                         hpx_addr_t rsync) {
  dbg_assert(here && here->gas);
  auto gas = static_cast<gas_t*>(here->gas);
  dbg_assert(gas->string.memput_lsync);
  return gas->string.memput_lsync(here->gas, to, from, size, rsync);
}

int hpx_gas_memput_rsync(hpx_addr_t to, const void *from, size_t size) {
  dbg_assert(here && here->gas);
  auto gas = static_cast<gas_t*>(here->gas);
  dbg_assert(gas->string.memput_rsync);
  return gas->string.memput_rsync(here->gas, to, from, size);
}

int hpx_gas_memcpy(hpx_addr_t to, hpx_addr_t from, size_t size, hpx_addr_t sync)
{
  dbg_assert(here && here->gas);
  auto gas = static_cast<gas_t*>(here->gas);
  dbg_assert(gas->string.memcpy);
  return gas->string.memcpy(here->gas, to, from, size, sync);
}

int hpx_gas_memcpy_sync(hpx_addr_t to, hpx_addr_t from, size_t size) {
  dbg_assert(here && here->gas);
  auto gas = static_cast<gas_t*>(here->gas);
  dbg_assert(gas->string.memcpy_sync);
  return gas->string.memcpy_sync(here->gas, to, from, size);
}

static int _gas_alloc_local_at_handler(size_t n, size_t bsize,
                                       uint32_t boundary, uint32_t attr) {
  hpx_addr_t addr = hpx_gas_alloc_local_attr(n, bsize, boundary, attr);
  return HPX_THREAD_CONTINUE(addr);
}
LIBHPX_ACTION(HPX_DEFAULT, 0, hpx_gas_alloc_local_at_action,
              _gas_alloc_local_at_handler, HPX_SIZE_T, HPX_SIZE_T,
              HPX_UINT32, HPX_UINT32);

hpx_addr_t hpx_gas_alloc_local_at_sync(size_t n, size_t bsize,
                                       uint32_t boundary, hpx_addr_t loc) {
  hpx_addr_t addr = 0;
  uint32_t attr = HPX_GAS_ATTR_NONE;
  dbg_check( hpx_call_sync(loc, hpx_gas_alloc_local_at_action, &addr,
                           sizeof(addr), &n, &bsize, &boundary, &attr),
             "Failed synchronous call during allocation\n");
  return addr;
}

void hpx_gas_alloc_local_at_async(size_t n, size_t bsize, uint32_t boundary,
                                  hpx_addr_t loc, hpx_addr_t lco) {
  uint32_t attr = HPX_GAS_ATTR_NONE;
  dbg_check( hpx_call(loc, hpx_gas_alloc_local_at_action, lco, &n, &bsize,
                      &boundary, &attr),
             "Failed async call during allocation\n");
}

static int _gas_calloc_at_handler(size_t n, size_t bsize, uint32_t boundary,
                                  uint32_t attr) {
  hpx_addr_t addr = hpx_gas_calloc_local_attr(n, bsize, boundary, attr);
  return HPX_THREAD_CONTINUE(addr);
}
LIBHPX_ACTION(HPX_DEFAULT, 0, hpx_gas_calloc_local_at_action,
              _gas_calloc_at_handler, HPX_SIZE_T, HPX_SIZE_T,
              HPX_UINT32, HPX_UINT32);

hpx_addr_t hpx_gas_calloc_local_at_sync(size_t n, size_t bsize,
                                        uint32_t boundary, hpx_addr_t loc) {
  hpx_addr_t addr = 0;
  uint32_t attr = HPX_GAS_ATTR_NONE;
  dbg_check( hpx_call_sync(loc, hpx_gas_calloc_local_at_action, &addr,
                           sizeof(addr), &n, &bsize, &boundary, &attr),
            "Failed synchronous call during allocation\n");
  return addr;
}

void hpx_gas_calloc_local_at_async(size_t n, size_t bsize, uint32_t boundary,
                                   hpx_addr_t loc, hpx_addr_t lco) {
  uint32_t attr = HPX_GAS_ATTR_NONE;
  dbg_check( hpx_call(loc, hpx_gas_calloc_local_at_action, lco, &n, &bsize,
                      &boundary, &attr),
             "Failed async call during allocation\n");
}

namespace {
LIBHPX_ACTION(HPX_INTERRUPT, 0, _set_attr_action, hpx_gas_set_attr, HPX_ADDR,
              HPX_UINT32);
}
void hpx_gas_set_attr(hpx_addr_t addr, uint32_t attr) {
  if (attr != HPX_GAS_ATTR_NONE &&
      attr != HPX_GAS_ATTR_RO   &&
      attr != HPX_GAS_ATTR_LB) {
    log_dflt("invalid attribute %d for addr %" PRIu64 ".\n",
             attr, addr);
    return;
  }
  dbg_assert(here && here->gas);
  auto gas = static_cast<gas_t*>(here->gas);
  if (gas->set_attr) {
    if (!hpx_gas_try_pin(addr, NULL)) {
      int e = hpx_call_sync(addr, _set_attr_action, NULL, 0, &addr, &attr);
      dbg_check(e, "Could not forward hpx_gas_set_attr\n");
      return;
    }
    gas->set_attr(here->gas, addr, attr);
    hpx_gas_unpin(addr);
  }
}

void hpx_gas_rebalance(hpx_addr_t async, hpx_addr_t psync, hpx_addr_t msync) {
  rebalancer_start(async, psync, msync);
}

void hpx_gas_set_affinity(hpx_addr_t gva, int worker) {
  gas_set_affinity(here->gas, gva, worker);
}

void hpx_gas_clear_affinity(hpx_addr_t gva) {
  gas_clear_affinity(here->gas, gva);
}

int
hpx_register_memory(const void *buffer, size_t bytes)
{
  dbg_assert(here && here->net);
  network_register_dma(here->net, buffer, bytes, NULL);
  return HPX_SUCCESS;
}

int
hpx_deregister_memory(const void *buffer, size_t bytes)
{
  dbg_assert(here && here->net);
  network_release_dma(here->net, buffer, bytes);
  return HPX_SUCCESS;
}
