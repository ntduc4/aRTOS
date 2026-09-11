#include "stm32f446xx.h"
#include <stdint.h>

void unityOutputStart(unsigned long baudrate) {
  RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
  RCC->APB1ENR |= RCC_APB1ENR_USART2EN;

  GPIOA->MODER &= ~(3UL << (2U * 2U));
  GPIOA->MODER |= 2UL << (2U * 2U);
  GPIOA->AFR[0] &= ~(15UL << (2U * 4U));
  GPIOA->AFR[0] |= 7UL << (2U * 4U);

  USART2->CR1 = 0U;
  USART2->CR2 = 0U;
  USART2->CR3 = 0U;
  USART2->BRR = (SystemCoreClock + (baudrate / 2UL)) / baudrate;
  USART2->CR1 = USART_CR1_TE | USART_CR1_UE;
}

void unityOutputChar(unsigned int character) {
  while ((USART2->SR & USART_SR_TXE) == 0U) {
  }
  USART2->DR = (uint8_t)character;
}

void unityOutputFlush(void) {
  while ((USART2->SR & USART_SR_TC) == 0U) {
  }
}

void unityOutputComplete(void) { unityOutputFlush(); }
