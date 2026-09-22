#ifndef ARTOS_PORT_H
#define ARTOS_PORT_H

#include "rtos.h"

artos_stack_word_t *rtos_port_initialize_stack(artos_stack_word_t *stack_top,
                                               artos_task_fn_t entry,
                                               void *argument);

// Arch-specific setup (making PendSV lowest priority exception on cortex-m4)
void rtos_port_scheduler_init(void);

void rtos_port_start_first_task(artos_stack_word_t *saved_stack_pointer);
void rtos_port_request_context_switch(void);
void rtos_port_request_context_switch_from_isr(void);
uint8_t rtos_port_find_msb32(uint32_t bitmap);
void rtos_port_idle_task(void *argument);
void rtos_port_halt(void) __attribute__((noreturn));

typedef uint32_t rtos_port_irq_state_t;

rtos_port_irq_state_t rtos_port_enter_critical(void);
void rtos_port_exit_critical(rtos_port_irq_state_t previous_state);

typedef struct rtos_fault_info rtos_fault_info_t;

extern volatile rtos_fault_info_t rtos_fault_info;

#endif // !ARTOS_PORT_H
