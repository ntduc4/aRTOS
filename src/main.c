#include "stm32f446xx.h"
#include <stdint.h>

void setup() {
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

int main() {
  setup();
  setup_USART2();

  char str[] = "Hello worlds!";
  for (int i = 0; str[i] != '\0'; i++)
    USART2_write_char(str[i]);

  // Blink
  while (1) {
    // Atomic write instead of using ODR (ref manual 7.3.5)
    GPIOA->BSRR |= 1 << 5;
    delay(500000);
    GPIOA->BSRR |= 1 << (5 + 16);
    delay(500000);
  }
}
