#ifndef ARTOS_INTERNAL_TASKS_H
#define ARTOS_INTERNAL_TASKS_H
#include "list.h"
#include "rtos.h"
#include "stddef.h"
#include <stdbool.h>

#define ARTOS_STACK_FILL_PATTERN 0xA5A5A5A5U
#define ARTOS_STACK_GUARD_PATTERN 0xDEADBEEFU
#define ARTOS_STACK_GUARD_WORDS 4U

typedef enum {
  WAIT_NO_REASON = 0,
  WAIT_SIGNALED,
  WAIT_TIMED_OUT
} rtos_wait_reason_t;

typedef struct {
  // Current saved context
  artos_stack_word_t *stack_pointer;
  // Lowest C array address
  artos_stack_word_t *stack_buffer;
  uint32_t stack_word_count;

  rtos_task_fn_t entry;
  void *argument;

  uint8_t priority;
  uint8_t effective_priority;
  uint8_t mutexes_held;

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
uint8_t rtos_current_effective_priority(void);

rtos_tcb_t *rtos_scheduler_current_task(void);

void rtos_increment_mutex_count(rtos_tcb_t *task);
void rtos_decrement_mutex_count(rtos_tcb_t *task);
void rtos_priority_inherit(rtos_tcb_t *inheritor, uint8_t donor_priority);
bool rtos_priority_relinquish_after_unlock(rtos_tcb_t *task);
bool rtos_priority_disinherit_after_timeout(rtos_tcb_t *task,
                                            uint8_t required_priority);

// IMPORTANT FOR PORT, PORT NEED TO CALL THIS
artos_stack_word_t *
rtos_scheduler_switch_context(artos_stack_word_t *current_stack_pointer);

#endif // !ARTOS_INTERNAL_TASKS_H
