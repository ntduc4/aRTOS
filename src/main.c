#include "stm32f446xx.h"

void setup() {
  // GPIOA clock enable
  RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;

  // ref manual 7.4.1
  GPIOA->MODER &= ~(0b11 << (5 * 2));
  GPIOA->MODER |= (0b01 << (5 * 2));
}

void delay(volatile uint32_t count) {
  while (count--)
    __NOP();
}

int main() {
  setup();

  while (1) {
    // Atomic write instead of using ODR (ref manual 7.3.5)
    GPIOA->BSRR |= 1 << 5;
    delay(500000);
    GPIOA->BSRR |= 1 << (5 + 16);
    delay(500000);
  }
}
