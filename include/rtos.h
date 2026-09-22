#ifndef ARTOS_H
#define ARTOS_H

/**
 * @file rtos.h
 * @brief Public API for the completely static, fixed-priority aRTOS kernel.
 *
 * @par Scheduling model
 * aRTOS uses preemptive fixed-priority scheduling. Priority 0 is the lowest
 * priority, and larger values represent higher priorities. The scheduler
 * always selects a ready task at the highest priority. Ready tasks at the same
 * priority execute round-robin in FIFO order.
 */

#include "rtos_config.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * @brief Convert milliseconds to kernel ticks.
 * @param ms Duration in milliseconds.
 * @return Duration converted using ARTOS_TICK_HZ, rounded down.
 */
#define ARTOS_MS_TO_TICKS(ms)                                                  \
  ((uint32_t)(((uint64_t)(ms) * (uint64_t)ARTOS_TICK_HZ) / 1000U))

/** @brief Timeout value that represents an infinite wait. */
#define ARTOS_DELAY_INFINITY 0xFFFFFFFFU

/**
 * @brief Task entry-point function type.
 * @param argument Application argument supplied when the task is created.
 * @note Task entry functions must not return.
 */
typedef void (*artos_task_fn_t)(void *argument);

/**
 * @brief Initialize the kernel and architecture port.
 * @note Call once before creating tasks or starting the scheduler.
 */
void artos_init(void);

// =========================
//           Tasks
// =========================

/** @brief Status codes returned by kernel lifecycle operations. */
typedef enum {
  ARTOS_OK = 0,
  ARTOS_ERROR_INVALID_ARGUMENT,
  ARTOS_ERROR_TASK_LIMIT,
  ARTOS_ERROR_STACK_TOO_SMALL,
  ARTOS_ERROR_NO_TASKS
} artos_status_t;

/** @brief Native stack word type used by the active architecture port. */
typedef uintptr_t artos_stack_word_t;

/**
 * @brief Create a task using caller-owned stack storage.
 * @param entry Task entry point. Must not be NULL and must not return.
 * @param argument Argument passed to @p entry. May be NULL.
 * @param stack Persistent stack buffer owned by the caller.
 * @param stack_word_count Number of artos_stack_word_t elements in @p stack.
 * @param priority Task priority in the range 0 through
 *        `ARTOS_PRIORITY_COUNT - 1`. Larger values have higher priority.
 * @return ARTOS_OK on success.
 * @return ARTOS_ERROR_INVALID_ARGUMENT if @p entry or @p stack is NULL, or if
 *         @p priority is outside the configured range, or if the stack top is
 *         not 8-byte aligned.
 * @return ARTOS_ERROR_STACK_TOO_SMALL if the stack contains fewer than
 *         ARTOS_MIN_STACK_WORDS elements.
 * @return ARTOS_ERROR_TASK_LIMIT if the static task pool is full.
 * @warning The stack storage must remain valid and must not be moved while the
 *          task exists.
 * @warning Do not call this function from interrupt context.
 * @note Before the scheduler starts, a successfully created task becomes ready
 *       without requesting a context switch.
 * @note When called from task context after the scheduler starts, this function
 *       requests a context switch if the new task has higher priority than the
 *       calling task. Equal- and lower-priority tasks become ready without an
 *       immediate context switch.
 */
artos_status_t artos_task_create(artos_task_fn_t entry, void *argument,
                                 artos_stack_word_t *stack,
                                 uint32_t stack_word_count, uint8_t priority);

/**
 * @brief Start scheduling with the highest-priority created task.
 * @return ARTOS_ERROR_NO_TASKS if no user task has been created.
 * @note A successful call does not return.
 */
artos_status_t artos_start(void);

/**
 * @brief Voluntarily yield the processor from task context.
 * @note The current task moves behind other ready tasks at its priority. A
 *       lower-priority task cannot run while a higher-priority task is ready.
 */
void artos_yield(void);

/**
 * @brief Request a context switch from interrupt context.
 * @note Call after the interrupt source and peripheral state have been handled.
 * @note The scheduler selects the highest-priority ready task. Among equal
 *       priorities, the task waiting longest is selected first.
 */
void artos_yield_from_isr(void);

/**
 * @brief Block the current task for a relative number of ticks.
 * @param tick_count Number of ticks to wait. Zero behaves like artos_yield().
 *        ARTOS_DELAY_INFINITY blocks indefinitely.
 * @note When the delay expires, the task becomes ready and preempts the running
 *       task if it has equal or higher priority.
 * @warning Task-context-only. Do not call while already in a critical section.
 */
