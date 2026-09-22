#ifndef RTOS_CONFIG_H
#define RTOS_CONFIG_H

#define RTOS_MAX_TASKS 16U
#define RTOS_TICK_HZ 1000U
#define RTOS_PRIORITY_COUNT 2U
#define ARTOS_ENABLE_ASSERTS 1
#define ARTOS_DEBUG_BREAK_ON_FAILURE 1
#define ARTOS_CPU_CLOCK_HZ 16000000UL

// Recommend to increase this to 128U if floating point is used
// MUST BE MULTIPLE OF 8
#define RTOS_MIN_STACK_WORDS 64U

#endif // !RTOS_CONFIG_H
