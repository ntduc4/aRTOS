#ifndef RTOS_H
#define RTOS_H
#include "rtos_config.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define RTOS_MS_TO_TICKS(ms)                                                   \
  ((uint32_t)(((uint64_t)(ms) * (uint64_t)RTOS_TICK_HZ) / 1000U))
#define RTOS_DELAY_INFINITY 0xFFFFFFFFU

typedef void (*rtos_task_fn_t)(void *argument);

void rtos_init(void);

// ===============
// Tasks
// ===============

// Status
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
// Does not have RTOS_DELAY_INFINITY semantic
// Valid future horizon is 1 - 0x7fffffff, longer future = past
void rtos_wait_until(uint32_t wake_tick);
uint32_t rtos_get_tick(void);

// ===============
// Sync primitives
// ===============

// Semaphore
typedef struct rtos_binary_semaphore rtos_binary_semaphore_t;

// Kernel private, do not use this
typedef struct {
  uint32_t value;
  void *_p, *_n, *_o, *_c;
} rtos_static_list_item_t;

// Kernel private, do not use this
typedef struct {
  uint32_t _c;
  rtos_static_list_item_t _s;
} rtos_static_list_t;

// Must not be copied or moved
typedef struct {
  // Kernel private, do not touch this
  bool _a;
  // Kernel private, do not touch this
  rtos_static_list_t _w;
} rtos_semaphore_storage_t;

rtos_binary_semaphore_t *
rtos_binary_semaphore_init(rtos_semaphore_storage_t *storage,
                           bool initially_available);

// tick_timeout == 0 won't block
// tick_timeout == RTOS_DELAY_INFINITY will wait indefinitely until signaled
// Return true if successfully obtain the semaphore, false if timeout
// Task-context-only and may block, must not be in critical section beforehand
bool rtos_binary_semaphore_wait(rtos_binary_semaphore_t *semaphore,
                                uint32_t tick_timeout);
// Never block, return successfully take or not
bool rtos_binary_semaphore_take_isr(rtos_binary_semaphore_t *semaphore);
// Never block, return whether a task was awaken not
bool rtos_binary_semaphore_signal_isr(rtos_binary_semaphore_t *semaphore);

// Will immidiately yield if there's one waiting for better latency
void rtos_binary_semaphore_signal(rtos_binary_semaphore_t *semaphore);

#endif // !RTOS_H
