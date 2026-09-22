#include "rtos/ports/rtos_port.h"
#include <stdint.h>

#include "cmsis_gcc.h"
#include "rtos.h"
#include "rtos/diagnostics.h"
#include "rtos/tasks.h"
#include "rtos_config.h"

_Static_assert(sizeof(artos_stack_word_t) == sizeof(uint32_t),
               "Cortex-M requires 32-bit stack words");

#if !defined(__ARM_FP) || ((__ARM_FP & 0x04U) == 0U)
#error "Cortex-M4F port requires single-precision floating-point support"
#endif

#if RTOS_TICK_HZ == 0U
#error "RTOS_TICK_HZ must be greater than zero"
#endif

#if ARTOS_CPU_CLOCK_HZ < RTOS_TICK_HZ
#error "SysTick frequency exceeds CPU clock"
#endif

#if (ARTOS_CPU_CLOCK_HZ / RTOS_TICK_HZ) > 0x1000000UL
#error "SysTick reload exceeds 24 bits"
#endif

#define RTOS_PORT_INITIAL_XPSR (1UL << 24)
#define RTOS_PORT_INITIAL_EXC_RETURN (0xFFFFFFFDU)

#define ARTOS_SCS 0xE000E000UL

#define ARTOS_SysTick (ARTOS_SCS + 0x0010UL)
#define ARTOS_SysTick_CTRL (*(volatile uint32_t *)ARTOS_SysTick)
#define ARTOS_SysTick_LOAD (*(volatile uint32_t *)(ARTOS_SysTick + 0x04UL))
#define ARTOS_SysTick_VAL (*(volatile uint32_t *)(ARTOS_SysTick + 0x08UL))

#define ARTOS_SCB (ARTOS_SCS + 0x0D00UL)
#define ARTOS_SCB_ICSR (*(volatile uint32_t *)(ARTOS_SCB + 0x04UL))
#define ARTOS_SCB_SHPR3 (*(volatile uint32_t *)(ARTOS_SCB + 0x20UL))
#define ARTOS_SCB_CFSR (*(volatile uint32_t *)(ARTOS_SCB + 0x28UL))
#define ARTOS_SCB_HFSR (*(volatile uint32_t *)(ARTOS_SCB + 0x2CUL))
#define ARTOS_SCB_MMFAR (*(volatile uint32_t *)(ARTOS_SCB + 0x34UL))
#define ARTOS_SCB_BFAR (*(volatile uint32_t *)(ARTOS_SCB + 0x38UL))
#define ARTOS_SCB_CPACR (*(volatile uint32_t *)(ARTOS_SCB + 0x88UL))

#define ARTOS_FPU (ARTOS_SCS + 0xF30UL)
#define ARTOS_FPU_FPCCR (*(volatile uint32_t *)(ARTOS_FPU + 0x04UL))

void __attribute__((noreturn)) rtos_port_halt(void) {
  __DSB();
  __ISB();

#if ARTOS_DEBUG_BREAK_ON_FAILURE
  __BKPT(0);
#endif

  __disable_irq();
  for (;;)
    __WFI();
}

static void rtos_port_task_return_trap(void) __attribute__((noreturn));

// Literally just a trap that do nothing
static void rtos_port_task_return_trap(void) {
  rtos_record_failure(ARTOS_FAILURE_TASK_RETURN, NULL, 0);
}

void rtos_port_idle_task(void *argument) {
  for (;;)
    __WFI();
}

uint8_t rtos_port_find_msb32(uint32_t bitmap) {
  if (bitmap == 0)
    return 0;
  return 31U - __CLZ(bitmap);
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

inline static void rtos_port_tick_init(void) {
  // Copied from SysTick_Config() from "core_cm4"
  // Check 4.5 programming manual also
  ARTOS_SysTick_LOAD = (uint32_t)(ARTOS_CPU_CLOCK_HZ / RTOS_TICK_HZ) -
                       1U;            /* set reload register */
  ARTOS_SCB_SHPR3 &= ~(0xFFU << 24U); // Check 4.4.8 programming manual
  ARTOS_SCB_SHPR3 |= 14U << 28;       // Check 4.4.8 programming manual
  ARTOS_SysTick_VAL = 0UL;            /* Load the SysTick Counter Value */
  ARTOS_SysTick_CTRL = 0b111UL;       /* Enable SysTick IRQ and SysTick Timer */
}

artos_stack_word_t *rtos_port_initialize_stack(artos_stack_word_t *stack_top,
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

void rtos_port_start_first_task(artos_stack_word_t *saved_stack_pointer) {
  // PSP initially have software-saved registers
  __set_PSP((uint32_t)(uintptr_t)saved_stack_pointer);
  rtos_port_tick_init();
  __enable_irq(); // Make SVC able to execute

  // NOTE: Theoretically a SysTick can land here
  //       Practically never happen (unless RTOS_TICK_HZ is much higher)

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
  ARTOS_SCB_SHPR3 &= ~(0xFFU << 16U); // Check 4.4.8 programming manual
  ARTOS_SCB_SHPR3 |= 15U << 20;       // Check 4.4.8 programming manual
  ARTOS_SCB_CPACR |= 0xFUL << 20;
  __DSB();
  __ISB();
  ARTOS_FPU_FPCCR |= (0b11UL << 30);
}

void rtos_port_request_context_switch(void) {
  ARTOS_SCB_ICSR = (1 << 28);
  __DSB();
  __ISB();
}

void rtos_port_request_context_switch_from_isr(void) {
  ARTOS_SCB_ICSR = (1 << 28);
  __DSB();
  __ISB();
}

void SysTick_Handler(void) { rtos_tick_handler(); }

struct rtos_fault_info {
  uint32_t r0, r1, r2, r3, r12, lr, pc, xpsr;
  uint32_t exc_return; // LR at fault entry
  uint32_t cfsr, hfsr, mmfar, bfar;
};

__attribute__((section(".noinit"))) volatile rtos_fault_info_t rtos_fault_info;

void rtos_port_hard_fault_c(const uint32_t *frame, uint32_t exc_return)
    __attribute__((noreturn));

void rtos_port_hard_fault_c(const uint32_t *frame, uint32_t exc_return) {
  if ((exc_return & (1UL << 4)) == 0U)
    frame += 18U;
  rtos_fault_info.r0 = frame[0];
  rtos_fault_info.r1 = frame[1];
  rtos_fault_info.r2 = frame[2];
  rtos_fault_info.r3 = frame[3];
  rtos_fault_info.r12 = frame[4];
  rtos_fault_info.lr = frame[5];
  rtos_fault_info.pc = frame[6];
  rtos_fault_info.xpsr = frame[7];
  rtos_fault_info.exc_return = exc_return;

  rtos_fault_info.cfsr = ARTOS_SCB_CFSR;
  rtos_fault_info.hfsr = ARTOS_SCB_HFSR;
  rtos_fault_info.mmfar = ARTOS_SCB_MMFAR;
  rtos_fault_info.bfar = ARTOS_SCB_BFAR;

  __DSB();
  rtos_record_failure(ARTOS_FAILURE_PORT_FAULT, NULL, 0);
}

void HardFault_Handler(void) __attribute__((naked));
void HardFault_Handler(void) {
  __asm volatile("tst lr, #0x4 \n" // EXC_RETURN bit 2: 0 = MSP, 1 = PSP
                 "ite eq \n"
                 "mrseq r0, msp \n"
                 "mrsne r0, psp \n"
                 "mov r1, lr \n" // pass EXC_RETURN
                 "b rtos_port_hard_fault_c \n");
}
