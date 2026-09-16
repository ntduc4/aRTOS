#include <stdint.h>

#include "cmsis_gcc.h"
#include "rtos.h"
#include "rtos/ports/rtos_port.h"
#include "rtos_config.h"
#include "tasks.h"

static rtos_tcb_t task_pool[RTOS_MAX_TASKS];
static uint32_t task_count;
static rtos_tcb_t *current_task;
static uint32_t _index = 0;
static uint32_t _ticks = 0;

static _Alignas(8) rtos_stack_word_t idle_task_stack[RTOS_MIN_STACK_WORDS];

static void idle_task_entry(void *argument) {
  for (;;)
    __WFI();
}

static rtos_tcb_t idle_task;

// Make sure to only call when context switch
// Make sure to already in critical state
//
// TODO: Add different scheduling algo support later
static rtos_tcb_t *
rtos_scheduler_select_next(rtos_stack_word_t *current_stack_pointer) {
  // Treat as start if NULL
  if (current_task && current_task->state == RTOS_TASK_RUNNING)
    current_task->state = RTOS_TASK_READY;

  if (current_stack_pointer == NULL) {
    for (uint32_t i = 0U; i < RTOS_MAX_TASKS; i++) {
      rtos_tcb_t *task = &task_pool[i];
      if (task->state == RTOS_TASK_READY) {
        current_task = task;
        current_task->state = RTOS_TASK_RUNNING;
        _index = i;
        return current_task;
      }
    }

    return NULL;
  }

  current_task->stack_pointer = current_stack_pointer;
  uint32_t i = _index;
  do {
    i = (i + 1) % RTOS_MAX_TASKS;
    rtos_tcb_t *task = &task_pool[i];
    if (task->state == RTOS_TASK_READY) {
      current_task = task;
      current_task->state = RTOS_TASK_RUNNING;
      _index = i;
      return current_task;
    }
  } while (i != _index);

  // Return idle task as default if no task is schedulable
  current_task = &idle_task;
  current_task->state = RTOS_TASK_RUNNING;
  return current_task;
};

rtos_status_t rtos_task_create(rtos_task_fn_t entry, void *argument,
                               rtos_stack_word_t *stack,
                               uint32_t stack_word_count) {
  if (stack == NULL || entry == NULL)
    return RTOS_ERROR_INVALID_ARGUMENT;
  rtos_stack_word_t *stack_top = stack + stack_word_count;
  if (((uintptr_t)(stack_top) & 0b111U) != 0U)
    return RTOS_ERROR_INVALID_ARGUMENT;
  if (stack_word_count < RTOS_MIN_STACK_WORDS)
    return RTOS_ERROR_STACK_TOO_SMALL;

  for (uint32_t i = 0; i < RTOS_MAX_TASKS; i++) {
    if (task_pool[i].state == RTOS_TASK_UNUSED) {
      rtos_tcb_t new_tcb = {.stack_pointer = rtos_port_initialize_stack(
                                stack_top, entry, argument),
                            .stack_buffer = stack,
                            .stack_word_count = stack_word_count,
                            .entry = entry,
                            .argument = argument,
                            .state = RTOS_TASK_READY,
                            .wake_tick = 0};
      task_pool[i] = new_tcb;
      task_count++;
      return RTOS_OK;
    }
  }
  return RTOS_ERROR_TASK_LIMIT;
}

void rtos_system_init(void) {
  // Setup idle task
  idle_task = (rtos_tcb_t){
      .stack_pointer = rtos_port_initialize_stack(
          idle_task_stack + RTOS_MIN_STACK_WORDS, &idle_task_entry, NULL),
      .stack_buffer = idle_task_stack,
      .stack_word_count = RTOS_MIN_STACK_WORDS,
      .entry = &idle_task_entry,
      .argument = NULL,
      .state = RTOS_TASK_READY,
      .wake_tick = 0};

  for (uint32_t i = 0U; i < RTOS_MAX_TASKS; i++) {
    task_pool[i] = (rtos_tcb_t){0};
    task_pool[i].state = RTOS_TASK_UNUSED;
  }

  current_task = 0;
  _index = 0;
  _ticks = 0;
  task_count = 0U;

  rtos_port_scheduler_init();
}

rtos_tcb_t *rtos_scheduler_start(void) {
  return rtos_scheduler_select_next(NULL);
};

rtos_stack_word_t *
rtos_scheduler_switch_context(rtos_stack_word_t *current_stack_pointer) {
  rtos_port_irq_state_t prev_state = rtos_port_enter_critical();
  rtos_tcb_t *next = rtos_scheduler_select_next(current_stack_pointer);
  rtos_port_exit_critical(prev_state);

  // Should never happen
  if (next == NULL)
    return NULL;

  return next->stack_pointer;
}

void rtos_block_current_task(uint32_t wake_tick, void *wait_obj) {
  if (current_task == NULL)
    return;
  rtos_port_irq_state_t prev_state = rtos_port_enter_critical();
  current_task->wake_tick =
      (wake_tick == RTOS_DELAY_INFINITY) ? RTOS_DELAY_INFINITY : wake_tick;
  current_task->state = RTOS_TASK_BLOCKED;
  current_task->wait_obj = wait_obj;
  rtos_port_exit_critical(prev_state);
}

void rtos_unblock_task(uint32_t index) {
  if (index >= RTOS_MAX_TASKS)
    return;

  rtos_tcb_t *task = &task_pool[index];
  if (task->state == RTOS_TASK_BLOCKED) {
    rtos_port_irq_state_t prev_state = rtos_port_enter_critical();
    task->state = RTOS_TASK_READY;
    task->wait_obj = NULL;
    task->wake_tick = 0;
    rtos_port_exit_critical(prev_state);
  }
}

void rtos_tick_handler(void) {
  _ticks++;
  if (_ticks == RTOS_DELAY_INFINITY)
    _ticks = 0;

  uint8_t need_switch = 0;
  for (uint32_t i = 0; i < RTOS_MAX_TASKS; i++) {
    rtos_tcb_t *task = &task_pool[i];
    // Handle infinity and overflow in a later week when event + semaphore is
    // implemented and optimizing to ready list + wait list
    if (task->state == RTOS_TASK_BLOCKED && _ticks >= task->wake_tick)
      rtos_unblock_task(i);
    if (task->state == RTOS_TASK_READY)
      need_switch = 1;
  }

  if (need_switch)
    rtos_port_request_context_switch();
}

uint32_t rtos_current_tick(void) { return _ticks; }

const rtos_tcb_t *rtos_scheduler_current_task(void) { return current_task; }
