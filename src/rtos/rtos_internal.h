#ifndef RTOS_INTERNAL_H
#define RTOS_INTERNAL_H
#include "rtos.h"
#include "stddef.h"

typedef enum {
  RTOS_TASK_UNUSED = 0,
  RTOS_TASK_READY,
  RTOS_TASK_RUNNING,
  RTOS_TASK_BLOCKED
} rtos_task_state_t;

typedef struct {
  // Current saved context
  rtos_stack_word_t *stack_pointer;
  // Lowest C array address
  rtos_stack_word_t *stack_buffer;
  uint32_t stack_word_count;

  rtos_task_fn_t entry;
  void *argument;

  rtos_task_state_t state;
} rtos_tcb_t;

void rtos_task_system_init(void);
void rtos_scheduler_init(void);

rtos_tcb_t *rtos_task_at(uint32_t index);
rtos_tcb_t *rtos_scheduler_select_next(void);

#endif // !RTOS_INTERNAL_H
