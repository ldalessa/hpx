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

#include <stdio.h>
#include <inttypes.h>
#include <hpx/hpx.h>

static  hpx_action_t _main       = 0;
#define MAX_BYTES      1024*1024*100

static int _main_action(void *args) {
  uint64_t size = MAX_BYTES;
  int blocks = HPX_LOCALITIES;

  printf("Allocating a block of global memory of size = %" PRIu64 "\n", size);
  hpx_addr_t local = hpx_gas_alloc(size);
  printf("Free a global allocation\n");
  hpx_gas_free(local, HPX_NULL);

  printf("Allocating distributed global memory of num locailities = %d, \
         size = %" PRIu64 "\n", blocks, size);
  hpx_addr_t global = hpx_gas_global_alloc(blocks, size);
  printf("Free a global allocation\n");
  hpx_gas_free(global, HPX_NULL);
 
  printf("Allocating distributed global zeroed memory of num localities = %d, \
          size = %" PRIu64 "\n", blocks, size);
  hpx_addr_t calloc = hpx_gas_global_calloc(blocks, size);
  printf("Free a global allocation\n");
  hpx_gas_free(calloc, HPX_NULL);
 
  hpx_shutdown(HPX_SUCCESS);
}

int main(int argc, char *argv[]) {
  int e = hpx_init(&argc, &argv);
  if (e) {
    fprintf(stderr, "HPX: failed to initialize.\n");
    return e;
  }
   
  HPX_REGISTER_ACTION(&_main, _main_action);

  return hpx_run(&_main, NULL, 0);
}