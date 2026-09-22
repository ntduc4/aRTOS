#include "rtos.h"

#include "list.h"
#include "rtos/artos_assert.h"
#include "rtos/ports/rtos_port.h"
#include "rtos/tasks.h"

struct artos_mutex {
  // Modify by task provided method instead
  rtos_tcb_t *owner;
  rtos_list_t lock_list;
};

_Static_assert(sizeof(artos_mutex_t) == sizeof(artos_mutex_storage_t),
               "mutex storage size mismatch");

_Static_assert(_Alignof(artos_mutex_t) == _Alignof(artos_mutex_storage_t),
               "mutex storage alignment mismatch");

artos_mutex_t *artos_mutex_init(artos_mutex_storage_t *storage) {
  if (storage == NULL)
    return NULL;
  artos_mutex_t *mutex = (void *)storage;
  mutex->owner = NULL;
  rtos_init_list(&mutex->lock_list);
  return mutex;
}

static uint8_t rtos_mutex_required_priority(rtos_tcb_t *owner,
                                            rtos_list_t *waiters) {
  if (owner == NULL)
    return 0;
  if (waiters == NULL)
    return owner->effective_priority;

  uint8_t priority = owner->priority;

  if (waiters->count > 0) {
    rtos_tcb_t *waiter = waiters->sentinel.next->owner;
    if (waiter->effective_priority > priority)
      priority = waiter->effective_priority;
  }

  return priority;
}

bool artos_mutex_lock(artos_mutex_t *mutex, uint32_t tick_timeout) {
  if (mutex == NULL)
    return false;

  rtos_port_irq_state_t prev_state = rtos_port_enter_critical();
  rtos_tcb_t *task = rtos_scheduler_current_task();
  if (task == NULL || task->mutexes_held == 0xFFU) {
    rtos_port_exit_critical(prev_state);
    return false;
  }

  // No need for priority inheritance
  if (mutex->owner == NULL) {
    mutex->owner = task;
    rtos_increment_mutex_count(task);
    rtos_port_exit_critical(prev_state);
    return true;
  }

  // Recursive mutex not allowed, max 255 mutex held at a time
  if (task == mutex->owner || task->mutexes_held == 0xFFU ||
      tick_timeout == 0) {
    rtos_port_exit_critical(prev_state);
    return false;
  }

  if (tick_timeout == ARTOS_DELAY_INFINITY) {
    rtos_block_current_task(ARTOS_DELAY_INFINITY, &mutex->lock_list, true);
  } else {
    rtos_block_current_task(tick_timeout + rtos_current_tick(),
                            &mutex->lock_list, false);
  }
  rtos_priority_inherit(mutex->owner, rtos_mutex_required_priority(
                                          mutex->owner, &mutex->lock_list));
  rtos_port_exit_critical(prev_state);

  rtos_port_request_context_switch();

  prev_state = rtos_port_enter_critical();
  bool res = rtos_current_wait_reason() == WAIT_SIGNALED;
  rtos_clear_current_wait_reason();
  if (!res)
    rtos_priority_disinherit_after_timeout(
        mutex->owner,
        rtos_mutex_required_priority(mutex->owner, &mutex->lock_list));
  rtos_port_exit_critical(prev_state);
  return res;
}

bool artos_mutex_unlock(artos_mutex_t *mutex) {
  if (mutex == NULL)
    return false;

  rtos_port_irq_state_t prev_state = rtos_port_enter_critical();
  rtos_tcb_t *task = rtos_scheduler_current_task();
  if (task == NULL) {
    rtos_port_exit_critical(prev_state);
    return false;
  }

  if (mutex->owner == NULL || task != mutex->owner) {
    rtos_port_exit_critical(prev_state);
    return false;
  }

  ARTOS_ASSERT(task->mutexes_held > 0U);
  rtos_decrement_mutex_count(task);
  bool prio_changed = rtos_priority_relinquish_after_unlock(task);

  if (mutex->lock_list.count == 0) {
    mutex->owner = NULL;
    rtos_port_exit_critical(prev_state);
    if (prio_changed)
      rtos_port_request_context_switch();
    return true;
  } else {
    uint8_t self_prio = task->effective_priority;
    rtos_list_item_t *next_task_item = mutex->lock_list.sentinel.next;
    task = next_task_item->owner;

    rtos_unblock_task(next_task_item, WAIT_SIGNALED);

    mutex->owner = task;
    rtos_increment_mutex_count(task);
    if (mutex->lock_list.count)
      rtos_priority_inherit(mutex->owner, rtos_mutex_required_priority(
                                              mutex->owner, &mutex->lock_list));
    bool should_yield = mutex->owner->effective_priority > self_prio;
    rtos_port_exit_critical(prev_state);

    if (should_yield)
      rtos_port_request_context_switch();

    return true;
  }
}
