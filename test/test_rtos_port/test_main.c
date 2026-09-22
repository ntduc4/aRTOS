#include "rtos/ports/rtos_port.h"
#include <stdint.h>
#include <unity.h>

enum {
  TEST_STACK_WORDS = 64U,
  INITIAL_FRAME_WORDS = 17U,
  SOFTWARE_FRAME_WORDS = 9U,
};

#define TEST_ARGUMENT_SENTINEL 0xDEADBEEFU

static artos_stack_word_t test_stack[TEST_STACK_WORDS]
    __attribute__((aligned(8)));

static void test_task(void *argument) { (void)argument; }

void setUp(void) {}

void tearDown(void) {}

static void test_initialize_stack_returns_expected_pointer(void) {
  artos_stack_word_t *const original_top = test_stack + TEST_STACK_WORDS;
  artos_stack_word_t *const saved_sp = rtos_port_initialize_stack(
      original_top, test_task, (void *)TEST_ARGUMENT_SENTINEL);

  TEST_ASSERT_EQUAL_PTR(original_top - INITIAL_FRAME_WORDS, saved_sp);
  TEST_ASSERT_EQUAL_UINT32(0U,
                           (uintptr_t)(saved_sp + SOFTWARE_FRAME_WORDS) & 7U);
}

static void test_initialize_stack_builds_correct_frame(void) {
  artos_stack_word_t *const original_top = test_stack + TEST_STACK_WORDS;
  artos_stack_word_t *const saved_sp = rtos_port_initialize_stack(
      original_top, test_task, (void *)TEST_ARGUMENT_SENTINEL);

  TEST_ASSERT_EQUAL_UINT32(1U << 24, saved_sp[16]); // xPSR
  TEST_ASSERT_EQUAL_UINT32((uintptr_t)test_task & ~1U,
                           saved_sp[15]);         // PC
  TEST_ASSERT_NOT_EQUAL_UINT32(0U, saved_sp[14]); // LR: return trap
  TEST_ASSERT_EQUAL_UINT32(0U, saved_sp[13]);     // R12
  TEST_ASSERT_EQUAL_UINT32(0U, saved_sp[12]);     // R3
  TEST_ASSERT_EQUAL_UINT32(0U, saved_sp[11]);     // R2
  TEST_ASSERT_EQUAL_UINT32(0U, saved_sp[10]);     // R1
  TEST_ASSERT_EQUAL_UINT32(TEST_ARGUMENT_SENTINEL,
                           saved_sp[9]);              // R0
  TEST_ASSERT_EQUAL_UINT32(0xFFFFFFFDU, saved_sp[8]); // EXC_RETURN
  TEST_ASSERT_EQUAL_UINT32(0U, saved_sp[7]);          // R11
  TEST_ASSERT_EQUAL_UINT32(0U, saved_sp[6]);          // R10
  TEST_ASSERT_EQUAL_UINT32(0U, saved_sp[5]);          // R9
  TEST_ASSERT_EQUAL_UINT32(0U, saved_sp[4]);          // R8
  TEST_ASSERT_EQUAL_UINT32(0U, saved_sp[3]);          // R7
  TEST_ASSERT_EQUAL_UINT32(0U, saved_sp[2]);          // R6
  TEST_ASSERT_EQUAL_UINT32(0U, saved_sp[1]);          // R5
  TEST_ASSERT_EQUAL_UINT32(0U, saved_sp[0]);          // R4
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_initialize_stack_returns_expected_pointer);
  RUN_TEST(test_initialize_stack_builds_correct_frame);
  (void)UNITY_END();

  for (;;) {
  }
}
