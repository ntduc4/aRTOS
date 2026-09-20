#include "rtos.h"
#include "stm32f446xx.h"
#include <stdint.h>

#define TASK_STACK_WORDS 128U
#define BUTTON_PIN 13U
#define BUTTON_MASK (1UL << BUTTON_PIN)
#define QUEUE_CAPACITY 8U
#define BUTTON_QUEUE_CAPACITY 4U
#define HEARTBEAT_INTERVAL_MS 2000U
#define BUTTON_DEBOUNCE_MS 50U
#define CONSUMER_COUNT 3U
#define CONSUMER_TIMEOUT_MS 1200U
#define DEFAULT_PRIORITY 1U

static rtos_stack_word_t led_stack[TASK_STACK_WORDS]
    __attribute__((aligned(8)));
static rtos_stack_word_t heartbeat_stack[TASK_STACK_WORDS]
    __attribute__((aligned(8)));
static rtos_stack_word_t logger_stacks[CONSUMER_COUNT][TASK_STACK_WORDS]
    __attribute__((aligned(8)));
static rtos_stack_word_t button_logger_stack[TASK_STACK_WORDS]
    __attribute__((aligned(8)));
static rtos_stack_word_t load_stack[TASK_STACK_WORDS]
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

static rtos_queue_control_storage_t queue_control;
static demo_message_t queue_items[QUEUE_CAPACITY];
static rtos_queue_t *queue;
static rtos_queue_control_storage_t button_queue_control;
static demo_message_t button_queue_items[BUTTON_QUEUE_CAPACITY];
static rtos_queue_t *button_queue;
static rtos_semaphore_storage_t uart_lock_storage;
static rtos_semaphore_t *uart_lock;
static consumer_argument_t consumer_arguments[CONSUMER_COUNT] = {
    {1U, 2500U}, {2U, 3500U}, {3U, 4500U}};
static uint32_t button_sequence;
static uint32_t last_button_tick;
static bool button_seen;
static volatile uint32_t load_sink;

void setup_gpio() {
  // GPIOA clock enable (ref manual 6.3.10)
  RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;

  // ref manual 7.4.1
  GPIOA->MODER &= ~(0b11 << (5 * 2));
  GPIOA->MODER |= (0b01 << (5 * 2));
}

void setup_USART2() {
  // RCC->AHB1ENR |= RCC_AHB1ENR_DMA1EN; // For future DMA USART
  // ref manual 6.3.10
  RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
  RCC->APB1ENR |= RCC_APB1ENR_USART2EN;

  // PA2
  GPIOA->MODER &= ~(0b11U << (2 * 2));
  GPIOA->MODER |= (0b10U << (2 * 2)); // alternate mode

  GPIOA->AFR[0] &= ~(0b1111U << (2 * 4));
  GPIOA->AFR[0] |= (0b0111U << (2 * 4)); // AF7

  // ref manual 25.6
  USART2->CR1 &= ~USART_CR1_UE;
  // 8 data bits, no parity, 1 stop bit, 16x oversampling
  USART2->CR1 &= ~(USART_CR1_M | USART_CR1_PCE | USART_CR1_OVER8);
  USART2->CR2 &= ~USART_CR2_STOP;
  // Baud rate: 115200, -> BRR = 16 MHz / 115200 ~= 139;
  USART2->BRR = 139U;
  // Transmitter
  USART2->CR1 |= USART_CR1_TE;
  USART2->CR1 |= USART_CR1_UE;
}

void setup_button() {
  RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN;
  RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;

  // PC13 input with pull-up; the Nucleo user button is active-low.
  GPIOC->MODER &= ~(0b11UL << (BUTTON_PIN * 2U));
  GPIOC->PUPDR &= ~(0b11UL << (BUTTON_PIN * 2U));
  GPIOC->PUPDR |= 0b01UL << (BUTTON_PIN * 2U);

  // Route PC13 to EXTI13 (EXTICR4, port C = 0b0010).
  SYSCFG->EXTICR[3] &= ~(0b1111UL << 4U);
  SYSCFG->EXTICR[3] |= 0b0010UL << 4U;

  EXTI->IMR &= ~BUTTON_MASK;
  EXTI->RTSR &= ~BUTTON_MASK;
  EXTI->FTSR |= BUTTON_MASK;
  EXTI->PR = BUTTON_MASK;

  NVIC_ClearPendingIRQ(EXTI15_10_IRQn);
  NVIC_SetPriority(EXTI15_10_IRQn, 13U);
  EXTI->IMR |= BUTTON_MASK;
  NVIC_EnableIRQ(EXTI15_10_IRQn);
}

