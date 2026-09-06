#ifndef RTOS_H
#define RTOS_H
#include <stdint.h>

typedef void (*rtos_task_fn_t)(void *argument);

void rtos_init(void);

typedef enum {
  RTOS_OK = 0,
  RTOS_ERROR_INVALID_ARGUMENT,
  RTOS_ERROR_TASK_LIMIT,
  RTOS_ERROR_STACK_TOO_SMALL
} rtos_status_t;

// Static task allocation
rtos_status_t rtos_task_create(rtos_task_fn_t entry, void *argument,
                               uint32_t *stack, uint32_t stack_word_count);

#endif // !RTOS_H
