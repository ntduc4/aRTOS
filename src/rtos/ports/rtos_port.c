#include "rtos_port.h"
#include "cmsis_gcc.h"
#include "core_cm4.h"
#include "rtos.h"
#include "rtos_config.h"
#include "stm32f446xx.h"

_Static_assert(sizeof(rtos_stack_word_t) == sizeof(uint32_t),
               "Cortex-M requires 32-bit stack words");
#if (__FPU_USED != 1U)
#error "Cortex-M4F port requires hardware floating-point support"
#endif

#define RTOS_PORT_INITIAL_XPSR (1UL << 24)
#define RTOS_PORT_INITIAL_EXC_RETURN (0xFFFFFFFDU)

static void rtos_port_task_return_trap(void) __attribute__((noreturn));

// Literally just a trap that do nothing
static void rtos_port_task_return_trap(void) {
  __disable_irq();
  for (;;)
    __WFI();
}

rtos_port_irq_state_t rtos_port_enter_critical(void) {
  rtos_port_irq_state_t previous_state = __get_PRIMASK();

  __disable_irq();
  __DMB();
  return previous_state;
}

void rtos_port_exit_critical(rtos_port_irq_state_t previous_state) {
  __DMB();
  __set_PRIMASK(previous_state);
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

void PendSV_Handler(void) __attribute__((naked));

// - Move psp -> r0
// - Check bit 4 of LR == 0 or not (0 = floating point, 1 = non FP)
// - Store (or don't) the FP register to the stack
// - Store software-saved register to r0, decrease before since stack top point
//   to current top of stack (stack grow downard)
//
// - Call scheduler
//
// - Load software-saved register from r0, increase after since stack top point
//   to current top of stack (stack grow downard)
// - Check bit 4 of LR == 0 or not (0 = floating point, 1 = non FP)
// - Load (or don't) the FP register to the stack
// - Move r0 -> psp
void PendSV_Handler(void) {
  __asm volatile("mrs r0, psp \n"
                 "tst lr, #0x10 \n"
                 "it eq \n"
                 "vstmdbeq r0!, {s16-s31} \n"
                 "stmdb r0!, {r4-r11, lr} \n"
                 "bl rtos_scheduler_switch_context \n"
                 "ldmia r0!, {r4-r11, lr} \n"
                 "tst lr, #0x10 \n"
                 "it eq \n"
                 "vldmiaeq r0!, {s16-s31} \n"
                 "msr psp, r0 \n"
                 "isb \n"
                 "bx lr \n");
}

void rtos_port_scheduler_init(void) {
  NVIC_SetPriority(PendSV_IRQn, 15U);
  SCB->CPACR |= 0xFUL << 20;
  __DSB();
  __ISB();
  FPU->FPCCR |= FPU_FPCCR_ASPEN_Msk | FPU_FPCCR_LSPEN_Msk;
}

void rtos_port_request_context_switch(void) {
  SCB->ICSR = (1 << 28);
  __DSB();
  __ISB();
}

void rtos_port_tick_init(void) {
  // Copied from SysTick_Config() from "core_cm4"
  // Check 4.5 programming manual also
  SysTick->LOAD =
      (uint32_t)(SystemCoreClock / RTOS_TICK_HZ) - 1U; /* set reload register */
  NVIC_SetPriority(SysTick_IRQn, 14U); /* set Priority for Systick Interrupt */
  SysTick->VAL = 0UL;                  /* Load the SysTick Counter Value */
  SysTick->CTRL =
      SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_TICKINT_Msk |
      SysTick_CTRL_ENABLE_Msk; /* Enable SysTick IRQ and SysTick Timer */
}

void SysTick_Handler(void) __attribute__((naked));
void SysTick_Handler(void) {
  __asm volatile("bl rtos_scheduler_tick \n"
                 "bx lr \n");
}
