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

#include <libhpx/gpa.h>
#include <libhpx/locality.h>
#include <libhpx/network.h>

static int _lco_set_handler(int src, uint64_t command) {
  hpx_addr_t lco = offset_to_gpa(here->rank, command);
  hpx_lco_set(lco, 0, NULL, HPX_NULL, HPX_NULL);
  return HPX_SUCCESS;
}
COMMAND_DEF(lco_set, _lco_set_handler);