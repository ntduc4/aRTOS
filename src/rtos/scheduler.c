#include "rtos/ports/rtos_port.h"
#include "rtos_config.h"
#include "rtos_internal.h"

rtos_tcb_t *volatile current_task;

void rtos_scheduler_init() {
  rtos_port_scheduler_init();
  current_task = 0;
}

// TODO: Change to round robin / whatever the scheduling policy is
static rtos_tcb_t *
rtos_scheduler_select_next(rtos_stack_word_t *current_stack_pointer) {
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

rtos_tcb_t *rtos_scheduler_start(void) {
  return rtos_scheduler_select_next(NULL);
};

rtos_stack_word_t *
rtos_scheduler_switch_context(rtos_stack_word_t *current_stack_pointer) {
  rtos_tcb_t *next = rtos_scheduler_select_next(current_stack_pointer);
  // TODO: fix this somehow probably???
  if (next == NULL)
    return NULL;
  return next->stack_pointer;
}
