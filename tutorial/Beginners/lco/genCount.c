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
#include <hpx/hpx.h>

static  hpx_action_t _main = 0;
static  hpx_action_t _increment  = 0;

int ninplace = 5;

static int _increment_action(void *args) {
  hpx_lco_gencount_inc(*(hpx_addr_t*)args, HPX_NULL);
  return HPX_SUCCESS;
}

static int _main_action(void *args) {
  hpx_addr_t lco = hpx_lco_gencount_new(ninplace);
  hpx_addr_t done = hpx_lco_future_new(0);
  for (int i = 0; i < ninplace; i++)
    hpx_call(HPX_HERE, _increment, done, &lco, sizeof(lco));

  hpx_lco_gencount_wait(lco, ninplace - 1);

  hpx_lco_wait(done);

  hpx_lco_delete(done, HPX_NULL);
  hpx_lco_delete(lco, HPX_NULL);

  printf("Gencount Set succeeded\n");

  hpx_shutdown(HPX_SUCCESS);
}

int main(int argc, char *argv[]) {
  int e = hpx_init(&argc, &argv);
  if (e) {
    fprintf(stderr, "HPX: failed to initialize.\n");
    return e;
  }
   
  HPX_REGISTER_ACTION(_main_action, &_main);
  HPX_REGISTER_ACTION(_increment_action, &_increment);

  return hpx_run(&_main, NULL, 0);
}
