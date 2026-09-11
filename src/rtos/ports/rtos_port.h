#ifndef RTOS_PORT_H
#define RTOS_PORT_H

#include "rtos.h"

rtos_stack_word_t *rtos_port_initialize_stack(rtos_stack_word_t *stack_top,
                                              rtos_task_fn_t entry,
                                              void *argument);

void rtos_port_start_first_task(void);
void rtos_port_request_context_switch(void);

#endif
