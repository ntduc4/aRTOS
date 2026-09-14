#ifndef RTOS_PORT_H
#define RTOS_PORT_H

#include "rtos.h"

rtos_stack_word_t *rtos_port_initialize_stack(rtos_stack_word_t *stack_top,
                                              rtos_task_fn_t entry,
                                              void *argument);

// Arch-specific setup (making PendSV lowest priority exception on cortex-m4)
void rtos_port_scheduler_init(void);

void rtos_port_start_first_task(rtos_stack_word_t *saved_stack_pointer);
void rtos_port_request_context_switch(void);

#endif
