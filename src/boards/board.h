#ifndef ARTOS_BOARD_H
#define ARTOS_BOARD_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef void (*board_button_callback_t)(void);

void board_init(void);
void board_button_init(board_button_callback_t callback);
void board_led_set(bool enabled);
void board_uart_write_char(char character);
void board_uart_write_string(const char *text);
void board_uart_write_uint(uint32_t value);

#endif
