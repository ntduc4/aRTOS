#include "boards/board.h"
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
static artos_stack_word_t high_stack[TASK_STACK_WORDS]
    __attribute__((aligned(8)));

static artos_mutex_storage_t mutex_storage;
static artos_mutex_t *mutex;

static void blink_with_delay(uint32_t duration_ms, uint32_t interval_ms) {
  uint32_t start_tick = artos_get_tick();
  bool led_enabled = false;

  while (artos_get_tick() - start_tick < ARTOS_MS_TO_TICKS(duration_ms)) {
    led_enabled = !led_enabled;
    board_led_set(led_enabled);
    artos_wait(ARTOS_MS_TO_TICKS(interval_ms));
  }

  board_led_set(false);
}

static void high_priority_busy_blink(void) {
  uint32_t start_tick = artos_get_tick();
  uint32_t next_toggle = start_tick + ARTOS_MS_TO_TICKS(500U);
  bool led_enabled = false;

  while (artos_get_tick() - start_tick < ARTOS_MS_TO_TICKS(3000U)) {
    if (artos_get_tick() >= next_toggle) {
      led_enabled = !led_enabled;
      board_led_set(led_enabled);
      next_toggle += ARTOS_MS_TO_TICKS(500U);
    }
  }

  board_led_set(false);
}

static void low_task(void *argument) {
  (void)argument;

  if (!artos_mutex_lock(mutex, ARTOS_DELAY_INFINITY)) {
    for (;;) {
    }
  }

  board_uart_write_string(
      "LOW acquired mutex and will fast-blink for five seconds\n");
  uint32_t start_tick = artos_get_tick();
  bool inheritance_reported = false;
  bool led_enabled = false;

  while (artos_get_tick() - start_tick < ARTOS_MS_TO_TICKS(5000U)) {
    led_enabled = !led_enabled;
    board_led_set(led_enabled);
    artos_wait(ARTOS_MS_TO_TICKS(100U));

    if (!inheritance_reported &&
        artos_get_tick() - start_tick >= ARTOS_MS_TO_TICKS(1500U)) {
      artos_task_info_t info;
      if (artos_task_get_info(LOW_TASK_INDEX, &info)) {
        board_uart_write_string("LOW while HIGH waits: base=");
        board_uart_write_uint(info.base_priority);
        board_uart_write_string(", effective=");
        board_uart_write_uint(info.effective_priority);
        board_uart_write_char('\n');
      }
      inheritance_reported = true;
    }
  }

  board_led_set(false);
  board_uart_write_string("LOW releasing mutex; inherited priority restored\n");
  (void)artos_mutex_unlock(mutex);

  for (;;)
    artos_wait(ARTOS_DELAY_INFINITY);
}

static void high_task(void *argument) {
  (void)argument;

  board_uart_write_string("HIGH slow-blinking for three seconds\n");
  high_priority_busy_blink();

  board_uart_write_string("HIGH sleeping so LOW can acquire the mutex\n");
  artos_wait(ARTOS_MS_TO_TICKS(1000U));

  board_uart_write_string("HIGH waiting for mutex at priority 1\n");
  if (!artos_mutex_lock(mutex, ARTOS_DELAY_INFINITY)) {
    for (;;) {
    }
  }

  board_uart_write_string("HIGH received mutex directly from LOW\n");
  board_uart_write_string("HIGH slow-blinking for three seconds with mutex\n");
  blink_with_delay(3000U, 500U);
  (void)artos_mutex_unlock(mutex);
  board_uart_write_string("Mutex demo complete\n");

  for (;;)
    artos_wait(ARTOS_DELAY_INFINITY);
}

static void create_task_or_halt(artos_task_fn_t entry,
                                artos_stack_word_t *stack, uint8_t priority) {
  if (artos_task_create(entry, NULL, stack, TASK_STACK_WORDS, priority) !=
      ARTOS_OK) {
    for (;;) {
    }
  }
}

int main(void) {
  board_init();
  artos_init();

  mutex = artos_mutex_init(&mutex_storage);
  if (mutex == NULL) {
    for (;;) {
    }
  }

  board_uart_write_string("aRTOS mutex priority-inheritance demo\n");
  board_uart_write_string(
      "HIGH runs first, then sleeps so LOW can acquire the mutex.\n");

  create_task_or_halt(low_task, low_stack, LOW_PRIORITY);
  create_task_or_halt(high_task, high_stack, HIGH_PRIORITY);

  (void)artos_start();
  for (;;) {
  }
}
