#include "rtos.h"

#include "list.h"
#include "rtos/ports/rtos_port.h"
#include "rtos/tasks.h"
#include <assert.h>
#include <string.h>

// ===========================
//          Semaphore
// ===========================

struct artos_semaphore {
  uint32_t count, max;
  rtos_list_t wait_list;
};

_Static_assert(sizeof(artos_semaphore_t) == sizeof(artos_semaphore_storage_t),
               "Semaphore storage size mismatch");

_Static_assert(_Alignof(artos_semaphore_t) ==
                   _Alignof(artos_semaphore_storage_t),
               "Semaphore storage alignment mismatch");

artos_semaphore_t *
artos_binary_semaphore_init(artos_semaphore_storage_t *storage,
                            bool initially_available) {
  if (storage == NULL)
    return NULL;
  artos_semaphore_t *sem = (void *)storage;

  sem->count = initially_available ? 1 : 0;
  sem->max = 1;
  rtos_init_list(&sem->wait_list);

  return sem;
}

artos_semaphore_t *
rtos_counting_semaphore_init(artos_semaphore_storage_t *storage,
                             uint32_t max_count, uint32_t initial_count) {
  if (storage == NULL || max_count == 0 || initial_count > max_count)
    return NULL;
  artos_semaphore_t *sem = (void *)storage;

  sem->count = initial_count;
  sem->max = max_count;
  rtos_init_list(&sem->wait_list);

  return sem;
}

bool artos_semaphore_take(artos_semaphore_t *sem, uint32_t tick_timeout) {
  if (sem == NULL)
    return false;

  rtos_port_irq_state_t prev_state = rtos_port_enter_critical();
  if (sem->count) {
    sem->count--;
    rtos_port_exit_critical(prev_state);
    return true;
  }
  if (tick_timeout == 0) {
    rtos_port_exit_critical(prev_state);
    return false;
  }

  if (tick_timeout == RTOS_DELAY_INFINITY) {
    rtos_block_current_task(RTOS_DELAY_INFINITY, &sem->wait_list, true);
  } else {
    rtos_block_current_task(tick_timeout + rtos_current_tick(), &sem->wait_list,
                            false);
  }
  rtos_port_exit_critical(prev_state);
  rtos_port_request_context_switch();
  bool res = rtos_current_wait_reason() == WAIT_SIGNALED;
  rtos_clear_current_wait_reason();
  return res;
}

bool artos_semaphore_signal(artos_semaphore_t *sem) {
  if (sem == NULL)
    return false;

  rtos_port_irq_state_t prev_state = rtos_port_enter_critical();
  if (sem->count != 0) {
    bool res = sem->count != sem->max;
    if (res)
      sem->count++;
    rtos_port_exit_critical(prev_state);
    return res;
  }

  if (sem->wait_list.count == 0) {
    sem->count++;
    rtos_port_exit_critical(prev_state);
    return true;
  }

  bool gt_prio =
      sem->wait_list.sentinel.next->value > rtos_current_effective_priority();
  rtos_unblock_task(sem->wait_list.sentinel.next, WAIT_SIGNALED);
  rtos_port_exit_critical(prev_state);
  if (gt_prio)
    rtos_port_request_context_switch();
  return true;
}

bool artos_semaphore_take_isr(artos_semaphore_t *sem) {
  if (sem == NULL)
    return false;
  rtos_port_irq_state_t prev_state = rtos_port_enter_critical();
  if (sem->count) {
    sem->count--;
    rtos_port_exit_critical(prev_state);
    return true;
  }
  rtos_port_exit_critical(prev_state);
  return false;
}

bool artos_semaphore_signal_isr(artos_semaphore_t *sem, bool *gt_task_woken) {
  if (sem == NULL)
    return false;

  rtos_port_irq_state_t prev_state = rtos_port_enter_critical();
  if (sem->count != 0) {
    bool res = sem->count != sem->max;
    if (res)
      sem->count++;
    rtos_port_exit_critical(prev_state);
    return res;
  }

  if (sem->wait_list.count == 0) {
    sem->count++;
    rtos_port_exit_critical(prev_state);
    return true;
  }

  bool gt_prio =
      sem->wait_list.sentinel.next->value > rtos_current_effective_priority();
  if (gt_task_woken != NULL && gt_prio)
    *gt_task_woken = true;
  rtos_unblock_task(sem->wait_list.sentinel.next, WAIT_SIGNALED);
  rtos_port_exit_critical(prev_state);
  return true;
}
