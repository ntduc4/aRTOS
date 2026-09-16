#ifndef RTOS_INTERNAL_H
#define RTOS_INTERNAL_H
#include "rtos.h"
#include "stddef.h"

#define RTOS_INFINITY 0xFFFFFFFFU

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
  uint32_t wake_tick;
  void *wait_obj; // For future wake events (semaphore, event flag)
} rtos_tcb_t;

// Task and scheduler
void rtos_system_init(void);
rtos_tcb_t *rtos_scheduler_start(void);
void rtos_tick_handler(void);
uint32_t rtos_current_tick(void);
void rtos_block_current_task(uint32_t wake_tick, void *wait_obj);
void rtos_unblock_task(uint32_t index);

// For debug purposes ONLY
const rtos_tcb_t *rtos_scheduler_current_task(void);

// IMPORTANT FOR PORT, PORT NEED TO CALL THIS
rtos_stack_word_t *
rtos_scheduler_switch_context(rtos_stack_word_t *current_stack_pointer);

#endif // !RTOS_INTERNAL_H