void artos_wait(uint32_t tick_count);

/**
 * @brief Block the current task until an absolute kernel tick.
 * @param wake_tick Absolute tick at which the task should become ready.
 * @note ARTOS_DELAY_INFINITY has no special meaning for this function.
 * @note The valid future horizon is 1 through 0x7fffffff ticks. A value outside
 *       that horizon is treated as current or past time and does not block.
 * @note On expiry, the task becomes ready and preempts the running task if it
 *       has equal or higher priority.
 * @warning Task-context-only. Do not call while already in a critical section.
 */
void artos_wait_until(uint32_t wake_tick);

/**
 * @brief Read the current kernel tick count.
 * @return Current 32-bit tick count, which wraps naturally.
 */
uint32_t artos_get_tick(void);

/**
 * @brief Read current task count.
 * @return Current task count.
 */
uint32_t artos_task_count(void);

// =================================
//           Dummy storage
// =================================

/** @brief Kernel-private static list-item storage. Do not access its fields. */
typedef struct {
  uint32_t value;
  void *_p, *_n, *_o, *_c;
} artos_static_list_item_t;

/** @brief Kernel-private static list storage. Do not access its fields. */
typedef struct {
  uint32_t _c;
  artos_static_list_item_t _s;
} artos_static_list_t;

// =============================
//           Semaphore
// =============================

/**
 * @brief Opaque binary or counting semaphore type.
 * @note Waiting tasks are ordered by priority, with FIFO ordering among tasks
 *       at the same priority.
 */
typedef struct artos_semaphore artos_semaphore_t;

/**
 * @brief Caller-owned storage for a semaphore control block.
 * @warning Initialized storage must not be copied or moved.
 */
typedef struct {
  /** @cond INTERNAL */
  uint32_t _a, _b;
  artos_static_list_t _w;
  /** @endcond */
} artos_semaphore_storage_t;

/**
 * @brief Initialize a binary semaphore in caller-owned storage.
 * @param storage Persistent storage for the semaphore control block.
 * @param initially_available True to create one available token, false to
 *        create the semaphore empty.
 * @return A semaphore handle on success, or NULL if @p storage is NULL.
 * @warning The storage must remain valid and must not be moved after
 *          initialization.
 */
artos_semaphore_t *
artos_binary_semaphore_init(artos_semaphore_storage_t *storage,
                            bool initially_available);

/**
 * @brief Initialize a counting semaphore in caller-owned storage.
 * @param storage Persistent storage for the semaphore control block.
 * @param max_count Maximum number of stored tokens. Must be greater than zero.
 * @param initial_count Number of initially available tokens. Must not exceed
 *        @p max_count.
 * @return A semaphore handle on success.
 * @return NULL if @p storage is NULL, @p max_count is zero, or
 *         @p initial_count exceeds @p max_count.
 * @warning The storage must remain valid and must not be moved after
 *          initialization.
 */
artos_semaphore_t *
artos_counting_semaphore_init(artos_semaphore_storage_t *storage,
                              uint32_t max_count, uint32_t initial_count);

/**
 * @brief Take one semaphore token, optionally blocking until one is available.
 * @param semaphore Semaphore to take.
 * @param tick_timeout Maximum ticks to wait. Zero is nonblocking and
 *        ARTOS_DELAY_INFINITY waits indefinitely.
 * @return True if a token was obtained, otherwise false for an invalid handle
 *         or timeout.
 * @note If the task blocks, semaphore signals are handed to the
 *       highest-priority waiter first. Equal-priority waiters are served FIFO.
 * @warning Task-context-only. Do not call while already in a critical section.
 */
bool artos_semaphore_take(artos_semaphore_t *semaphore, uint32_t tick_timeout);

/**
 * @brief Attempt to take one semaphore token from interrupt context.
 * @param semaphore Semaphore to take.
 * @return True if a token was obtained, otherwise false.
 * @note This operation never blocks.
 */
bool artos_semaphore_take_isr(artos_semaphore_t *semaphore);

