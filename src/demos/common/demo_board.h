#ifndef ARTOS_DEMO_BOARD_H
#define ARTOS_DEMO_BOARD_H

#include <stdbool.h>
#include <stdint.h>

void demo_board_init(void);
void demo_led_set(bool enabled);
void demo_uart_write_char(char character);
void demo_uart_write_string(const char *text);
void demo_uart_write_uint(uint32_t value);

#endif
