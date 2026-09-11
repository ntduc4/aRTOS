#include "rtos_config.h"
#include "rtos_internal.h"

rtos_tcb_t *volatile current_task;

void rtos_scheduler_init() { current_task = 0; }

// TODO: Change to round robin / whatever the scheduling policy is
rtos_tcb_t *rtos_scheduler_select_next(void) {
  for (uint32_t i = 0U; i < RTOS_MAX_TASKS; i++) {
    rtos_tcb_t *task = rtos_task_at(i);
    if (task->state == RTOS_TASK_READY) {
      current_task = task;
      current_task->state = RTOS_TASK_RUNNING;
      return current_task;
    }
  }

  return NULL;
};
