#include "rtos_port.h"
#include "cmsis_gcc.h"

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

void rtos_port_start_first_task(rtos_stack_word_t *saved_stack_pointer) {
  // PSP initially have software-saved registers
  __set_PSP((uint32_t)(uintptr_t)saved_stack_pointer);

  // Make SVC able to execute
  __enable_irq();

  // Enger handler mode so EXC_RETURN can be used
  __asm volatile("svc 0" ::: "memory");

  // Should not execute here
  for (;;) {
  }
}

// Overwrite weak default from cmsis
void SVC_Handler(void) __attribute__((naked));

void SVC_Handler(void) {
  __asm volatile("mrs r0, psp \n"
                 "ldmia r0!, {r4-r11, lr} \n"
                 "msr psp, r0 \n"
                 "isb \n"
                 "bx lr \n");
}