/**
 * @brief Signal a semaphore from interrupt context.
 * @param semaphore Semaphore to signal.
 * @param[in,out] gt_task_woken Optional accumulating wake flag. The caller
 *        should initialize it to false before the first ISR-safe kernel
 *        operation. The function only changes it to true when it unblocks a
 *        task whose priority is higher than the interrupted task.
 * @return True if the token was stored or handed directly to a waiting task.
 * @return False if @p semaphore is NULL or already at its maximum count.
 * @note The highest-priority waiter is unblocked first. Equal-priority waiters
 *       are served FIFO.
 * @note This operation never blocks and @p gt_task_woken may be NULL.
 * @note If @p gt_task_woken becomes true, call artos_yield_from_isr() after
 *       completing the required peripheral cleanup.
 */
bool artos_semaphore_signal_isr(artos_semaphore_t *semaphore,
                                bool *gt_task_woken);

/**
 * @brief Signal a semaphore from task context.
 * @param semaphore Semaphore to signal.
 * @return True if the token was stored or handed directly to a waiting task.
 * @return False if @p semaphore is NULL or already at its maximum count.
 * @note The highest-priority waiter is unblocked first. Equal-priority waiters
 *       are served FIFO.
 * @note Requests a context switch only when the unblocked task has higher
 *       priority than the calling task.
 */
bool artos_semaphore_signal(artos_semaphore_t *semaphore);

// =========================
//           Queue
// =========================

/**
 * @brief Opaque fixed-capacity message queue type.
 * @note Message ordering is FIFO. Readers and writers waiting on the queue are
 *       ordered by priority, with FIFO ordering among equal-priority tasks.
 */
typedef struct artos_queue artos_queue_t;

/**
 * @brief Caller-owned storage for a queue control block.
 * @warning Initialized storage must not be copied or moved.
 */
typedef struct {
  /** @cond INTERNAL */
  uint32_t _a, _b, _c, _d;
  size_t _e;
  artos_static_list_t _r;
  artos_static_list_t _w;
  uint8_t *_s;
  /** @endcond */
} artos_queue_control_storage_t;

/**
 * @brief Initialize a bounded FIFO queue using caller-owned storage.
 * @param control Persistent storage for the queue control block.
 * @param storage Persistent buffer for @p capacity items of @p item_size bytes.
 * @param item_size Size of each queue item in bytes. Must be greater than zero.
 * @param capacity Maximum number of items. Must be greater than zero.
 * @return A queue handle on success.
 * @return NULL for invalid pointers, zero sizes, or an overflowing total buffer
 *         size calculation.
 * @warning The caller must provide at least `item_size * capacity` addressable
 *          bytes. Both storage objects must remain valid and must not be moved.
 */
artos_queue_t *artos_queue_init(artos_queue_control_storage_t *control,
                                uint8_t *storage, size_t item_size,
                                uint32_t capacity);

/**
 * @brief Copy one item into a queue, optionally waiting for free space.
 * @param queue Queue to receive the item.
 * @param data Source buffer containing at least the queue's item size in bytes.
 * @param tick_timeout Maximum ticks to wait. Zero is nonblocking and
 *        ARTOS_DELAY_INFINITY waits indefinitely.
 * @return True if the item was enqueued, otherwise false for invalid arguments
 *         or timeout.
 * @note A successful enqueue unblocks the highest-priority waiting reader. A
 *       context switch is requested if that reader has higher priority than
 *       the calling task.
 * @warning Task-context-only. Do not call while already in a critical section.
 */
bool artos_queue_enqueue(artos_queue_t *queue, uint8_t *data,
                         uint32_t tick_timeout);

/**
 * @brief Copy one item out of a queue, optionally waiting for data.
 * @param queue Queue from which to receive the item.
 * @param dst Destination buffer with space for the queue's item size in bytes.
 * @param tick_timeout Maximum ticks to wait. Zero is nonblocking and
 *        ARTOS_DELAY_INFINITY waits indefinitely.
 * @return True if an item was dequeued, otherwise false for invalid arguments
 *         or timeout.
 * @note A successful dequeue unblocks the highest-priority waiting writer. A
 *       context switch is requested if that writer has higher priority than
 *       the calling task.
 * @warning Task-context-only. Do not call while already in a critical section.
 */
bool artos_queue_dequeue(artos_queue_t *queue, uint8_t *dst,
                         uint32_t tick_timeout);

