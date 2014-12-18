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

#define _HPX(type, action)                              \
  static int action##_##type(void*);                    \
  static hpx_action_t action;                           \
  static __attribute__((constructor))                   \
  void _register_##action##_##type(void) {              \
    HPX_REGISTER_##type(&action, action##_##type);      \
  }                                                     \
  static int action##_##type

#define HPX_ACTION(n)       _HPX(ACTION, n)
#define HPX_TASK(n)         _HPX(TASK, n)
#define HPX_INTERRUPT(n)    _HPX(INTERRUPT, n)


HPX_INTERRUPT(_my_interrupt)(void *args) {
  printf("Hi, I am an interrupt!\n");
  return HPX_SUCCESS;
}

HPX_TASK(_my_task)(void *args) {
  printf("Hi, I am a task!\n");
  hpx_call_cc(HPX_HERE, _my_interrupt, NULL, 0, NULL, NULL);
  return HPX_SUCCESS;
}

HPX_ACTION(_my_action)(void *args) {
  printf("Hi, I am an action!\n");
  hpx_call_cc(HPX_HERE, _my_task, NULL, 0, NULL, NULL);
  return HPX_SUCCESS;
}

HPX_ACTION(_main)(void *args) {
  hpx_call_sync(HPX_HERE, _my_action, NULL, 0, NULL, 0);
  hpx_shutdown(HPX_SUCCESS);
}


int main(int argc, char *argv[]) {
  hpx_init(&argc, &argv);
  return hpx_run(&_main, NULL, 0);
}