#ifndef RTOS_H
#define RTOS_H
#include <stddef.h>
#include <stdint.h>

typedef void (*rtos_task_fn_t)(void *argument);

void rtos_init(void);

typedef enum {
  RTOS_OK = 0,
  RTOS_ERROR_INVALID_ARGUMENT,
  RTOS_ERROR_TASK_LIMIT,
  RTOS_ERROR_STACK_TOO_SMALL
} rtos_status_t;

typedef uintptr_t rtos_stack_word_t;

// Static task allocation
// `stack` should be 8 bytes aligned
rtos_status_t rtos_task_create(rtos_task_fn_t entry, void *argument,
                               rtos_stack_word_t *stack,
                               uint32_t stack_word_count);
// TO BE IMPLEMENT
void rtos_start(void);
// void rtos_wait(void);
// void rtos_yield(void);

#endif // !RTOS_H