/**
 * @brief Copy one item into a queue from interrupt context.
 * @param queue Queue to receive the item.
 * @param data Source buffer containing at least the queue's item size in bytes.
 * @param[in,out] gt_task_woken Optional accumulating wake flag. The caller
 *        should initialize it to false before the first ISR-safe kernel
 *        operation. The function only changes it to true when it unblocks a
 *        task whose priority is higher than the interrupted task.
 * @return True if the item was enqueued, otherwise false if the arguments are
 *         invalid or the queue is full.
 * @note The highest-priority waiting reader is unblocked first. Equal-priority
 *       waiters are served FIFO.
 * @note This operation never blocks and @p gt_task_woken may be NULL.
 * @note If @p gt_task_woken becomes true, call artos_yield_from_isr() after
 *       completing the required peripheral cleanup.
 */
bool artos_queue_enqueue_from_isr(artos_queue_t *queue, uint8_t *data,
                                  bool *gt_task_woken);

/**
 * @brief Copy one item out of a queue from interrupt context.
 * @param queue Queue from which to receive the item.
 * @param dst Destination buffer with space for the queue's item size in bytes.
 * @param[in,out] gt_task_woken Optional accumulating wake flag. The caller
 *        should initialize it to false before the first ISR-safe kernel
 *        operation. The function only changes it to true when it unblocks a
 *        task whose priority is higher than the interrupted task.
 * @return True if an item was dequeued, otherwise false if the arguments are
 *         invalid or the queue is empty.
 * @note The highest-priority waiting writer is unblocked first. Equal-priority
 *       waiters are served FIFO.
 * @note This operation never blocks and @p gt_task_woken may be NULL.
 * @note If @p gt_task_woken becomes true, call artos_yield_from_isr() after
 *       completing the required peripheral cleanup.
 */
bool artos_queue_dequeue_from_isr(artos_queue_t *queue, uint8_t *dst,
                                  bool *gt_task_woken);

// =========================
//           Mutex
// =========================

/**
 * @brief Opaque nonrecursive mutex type with priority inheritance.
 * @note Waiting tasks are ordered by effective priority, with FIFO ordering
 *       among tasks at the same priority.
 */
typedef struct artos_mutex artos_mutex_t;

/**
 * @brief Caller-owned storage for a mutex control block.
 * @warning Initialized storage must not be copied or moved.
 */
typedef struct {
  /** @cond INTERNAL */
  void *_a;
  artos_static_list_t _w;
  /** @endcond */
} artos_mutex_storage_t;

/**
 * @brief Initialize a mutex in caller-owned storage.
 * @param storage Persistent storage for the mutex control block.
 * @return A mutex handle on success, or NULL if @p storage is NULL.
 * @warning The storage must remain valid and must not be moved after
 *          initialization.
 */
artos_mutex_t *artos_mutex_init(artos_mutex_storage_t *storage);

/**
 * @brief Lock a mutex, optionally blocking until ownership is available.
 * @param mutex Mutex to lock.
 * @param timeout Maximum ticks to wait. Zero performs a nonblocking try-lock,
 *        and ARTOS_DELAY_INFINITY waits indefinitely.
 * @return True if the calling task acquired ownership, otherwise false for a
 *         null handle, recursive acquisition, held-mutex limit, or timeout.
 * @note Ownership is handed directly to the highest-effective-priority waiter.
 *       Equal-priority waiters are served FIFO.
 * @note The owner inherits the highest waiting task's effective priority.
 *       Inheritance is simplified and is not propagated transitively through
 *       an existing chain of mutex waits.
 * @note After a timeout, inherited priority is recalculated only when the owner
 *       holds exactly one mutex. Otherwise it is retained conservatively.
 * @warning Task-context-only. Do not call from interrupt context or while
 *          already in a critical section.
 * @warning The mutex is nonrecursive. Its owner cannot lock it again.
 */
bool artos_mutex_lock(artos_mutex_t *mutex, uint32_t timeout);

/**
 * @brief Release a mutex owned by the calling task.
 * @param mutex Mutex to unlock.
 * @return True if ownership was released, otherwise false if @p mutex is NULL
 *         or the calling task is not its owner.
 * @note If waiters exist, ownership is transferred directly to the
 *       highest-effective-priority waiter, with FIFO ordering among equals.
 * @note A task that still owns other mutexes conservatively retains inherited
 *       priority. Releasing its final mutex restores its base priority.
 * @warning Task-context-only. Do not call from interrupt context or while
 *          already in a critical section.
 */
bool artos_mutex_unlock(artos_mutex_t *mutex);

#endif // !ARTOS_H
