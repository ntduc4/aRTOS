#include "rtos.h"

#include "list.h"
#include "rtos/ports/rtos_port.h"
#include "rtos/tasks.h"
#include <assert.h>
#include <stdbool.h>

struct rtos_binary_semaphore {
  bool availability;
  rtos_list_t wait_list;
};

_Static_assert(sizeof(rtos_binary_semaphore_t) ==
                   sizeof(rtos_semaphore_storage_t),
               "Semaphore storage size mismatch");

_Static_assert(_Alignof(rtos_binary_semaphore_t) ==
                   _Alignof(rtos_semaphore_storage_t),
               "Semaphore storage alignment mismatch");

rtos_binary_semaphore_t *
rtos_binary_semaphore_init(rtos_semaphore_storage_t *storage,
                           bool initially_available) {
  if (storage == NULL)
    return NULL;
  rtos_binary_semaphore_t *sem = (void *)storage;

  sem->availability = initially_available;
  rtos_init_list(&sem->wait_list);

  return sem;
}

bool rtos_binary_semaphore_wait(rtos_binary_semaphore_t *sem,
                                uint32_t tick_timeout) {
  if (sem == NULL)
    return false;

  rtos_port_irq_state_t prev_state = rtos_port_enter_critical();
  if (sem->availability) {
    sem->availability = false;
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

void rtos_binary_semaphore_signal(rtos_binary_semaphore_t *sem) {
  if (sem == NULL)
    return;

  rtos_port_irq_state_t prev_state = rtos_port_enter_critical();
  if (sem->availability) {
    rtos_port_exit_critical(prev_state);
    return;
  }

  if (sem->wait_list.count == 0) {
    sem->availability = true;
    rtos_port_exit_critical(prev_state);
    return;
  }

  rtos_unblock_task(sem->wait_list.sentinel.next, WAIT_SIGNALED);
  rtos_port_exit_critical(prev_state);
  rtos_port_request_context_switch();
}

bool rtos_binary_semaphore_take_isr(rtos_binary_semaphore_t *sem) {
  if (sem == NULL)
    return false;
  rtos_port_irq_state_t prev_state = rtos_port_enter_critical();
  if (sem->availability) {
    sem->availability = false;
    rtos_port_exit_critical(prev_state);
    return true;
  }
  rtos_port_exit_critical(prev_state);
  return false;
}

bool rtos_binary_semaphore_signal_isr(rtos_binary_semaphore_t *sem) {
  if (sem == NULL)
    return false;

  rtos_port_irq_state_t prev_state = rtos_port_enter_critical();
  if (sem->availability) {
    rtos_port_exit_critical(prev_state);
    return false;
  }

  if (sem->wait_list.count == 0) {
    sem->availability = true;
    rtos_port_exit_critical(prev_state);
    return false;
  }

  rtos_unblock_task(sem->wait_list.sentinel.next, WAIT_SIGNALED);
  rtos_port_exit_critical(prev_state);
  return true;
}
