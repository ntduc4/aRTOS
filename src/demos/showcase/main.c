#include "boards/board.h"
#include "rtos.h"
#include <stdint.h>

#define TASK_STACK_WORDS 128U
#define QUEUE_CAPACITY 8U
#define BUTTON_QUEUE_CAPACITY 4U
#define HEARTBEAT_INTERVAL_MS 2000U
#define BUTTON_DEBOUNCE_MS 50U
#define CONSUMER_COUNT 3U
#define CONSUMER_TIMEOUT_MS 1200U
#define DEFAULT_PRIORITY 1U

static artos_stack_word_t led_stack[TASK_STACK_WORDS]
    __attribute__((aligned(8)));
static artos_stack_word_t heartbeat_stack[TASK_STACK_WORDS]
    __attribute__((aligned(8)));
static artos_stack_word_t logger_stacks[CONSUMER_COUNT][TASK_STACK_WORDS]
    __attribute__((aligned(8)));
static artos_stack_word_t button_logger_stack[TASK_STACK_WORDS]
    __attribute__((aligned(8)));
static artos_stack_word_t load_stack[TASK_STACK_WORDS]
    __attribute__((aligned(8)));
typedef enum {
  DEMO_HEARTBEAT = 0,
  DEMO_BUTTON,
} demo_event_t;

typedef struct {
  demo_event_t event;
  uint32_t sequence;
  uint32_t created_tick;
} demo_message_t;

typedef struct {
  uint32_t id;
  uint32_t delay_ms;
} consumer_argument_t;

static artos_queue_control_storage_t queue_control;
static demo_message_t queue_items[QUEUE_CAPACITY];
static artos_queue_t *queue;
static artos_queue_control_storage_t button_queue_control;
static demo_message_t button_queue_items[BUTTON_QUEUE_CAPACITY];
static artos_queue_t *button_queue;
static artos_semaphore_storage_t uart_lock_storage;
static artos_semaphore_t *uart_lock;
static consumer_argument_t consumer_arguments[CONSUMER_COUNT] = {
    {1U, 2500U}, {2U, 3500U}, {3U, 4500U}};
static uint32_t button_sequence;
static uint32_t last_button_tick;
static bool button_seen;
static volatile uint32_t load_sink;

static void button_event_isr(void) {
  uint32_t now = artos_get_tick();
  if (button_seen &&
      now - last_button_tick < ARTOS_MS_TO_TICKS(BUTTON_DEBOUNCE_MS))
    return;

  button_seen = true;
  last_button_tick = now;
  demo_message_t message = {DEMO_BUTTON, ++button_sequence, now};
  bool task_woken = false;
  artos_queue_enqueue_from_isr(button_queue, (uint8_t *)&message, &task_woken);
  artos_queue_enqueue_from_isr(queue, (uint8_t *)&message, &task_woken);
  if (task_woken)
    artos_yield_from_isr();
}

static void led_task(void *argument) {
  uint32_t next_run_tick = artos_get_tick();
  for (;;) {
    next_run_tick += ARTOS_MS_TO_TICKS(2000U);
    board_led_set(true);
    artos_wait(500);
    board_led_set(false);
    artos_wait(100);
    board_led_set(true);
    artos_wait(100);
    board_led_set(false);
    artos_wait(100);
    board_led_set(true);
    artos_wait(100);
    board_led_set(false);
    artos_wait(100);
    board_led_set(true);
    artos_wait(100);
    board_led_set(false);
    artos_wait_until(next_run_tick);
  }
}

static void heartbeat_task(void *argument) {
  uint32_t sequence = 0;
  uint32_t next_tick = artos_get_tick();
  for (;;) {
    demo_message_t message = {DEMO_HEARTBEAT, ++sequence, artos_get_tick()};
    artos_queue_enqueue(queue, (uint8_t *)&message, 0);
    next_tick += ARTOS_MS_TO_TICKS(HEARTBEAT_INTERVAL_MS);
    artos_wait_until(next_tick);
  }
}

