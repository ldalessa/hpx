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

// Goal of this testcase is to test the HPX Memory Allocation
// 1. hpx_gas_global_free() -- Free a global allocation.
// 2. hpx_gas_global_alloc() -- Allocates the distributed
//                              global memory
#include "hpx/hpx.h"
#include "tests.h"

typedef struct {
  int nDoms;
  int maxCycles;
  int cores;
} main_args_t;

typedef struct Domain {
  hpx_addr_t complete;
  hpx_addr_t newdt;
  int nDoms;
  int rank;
  int maxcycles;
  int cycle;
} Domain;

static int _initDomain_handler(int rank, int max, int n) {
  Domain *ld = hpx_thread_current_local_target();
  ld->rank = rank;
  ld->maxcycles = max;
  ld->nDoms = n;
  return HPX_SUCCESS;
}

HPX_ACTION_DEF(PINNED, _initDomain_handler, _initDomain, HPX_INT, HPX_INT, HPX_INT);

// Test code -- for global memory allocation
static HPX_ACTION(test_libhpx_gas_global_alloc, void *UNUSED) {
  // allocate the default argument structure on the stack

  int nDoms = 8;
  int maxCycles = 1;

  printf("Starting the GAS global memory allocation test\n");
  // allocate and start a timer
  hpx_time_t t1 = hpx_time_now();

 // output the arguments we're running with
  printf("Number of domains: %d maxCycles: %d cores: %d\n",
          nDoms, maxCycles, 8);
  fflush(stdout);

  // Allocate the domain array
  hpx_addr_t domain = hpx_gas_global_alloc(nDoms, sizeof(Domain));

  // Allocate an and gate that we can wait on to detect that all of the domains
  // have completed initialization.
  hpx_addr_t done = hpx_lco_and_new(nDoms);

  // Send the initDomain action to all of the domains, in parallel.
  for (int i = 0, e = nDoms; i < e; ++i) {
    // compute the offset for this domain and send the initDomain action, with
    // the done LCO as the continuation
    hpx_addr_t block = hpx_addr_add(domain, sizeof(Domain) * i, sizeof(Domain));
    hpx_call(block, _initDomain, done, &i, &maxCycles, &nDoms);
  }

  // wait for initialization
  hpx_lco_wait(done);
  hpx_lco_delete(done, HPX_NULL);

  // and free the domain
  hpx_gas_free(domain, HPX_NULL);

  printf(" Elapsed: %g\n", hpx_time_elapsed_ms(t1));
}

static HPX_ACTION(test_libhpx_gas_global_alloc_block, void *UNUSED) {
  hpx_addr_t data = hpx_gas_global_alloc(1, 1024 * sizeof(char));
  hpx_gas_free(data, HPX_NULL);
}

TEST_MAIN({
 ADD_TEST(test_libhpx_gas_global_alloc);
 ADD_TEST(test_libhpx_gas_global_alloc_block);
});