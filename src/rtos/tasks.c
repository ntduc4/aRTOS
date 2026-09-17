#include <stdint.h>

#include "cmsis_gcc.h"
#include "rtos/list.h"
#include "rtos/ports/rtos_port.h"
#include "tasks.h"

static rtos_tcb_t _task_pool[RTOS_MAX_TASKS];
static uint32_t _tsk_cnt;
static rtos_tcb_t *_cur_task;
static uint32_t _index = 0;
static uint32_t _ticks = 0;

static rtos_list_t _ready_l;
static rtos_list_t _delayed_l;
static rtos_list_t _delayed_overflow_l;
static rtos_list_t _suspend_l;

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
  if (_cur_task && _cur_task->state == RTOS_TASK_RUNNING)
    _cur_task->state = RTOS_TASK_READY;

  if (current_stack_pointer == NULL) {
    for (uint32_t i = 0U; i < RTOS_MAX_TASKS; i++) {
      rtos_tcb_t *task = &_task_pool[i];
      if (task->state == RTOS_TASK_READY) {
        _cur_task = task;
        _cur_task->state = RTOS_TASK_RUNNING;
        _index = i;
        return _cur_task;
      }
    }

    return NULL;
  }

  _cur_task->stack_pointer = current_stack_pointer;
  uint32_t i = _index;
  do {
    i = (i + 1) % RTOS_MAX_TASKS;
    rtos_tcb_t *task = &_task_pool[i];
    if (task->state == RTOS_TASK_READY) {
      _cur_task = task;
      _cur_task->state = RTOS_TASK_RUNNING;
      _index = i;
      return _cur_task;
    }
  } while (i != _index);

  // Return idle task as default if no task is schedulable
  _cur_task = &idle_task;
  _cur_task->state = RTOS_TASK_RUNNING;
  return _cur_task;
};

static inline void rtos_init_tcb(rtos_tcb_t *tcb, rtos_task_fn_t entry,
                                 void *argument, rtos_stack_word_t *stack,
                                 uint32_t stack_word_count) {
  tcb->stack_pointer =
      rtos_port_initialize_stack(stack + stack_word_count, entry, argument);
  tcb->stack_buffer = stack;
  tcb->stack_word_count = stack_word_count;

  tcb->entry = entry;
  tcb->argument = argument;

  tcb->state = RTOS_TASK_READY;
  tcb->wake_tick = 0;
  tcb->wait_obj = NULL;
  rtos_init_list_item(&tcb->state_item, tcb);
  rtos_init_list_item(&tcb->event_item, tcb);
}

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
    if (_task_pool[i].state == RTOS_TASK_UNUSED) {
      rtos_init_tcb(&_task_pool[i], entry, argument, stack, stack_word_count);
      _tsk_cnt++;
      return RTOS_OK;
    }
  }
  return RTOS_ERROR_TASK_LIMIT;
}

void rtos_system_init(void) {
  // Setup idle task

  rtos_init_tcb(&idle_task, idle_task_entry, NULL, idle_task_stack,
                RTOS_MIN_STACK_WORDS);

  for (uint32_t i = 0U; i < RTOS_MAX_TASKS; i++) {
    _task_pool[i] = (rtos_tcb_t){0};
    _task_pool[i].state = RTOS_TASK_UNUSED;
  }

  _cur_task = 0;
  _index = 0;
  _ticks = 0;
  _tsk_cnt = 0U;

  rtos_init_list(&_ready_l);
  rtos_init_list(&_delayed_l);
  rtos_init_list(&_delayed_overflow_l);
  rtos_init_list(&_suspend_l);

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
  if (_cur_task == NULL)
    return;
  rtos_port_irq_state_t prev_state = rtos_port_enter_critical();
  _cur_task->wake_tick =
      (wake_tick == RTOS_DELAY_INFINITY) ? RTOS_DELAY_INFINITY : wake_tick;
  _cur_task->state = RTOS_TASK_BLOCKED;
  _cur_task->wait_obj = wait_obj;
  rtos_port_exit_critical(prev_state);
}

void rtos_unblock_task(uint32_t index) {
  if (index >= RTOS_MAX_TASKS)
    return;

  rtos_tcb_t *task = &_task_pool[index];
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
    rtos_tcb_t *task = &_task_pool[i];
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

const rtos_tcb_t *rtos_scheduler_current_task(void) { return _cur_task; }
