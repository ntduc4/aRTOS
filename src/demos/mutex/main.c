#include "demos/common/demo_board.h"
#include "rtos.h"
#include "rtos_diagnostics.h"

#include <stdbool.h>
#include <stdint.h>

enum {
  TASK_STACK_WORDS = 128U,
  LOW_PRIORITY = 0U,
  HIGH_PRIORITY = 1U,
  LOW_TASK_INDEX = 0U,
};

static artos_stack_word_t low_stack[TASK_STACK_WORDS]
    __attribute__((aligned(8)));
static artos_stack_word_t interferer_stack[TASK_STACK_WORDS]
    __attribute__((aligned(8)));
static artos_stack_word_t high_stack[TASK_STACK_WORDS]
    __attribute__((aligned(8)));

static artos_mutex_storage_t mutex_storage;
static artos_mutex_t *mutex;
static volatile uint32_t work_sink;
static volatile uint32_t interferer_rounds;

static void low_task(void *argument) {
  (void)argument;

  if (!artos_mutex_lock(mutex, RTOS_DELAY_INFINITY)) {
    for (;;) {
    }
  }

  demo_uart_write_string("LOW acquired mutex at base priority 0\n");
  uint32_t start_tick = artos_get_tick();
  bool inheritance_reported = false;

  while (artos_get_tick() - start_tick < RTOS_MS_TO_TICKS(1000U)) {
    uint32_t value = work_sink;
    for (uint32_t i = 0U; i < 2000U; i++)
      value = value * 1664525U + 1013904223U;
    work_sink = value;

    if (!inheritance_reported &&
        artos_get_tick() - start_tick >= RTOS_MS_TO_TICKS(200U)) {
      artos_task_info_t info;
      if (artos_task_get_info(LOW_TASK_INDEX, &info)) {
        demo_uart_write_string("LOW while HIGH waits: base=");
        demo_uart_write_uint(info.base_priority);
        demo_uart_write_string(", effective=");
        demo_uart_write_uint(info.effective_priority);
        demo_uart_write_char('\n');
      }
      inheritance_reported = true;
    }
  }

  (void)artos_mutex_unlock(mutex);
  demo_uart_write_string("LOW released mutex; inherited priority restored\n");

  for (;;)
    artos_wait(RTOS_DELAY_INFINITY);
}

static void interferer_task(void *argument) {
  (void)argument;
  for (;;) {
    uint32_t value = work_sink;
    for (uint32_t i = 0U; i < 2000U; i++)
      value = value * 1103515245U + 12345U;
    work_sink = value;
    interferer_rounds++;
    artos_yield();
  }
}

static void high_task(void *argument) {
  (void)argument;
  artos_wait(RTOS_MS_TO_TICKS(100U));

  demo_uart_write_string("HIGH waiting for mutex at priority 1\n");
  if (!artos_mutex_lock(mutex, RTOS_DELAY_INFINITY)) {
    for (;;) {
    }
  }

  demo_led_set(true);
  demo_uart_write_string("HIGH received mutex directly from LOW\n");
  demo_uart_write_string("Background rounds before handoff: ");
  demo_uart_write_uint(interferer_rounds);
  demo_uart_write_char('\n');
  (void)artos_mutex_unlock(mutex);

  for (;;)
    artos_wait(RTOS_DELAY_INFINITY);
}

static void create_task_or_halt(rtos_task_fn_t entry, artos_stack_word_t *stack,
                                uint8_t priority) {
  if (artos_task_create(entry, NULL, stack, TASK_STACK_WORDS, priority) !=
      ARTOS_OK) {
    for (;;) {
    }
  }
}

int main(void) {
  demo_board_init();
  rtos_init();

  mutex = artos_mutex_init(&mutex_storage);
  if (mutex == NULL) {
    for (;;) {
    }
  }

  demo_uart_write_string("aRTOS mutex priority-inheritance demo\n");
  demo_uart_write_string("LOW owns the mutex before HIGH wakes; the background "
                         "task stays ready.\n");

  create_task_or_halt(low_task, low_stack, LOW_PRIORITY);
  create_task_or_halt(interferer_task, interferer_stack, LOW_PRIORITY);
  create_task_or_halt(high_task, high_stack, HIGH_PRIORITY);

  (void)artos_start();
  for (;;) {
  }
}
