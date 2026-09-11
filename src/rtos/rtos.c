#include "rtos.h"
#include "rtos/ports/rtos_port.h"
#include "rtos_config.h"
#include "rtos_internal.h"

static rtos_tcb_t task_table[RTOS_MAX_TASKS];
static uint32_t task_count;
static rtos_tcb_t *current_task;

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
    if (task_table[i].state == RTOS_TASK_UNUSED) {
      rtos_tcb_t new_tcb = {.stack_pointer = rtos_port_initialize_stack(
                                stack_top, entry, argument),
                            .stack_buffer = stack,
                            .stack_word_count = stack_word_count,
                            .entry = entry,
                            .argument = argument,
                            .state = RTOS_TASK_READY};
      task_table[i] = new_tcb;
      task_count++;
      return RTOS_OK;
    }
  }
  return RTOS_ERROR_TASK_LIMIT;
}

void rtos_init(void) {
  for (uint32_t i = 0U; i < RTOS_MAX_TASKS; i++) {
    task_table[i] = (rtos_tcb_t){0};
    task_table[i].state = RTOS_TASK_UNUSED;
  }
  task_count = 0U;
  current_task = 0;
}
