#ifndef RTOS_CONFIG_H
#define RTOS_CONFIG_H

#define RTOS_MAX_TASKS 4U
#define RTOS_TICK_HZ 1000U

// Recommend to increase this to 128U if floating point is used
// MUST BE MULTIPLE OF 8
#define RTOS_MIN_STACK_WORDS 64U

#endif // !RTOS_CONFIG_H
