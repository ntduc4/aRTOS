#include "boards/board.h"

#include "stm32f446xx.h"

enum {
  BOARD_LED_PIN = 5U,
  BOARD_BUTTON_PIN = 13U,
  BOARD_USART_BRR_16_MHZ_115200 = 139U,
};

#define BOARD_BUTTON_MASK (1UL << BOARD_BUTTON_PIN)

static board_button_callback_t button_callback;

void board_init(void) {
  RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
  RCC->APB1ENR |= RCC_APB1ENR_USART2EN;

  GPIOA->MODER &= ~(3UL << (BOARD_LED_PIN * 2U));
  GPIOA->MODER |= 1UL << (BOARD_LED_PIN * 2U);

  GPIOA->MODER &= ~(3UL << (2U * 2U));
  GPIOA->MODER |= 2UL << (2U * 2U);
  GPIOA->AFR[0] &= ~(15UL << (2U * 4U));
  GPIOA->AFR[0] |= 7UL << (2U * 4U);

  USART2->CR1 = 0U;
  USART2->CR2 = 0U;
  USART2->CR3 = 0U;
  USART2->BRR = BOARD_USART_BRR_16_MHZ_115200;
  USART2->CR1 = USART_CR1_TE | USART_CR1_UE;

  board_led_set(false);
}

void board_button_init(board_button_callback_t callback) {
  button_callback = callback;

  RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN;
  RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;

  GPIOC->MODER &= ~(3UL << (BOARD_BUTTON_PIN * 2U));
  GPIOC->PUPDR &= ~(3UL << (BOARD_BUTTON_PIN * 2U));
  GPIOC->PUPDR |= 1UL << (BOARD_BUTTON_PIN * 2U);

  SYSCFG->EXTICR[3] &= ~(15UL << 4U);
  SYSCFG->EXTICR[3] |= 2UL << 4U;

  EXTI->IMR &= ~BOARD_BUTTON_MASK;
  EXTI->RTSR &= ~BOARD_BUTTON_MASK;
  EXTI->FTSR |= BOARD_BUTTON_MASK;
  EXTI->PR = BOARD_BUTTON_MASK;

  NVIC_ClearPendingIRQ(EXTI15_10_IRQn);
  NVIC_SetPriority(EXTI15_10_IRQn, 13U);
  EXTI->IMR |= BOARD_BUTTON_MASK;
  NVIC_EnableIRQ(EXTI15_10_IRQn);
}

void EXTI15_10_IRQHandler(void) {
  if ((EXTI->PR & BOARD_BUTTON_MASK) == 0U)
    return;

  EXTI->PR = BOARD_BUTTON_MASK;
  if (button_callback != NULL)
    button_callback();
}

void board_led_set(bool enabled) {
  GPIOA->BSRR = enabled ? (1UL << BOARD_LED_PIN)
                        : (1UL << (BOARD_LED_PIN + 16U));
}

void board_uart_write_char(char character) {
  while ((USART2->SR & USART_SR_TXE) == 0U) {
  }
  USART2->DR = (uint8_t)character;
}

void board_uart_write_string(const char *text) {
  while (*text != '\0')
    board_uart_write_char(*text++);
}

void board_uart_write_uint(uint32_t value) {
  char buffer[10];
  uint32_t length = 0U;

  if (value == 0U) {
    board_uart_write_char('0');
    return;
  }

  while (value > 0U) {
    buffer[length++] = (char)('0' + (value % 10U));
    value /= 10U;
  }

  while (length > 0U)
    board_uart_write_char(buffer[--length]);
}
