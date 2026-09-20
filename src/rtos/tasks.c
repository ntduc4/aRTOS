#include <stdint.h>

#include "rtos.h"
#include "rtos/list.h"
#include "rtos/ports/rtos_port.h"
#include "rtos_config.h"
#include "tasks.h"

_Static_assert(RTOS_PRIORITY_COUNT <= 32,
               "aRTOS only support up to 32 priorities");
_Static_assert(RTOS_PRIORITY_COUNT > 0, "aRTOS requires atleast 1 priority");

static rtos_tcb_t _task_pool[RTOS_MAX_TASKS];
static uint32_t _tsk_cnt;
static rtos_tcb_t *_cur_task;
static uint32_t _ticks = 0;

static rtos_list_t _ready_l[RTOS_PRIORITY_COUNT];
static uint32_t _ready_bitmap;
static rtos_list_t _delayed_l;
static rtos_list_t _delayed_overflow_l;
static rtos_list_t _suspend_l;

static rtos_list_t *_cur_delayed;
static rtos_list_t *_next_delayed;

static _Alignas(8) rtos_stack_word_t idle_task_stack[RTOS_MIN_STACK_WORDS];

static rtos_tcb_t idle_task;

static void rtos_insert_ready_list(rtos_tcb_t *task) {
  if (task == NULL || task->effective_priority >= RTOS_PRIORITY_COUNT ||
      task->state_item.container != NULL)
    return;
  rtos_list_insert_end(&_ready_l[task->effective_priority], &task->state_item);
  _ready_bitmap |= 0x1U << task->effective_priority;
}

static bool rtos_check_runnable_task(rtos_tcb_t *task_compare) {
  uint8_t compared_prio = 0;
  if (task_compare != NULL)
    compared_prio = task_compare->effective_priority;
  return (_ready_bitmap >> compared_prio) != 0U;
}

// For future uses
static __attribute__((unused)) bool
rtos_check_preemptable_task(rtos_tcb_t *task_compare) {
  uint8_t compared_prio = 0;
  if (task_compare != NULL)
    compared_prio = task_compare->effective_priority;
  return (_ready_bitmap >> compared_prio) >> 1 != 0U;
}

static rtos_tcb_t *rtos_get_highest_prio_task() {
  if (_ready_bitmap == 0)
    return NULL;
  uint8_t highest_prio = rtos_port_find_msb32(_ready_bitmap);
  rtos_list_t *ready_l = &_ready_l[highest_prio];
  rtos_tcb_t *task = ready_l->sentinel.next->owner;
  rtos_list_remove(&task->state_item);
  if (ready_l->count == 0)
    _ready_bitmap &= ~(0x1U << highest_prio);
  return task;
}

// Make sure to only call when context switch
// Make sure to already in critical state
static rtos_tcb_t *
rtos_scheduler_select_next(rtos_stack_word_t *current_stack_pointer) {
  // NULL ONLY VALID WHEN IT'S THE FIRST CALL (NO RUNNING TASK)
  if (current_stack_pointer == NULL && _cur_task != NULL)
    return NULL;

  // Treat as start if NULL
  if (current_stack_pointer == NULL) {
    if (!rtos_check_runnable_task(NULL))
      return NULL;

    _cur_task = rtos_get_highest_prio_task();
    return _cur_task;
  }

  _cur_task->stack_pointer = current_stack_pointer;
  // Not idle task and not in another delayed/suspend list
  if (_cur_task != &idle_task && _cur_task->state_item.container == NULL)
    rtos_insert_ready_list(_cur_task);

  // Return idle task as default if no task is schedulable
  if (!rtos_check_runnable_task(NULL)) {
    _cur_task = &idle_task;
  } else {
    _cur_task = rtos_get_highest_prio_task();
  }
  return _cur_task;
};

static inline void rtos_init_tcb(rtos_tcb_t *tcb, rtos_task_fn_t entry,
                                 void *argument, rtos_stack_word_t *stack,
                                 uint32_t stack_word_count, uint8_t priority) {
  tcb->stack_pointer =
      rtos_port_initialize_stack(stack + stack_word_count, entry, argument);
  tcb->stack_buffer = stack;
  tcb->stack_word_count = stack_word_count;

  tcb->entry = entry;
  tcb->argument = argument;

  tcb->priority = priority;
  tcb->effective_priority = priority;
  tcb->mutexes_held = 0;

  rtos_init_list_item(&tcb->state_item, tcb);
  rtos_init_list_item(&tcb->event_item, tcb);
  tcb->wait_reason = WAIT_NO_REASON;
}

