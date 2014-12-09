#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include "hpx/hpx.h"

#define SIZE 1024
static uint64_t src[SIZE];

static hpx_action_t _main = 0;
static hpx_action_t _verify = 0;

int _verify_action(void *args) {
  hpx_addr_t target = hpx_thread_current_target();
  uint64_t *local;
  if (!hpx_gas_try_pin(target, (void**)&local))
    return HPX_RESEND;

  bool result = false;
  for (int i = 0; i < SIZE; ++i)
    result |= (local[i] != src[i]);

  hpx_gas_unpin(target);
  HPX_THREAD_CONTINUE(result);
}

static int _main_action(void *args) {
  int size = HPX_LOCALITIES;

  for (int i = 0; i < SIZE; i++)
    src[i] = (uint64_t)i;

  hpx_addr_t data = hpx_gas_global_alloc(size, sizeof(src));

  hpx_addr_t done = hpx_lco_future_new(0);
  hpx_gas_memput(data, src, sizeof(src), HPX_NULL, done);
  hpx_lco_wait(done);
  hpx_lco_delete(done, HPX_NULL);

  bool output = false;
  hpx_call_sync(data, _verify, NULL, 0, &output, sizeof(output));
  assert(output == false);
  printf("The hpx_gas_memput succeeded for size = %d\n", SIZE);

  hpx_gas_free(data, HPX_NULL);
  hpx_shutdown(HPX_SUCCESS);
}

int main(int argc, char *argv[argc]) {
  if (hpx_init(&argc, &argv)) {
    fprintf(stderr, "HPX failed to initialize.\n");
    return 1;
  }

  HPX_REGISTER_ACTION(&_main, _main_action);
  HPX_REGISTER_ACTION(&_verify, _verify_action);
  return hpx_run(&_main, NULL, 0);
}