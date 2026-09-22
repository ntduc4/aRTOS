#include "rtos.h"
#include "rtos/list.h"
#include "rtos_diagnostics.h"

#include <stdint.h>
#include <unity.h>

enum {
  QUEUE_CAPACITY = 3U,
  TEST_STACK_WORDS = 128U,
};

static artos_stack_word_t test_stack[TEST_STACK_WORDS]
    __attribute__((aligned(8)));
static artos_stack_word_t misaligned_stack[TEST_STACK_WORDS + 1U]
    __attribute__((aligned(8)));
static artos_stack_word_t task_limit_stacks[ARTOS_MAX_TASKS]
                                           [ARTOS_MIN_STACK_WORDS]
    __attribute__((aligned(8)));

static void test_task(void *argument) {
  (void)argument;
  for (;;) {
  }
}

void setUp(void) { artos_init(); }

void tearDown(void) {}

static void test_list_initialization(void) {
  rtos_list_t list;
  rtos_init_list(&list);

  TEST_ASSERT_EQUAL_UINT32(0U, list.count);
  TEST_ASSERT_EQUAL_PTR(&list.sentinel, list.sentinel.next);
  TEST_ASSERT_EQUAL_PTR(&list.sentinel, list.sentinel.prev);
  TEST_ASSERT_EQUAL_PTR(&list, list.sentinel.container);
  TEST_ASSERT_NULL(list.sentinel.owner);
}

static void test_list_end_insertion_and_removal(void) {
  rtos_list_t list;
  rtos_list_item_t first;
  rtos_list_item_t second;
  uint32_t first_owner = 1U;
  uint32_t second_owner = 2U;

  rtos_init_list(&list);
  rtos_init_list_item(&first, &first_owner);
  rtos_init_list_item(&second, &second_owner);
  rtos_list_insert_end(&list, &first);
  rtos_list_insert_end(&list, &second);

  TEST_ASSERT_EQUAL_UINT32(2U, list.count);
  TEST_ASSERT_EQUAL_PTR(&first, list.sentinel.next);
  TEST_ASSERT_EQUAL_PTR(&second, first.next);
  TEST_ASSERT_EQUAL_PTR(&list.sentinel, second.next);

  rtos_list_remove(&first);
  TEST_ASSERT_EQUAL_UINT32(1U, list.count);
  TEST_ASSERT_EQUAL_PTR(&second, list.sentinel.next);
  TEST_ASSERT_NULL(first.container);
  TEST_ASSERT_NULL(first.next);
  TEST_ASSERT_NULL(first.prev);
}

static void test_list_sorted_insertion_is_stable(void) {
  rtos_list_t list;
  rtos_list_item_t one;
  rtos_list_item_t two_a;
  rtos_list_item_t two_b;
  uint32_t owners[3] = {1U, 2U, 3U};

  rtos_init_list(&list);
  rtos_init_list_item(&two_a, &owners[0]);
  rtos_init_list_item(&one, &owners[1]);
  rtos_init_list_item(&two_b, &owners[2]);
  two_a.value = 2U;
  one.value = 1U;
  two_b.value = 2U;

  rtos_list_insert_sorted(&list, &two_a);
  rtos_list_insert_sorted(&list, &one);
  rtos_list_insert_sorted(&list, &two_b);

  TEST_ASSERT_EQUAL_PTR(&one, list.sentinel.next);
  TEST_ASSERT_EQUAL_PTR(&two_a, one.next);
  TEST_ASSERT_EQUAL_PTR(&two_b, two_a.next);
}

static void test_list_reversed_sorted_insertion_is_stable(void) {
  rtos_list_t list;
  rtos_list_item_t one;
  rtos_list_item_t two_a;
  rtos_list_item_t two_b;
  uint32_t owners[3] = {1U, 2U, 3U};

  rtos_init_list(&list);
  rtos_init_list_item(&two_a, &owners[0]);
  rtos_init_list_item(&one, &owners[1]);
  rtos_init_list_item(&two_b, &owners[2]);
  two_a.value = 2U;
  one.value = 1U;
  two_b.value = 2U;

  rtos_list_insert_reversed_sorted(&list, &two_a);
  rtos_list_insert_reversed_sorted(&list, &one);
  rtos_list_insert_reversed_sorted(&list, &two_b);

  TEST_ASSERT_EQUAL_PTR(&two_a, list.sentinel.next);
  TEST_ASSERT_EQUAL_PTR(&two_b, two_a.next);
  TEST_ASSERT_EQUAL_PTR(&one, two_b.next);
}