rtos_status_t rtos_task_create(rtos_task_fn_t entry, void *argument,
                               rtos_stack_word_t *stack,
                               uint32_t stack_word_count, uint8_t priority) {
  if (stack == NULL || entry == NULL || priority >= RTOS_PRIORITY_COUNT)
    return RTOS_ERROR_INVALID_ARGUMENT;
  rtos_stack_word_t *stack_top = stack + stack_word_count;
  if (((uintptr_t)(stack_top) & 0b111U) != 0U)
    return RTOS_ERROR_INVALID_ARGUMENT;
  if (stack_word_count < RTOS_MIN_STACK_WORDS)
    return RTOS_ERROR_STACK_TOO_SMALL;
  if (_tsk_cnt == RTOS_MAX_TASKS)
    return RTOS_ERROR_TASK_LIMIT;

  rtos_port_irq_state_t prev_state = rtos_port_enter_critical();
  for (uint32_t i = 0; i < RTOS_MAX_TASKS; i++) {
    if (_task_pool[i].stack_pointer == NULL) {
      rtos_init_tcb(&_task_pool[i], entry, argument, stack, stack_word_count,
                    priority);
      _tsk_cnt++;
      rtos_insert_ready_list(&_task_pool[i]);
      bool should_yield =
          _cur_task != NULL && priority > _cur_task->effective_priority;
      rtos_port_exit_critical(prev_state);
      if (should_yield)
        rtos_port_request_context_switch();
      return RTOS_OK;
    }
  }
  rtos_port_exit_critical(prev_state);
  return RTOS_ERROR_TASK_LIMIT;
}

void rtos_system_init(void) {
  // Setup idle task

  rtos_init_tcb(&idle_task, rtos_port_idle_task, NULL, idle_task_stack,
                RTOS_MIN_STACK_WORDS, 0);

  for (uint32_t i = 0U; i < RTOS_MAX_TASKS; i++)
    _task_pool[i] = (rtos_tcb_t){0};

  _cur_task = 0;
  _ticks = 0;
  _tsk_cnt = 0;

  for (uint8_t i = 0; i < RTOS_PRIORITY_COUNT; i++)
    rtos_init_list(&_ready_l[i]);
  rtos_init_list(&_delayed_l);
  rtos_init_list(&_delayed_overflow_l);
  rtos_init_list(&_suspend_l);
  _ready_bitmap = 0U;

  _cur_delayed = &_delayed_l;
  _next_delayed = &_delayed_overflow_l;

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

void rtos_block_current_task(uint32_t wake_tick, rtos_list_t *wait_obj,
                             bool infinite) {
  // No cur_task (just started) or cur task already blocked
  if (_cur_task == NULL || _cur_task->state_item.container != NULL)
    return;

  _cur_task->state_item.value = wake_tick;
  // Delay (always append to a list)
  if (infinite)
    rtos_list_insert_end(&_suspend_l, &_cur_task->state_item);
  else if (wake_tick < _ticks) // wake_tick == _ticks should not happen
    rtos_list_insert_sorted(_next_delayed, &_cur_task->state_item);
  else
    rtos_list_insert_sorted(_cur_delayed, &_cur_task->state_item);

  // Event (may or may not append to a list)
  if (wait_obj != NULL) {
    _cur_task->event_item.value = _cur_task->effective_priority;
    rtos_list_insert_reversed_sorted(wait_obj, &_cur_task->event_item);
  }
}

void rtos_unblock_task(rtos_list_item_t *task_item, rtos_wait_reason_t reason) {
  if (task_item == NULL || task_item->owner == NULL ||
      task_item->container == NULL)
    return;

  rtos_tcb_t *task = task_item->owner;
  rtos_list_t *tsk_state_l = task->state_item.container;
  if (tsk_state_l == &_delayed_l || tsk_state_l == &_delayed_overflow_l ||
      tsk_state_l == &_suspend_l) {
    rtos_list_remove(&task->state_item);
    rtos_list_remove(&task->event_item);
    rtos_insert_ready_list(task);
    task->wait_reason = reason;
  }
}

void rtos_tick_handler(void) {
  rtos_port_irq_state_t prev_state = rtos_port_enter_critical();
  _ticks++;
  if (_ticks == 0) {
    rtos_list_t *temp = _cur_delayed;
    // safe fall back, should never happen
    while (_cur_delayed->count) {
      rtos_list_remove(_cur_delayed->sentinel.next);
    }

    _cur_delayed = _next_delayed;
    _next_delayed = temp;
  }

  while (_cur_delayed->count && _cur_delayed->sentinel.next->value <= _ticks) {
    rtos_tcb_t *task = _cur_delayed->sentinel.next->owner;
    rtos_unblock_task(&task->state_item, WAIT_TIMED_OUT);
  }
  rtos_port_exit_critical(prev_state);

  if (rtos_check_runnable_task(_cur_task))
    rtos_port_request_context_switch();
}

uint32_t rtos_current_tick(void) { return _ticks; }

const rtos_tcb_t *rtos_scheduler_current_task(void) { return _cur_task; }

rtos_wait_reason_t rtos_current_wait_reason(void) {
  if (_cur_task == NULL)
    return WAIT_NO_REASON;
  return _cur_task->wait_reason;
}

void rtos_clear_current_wait_reason(void) {
  if (_cur_task == NULL)
    return;

  rtos_port_irq_state_t prev_state = rtos_port_enter_critical();
  _cur_task->wait_reason = WAIT_NO_REASON;
  rtos_port_exit_critical(prev_state);
};

uint8_t rtos_current_effective_priority(void) {
  if (_cur_task == NULL)
    return 0;
  return _cur_task->effective_priority;
}
