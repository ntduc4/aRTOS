#include "rtos.h"
#include "rtos/ports/rtos_port.h"
#include "rtos_internal.h"

void rtos_init(void) {
  rtos_task_system_init();
  rtos_scheduler_init();
}

void rtos_start(void) {
  rtos_tcb_t *first_task = rtos_scheduler_select_next();
  if (first_task == NULL)
    return;

  rtos_port_start_first_task(first_task->stack_pointer);

  // Should never reach here
  for (;;) {
  }
}
