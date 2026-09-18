#include "rtos.h"

#include "rtos/ports/rtos_port.h"
#include "tasks.h"

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
void rtos_yield_from_isr(void) { rtos_port_request_context_switch_from_isr(); }

void rtos_wait(uint32_t tick_count) {
  if (tick_count > 0) {
    rtos_port_irq_state_t prev_state = rtos_port_enter_critical();
    rtos_block_current_task(tick_count == RTOS_DELAY_INFINITY
                                ? RTOS_DELAY_INFINITY
                                : rtos_current_tick() + tick_count,
                            NULL, tick_count == RTOS_DELAY_INFINITY);
    rtos_port_exit_critical(prev_state);
  }

  rtos_port_request_context_switch();
}

uint32_t rtos_get_tick(void) { return rtos_current_tick(); }

void rtos_wait_until(uint32_t wake_tick) {
  rtos_port_irq_state_t prev_state = rtos_port_enter_critical();
  uint32_t delta = (wake_tick - rtos_current_tick());
  if (delta != 0 && delta < 0x80000000)
    rtos_block_current_task(wake_tick, NULL, 0);
  rtos_port_exit_critical(prev_state);
  rtos_port_request_context_switch();
}