void EXTI15_10_IRQHandler(void) {
  if ((EXTI->PR & BUTTON_MASK) == 0U)
    return;

  EXTI->PR = BUTTON_MASK;
  uint32_t now = rtos_get_tick();
  if (button_seen &&
      now - last_button_tick < RTOS_MS_TO_TICKS(BUTTON_DEBOUNCE_MS))
    return;

  button_seen = true;
  last_button_tick = now;
  demo_message_t message = {DEMO_BUTTON, ++button_sequence, now};
  bool task_woken = false;
  rtos_queue_enqueue_from_isr(button_queue, (uint8_t *)&message, &task_woken);
  rtos_queue_enqueue_from_isr(queue, (uint8_t *)&message, &task_woken);
  if (task_woken)
    rtos_yield_from_isr();
}

void USART2_write_char(char c) {
  while ((USART2->SR & USART_SR_TXE) == 0) {
  }
  USART2->DR = (uint8_t)c;
}

static void USART2_write_uint(uint32_t value) {
  char buf[10];
  uint32_t i = 0;
  if (value == 0) {
    USART2_write_char('0');
    return;
  }
  while (value > 0) {
    buf[i++] = (char)('0' + (value % 10));
    value /= 10;
  }
  while (i > 0)
    USART2_write_char(buf[--i]);
}

static void USART2_write_string(const char *text) {
  while (*text)
    USART2_write_char(*text++);
}

static void led_task(void *argument) {
  // Blink
  uint32_t next_run_ms = 0;
  for (;;) {
    next_run_ms += 2000;
    // Atomic write instead of using ODR (ref manual 7.3.5)
    GPIOA->BSRR = 1 << 5;
    rtos_wait(500);
    GPIOA->BSRR = 1 << (5 + 16);
    rtos_wait(100);
    GPIOA->BSRR = 1 << 5;
    rtos_wait(100);
    GPIOA->BSRR = 1 << (5 + 16);
    rtos_wait(100);
    GPIOA->BSRR = 1 << 5;
    rtos_wait(100);
    GPIOA->BSRR = 1 << (5 + 16);
    rtos_wait(100);
    GPIOA->BSRR = 1 << 5;
    rtos_wait(100);
    GPIOA->BSRR = 1 << (5 + 16);
    rtos_wait_until(RTOS_MS_TO_TICKS(next_run_ms));
  }
}

static void heartbeat_task(void *argument) {
  uint32_t sequence = 0;
  uint32_t next_tick = rtos_get_tick();
  for (;;) {
    demo_message_t message = {DEMO_HEARTBEAT, ++sequence, rtos_get_tick()};
    rtos_queue_enqueue(queue, (uint8_t *)&message, 0);
    next_tick += RTOS_MS_TO_TICKS(HEARTBEAT_INTERVAL_MS);
    rtos_wait_until(next_tick);
  }
}

static void logger_task(void *argument) {
  consumer_argument_t *consumer = argument;
  for (;;) {
    demo_message_t message;
    if (!rtos_queue_dequeue(queue, (uint8_t *)&message,
                            RTOS_MS_TO_TICKS(CONSUMER_TIMEOUT_MS))) {
      rtos_semaphore_take(uart_lock, RTOS_DELAY_INFINITY);
      USART2_write_char('C');
      USART2_write_uint(consumer->id);
      USART2_write_string("\tNONE\t\t#NONE\t@");
      USART2_write_uint(rtos_get_tick());
      USART2_write_string("\ttimeout=");
      USART2_write_uint(RTOS_MS_TO_TICKS(CONSUMER_TIMEOUT_MS));
      USART2_write_char('\n');
      rtos_semaphore_signal(uart_lock);
      continue;
    }

    uint32_t received_tick = rtos_get_tick();
    rtos_semaphore_take(uart_lock, RTOS_DELAY_INFINITY);
    USART2_write_char('C');
    USART2_write_uint(consumer->id);
    USART2_write_char('\t');
    USART2_write_string(message.event == DEMO_HEARTBEAT ? "HEARTBEAT"
                                                        : "BUTTON");
    USART2_write_string("\t#");
    USART2_write_uint(message.sequence);
    USART2_write_string("\tcreated=@");
    USART2_write_uint(message.created_tick);
    USART2_write_string("\treceived=@");
    USART2_write_uint(received_tick);
    USART2_write_string("\tage=");
    USART2_write_uint(received_tick - message.created_tick);
    USART2_write_char('\n');
    rtos_semaphore_signal(uart_lock);
    rtos_wait(RTOS_MS_TO_TICKS(consumer->delay_ms));
  }
}

