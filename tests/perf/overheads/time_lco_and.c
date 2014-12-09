#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <unistd.h>
#include "hpx/hpx.h"

#define BENCHMARK "HPX COST OF LCO AND"

#define HEADER "# " BENCHMARK "\n"
#define FIELD_WIDTH 10
#define HEADER_FIELD_WIDTH 5

static int num[] = {
  1000000,
  2000000,
  3000000,
  4000000,
  5000000
};

static void _usage(FILE *stream) {
  fprintf(stream, "Usage: time_lco_and overhead \n"
          "\t-h, this help display\n");
  hpx_print_help();
  fflush(stream);
}

static hpx_action_t _lco_set  = 0;
static hpx_action_t _main = 0;
static hpx_action_t _empty = 0;


static int _lco_set_action(void *args) {
  hpx_lco_and_set(*(hpx_addr_t*)args, HPX_NULL);
  return HPX_SUCCESS;
}

static int _empty_action(hpx_addr_t *args) {
  return HPX_SUCCESS;
}

static int _main_action(int *args) {
  hpx_time_t t, t1;
  fprintf(stdout, HEADER);
  fprintf(stdout, "# Latency in (ms)\n");
  fprintf(stdout, "%s%*s%*s%*s\n", "# Iters " , FIELD_WIDTH, "Init time ",
          FIELD_WIDTH, "LCO Set", FIELD_WIDTH, "Delete");

  for (int i = 0; i < sizeof(num)/sizeof(num[0]) ; i++) {
    fprintf(stdout, "%d", num[i]);

    hpx_addr_t done = hpx_lco_future_new(num[i]);

    t = hpx_time_now();
    hpx_addr_t setlco = hpx_lco_and_new(num[i]);
    fprintf(stdout, "%*g", FIELD_WIDTH, hpx_time_elapsed_ms(t));

    // Time it take to set empty action
    hpx_addr_t completed = hpx_lco_and_new(num[i]);
    t1 = hpx_time_now();
    for (int j = 0; j < num[i]; j++)
      hpx_call(HPX_HERE, _empty, &setlco, sizeof(setlco), completed);
    hpx_lco_wait(completed);
    double empty_t = hpx_time_elapsed_ms(t1);
    hpx_lco_delete(completed, HPX_NULL);

    // Time to set for LCO argument
    t = hpx_time_now();
    for (int j = 0; j < num[i]; j++)
      hpx_call(HPX_HERE, _lco_set, &setlco, sizeof(setlco), done);
    hpx_lco_wait(setlco);
    double end_t = hpx_time_elapsed_ms(t);
    fprintf(stdout, "%*g",FIELD_WIDTH, end_t - empty_t);

    t = hpx_time_now();
    hpx_lco_delete(setlco, HPX_NULL);
    fprintf(stdout, "%*g\n",FIELD_WIDTH, hpx_time_elapsed_ms(t));

    hpx_lco_delete(done, HPX_NULL);
  }

  fclose(stdout);
  hpx_shutdown(HPX_SUCCESS);
}

int main(int argc, char *argv[]) {

  if (hpx_init(&argc, &argv)) {
    fprintf(stderr, "HPX: failed to initialize.\n");
    return 1;
  }

  int opt = 0;
  while ((opt = getopt(argc, argv, "h?")) != -1) {
    switch (opt) {
     case 'h':
      _usage(stdout);
      return 0;
     case '?':
     default:
      _usage(stderr);
      return -1;
    }
  }

  // register the actions
  HPX_REGISTER_ACTION(&_lco_set, _lco_set_action);
  HPX_REGISTER_ACTION(&_main, _main_action);
  HPX_REGISTER_ACTION(&_empty, _empty_action);

  // run the main action
  return hpx_run(&_main, NULL, 0);
}