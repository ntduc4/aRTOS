#ifndef RTOS_H
#define RTOS_H
#include "rtos_config.h"
#include <stddef.h>
#include <stdint.h>

#define RTOS_MS_TO_TICKS(ms)                                                   \
  ((uint32_t)(((uint64_t)(ms) * (uint64_t)RTOS_TICK_HZ) / 1000U))
#define RTOS_DELAY_INFINITY 0xFFFFFFFFU
#define RTOS_TICK_MSK 0x80000000U

typedef void (*rtos_task_fn_t)(void *argument);

void rtos_init(void);

typedef enum {
  RTOS_OK = 0,
  RTOS_ERROR_INVALID_ARGUMENT,
  RTOS_ERROR_TASK_LIMIT,
  RTOS_ERROR_STACK_TOO_SMALL,
  RTOS_ERROR_NO_TASKS
} rtos_status_t;

typedef uintptr_t rtos_stack_word_t;

// Static task allocation
// `stack` should be 8 bytes aligned
rtos_status_t rtos_task_create(rtos_task_fn_t entry, void *argument,
                               rtos_stack_word_t *stack,
                               uint32_t stack_word_count);
rtos_status_t rtos_start(void);
void rtos_yield(void);
// Passing tick_count = 0 have the same behaviour as rtos_yield
void rtos_wait(uint32_t tick_count);
void rtos_wait_until(uint32_t wake_tick);
uint32_t rtos_get_tick(void);

#endif // !RTOS_H
