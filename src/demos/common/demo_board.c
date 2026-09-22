#include "demo_board.h"

#include "stm32f446xx.h"

enum {
  DEMO_LED_PIN = 5U,
  DEMO_USART_BRR_16_MHZ_115200 = 139U,
};

void demo_board_init(void) {
  RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
  RCC->APB1ENR |= RCC_APB1ENR_USART2EN;

  GPIOA->MODER &= ~(3UL << (DEMO_LED_PIN * 2U));
  GPIOA->MODER |= 1UL << (DEMO_LED_PIN * 2U);

  GPIOA->MODER &= ~(3UL << (2U * 2U));
  GPIOA->MODER |= 2UL << (2U * 2U);
  GPIOA->AFR[0] &= ~(15UL << (2U * 4U));
  GPIOA->AFR[0] |= 7UL << (2U * 4U);

  USART2->CR1 = 0U;
  USART2->CR2 = 0U;
  USART2->CR3 = 0U;
  USART2->BRR = DEMO_USART_BRR_16_MHZ_115200;
  USART2->CR1 = USART_CR1_TE | USART_CR1_UE;

  demo_led_set(false);
}

void demo_led_set(bool enabled) {
  GPIOA->BSRR = enabled ? (1UL << DEMO_LED_PIN)
                        : (1UL << (DEMO_LED_PIN + 16U));
}

void demo_uart_write_char(char character) {
  while ((USART2->SR & USART_SR_TXE) == 0U) {
  }
  USART2->DR = (uint8_t)character;
}

void demo_uart_write_string(const char *text) {
  while (*text != '\0')
    demo_uart_write_char(*text++);
}

void demo_uart_write_uint(uint32_t value) {
  char buffer[10];
  uint32_t length = 0U;

  if (value == 0U) {
    demo_uart_write_char('0');
    return;
  }

  while (value > 0U) {
    buffer[length++] = (char)('0' + (value % 10U));
    value /= 10U;
  }

  while (length > 0U)
    demo_uart_write_char(buffer[--length]);
}
