#include "rtos.h"
#include "rtos/ports/rtos_port.h"
#include "rtos_internal.h"

void rtos_init(void) { rtos_system_init(); }

rtos_status_t rtos_start(void) {
  rtos_tcb_t *first_task = rtos_scheduler_start();
  if (first_task == NULL)
    return RTOS_ERROR_NO_TASKS;

  rtos_port_start_first_task(first_task->stack_pointer);

  // Should never reach here
  for (;;) {
  }
}

void rtos_yield(void) { rtos_port_request_context_switch(); }

void rtos_wait(uint32_t tick_count) {
  if (tick_count > 0)
    rtos_block_current_task(rtos_current_tick() + tick_count, NULL);
  rtos_port_request_context_switch();
}

uint32_t rtos_get_tick(void) { return rtos_current_tick(); }

void rtos_wait_until(uint32_t wake_tick) {
  if (wake_tick > rtos_current_tick())
    rtos_block_current_task(wake_tick, NULL);
  rtos_port_request_context_switch();
}
