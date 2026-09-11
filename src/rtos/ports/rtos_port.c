#include "rtos_port.h"
#include "cmsis_compiler.h"
#include <stdint.h>

_Static_assert(sizeof(rtos_stack_word_t) == sizeof(uint32_t),
               "Cortex-M requires 32-bit stack words");

#define RTOS_PORT_INITIAL_XPSR (1UL << 24)
#define RTOS_PORT_INITIAL_EXC_RETURN (0xFFFFFFFDU)

static void rtos_port_task_return_trap(void) __attribute__((noreturn));

// Literally just a trap that do nothing
static void rtos_port_task_return_trap(void) {
  __disable_irq();
  for (;;)
    __WFI();
}

// TO BE IMPLEMENT
rtos_stack_word_t *rtos_port_initialize_stack(rtos_stack_word_t *stack_top,
                                              rtos_task_fn_t entry,
                                              void *argument) {
  // Auto saved registers
  *(--stack_top) = RTOS_PORT_INITIAL_XPSR;                // xPSR register;
  *(--stack_top) = ((uintptr_t)entry) & ~(uintptr_t)1U;   // PC
  *(--stack_top) = (uintptr_t)rtos_port_task_return_trap; // Task LR
  *(--stack_top) = 0U;                                    // R12
  *(--stack_top) = 0U;                                    // R3
  *(--stack_top) = 0U;                                    // R2
  *(--stack_top) = 0U;                                    // R1
  *(--stack_top) = (uintptr_t)argument;                   // R0

  // Software saved registers
  *(--stack_top) = RTOS_PORT_INITIAL_EXC_RETURN; // Table 18 programming manual
  for (uint32_t i = 0U; i < 8U; i++)
    *(--stack_top) = 0U;

  return stack_top;
}
