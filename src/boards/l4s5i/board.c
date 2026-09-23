#include "boards/board.h"

#include "stm32l4s5xx.h"

enum {
  BOARD_LED_PIN = 5U,
  BOARD_BUTTON_PIN = 13U,
  BOARD_UART_TX_PIN = 6U,
  BOARD_USART_BRR_16_MHZ_115200 = 139U,
};

#define BOARD_BUTTON_MASK (1UL << BOARD_BUTTON_PIN)

static board_button_callback_t button_callback;

static void board_clock_init(void) {
  RCC->CR |= RCC_CR_HSION;
  while ((RCC->CR & RCC_CR_HSIRDY) == 0U) {
  }

  RCC->CFGR &= ~(RCC_CFGR_HPRE | RCC_CFGR_PPRE1 | RCC_CFGR_PPRE2 |
                 RCC_CFGR_SW);
  RCC->CFGR |= RCC_CFGR_SW_HSI;
  while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_HSI) {
  }

  SystemCoreClock = 16000000U;
}

void board_init(void) {
  board_clock_init();

  RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN | RCC_AHB2ENR_GPIOBEN;
  RCC->APB2ENR |= RCC_APB2ENR_USART1EN;

  GPIOA->MODER &= ~(3UL << (BOARD_LED_PIN * 2U));
  GPIOA->MODER |= 1UL << (BOARD_LED_PIN * 2U);

  GPIOB->MODER &= ~(3UL << (BOARD_UART_TX_PIN * 2U));
  GPIOB->MODER |= 2UL << (BOARD_UART_TX_PIN * 2U);
  GPIOB->AFR[0] &= ~(15UL << (BOARD_UART_TX_PIN * 4U));
  GPIOB->AFR[0] |= 7UL << (BOARD_UART_TX_PIN * 4U);

  USART1->CR1 = 0U;
  USART1->CR2 = 0U;
  USART1->CR3 = 0U;
  USART1->BRR = BOARD_USART_BRR_16_MHZ_115200;
  USART1->CR1 = USART_CR1_TE | USART_CR1_UE;

  board_led_set(false);
}

void board_button_init(board_button_callback_t callback) {
  button_callback = callback;

  RCC->AHB2ENR |= RCC_AHB2ENR_GPIOCEN;
  RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;

  GPIOC->MODER &= ~(3UL << (BOARD_BUTTON_PIN * 2U));
  GPIOC->PUPDR &= ~(3UL << (BOARD_BUTTON_PIN * 2U));
  GPIOC->PUPDR |= 1UL << (BOARD_BUTTON_PIN * 2U);

  SYSCFG->EXTICR[3] &= ~(15UL << 4U);
  SYSCFG->EXTICR[3] |= 2UL << 4U;

  EXTI->IMR1 &= ~BOARD_BUTTON_MASK;
  EXTI->RTSR1 &= ~BOARD_BUTTON_MASK;
  EXTI->FTSR1 |= BOARD_BUTTON_MASK;
  EXTI->PR1 = BOARD_BUTTON_MASK;

  NVIC_ClearPendingIRQ(EXTI15_10_IRQn);
  NVIC_SetPriority(EXTI15_10_IRQn, 13U);
  EXTI->IMR1 |= BOARD_BUTTON_MASK;
  NVIC_EnableIRQ(EXTI15_10_IRQn);
}

void EXTI15_10_IRQHandler(void) {
  if ((EXTI->PR1 & BOARD_BUTTON_MASK) == 0U)
    return;

  EXTI->PR1 = BOARD_BUTTON_MASK;
  if (button_callback != NULL)
    button_callback();
}

void board_led_set(bool enabled) {
  GPIOA->BSRR = enabled ? (1UL << BOARD_LED_PIN)
                        : (1UL << (BOARD_LED_PIN + 16U));
}

void board_uart_write_char(char character) {
  while ((USART1->ISR & USART_ISR_TXE_TXFNF) == 0U) {
  }
  USART1->TDR = (uint8_t)character;
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
