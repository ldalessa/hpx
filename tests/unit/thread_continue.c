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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <inttypes.h>
#include "hpx/hpx.h"
#include "tests.h"
#include "libhpx/locality.h"
#include "libsync/queues.h"

#define NUM_THREADS 5
#define ARRAY_SIZE 100
#define BUFFER_SIZE 128

const int DATA_SIZE = sizeof(uint64_t);
const int SET_CONT_VALUE = 1234;

// Finish the current thread's execution, sending value to the thread's
// continuation address (size is the size of the value and value is the value
// to be sent to the thread's continuation address.
static HPX_ACTION(_set_cont, void *args) {
  uint64_t value = SET_CONT_VALUE;
  hpx_thread_continue(DATA_SIZE, &value);
}

static HPX_ACTION(thread_continue, void *UNUSED) {
  printf("Starting the Thread continue test\n");
  // Start the timer
  hpx_time_t t1 = hpx_time_now();

  hpx_addr_t *cont_fut = calloc(hpx_get_num_ranks(), sizeof(hpx_addr_t));

  for (int i = 0; i < hpx_get_num_ranks(); i++) {
    cont_fut[i] = hpx_lco_future_new(DATA_SIZE);
    hpx_parcel_t *p = hpx_parcel_acquire(NULL, 0);
    hpx_parcel_set_target(p, HPX_THERE(i));
    hpx_parcel_set_action(p, _set_cont);
    hpx_parcel_set_cont_target(p, cont_fut[i]);
    hpx_parcel_set_cont_action(p, hpx_lco_set_action);
    hpx_parcel_send(p, HPX_NULL);
    printf("Sending action with continuation to %d\n", i);
  }

  for (int i = 0; i < hpx_get_num_ranks(); i++) {
    uint64_t result;
    printf("Waiting on continuation to %d\n", i);
    hpx_lco_get(cont_fut[i], DATA_SIZE, &result);
    printf("Received continuation from %d with value %" PRIu64 "\n", i, result);
    assert(result == SET_CONT_VALUE);
    hpx_lco_delete(cont_fut[i], HPX_NULL);
  }

  free(cont_fut);

  printf(" Elapsed: %g\n", hpx_time_elapsed_ms(t1));
  return HPX_SUCCESS;
}

// Finish the current thread's execution, sending value to the thread's
// continuation address (size is the size of the value and value is the value
// to be sent to the thread's continuation address. This version gives the
// application a chance to cleanup for instance, to free the value. After
// dealing with the continued data, it will run cleanup(env).
static HPX_ACTION(_thread_cont_cleanup, void *args) {
  hpx_addr_t addr = hpx_thread_current_target();
  uint64_t local;
  if (!hpx_gas_try_pin(addr, (void**)&local))
    return HPX_RESEND;

  local = SET_CONT_VALUE;
  uint64_t *value = malloc(sizeof(uint64_t));
  *value = local;

  hpx_gas_unpin(addr);
  hpx_thread_continue_cleanup(DATA_SIZE, value, free, value);
}

static HPX_ACTION(thread_continue_cleanup, void *UNUSED) {
  printf("Starting the Thread continue cleanup test\n");
  // Start the timer
  hpx_time_t t1 = hpx_time_now();

  hpx_addr_t src = hpx_gas_alloc_local(sizeof(uint64_t));
  int rank = hpx_get_my_rank();

  uint64_t *block = malloc(DATA_SIZE);
  assert(block);

  hpx_call_sync(src, _thread_cont_cleanup, block, DATA_SIZE, &rank, sizeof(rank));
  printf("value in block is %"PRIu64"\n", *block);

  free(block);
  hpx_gas_free(src, HPX_NULL);

  printf(" Elapsed: %g\n", hpx_time_elapsed_ms(t1));
  return HPX_SUCCESS;
}

TEST_MAIN({
  ADD_TEST(thread_continue);
  ADD_TEST(thread_continue_cleanup);
});