static void test_queue_rejects_invalid_initialization(void) {
  artos_queue_control_storage_t control;
  uint8_t storage[4];

  TEST_ASSERT_NULL(artos_queue_init(NULL, storage, 1U, 1U));
  TEST_ASSERT_NULL(artos_queue_init(&control, NULL, 1U, 1U));
  TEST_ASSERT_NULL(artos_queue_init(&control, storage, 0U, 1U));
  TEST_ASSERT_NULL(artos_queue_init(&control, storage, 1U, 0U));
  TEST_ASSERT_NULL(artos_queue_init(&control, storage, SIZE_MAX, 2U));
}

static void test_queue_is_fifo_and_wraps(void) {
  artos_queue_control_storage_t control;
  uint32_t storage[QUEUE_CAPACITY];
  artos_queue_t *queue = artos_queue_init(&control, (uint8_t *)storage,
                                          sizeof(storage[0]), QUEUE_CAPACITY);
  uint32_t values[] = {11U, 22U, 33U, 44U};
  uint32_t output = 0U;

  TEST_ASSERT_NOT_NULL(queue);
  TEST_ASSERT_TRUE(artos_queue_enqueue(queue, (uint8_t *)&values[0], 0U));
  TEST_ASSERT_TRUE(artos_queue_enqueue(queue, (uint8_t *)&values[1], 0U));
  TEST_ASSERT_TRUE(artos_queue_enqueue(queue, (uint8_t *)&values[2], 0U));
  TEST_ASSERT_FALSE(artos_queue_enqueue(queue, (uint8_t *)&values[3], 0U));

  TEST_ASSERT_TRUE(artos_queue_dequeue(queue, (uint8_t *)&output, 0U));
  TEST_ASSERT_EQUAL_UINT32(values[0], output);
  TEST_ASSERT_TRUE(artos_queue_enqueue(queue, (uint8_t *)&values[3], 0U));

  for (uint32_t i = 1U; i < 4U; i++) {
    TEST_ASSERT_TRUE(artos_queue_dequeue(queue, (uint8_t *)&output, 0U));
    TEST_ASSERT_EQUAL_UINT32(values[i], output);
  }
  TEST_ASSERT_FALSE(artos_queue_dequeue(queue, (uint8_t *)&output, 0U));
}

static void test_queue_isr_operations_preserve_wake_flag_without_waiters(void) {
  artos_queue_control_storage_t control;
  uint32_t storage[1];
  artos_queue_t *queue =
      artos_queue_init(&control, (uint8_t *)storage, sizeof(storage[0]), 1U);
  uint32_t input = 42U;
  uint32_t output = 0U;
  bool task_woken = false;

  TEST_ASSERT_TRUE(
      artos_queue_enqueue_from_isr(queue, (uint8_t *)&input, &task_woken));
  TEST_ASSERT_FALSE(task_woken);
  TEST_ASSERT_FALSE(
      artos_queue_enqueue_from_isr(queue, (uint8_t *)&input, &task_woken));
  TEST_ASSERT_TRUE(
      artos_queue_dequeue_from_isr(queue, (uint8_t *)&output, &task_woken));
  TEST_ASSERT_EQUAL_UINT32(input, output);
  TEST_ASSERT_FALSE(task_woken);
  TEST_ASSERT_FALSE(
      artos_queue_dequeue_from_isr(queue, (uint8_t *)&output, &task_woken));
}

static void test_binary_semaphore_enforces_one_token(void) {
  artos_semaphore_storage_t storage;
  artos_semaphore_t *semaphore = artos_binary_semaphore_init(&storage, true);

  TEST_ASSERT_NOT_NULL(semaphore);
  TEST_ASSERT_TRUE(artos_semaphore_take(semaphore, 0U));
  TEST_ASSERT_FALSE(artos_semaphore_take(semaphore, 0U));
  TEST_ASSERT_TRUE(artos_semaphore_signal(semaphore));
  TEST_ASSERT_FALSE(artos_semaphore_signal(semaphore));
}

static void test_counting_semaphore_enforces_maximum(void) {
  artos_semaphore_storage_t storage;
  artos_semaphore_t *semaphore =
      artos_counting_semaphore_init(&storage, 3U, 2U);

  TEST_ASSERT_NOT_NULL(semaphore);
  TEST_ASSERT_TRUE(artos_semaphore_take_isr(semaphore));
  TEST_ASSERT_TRUE(artos_semaphore_take_isr(semaphore));
  TEST_ASSERT_FALSE(artos_semaphore_take_isr(semaphore));
  TEST_ASSERT_TRUE(artos_semaphore_signal_isr(semaphore, NULL));
  TEST_ASSERT_TRUE(artos_semaphore_signal_isr(semaphore, NULL));
  TEST_ASSERT_TRUE(artos_semaphore_signal_isr(semaphore, NULL));
  TEST_ASSERT_FALSE(artos_semaphore_signal_isr(semaphore, NULL));
}

