#ifndef ARTOS_INTERNAL_DIAGNOSTICS_H
#define ARTOS_INTERNAL_DIAGNOSTICS_H

#include "rtos.h"

typedef enum {
  ARTOS_FAILURE_NONE,
  ARTOS_FAILURE_ASSERT,
  ARTOS_FAILURE_TASK_RETURN,
  ARTOS_FAILURE_STACK_OVERFLOW,
  ARTOS_FAILURE_SCHEDULER_CORRUPTION,
  ARTOS_FAILURE_PORT_FAULT
} artos_failure_reason_t;

typedef struct {
  artos_failure_reason_t reason;

  const char *file;
  uint32_t line;
  uint32_t tick;

  rtos_task_fn_t task_entry;
  uint8_t base_priority;
  uint8_t effective_priority;

  artos_stack_word_t *stack_pointer;
  artos_stack_word_t *stack_low;
  artos_stack_word_t *stack_high;
} rtos_failure_info_t;

extern volatile rtos_failure_info_t rtos_failure_info;

void __attribute__((noreturn))
rtos_record_failure(artos_failure_reason_t reason, const char *file,
                    uint32_t line);

#endif // !ARTOS_INTERNAL_DIAGNOSTICS_H
