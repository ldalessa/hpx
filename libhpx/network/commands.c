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

#include <libhpx/locality.h>
#include <libhpx/network.h>

#include "commands.h"

#include "../gas/pgas/gpa.h"

static int _lco_set_handler(int src, uint64_t command) {
  hpx_addr_t lco = pgas_offset_to_gpa(here->rank, command);
  hpx_lco_set(lco, 0, NULL, HPX_NULL, HPX_NULL);
  return HPX_SUCCESS;
}
COMMAND_DEF(INTERRUPT, _lco_set_handler, lco_set);

static int _release_parcel_handler(int src, command_t command) {
  uintptr_t arg = command_get_arg(command);
  hpx_parcel_t *p = (hpx_parcel_t *)arg;
  log_net("releasing sent parcel %p\n", (void*)p);
  hpx_parcel_release(p);
  return HPX_SUCCESS;
}
COMMAND_DEF(INTERRUPT, _release_parcel_handler, release_parcel);