static void test_semaphore_rejects_invalid_initialization(void) {
  artos_semaphore_storage_t storage;

  TEST_ASSERT_NULL(artos_binary_semaphore_init(NULL, false));
  TEST_ASSERT_NULL(artos_counting_semaphore_init(NULL, 1U, 0U));
  TEST_ASSERT_NULL(artos_counting_semaphore_init(&storage, 0U, 0U));
  TEST_ASSERT_NULL(artos_counting_semaphore_init(&storage, 1U, 2U));
}

static void test_mutex_requires_task_context(void) {
  artos_mutex_storage_t storage;
  artos_mutex_t *mutex = artos_mutex_init(&storage);

  TEST_ASSERT_NULL(artos_mutex_init(NULL));
  TEST_ASSERT_NOT_NULL(mutex);
  TEST_ASSERT_FALSE(artos_mutex_lock(mutex, 0U));
  TEST_ASSERT_FALSE(artos_mutex_unlock(mutex));
}

static void test_task_creation_validates_arguments(void) {
  TEST_ASSERT_EQUAL(
      ARTOS_ERROR_INVALID_ARGUMENT,
      artos_task_create(NULL, NULL, test_stack, TEST_STACK_WORDS, 0U));
  TEST_ASSERT_EQUAL(
      ARTOS_ERROR_INVALID_ARGUMENT,
      artos_task_create(test_task, NULL, NULL, TEST_STACK_WORDS, 0U));
  TEST_ASSERT_EQUAL(ARTOS_ERROR_INVALID_ARGUMENT,
                    artos_task_create(test_task, NULL, test_stack,
                                      TEST_STACK_WORDS, RTOS_PRIORITY_COUNT));
  TEST_ASSERT_EQUAL(ARTOS_ERROR_INVALID_ARGUMENT,
                    artos_task_create(test_task, NULL, misaligned_stack + 1U,
                                      TEST_STACK_WORDS, 0U));
  TEST_ASSERT_EQUAL(ARTOS_ERROR_STACK_TOO_SMALL,
                    artos_task_create(test_task, NULL, test_stack,
                                      RTOS_MIN_STACK_WORDS - 2U, 0U));
}

static void test_task_creation_and_inspection(void) {
  artos_task_info_t info;

  TEST_ASSERT_EQUAL(ARTOS_OK, artos_task_create(test_task, NULL, test_stack,
                                                TEST_STACK_WORDS, 1U));
  TEST_ASSERT_EQUAL_UINT32(1U, artos_task_count());
  TEST_ASSERT_TRUE(artos_task_get_info(0U, &info));
  TEST_ASSERT_EQUAL_PTR(test_task, info.entry);
  TEST_ASSERT_EQUAL_UINT8(1U, info.base_priority);
  TEST_ASSERT_EQUAL_UINT8(1U, info.effective_priority);
  TEST_ASSERT_EQUAL_UINT32(TEST_STACK_WORDS, info.stack_word_count);
  TEST_ASSERT_EQUAL(ARTOS_TASK_STATE_READY, info.state);
  TEST_ASSERT_FALSE(artos_task_get_info(1U, &info));
  TEST_ASSERT_FALSE(artos_task_get_info(0U, NULL));
}

static void test_task_pool_limit(void) {
  for (uint32_t i = 0U; i < RTOS_MAX_TASKS; i++) {
    TEST_ASSERT_EQUAL(ARTOS_OK,
                      artos_task_create(test_task, NULL, task_limit_stacks[i],
                                        RTOS_MIN_STACK_WORDS, 0U));
  }

  TEST_ASSERT_EQUAL(
      ARTOS_ERROR_TASK_LIMIT,
      artos_task_create(test_task, NULL, test_stack, TEST_STACK_WORDS, 0U));
}

static void test_start_rejects_empty_task_set(void) {
  TEST_ASSERT_EQUAL(ARTOS_ERROR_NO_TASKS, artos_start());
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_list_initialization);
  RUN_TEST(test_list_end_insertion_and_removal);
  RUN_TEST(test_list_sorted_insertion_is_stable);
  RUN_TEST(test_list_reversed_sorted_insertion_is_stable);
  RUN_TEST(test_queue_rejects_invalid_initialization);
  RUN_TEST(test_queue_is_fifo_and_wraps);
  RUN_TEST(test_queue_isr_operations_preserve_wake_flag_without_waiters);
  RUN_TEST(test_binary_semaphore_enforces_one_token);
  RUN_TEST(test_counting_semaphore_enforces_maximum);
  RUN_TEST(test_semaphore_rejects_invalid_initialization);
  RUN_TEST(test_mutex_requires_task_context);
  RUN_TEST(test_task_creation_validates_arguments);
  RUN_TEST(test_task_creation_and_inspection);
  RUN_TEST(test_task_pool_limit);
  RUN_TEST(test_start_rejects_empty_task_set);
  (void)UNITY_END();

  for (;;) {
  }
}
