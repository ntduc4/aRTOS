#ifndef RTOS_PORT_H
#define RTOS_PORT_H

#include "rtos.h"
#include "rtos/rtos_internal.h"

rtos_stack_word_t *rtos_port_initialize_stack(rtos_stack_word_t *stack_top,
                                              rtos_task_fn_t entry,
                                              void *argument);

// Arch-specific setup (making PendSV lowest priority exception on cortex-m4)
void rtos_port_scheduler_init(void);

void rtos_port_start_first_task(rtos_stack_word_t *saved_stack_pointer);
void rtos_port_request_context_switch(void);

typedef uint32_t rtos_port_irq_state_t;

rtos_port_irq_state_t rtos_port_enter_critical(void);
void rtos_port_exit_critical(rtos_port_irq_state_t previous_state);

typedef struct {
  uint32_t r0, r1, r2, r3, r12, lr, pc, xpsr;
  uint32_t exc_return; // LR at fault entry
  uint32_t cfsr, hfsr, mmfar, bfar;
  const rtos_tcb_t *task;
} rtos_fault_info_t;

extern volatile rtos_fault_info_t rtos_fault_info;

#endif
