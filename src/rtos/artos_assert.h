#ifndef ARTOS_INTERNAL_ASSERT_H
#define ARTOS_INTERNAL_ASSERT_H

#include "rtos_config.h"
#include <stdint.h>

void rtos_assert_failed(const char *file, uint32_t line)
    __attribute__((noreturn));

#if ARTOS_ENABLE_ASSERTS
#define ARTOS_ASSERT(condition)                                                \
  do {                                                                         \
    if (!(condition))                                                          \
      rtos_assert_failed(__FILE__, __LINE__);                                  \
  } while (0)
#else
#define ARTOS_ASSERT(condition) ((void)0)
#endif

#endif // !ARTOS_INTERNAL_ASSERT_H