static void button_logger_task(void *argument) {
  for (;;) {
    demo_message_t message;
    if (!rtos_queue_dequeue(button_queue, (uint8_t *)&message,
                            RTOS_DELAY_INFINITY))
      continue;

    uint32_t received_tick = rtos_get_tick();
    rtos_semaphore_take(uart_lock, RTOS_DELAY_INFINITY);
    USART2_write_string("BUTTON-ONLY\tBUTTON\t#");
    USART2_write_uint(message.sequence);
    USART2_write_string("\tcreated=@");
    USART2_write_uint(message.created_tick);
    USART2_write_string("\treceived=@");
    USART2_write_uint(received_tick);
    USART2_write_string("\tage=");
    USART2_write_uint(received_tick - message.created_tick);
    USART2_write_char('\n');
    rtos_semaphore_signal(uart_lock);
  }
}

static void load_task(void *argument) {
  for (;;) {
    uint32_t value = load_sink;
    for (uint32_t i = 0; i < 50000U; i++)
      value = value * 1664525U + 1013904223U;
    load_sink = value;
    rtos_yield();
  }
}

int main() {
  setup_gpio();
  setup_USART2();

  rtos_init();

  uart_lock = rtos_binary_semaphore_init(&uart_lock_storage, true);
  queue = rtos_queue_init(&queue_control, (uint8_t *)queue_items,
                          sizeof(demo_message_t), QUEUE_CAPACITY);
  button_queue =
      rtos_queue_init(&button_queue_control, (uint8_t *)button_queue_items,
                      sizeof(demo_message_t), BUTTON_QUEUE_CAPACITY);
  if (uart_lock == NULL || queue == NULL || button_queue == NULL)
    for (;;) {
    }
  USART2_write_string(
      "aRTOS showcase: 0.5 Hz heartbeat + button -> shared C1-C3 queue\n");
  USART2_write_string(
      "Button events are also copied to the immediate BUTTON-ONLY queue.\n");
  USART2_write_string("TASK\tEVENT\t\tMSG\tCREATED\tRECEIVED\tDETAIL\n");

  rtos_status_t status1 = rtos_task_create(led_task, NULL, led_stack,
                                           TASK_STACK_WORDS, DEFAULT_PRIORITY);
  if (status1 != RTOS_OK)
    for (;;) {
    }

  if (rtos_task_create(heartbeat_task, NULL, heartbeat_stack, TASK_STACK_WORDS,
                       DEFAULT_PRIORITY) != RTOS_OK)
    for (;;) {
    }

  for (uint32_t i = 0; i < CONSUMER_COUNT; i++) {
    if (rtos_task_create(logger_task, &consumer_arguments[i], logger_stacks[i],
                         TASK_STACK_WORDS, DEFAULT_PRIORITY) != RTOS_OK)
      for (;;) {
      }
  }

  if (rtos_task_create(button_logger_task, NULL, button_logger_stack,
                       TASK_STACK_WORDS, DEFAULT_PRIORITY) != RTOS_OK)
    for (;;) {
    }

  if (rtos_task_create(load_task, NULL, load_stack, TASK_STACK_WORDS,
                       DEFAULT_PRIORITY) != RTOS_OK)
    for (;;) {
    }

  setup_button();
  rtos_start();

  // Just in case
  for (;;) {
  }
}