static void logger_task(void *argument) {
  consumer_argument_t *consumer = argument;
  for (;;) {
    demo_message_t message;
    if (!artos_queue_dequeue(queue, (uint8_t *)&message,
                             ARTOS_MS_TO_TICKS(CONSUMER_TIMEOUT_MS))) {
      artos_semaphore_take(uart_lock, ARTOS_DELAY_INFINITY);
      board_uart_write_char('C');
      board_uart_write_uint(consumer->id);
      board_uart_write_string("\tNONE\t\t#NONE\t@");
      board_uart_write_uint(artos_get_tick());
      board_uart_write_string("\ttimeout=");
      board_uart_write_uint(ARTOS_MS_TO_TICKS(CONSUMER_TIMEOUT_MS));
      board_uart_write_char('\n');
      artos_semaphore_signal(uart_lock);
      continue;
    }

    uint32_t received_tick = artos_get_tick();
    artos_semaphore_take(uart_lock, ARTOS_DELAY_INFINITY);
    board_uart_write_char('C');
    board_uart_write_uint(consumer->id);
    board_uart_write_char('\t');
    board_uart_write_string(message.event == DEMO_HEARTBEAT ? "HEARTBEAT"
                                                           : "BUTTON");
    board_uart_write_string("\t#");
    board_uart_write_uint(message.sequence);
    board_uart_write_string("\tcreated=@");
    board_uart_write_uint(message.created_tick);
    board_uart_write_string("\treceived=@");
    board_uart_write_uint(received_tick);
    board_uart_write_string("\tage=");
    board_uart_write_uint(received_tick - message.created_tick);
    board_uart_write_char('\n');
    artos_semaphore_signal(uart_lock);
    artos_wait(ARTOS_MS_TO_TICKS(consumer->delay_ms));
  }
}

static void button_logger_task(void *argument) {
  for (;;) {
    demo_message_t message;
    if (!artos_queue_dequeue(button_queue, (uint8_t *)&message,
                             ARTOS_DELAY_INFINITY))
      continue;

    uint32_t received_tick = artos_get_tick();
    artos_semaphore_take(uart_lock, ARTOS_DELAY_INFINITY);
    board_uart_write_string("BUTTON-ONLY\tBUTTON\t#");
    board_uart_write_uint(message.sequence);
    board_uart_write_string("\tcreated=@");
    board_uart_write_uint(message.created_tick);
    board_uart_write_string("\treceived=@");
    board_uart_write_uint(received_tick);
    board_uart_write_string("\tage=");
    board_uart_write_uint(received_tick - message.created_tick);
    board_uart_write_char('\n');
    artos_semaphore_signal(uart_lock);
  }
}

static void load_task(void *argument) {
  for (;;) {
    uint32_t value = load_sink;
    for (uint32_t i = 0; i < 50000U; i++)
      value = value * 1664525U + 1013904223U;
    load_sink = value;
    artos_yield();
  }
}

int main(void) {
  board_init();

  artos_init();

  uart_lock = artos_binary_semaphore_init(&uart_lock_storage, true);
  queue = artos_queue_init(&queue_control, (uint8_t *)queue_items,
                           sizeof(demo_message_t), QUEUE_CAPACITY);
  button_queue =
      artos_queue_init(&button_queue_control, (uint8_t *)button_queue_items,
                       sizeof(demo_message_t), BUTTON_QUEUE_CAPACITY);
  if (uart_lock == NULL || queue == NULL || button_queue == NULL)
    for (;;) {
    }
  board_uart_write_string(
      "aRTOS showcase: 0.5 Hz heartbeat + button -> shared C1-C3 queue\n");
  board_uart_write_string(
      "Button events are also copied to the immediate BUTTON-ONLY queue.\n");
  board_uart_write_string("TASK\tEVENT\t\tMSG\tCREATED\tRECEIVED\tDETAIL\n");

  artos_status_t status1 = artos_task_create(
      led_task, NULL, led_stack, TASK_STACK_WORDS, DEFAULT_PRIORITY);
  if (status1 != ARTOS_OK)
    for (;;) {
    }

  if (artos_task_create(heartbeat_task, NULL, heartbeat_stack, TASK_STACK_WORDS,
                        DEFAULT_PRIORITY) != ARTOS_OK)
    for (;;) {
    }

  for (uint32_t i = 0; i < CONSUMER_COUNT; i++) {
    if (artos_task_create(logger_task, &consumer_arguments[i], logger_stacks[i],
                          TASK_STACK_WORDS, DEFAULT_PRIORITY) != ARTOS_OK)
      for (;;) {
      }
  }

  if (artos_task_create(button_logger_task, NULL, button_logger_stack,
                        TASK_STACK_WORDS, DEFAULT_PRIORITY) != ARTOS_OK)
    for (;;) {
    }

  if (artos_task_create(load_task, NULL, load_stack, TASK_STACK_WORDS,
                        DEFAULT_PRIORITY) != ARTOS_OK)
    for (;;) {
    }

  board_button_init(button_event_isr);
  artos_start();

  // Just in case
  for (;;) {
  }
}
