#include "rtos.h"
#include "stm32f446xx.h"
#include <stdint.h>

#define TASK_STACK_WORDS 128U

static rtos_stack_word_t led_stack[TASK_STACK_WORDS]
    __attribute__((aligned(8)));
static rtos_stack_word_t usart_stack[TASK_STACK_WORDS]
    __attribute__((aligned(8)));

void setup_gpio() {
  // GPIOA clock enable (ref manual 6.3.10)
  RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;

  // ref manual 7.4.1
  GPIOA->MODER &= ~(0b11 << (5 * 2));
  GPIOA->MODER |= (0b01 << (5 * 2));
}

void delay(volatile uint32_t count) {
  while (count--)
    __NOP();
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

void USART2_write_char(char c) {
  while ((USART2->SR & USART_SR_TXE) == 0) {
  }
  USART2->DR = (uint8_t)c;
}

static void led_task(void *argument) {
  // Blink
  for (;;) {
    // Atomic write instead of using ODR (ref manual 7.3.5)
    GPIOA->BSRR |= 1 << 5;
    delay(500000);
    GPIOA->BSRR |= 1 << (5 + 16);
    delay(500000);
  }
}

static void usart_task(void *argument) {
  char str[] = "Hello worlds!\n";
  for (;;) {
    for (int i = 0; str[i] != '\0'; i++)
      USART2_write_char(str[i]);
    delay(1000000U);
  }
}

int main() {
  setup_gpio();
  setup_USART2();

  rtos_init();

  rtos_status_t status1 =
      rtos_task_create(led_task, NULL, led_stack, TASK_STACK_WORDS);
  if (status1 != RTOS_OK)
    for (;;) {
    }

  rtos_status_t status2 =
      rtos_task_create(usart_task, NULL, usart_stack, TASK_STACK_WORDS);
  if (status2 != RTOS_OK)
    for (;;) {
    }

  rtos_start();

  // Just in case
  for (;;) {
  }
}
