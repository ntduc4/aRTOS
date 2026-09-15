#include "rtos/ports/rtos_port.h"
#include "rtos_config.h"
#include "rtos_internal.h"

static rtos_tcb_t *current_task;
static uint32_t index = 0;
static uint32_t tick = 0;

void rtos_scheduler_init() {
  rtos_port_scheduler_init();
  current_task = 0;
  index = 0;
  tick = 0;
}

// Make sure to only call when context switch
// Make sure to already in critical state
//
// TODO: Add different scheduling algo support later
static rtos_tcb_t *
rtos_scheduler_select_next(rtos_stack_word_t *current_stack_pointer) {
  // Treat as start if NULL
  if (current_task && current_task->state == RTOS_TASK_RUNNING)
    current_task->state = RTOS_TASK_READY;

  if (current_stack_pointer == NULL) {
    for (uint32_t i = 0U; i < RTOS_MAX_TASKS; i++) {
      rtos_tcb_t *task = rtos_task_at(i);
      if (task->state == RTOS_TASK_READY) {
        current_task = task;
        current_task->state = RTOS_TASK_RUNNING;
        index = i;
        return current_task;
      }
    }

    return NULL;
  }

  current_task->stack_pointer = current_stack_pointer;
  uint32_t i = index;
  do {
    i = (i + 1) % RTOS_MAX_TASKS;
    rtos_tcb_t *task = rtos_task_at(i);
    if (task->state == RTOS_TASK_READY) {
      current_task = task;
      current_task->state = RTOS_TASK_RUNNING;
      index = i;
      return current_task;
    }
  } while (i != index);

  // Return idle task as default if no task is schedulable
  current_task = rtos_idle_task();
  current_task->state = RTOS_TASK_RUNNING;
  return current_task;
};

rtos_tcb_t *rtos_scheduler_start(void) {
  return rtos_scheduler_select_next(NULL);
};

rtos_stack_word_t *
rtos_scheduler_switch_context(rtos_stack_word_t *current_stack_pointer) {
  rtos_port_irq_state_t prev_state = rtos_port_enter_critical();
  rtos_tcb_t *next = rtos_scheduler_select_next(current_stack_pointer);
  rtos_port_exit_critical(prev_state);

  // Should never happen
  if (next == NULL)
    return NULL;

  return next->stack_pointer;
}

void rtos_scheduler_block_current_task(void) {
  if (current_task != NULL)
    current_task->state = RTOS_TASK_BLOCKED;
}

void rtos_scheduler_unblock_task(uint32_t index) {
  if (index >= RTOS_MAX_TASKS)
    return;

  rtos_tcb_t *task = rtos_task_at(index);
  if (task->state == RTOS_TASK_BLOCKED)
    task->state = RTOS_TASK_READY;
}

void rtos_scheduler_tick(void) {
  tick++;
  // TODO: Add conditional preemption here once blocking is implemented
  rtos_port_request_context_switch();
}

const rtos_tcb_t *rtos_scheduler_current_task(void) { return current_task; }
