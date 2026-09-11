#include "rtos/ports/rtos_port.h"
#include <stdint.h>
#include <unity.h>

enum {
  TEST_STACK_WORDS = 64U,
  INITIAL_FRAME_WORDS = 17U,
  SOFTWARE_FRAME_WORDS = 9U,
};

static rtos_stack_word_t test_stack[TEST_STACK_WORDS]
    __attribute__((aligned(8)));

static void test_task(void *argument) { (void)argument; }

void setUp(void) {}

void tearDown(void) {}

static void test_initialize_stack_returns_expected_pointer(void) {
  rtos_stack_word_t *const original_top = test_stack + TEST_STACK_WORDS;
  rtos_stack_word_t *const saved_sp =
      rtos_port_initialize_stack(original_top, test_task, NULL);

  TEST_ASSERT_EQUAL_PTR(original_top - INITIAL_FRAME_WORDS, saved_sp);
  TEST_ASSERT_EQUAL_UINT32(
      0U, (uintptr_t)(saved_sp + SOFTWARE_FRAME_WORDS) & 7U);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_initialize_stack_returns_expected_pointer);
  (void)UNITY_END();

  for (;;) {
  }
}
