#include "rtos.h"

#include "list.h"
#include "rtos/ports/rtos_port.h"
#include <string.h>

// =======================
//          Queue
// =======================

struct rtos_queue {
  uint32_t count, capacity, read_index, write_index;
  size_t item_size;
  rtos_list_t read_list;
  rtos_list_t write_list;
  uint8_t *storage;
};

_Static_assert(sizeof(rtos_queue_t) == sizeof(rtos_queue_control_storage_t),
               "Queue storage size mismatch");

_Static_assert(_Alignof(rtos_queue_t) == _Alignof(rtos_queue_control_storage_t),
               "Queue storage alignment mismatch");

rtos_queue_t *rtos_queue_init(rtos_queue_control_storage_t *control,
                              uint8_t *storage, size_t item_size,
                              uint32_t capacity) {
  if (control == NULL || storage == NULL || capacity == 0 || item_size == 0)
    return NULL;
  if (SIZE_MAX / item_size < capacity)
    return NULL;
  rtos_queue_t *q = (void *)control;

  q->count = 0;
  q->item_size = item_size;
  q->capacity = capacity;
  q->read_index = 0;
  q->write_index = 0;
  q->storage = storage;
  rtos_init_list(&q->read_list);
  rtos_init_list(&q->write_list);

  return q;
}

bool rtos_queue_enqueue(rtos_queue_t *q, uint8_t *data, uint32_t tick_timeout) {
  if (q == NULL || data == NULL)
    return false;

  rtos_port_irq_state_t prev_state = rtos_port_enter_critical();

  if (tick_timeout == 0 && q->count == q->capacity) {
    rtos_port_exit_critical(prev_state);
    return false;
  }

  if (q->count != q->capacity) {
    memcpy(q->storage + (q->item_size * q->write_index), data, q->item_size);
    q->count++;
    if (++q->write_index == q->capacity)
      q->write_index = 0;
    bool need_unblock = q->read_list.count != 0;
    if (need_unblock)
      rtos_unblock_task(q->read_list.sentinel.next, WAIT_SIGNALED);
    rtos_port_exit_critical(prev_state);

    if (need_unblock)
      rtos_port_request_context_switch();
    return true;
  }

  uint32_t wake_tick = tick_timeout == RTOS_DELAY_INFINITY
                           ? RTOS_DELAY_INFINITY
                           : tick_timeout + rtos_current_tick();
  bool infinity = tick_timeout == RTOS_DELAY_INFINITY;

  // Guard from isr dequeuing before task
  while (q->count == q->capacity) {
    // Same as timeout (If delta == 0 and still at capacity, return to user)
    uint32_t delta = (wake_tick - rtos_current_tick());
    if (!infinity && (delta == 0 || delta >= 0x80000000)) {
      rtos_port_exit_critical(prev_state);
      return false;
    }

    // Block self
    rtos_block_current_task(wake_tick, &q->write_list, infinity);
    rtos_port_exit_critical(prev_state);

    rtos_port_request_context_switch();

    // If running -> No longer blocked
    prev_state = rtos_port_enter_critical();
    bool timed_out = rtos_current_wait_reason() == WAIT_TIMED_OUT;
    rtos_clear_current_wait_reason();
    if (timed_out) {
      rtos_port_exit_critical(prev_state);
      return false;
    }
  }

  memcpy(q->storage + (q->item_size * q->write_index), data, q->item_size);
  q->count++;
  if (++q->write_index == q->capacity)
    q->write_index = 0;

  bool need_unblock = q->read_list.count != 0;
  if (need_unblock)
    rtos_unblock_task(q->read_list.sentinel.next, WAIT_SIGNALED);
  rtos_port_exit_critical(prev_state);

  if (need_unblock)
    rtos_port_request_context_switch();
  return true;
}

