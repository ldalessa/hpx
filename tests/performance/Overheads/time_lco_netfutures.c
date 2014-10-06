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

#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "hpx/hpx.h"
#include "hpx/future.h"
#include "common.h"

#define BUFFER_SIZE 128
#define FIELD_WIDTH 20

/* command line options */
static bool         _text = false;            //!< send text data with the ping
static bool      _verbose = false;            //!< print to the terminal

/* actions */
static hpx_action_t _main = 0;
static hpx_action_t _ping = 0;
static hpx_action_t _pong = 0;

static int iters[] = {
  10,
  100,
  1000,
  10000,
  100000
};
/* helper functions */
static void _usage(FILE *stream) {
  fprintf(stream, "Usage: pingponghpx \n"
          "\t-c, the number of cores to run on\n"
          "\t-t, the number of scheduler threads\n"
          "\t-T, select a transport by number (see hpx_config.h)\n"
          "\t-m, send text in message\n"
          "\t-v, print verbose output \n"
          "\t-D, all localities wait for debugger\n"
          "\t-d, wait for debugger at specific locality\n"
          "\t-h, show help\n");
}

static void _register_actions(void);

/** the pingpong message type */
typedef struct {
  int iterations;
  hpx_netfuture_t pingpong;
} args_t;

/* utility macros */
#define CHECK_NOT_NULL(p, err)                                \
  do {                                                        \
  if (!p) {                                                 \
  fprintf(stderr, err);                                   \
  hpx_shutdown(1);                                        \
  }                                                         \
  } while (0)

#define RANK_PRINTF(format, ...)                                        \
  do {                                                                  \
  if (_verbose)                                                       \
    printf("\t%d,%d: " format, hpx_get_my_rank(), hpx_get_my_thread_id(), \
	   __VA_ARGS__);                                              \
  } while (0)

int main(int argc, char *argv[]) {
  hpx_config_t cfg = HPX_CONFIG_DEFAULTS;
  int opt = 0;
  while ((opt = getopt(argc, argv, "c:t:T:d:Dmvh")) != -1) {
    switch (opt) {
    case 'c':
      cfg.cores = atoi(optarg);
      break;
    case 't':
      cfg.threads = atoi(optarg);
      break;
    case 'T':
      cfg.transport = atoi(optarg);
      assert(0 <= cfg.transport && cfg.transport < HPX_TRANSPORT_MAX);
      break;
    case 'm':
      _text = true;
    case 'v':
      _verbose = true;
      break;
    case 'D':
      cfg.wait = HPX_WAIT;
      cfg.wait_at = HPX_LOCALITY_ALL;
      break;
    case 'd':
      cfg.wait = HPX_WAIT;
      cfg.wait_at = atoi(optarg);
      break;
    case 'h':
      _usage(stdout);
      return 0;
    case '?':
    default:
      _usage(stderr);
      return -1;
    }
  }

  int e = hpx_init(&cfg);
  if (e) {
    fprintf(stderr, "Failed to initialize hpx\n");
    return -1;
  }

  test_log = fopen("test.log", "a+");
  fprintf(test_log, "Starting the cost of Netfutures pingpong benchmark\n");
  fprintf(test_log, "%s%*s\n", "# ITERS ", FIELD_WIDTH, "LATENCY (ms)");
  _register_actions();

  return hpx_run(_main, NULL, 0);
}

static int _action_main(void *args) {
  hpx_status_t status =  hpx_netfutures_init();
  if (status != HPX_SUCCESS)
    return status;

  for (int i = 0; i < sizeof(iters)/sizeof(iters[0]); i++) {
    fprintf(test_log, "%d\t", iters[i]);

    args_t args = {
      .iterations = iters[i],
    };

    hpx_time_t start = hpx_time_now();
    hpx_addr_t done = hpx_lco_and_new(2);

    hpx_netfuture_t base = hpx_lco_netfuture_new_all(2, BUFFER_SIZE);
    args.pingpong = base;

    hpx_call(HPX_HERE, _ping, &args, sizeof(args), done);
    hpx_call(HPX_THERE(1), _pong, &args, sizeof(args), done);

    hpx_lco_wait(done);
    hpx_lco_delete(done, HPX_NULL);

    double elapsed = (double)hpx_time_elapsed_ms(start);
    double latency = elapsed / (args.iterations * 2);
    fprintf(test_log, "%*f\n", FIELD_WIDTH, latency);
  }
  hpx_netfutures_fini();
  hpx_shutdown(HPX_SUCCESS);
}

/**
 * Send a ping message.
 */
static int _action_ping(args_t *args) {
  hpx_addr_t msg_ping_gas = hpx_gas_alloc(BUFFER_SIZE);
  hpx_addr_t msg_pong_gas = hpx_gas_alloc(BUFFER_SIZE);
  char *msg_ping;
  char *msg_pong;
  hpx_gas_try_pin(msg_ping_gas, (void**)&msg_ping);

  for (int i = 0; i < args->iterations; i++) {
    hpx_addr_t lsync = hpx_lco_future_new(0);
    hpx_lco_netfuture_setat(args->pingpong, 1, BUFFER_SIZE, msg_ping_gas, lsync, HPX_NULL);
    hpx_lco_wait(lsync);
    hpx_lco_delete(lsync, HPX_NULL);
    msg_pong_gas = hpx_lco_netfuture_getat(args->pingpong, 0, BUFFER_SIZE);
    hpx_gas_try_pin(msg_pong_gas, (void**)&msg_pong);
  }

  return HPX_SUCCESS;
}


/**
 * Handle a pong action.
 */
static int _action_pong(args_t *args) {
  hpx_addr_t msg_ping_gas = hpx_gas_alloc(BUFFER_SIZE);
  hpx_addr_t msg_pong_gas = hpx_gas_alloc(BUFFER_SIZE);
  char *msg_ping;
  char *msg_pong;
  hpx_gas_try_pin(msg_pong_gas, (void**)&msg_pong);

  for (int i = 0; i < args->iterations; i++) {
    msg_ping_gas = hpx_lco_netfuture_getat(args->pingpong, 1, BUFFER_SIZE);
    hpx_gas_try_pin(msg_ping_gas, (void**)&msg_ping);

    hpx_addr_t lsync = hpx_lco_future_new(0);
    hpx_lco_netfuture_setat(args->pingpong, 0, BUFFER_SIZE, msg_pong_gas, lsync, HPX_NULL);
    hpx_lco_wait(lsync);
    hpx_lco_delete(lsync, HPX_NULL);

  }

  return HPX_SUCCESS;
}

/**
 * Registers functions as actions.
 */
void _register_actions(void) {
  /* register action for parcel (must be done by all ranks) */
  _main = HPX_REGISTER_ACTION(_action_main);
  _ping = HPX_REGISTER_ACTION(_action_ping);
  _pong = HPX_REGISTER_ACTION(_action_pong);
}

