#include "rtos.h"
#include "rtos/ports/rtos_port.h"
#include "rtos_internal.h"

void rtos_init(void) {
  rtos_task_system_init();
  rtos_scheduler_init();
}

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
