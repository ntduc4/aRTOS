#include "rtos.h"
#include "rtos_diagnostics.h"

#include <stdbool.h>
#include <stdint.h>
#include <unity.h>

enum {
  TASK_STACK_WORDS = 128U,
  LOW_PRIORITY = 0U,
  HIGH_PRIORITY = 1U,
  TASK_COUNT = 11U,
  QUEUE_CAPACITY = 1U,
  LOW_OWNER_INDEX = 0U,
};

static artos_stack_word_t task_stacks[TASK_COUNT][TASK_STACK_WORDS]
    __attribute__((aligned(8)));

static artos_queue_control_storage_t queue_control;
static uint32_t queue_storage[QUEUE_CAPACITY];
static artos_queue_t *queue;

static artos_queue_control_storage_t timeout_queue_control;
static uint32_t timeout_queue_storage[QUEUE_CAPACITY];
static artos_queue_t *timeout_queue;

static artos_semaphore_storage_t semaphore_storage;
static artos_semaphore_t *semaphore;
static artos_semaphore_storage_t timeout_semaphore_storage;
static artos_semaphore_t *timeout_semaphore;

static artos_mutex_storage_t mutex_storage;
static artos_mutex_t *mutex;

static volatile bool priority_high_ran;
static volatile bool priority_low_observed_preemption;
static volatile bool queue_consumer_ran;
static volatile bool queue_producer_observed_handoff;
static volatile bool semaphore_consumer_ran;
static volatile bool semaphore_signaler_observed_handoff;
static volatile bool mutex_owner_inherited;
static volatile bool mutex_waiter_acquired;
static volatile bool mutex_owner_resumed;
static volatile bool queue_timeout_passed;
static volatile bool semaphore_timeout_passed;
static volatile uint32_t queue_received;
static volatile uint32_t work_sink;

void setUp(void) {}

void tearDown(void) {}

static void suspend_forever(void) {
  for (;;)
    artos_wait(ARTOS_DELAY_INFINITY);
}

static void priority_low_task(void *argument) {
  (void)argument;
  while (!priority_high_ran)
    work_sink++;
  priority_low_observed_preemption = true;
  suspend_forever();
}

static void priority_high_task(void *argument) {
  (void)argument;
  artos_wait_until(10U);
  priority_high_ran = true;
  suspend_forever();
}

static void queue_consumer_task(void *argument) {
  (void)argument;
  uint32_t value = 0U;
  artos_wait_until(50U);
  if (artos_queue_dequeue(queue, (uint8_t *)&value, 20U)) {
    queue_received = value;
    queue_consumer_ran = true;
  }
  suspend_forever();
}

static void queue_producer_task(void *argument) {
  (void)argument;
  uint32_t value = 0x12345678U;
  artos_wait_until(60U);
  if (artos_queue_enqueue(queue, (uint8_t *)&value, 0U))
    queue_producer_observed_handoff = queue_consumer_ran;
  suspend_forever();
}

static void semaphore_consumer_task(void *argument) {
  (void)argument;
  artos_wait_until(90U);
  if (artos_semaphore_take(semaphore, 20U))
    semaphore_consumer_ran = true;
  suspend_forever();
}

static void semaphore_signaler_task(void *argument) {
  (void)argument;
  artos_wait_until(100U);
  if (artos_semaphore_signal(semaphore))
    semaphore_signaler_observed_handoff = semaphore_consumer_ran;
  suspend_forever();
}

static void mutex_owner_task(void *argument) {
  (void)argument;
  if (!artos_mutex_lock(mutex, ARTOS_DELAY_INFINITY))
    suspend_forever();

  artos_wait_until(150U);
  artos_task_info_t info;
  if (artos_task_get_info(LOW_OWNER_INDEX, &info))
    mutex_owner_inherited = info.base_priority == LOW_PRIORITY &&
                            info.effective_priority == HIGH_PRIORITY;

  (void)artos_mutex_unlock(mutex);
  mutex_owner_resumed = true;
  suspend_forever();
}

static void mutex_interferer_task(void *argument) {
  (void)argument;
  artos_wait_until(120U);
  while (!mutex_waiter_acquired)
    work_sink++;
  suspend_forever();
}

