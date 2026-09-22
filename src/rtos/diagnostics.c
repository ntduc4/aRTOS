#include "rtos/diagnostics.h"
#include "artos_assert.h"
#include "rtos/ports/rtos_port.h"
#include "rtos/tasks.h"

void __attribute__((noreturn)) rtos_assert_failed(const char *file,
                                                  uint32_t line) {
  rtos_record_failure(ARTOS_FAILURE_ASSERT, file, line);
}

__attribute__((
    section(".noinit"))) volatile rtos_failure_info_t rtos_failure_info;

void __attribute__((noreturn))
rtos_record_failure(artos_failure_reason_t reason, const char *file,
                    uint32_t line) {
  // Don't need prev state, don't need to return
  rtos_port_enter_critical();
  rtos_failure_info.reason = reason;
  rtos_failure_info.file = file;
  rtos_failure_info.line = line;
  rtos_failure_info.tick = rtos_current_tick();

  const rtos_tcb_t *task = rtos_scheduler_current_task();

  if (task != NULL) {
    rtos_failure_info.task_entry = task->entry;
    rtos_failure_info.base_priority = task->priority;
    rtos_failure_info.effective_priority = task->effective_priority;

    rtos_failure_info.stack_pointer = task->stack_pointer;
    rtos_failure_info.stack_low = task->stack_buffer;
    rtos_failure_info.stack_high = task->stack_buffer + task->stack_word_count;
  } else {
    rtos_failure_info.task_entry = NULL;
    rtos_failure_info.base_priority = 0;
    rtos_failure_info.effective_priority = 0;
    rtos_failure_info.stack_pointer = NULL;
    rtos_failure_info.stack_low = NULL;
    rtos_failure_info.stack_high = NULL;
  }
  rtos_port_halt();
}
