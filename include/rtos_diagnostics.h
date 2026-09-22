#ifndef ARTOS_DIAGNOSTICS_H
#define ARTOS_DIAGNOSTICS_H

#include "rtos.h"

typedef enum {
  ARTOS_TASK_STATE_RUNNING,
  ARTOS_TASK_STATE_READY,
  ARTOS_TASK_STATE_BLOCKED,
  ARTOS_TASK_STATE_SUSPENDED
} artos_task_state_t;

typedef struct {
  rtos_task_fn_t entry;
  artos_task_state_t state;

  uint8_t base_priority;
  uint8_t effective_priority;
  uint8_t mutexes_held;

  uint32_t stack_word_count;
  uint32_t stack_free_words;
  bool stack_guard_valid;
} artos_task_info_t;

bool artos_task_get_info(uint32_t index, artos_task_info_t *info);

#endif // !ARTOS_DIAGNOSTICS_H