static void mutex_waiter_task(void *argument) {
  (void)argument;
  artos_wait_until(120U);
  if (artos_mutex_lock(mutex, ARTOS_DELAY_INFINITY)) {
    mutex_waiter_acquired = true;
    (void)artos_mutex_unlock(mutex);
  }
  suspend_forever();
}

static void timeout_task(void *argument) {
  (void)argument;
  uint32_t value;
  artos_wait_until(180U);

  uint32_t start = artos_get_tick();
  bool received = artos_queue_dequeue(timeout_queue, (uint8_t *)&value, 5U);
  queue_timeout_passed = !received && artos_get_tick() - start >= 5U;

  start = artos_get_tick();
  bool taken = artos_semaphore_take(timeout_semaphore, 5U);
  semaphore_timeout_passed = !taken && artos_get_tick() - start >= 5U;
  suspend_forever();
}

static void test_runtime_results(void) {
  TEST_ASSERT_TRUE(priority_high_ran);
  TEST_ASSERT_TRUE(priority_low_observed_preemption);

  TEST_ASSERT_TRUE(queue_consumer_ran);
  TEST_ASSERT_TRUE(queue_producer_observed_handoff);
  TEST_ASSERT_EQUAL_HEX32(0x12345678U, queue_received);

  TEST_ASSERT_TRUE(semaphore_consumer_ran);
  TEST_ASSERT_TRUE(semaphore_signaler_observed_handoff);

  TEST_ASSERT_TRUE(mutex_owner_inherited);
  TEST_ASSERT_TRUE(mutex_waiter_acquired);
  TEST_ASSERT_TRUE(mutex_owner_resumed);

  TEST_ASSERT_TRUE(queue_timeout_passed);
  TEST_ASSERT_TRUE(semaphore_timeout_passed);
}

static void reporter_task(void *argument) {
  (void)argument;
  artos_wait_until(250U);
  RUN_TEST(test_runtime_results);
  (void)UNITY_END();
  suspend_forever();
}

static void create_task_or_halt(uint32_t stack_index, artos_task_fn_t entry,
                                uint8_t priority) {
  if (artos_task_create(entry, NULL, task_stacks[stack_index], TASK_STACK_WORDS,
                        priority) != ARTOS_OK) {
    for (;;) {
    }
  }
}

int main(void) {
  UNITY_BEGIN();
  artos_init();

  queue = artos_queue_init(&queue_control, (uint8_t *)queue_storage,
                           sizeof(queue_storage[0]), QUEUE_CAPACITY);
  timeout_queue =
      artos_queue_init(&timeout_queue_control, (uint8_t *)timeout_queue_storage,
                       sizeof(timeout_queue_storage[0]), QUEUE_CAPACITY);
  semaphore = artos_binary_semaphore_init(&semaphore_storage, false);
  timeout_semaphore =
      artos_binary_semaphore_init(&timeout_semaphore_storage, false);
  mutex = artos_mutex_init(&mutex_storage);

  if (queue == NULL || timeout_queue == NULL || semaphore == NULL ||
      timeout_semaphore == NULL || mutex == NULL) {
    TEST_FAIL_MESSAGE("Runtime test object initialization failed");
    (void)UNITY_END();
    for (;;) {
    }
  }

  create_task_or_halt(0U, mutex_owner_task, LOW_PRIORITY);
  create_task_or_halt(1U, queue_producer_task, LOW_PRIORITY);
  create_task_or_halt(2U, semaphore_signaler_task, LOW_PRIORITY);
  create_task_or_halt(3U, priority_low_task, LOW_PRIORITY);
  create_task_or_halt(4U, mutex_interferer_task, LOW_PRIORITY);

  create_task_or_halt(5U, priority_high_task, HIGH_PRIORITY);
  create_task_or_halt(6U, queue_consumer_task, HIGH_PRIORITY);
  create_task_or_halt(7U, semaphore_consumer_task, HIGH_PRIORITY);
  create_task_or_halt(8U, mutex_waiter_task, HIGH_PRIORITY);
  create_task_or_halt(9U, timeout_task, HIGH_PRIORITY);
  create_task_or_halt(10U, reporter_task, HIGH_PRIORITY);

  (void)artos_start();
  for (;;) {
  }
}
