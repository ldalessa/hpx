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

#include <string.h>
#include <libhpx/action.h>
#include <libhpx/debug.h>
#include <libhpx/locality.h>
#include <libhpx/network.h>
#include <libhpx/scheduler.h>
#include "agas.h"
#include "btt.h"
#include "../parcel/emulation.h"

void
agas_move(void *gas, hpx_addr_t src, hpx_addr_t dst, hpx_addr_t sync) {
}

static int _agas_lco_set_handler(int src, uint64_t command) {
  hpx_addr_t lco = command;
  hpx_lco_set(lco, 0, NULL, HPX_NULL, HPX_NULL);
  return HPX_SUCCESS;
}
static COMMAND_DEF(_agas_lco_set, _agas_lco_set_handler);

int
agas_memput(void *gas, hpx_addr_t to, const void *from, size_t n,
            hpx_addr_t lsync, hpx_addr_t rsync) {
  if (!n) {
    hpx_lco_set(lsync, 0, NULL, HPX_NULL, HPX_NULL);
    hpx_lco_set(rsync, 0, NULL, HPX_NULL, HPX_NULL);
    return HPX_SUCCESS;
  }

  agas_t *agas = gas;
  gva_t gva = { .addr = to };
  void *lto = NULL;
  if (btt_try_pin(agas->btt, gva, &lto)) {
    memcpy(lto, from, n);
    btt_unpin(agas->btt, gva);
    hpx_lco_set(lsync, 0, NULL, HPX_NULL, HPX_NULL);
    hpx_lco_set(rsync, 0, NULL, HPX_NULL, HPX_NULL);
    return HPX_SUCCESS;
  }

  hpx_action_t lop = lsync ? _agas_lco_set : HPX_ACTION_NULL;
  if (rsync) {
    return network_pwc(here->network, to, from, n, lop, lsync,
                       _agas_lco_set, rsync);
  }
  else {
    return network_put(here->network, to, from, n, lop, lsync);
  }
}

int
agas_memput_lsync(void *gas, hpx_addr_t to, const void *from, size_t n,
                  hpx_addr_t rsync) {
  if (!n) {
    hpx_lco_set(rsync, 0, NULL, HPX_NULL, HPX_NULL);
    return HPX_SUCCESS;
  }

  agas_t *agas = gas;
  gva_t gva = { .addr = to };
  void *lto = NULL;
  if (btt_try_pin(agas->btt, gva, &lto)) {
    memcpy(lto, from, n);
    btt_unpin(agas->btt, gva);
    hpx_lco_set(rsync, 0, NULL, HPX_NULL, HPX_NULL);
    return HPX_SUCCESS;
  }

  hpx_addr_t lsync = hpx_lco_future_new(0);
  int e = HPX_SUCCESS;
  if (rsync) {
    e = network_pwc(here->network, to, from, n, _agas_lco_set, lsync,
                        _agas_lco_set, rsync);
    dbg_check(e, "failed network pwc\n");
  }
  else {
    e = network_put(here->network, to, from, n, _agas_lco_set, lsync);
    dbg_check(e, "failed network put\n");
  }
  e = hpx_lco_wait(lsync);
  dbg_check(e, "lsync LCO reported error\n");
  hpx_lco_delete(lsync, HPX_NULL);              // TODO: async safe?
  return HPX_SUCCESS;
}

int
agas_memput_rsync(void *gas, hpx_addr_t to, const void *from, size_t n) {
  if (!n) {
    return HPX_SUCCESS;
  }

  agas_t *agas = gas;
  gva_t gva = { .addr = to };
  void *lto = NULL;
  if (btt_try_pin(agas->btt, gva, &lto)) {
    memcpy(lto, from, n);
    btt_unpin(agas->btt, gva);
    return HPX_SUCCESS;
  }

  hpx_addr_t rsync = hpx_lco_future_new(0);
  int e = network_pwc(here->network, to, from, n, HPX_ACTION_NULL, HPX_NULL,
                      _agas_lco_set, rsync);
  dbg_check(e, "failed network pwc\n");
  e = hpx_lco_wait(rsync);
  dbg_check(e, "lsync LCO reported error\n");
  hpx_lco_delete(rsync, HPX_NULL);              // TODO: async safe?
  return HPX_SUCCESS;
}

int
agas_memget(void *gas, void *to, hpx_addr_t from, size_t n, hpx_addr_t lsync) {
  if (!n) {
    hpx_lco_set(lsync, 0, NULL, HPX_NULL, HPX_NULL);
    return HPX_SUCCESS;
  }

  agas_t *agas = gas;
  gva_t gva = { .addr = from };
  void *lfrom = NULL;
  if (btt_try_pin(agas->btt, gva, &lfrom)) {
    memcpy(to, lfrom, n);
    btt_unpin(agas->btt, gva);
    hpx_lco_set(lsync, 0, NULL, HPX_NULL, HPX_NULL);
    return HPX_SUCCESS;
  }
  else {
    return network_get(here->network, to, from, n, _agas_lco_set, lsync);
  }
  return HPX_SUCCESS;
}

int
agas_memget_lsync(void *gas, void *to, hpx_addr_t from, size_t n) {
  if (!n) {
    return HPX_SUCCESS;
  }

  agas_t *agas = gas;
  gva_t gva = { .addr = from };
  void *lfrom = NULL;
  if (btt_try_pin(agas->btt, gva, &lfrom)) {
    memcpy(to, lfrom, n);
    btt_unpin(agas->btt, gva);
    return HPX_SUCCESS;
  }
  else {
    hpx_addr_t lsync = hpx_lco_future_new(0);
    int e = network_get(here->network, to, from, n, _agas_lco_set, lsync);
    dbg_check(e, "failed network get\n");
    e = hpx_lco_wait(lsync);
    dbg_check(e, "lsync LCO reported error\n");
    hpx_lco_delete(lsync, HPX_NULL);            // async okay?
    return HPX_SUCCESS;
  }
}

int
agas_memcpy(void *gas, hpx_addr_t to, hpx_addr_t from, size_t size,
            hpx_addr_t sync) {
  if (!size) {
    return HPX_SUCCESS;
  }

  void *lto;
  void *lfrom;

  if (!hpx_gas_try_pin(to, &lto)) {
    return parcel_memcpy(to, from, size, sync);
  }
  else if (!hpx_gas_try_pin(from, &lfrom)) {
    hpx_gas_unpin(to);
    return parcel_memcpy(to, from, size, sync);
  }

  memcpy(lto, lfrom, size);
  hpx_gas_unpin(to);
  hpx_gas_unpin(from);
  hpx_lco_set(sync, 0, NULL, HPX_NULL, HPX_NULL);
  return HPX_SUCCESS;
}
