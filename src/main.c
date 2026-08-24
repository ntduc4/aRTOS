#include "stm32f4xx.h"

void setup() { RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN; }

int main() {}
