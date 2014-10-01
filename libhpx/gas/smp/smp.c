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
# include "config.h"
#endif

#include <stdbool.h>
#include <jemalloc/jemalloc.h>
#include "libhpx/gas.h"
#include "libhpx/libhpx.h"

static int _smp_join(void) {
  return LIBHPX_OK;
}

static void _smp_leave(void) {
}

static void _smp_delete(gas_class_t *gas) {
}

static gas_class_t _smp_vtable = {
  .type   = HPX_GAS_SMP,
  .delete = _smp_delete,
  .join   = _smp_join,
  .leave  = _smp_leave,
  .global = {
    .malloc         = hpx_malloc,
    .free           = hpx_free,
    .calloc         = hpx_calloc,
    .realloc        = hpx_realloc,
    .valloc         = hpx_valloc,
    .memalign       = hpx_memalign,
    .posix_memalign = hpx_posix_memalign
  },
  .local  = {
    .malloc         = hpx_malloc,
    .free           = hpx_free,
    .calloc         = hpx_calloc,
    .realloc        = hpx_realloc,
    .valloc         = hpx_valloc,
    .memalign       = hpx_memalign,
    .posix_memalign = hpx_posix_memalign
  }
};

gas_class_t *gas_smp_new(size_t heap_size) {
  return &_smp_vtable;
}
