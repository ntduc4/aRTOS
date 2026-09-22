#include "demos/common/demo_board.h"
#include "rtos.h"

#include <stdint.h>

enum {
  TASK_STACK_WORDS = 128U,
  LOW_PRIORITY = 0U,
  HIGH_PRIORITY = 1U,
};

typedef struct {
  char name;
  uint32_t reports;
} worker_argument_t;

static artos_stack_word_t high_stack[TASK_STACK_WORDS]
    __attribute__((aligned(8)));
static artos_stack_word_t worker_a_stack[TASK_STACK_WORDS]
    __attribute__((aligned(8)));
static artos_stack_word_t worker_b_stack[TASK_STACK_WORDS]
    __attribute__((aligned(8)));

static artos_semaphore_storage_t uart_lock_storage;
static artos_semaphore_t *uart_lock;
static worker_argument_t worker_a = {'A', 0U};
static worker_argument_t worker_b = {'B', 0U};
static volatile uint32_t work_sink;

static void uart_lock_take(void) {
  (void)artos_semaphore_take(uart_lock, RTOS_DELAY_INFINITY);
}

static void high_task(void *argument) {
  (void)argument;
  uint32_t release_tick = artos_get_tick();
  uint32_t sequence = 0U;

  for (;;) {
    release_tick += RTOS_MS_TO_TICKS(1000U);
    demo_led_set(true);

    uart_lock_take();
    demo_uart_write_string("HIGH preempted low tasks at tick ");
    demo_uart_write_uint(artos_get_tick());
    demo_uart_write_string(", sequence ");
    demo_uart_write_uint(++sequence);
    demo_uart_write_char('\n');
    artos_semaphore_signal(uart_lock);

    artos_wait(RTOS_MS_TO_TICKS(100U));
    demo_led_set(false);
    artos_wait_until(release_tick);
  }
}

static void worker_task(void *argument) {
  worker_argument_t *worker = argument;
  uint32_t rounds = 0U;

  for (;;) {
    uint32_t value = work_sink;
    for (uint32_t i = 0U; i < 2000U; i++)
      value = value * 1664525U + 1013904223U;
    work_sink = value;

    if (++rounds == 100U) {
      rounds = 0U;
      uart_lock_take();
      demo_uart_write_string("LOW ");
      demo_uart_write_char(worker->name);
      demo_uart_write_string(" ran, report ");
      demo_uart_write_uint(++worker->reports);
      demo_uart_write_char('\n');
      artos_semaphore_signal(uart_lock);
    }

    artos_yield();
  }
}

static void create_task_or_halt(rtos_task_fn_t entry, void *argument,
                                artos_stack_word_t *stack, uint8_t priority) {
  if (artos_task_create(entry, argument, stack, TASK_STACK_WORDS, priority) !=
      ARTOS_OK) {
    for (;;) {
    }
  }
}

int main(void) {
  demo_board_init();
  rtos_init();

  uart_lock = artos_binary_semaphore_init(&uart_lock_storage, true);
  if (uart_lock == NULL) {
    for (;;) {
    }
  }

  demo_uart_write_string("aRTOS priority demo\n");
  demo_uart_write_string("Two priority-0 tasks round-robin; priority 1 "
                         "preempts them every second.\n");

  create_task_or_halt(worker_task, &worker_a, worker_a_stack, LOW_PRIORITY);
  create_task_or_halt(worker_task, &worker_b, worker_b_stack, LOW_PRIORITY);
  create_task_or_halt(high_task, NULL, high_stack, HIGH_PRIORITY);

  (void)artos_start();
  for (;;) {
  }
}