bool rtos_queue_dequeue(rtos_queue_t *q, uint8_t *dst, uint32_t tick_timeout) {
  if (q == NULL || dst == NULL)
    return false;

  rtos_port_irq_state_t prev_state = rtos_port_enter_critical();

  if (tick_timeout == 0 && q->count == 0) {
    rtos_port_exit_critical(prev_state);
    return false;
  }

  if (q->count != 0) {
    memcpy(dst, q->storage + (q->item_size * q->read_index), q->item_size);
    q->count--;
    if (++q->read_index == q->capacity)
      q->read_index = 0;
    bool need_unblock = q->write_list.count != 0;
    if (need_unblock)
      rtos_unblock_task(q->write_list.sentinel.next, WAIT_SIGNALED);
    rtos_port_exit_critical(prev_state);

    if (need_unblock)
      rtos_port_request_context_switch();
    return true;
  }

  uint32_t wake_tick = tick_timeout == RTOS_DELAY_INFINITY
                           ? RTOS_DELAY_INFINITY
                           : tick_timeout + rtos_current_tick();
  bool infinity = tick_timeout == RTOS_DELAY_INFINITY;

  // Guard from isr enqueuing before task
  while (q->count == 0) {
    // Same as timeout (If delta == 0 and still at capacity, return to user)
    uint32_t delta = (wake_tick - rtos_current_tick());
    if (!infinity && (delta == 0 || delta >= 0x80000000)) {
      rtos_port_exit_critical(prev_state);
      return false;
    }

    // Block self
    rtos_block_current_task(wake_tick, &q->read_list, infinity);
    rtos_port_exit_critical(prev_state);

    rtos_port_request_context_switch();

    // If running -> No longer blocked
    prev_state = rtos_port_enter_critical();
    bool timed_out = rtos_current_wait_reason() == WAIT_TIMED_OUT;
    rtos_clear_current_wait_reason();
    if (timed_out) {
      rtos_port_exit_critical(prev_state);
      return false;
    }
  }

  memcpy(dst, q->storage + (q->item_size * q->read_index), q->item_size);
  q->count--;
  if (++q->read_index == q->capacity)
    q->read_index = 0;
  bool need_unblock = q->write_list.count != 0;
  if (need_unblock)
    rtos_unblock_task(q->write_list.sentinel.next, WAIT_SIGNALED);
  rtos_port_exit_critical(prev_state);

  if (need_unblock)
    rtos_port_request_context_switch();
  return true;
}

bool rtos_queue_enqueue_from_isr(rtos_queue_t *q, uint8_t *data,
                                 bool *task_waken) {
  if (task_waken != NULL)
    *task_waken = false;
  if (q == NULL || data == NULL || task_waken == NULL)
    return false;

  rtos_port_irq_state_t prev_state = rtos_port_enter_critical();

  if (q->count == q->capacity) {
    rtos_port_exit_critical(prev_state);
    return false;
  }

  memcpy(q->storage + (q->item_size * q->write_index), data, q->item_size);
  q->count++;
  if (++q->write_index == q->capacity)
    q->write_index = 0;
  bool need_unblock = q->read_list.count != 0;
  if (need_unblock) {
    rtos_unblock_task(q->read_list.sentinel.next, WAIT_SIGNALED);
    *task_waken = true;
  }
  rtos_port_exit_critical(prev_state);
  return true;
}

bool rtos_queue_dequeue_from_isr(rtos_queue_t *q, uint8_t *dst,
                                 bool *task_waken) {
  if (task_waken != NULL)
    *task_waken = false;
  if (q == NULL || dst == NULL || task_waken == NULL)
    return false;

  rtos_port_irq_state_t prev_state = rtos_port_enter_critical();

  if (q->count == 0) {
    rtos_port_exit_critical(prev_state);
    return false;
  }

  memcpy(dst, q->storage + (q->item_size * q->read_index), q->item_size);
  q->count--;
  if (++q->read_index == q->capacity)
    q->read_index = 0;
  bool need_unblock = q->write_list.count != 0;
  if (need_unblock) {
    rtos_unblock_task(q->write_list.sentinel.next, WAIT_SIGNALED);
    *task_waken = true;
  }
  rtos_port_exit_critical(prev_state);
  return true;
}
