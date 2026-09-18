#ifndef RTOS_TASKS_H
#define RTOS_TASKS_H
#include "list.h"
#include "rtos.h"
#include "stddef.h"
#include <stdbool.h>

typedef enum {
  WAIT_NO_REASON = 0,
  WAIT_SIGNALED,
  WAIT_TIMED_OUT
} rtos_wait_reason_t;

typedef struct {
  // Current saved context
  rtos_stack_word_t *stack_pointer;
  // Lowest C array address
  rtos_stack_word_t *stack_buffer;
  uint32_t stack_word_count;

  rtos_task_fn_t entry;
  void *argument;

  rtos_list_item_t state_item;
  rtos_list_item_t event_item;
  rtos_wait_reason_t wait_reason;
} rtos_tcb_t;

void rtos_system_init(void);
rtos_tcb_t *rtos_scheduler_start(void);
void rtos_tick_handler(void);
uint32_t rtos_current_tick(void);
// Assume already in critical section
void rtos_block_current_task(uint32_t wake_tick, rtos_list_t *wait_obj,
                             bool infinite);
// Assume already in critical section
void rtos_unblock_task(rtos_list_item_t *task_item, rtos_wait_reason_t reason);

rtos_wait_reason_t rtos_current_wait_reason(void);
void rtos_clear_current_wait_reason(void);

// For debug purposes ONLY
const rtos_tcb_t *rtos_scheduler_current_task(void);

// IMPORTANT FOR PORT, PORT NEED TO CALL THIS
rtos_stack_word_t *
rtos_scheduler_switch_context(rtos_stack_word_t *current_stack_pointer);

#endif // !RTOS_TASKS_H
