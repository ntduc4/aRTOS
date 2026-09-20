# aRTOS

**aRTOS is a completely static RTOS kernel built from scratch in C and a small
amount of port-specific assembly.** It demonstrates the mechanics behind task
scheduling, context switching, blocking, synchronization, and interrupt-safe
communication on real microcontrollers.

The repository currently provides one Cortex-M4F port, tailored to the
STM32F446RE and demonstrated on the Nucleo-F446RE board. Additional boards and
architectures can be supported by implementing the port interface and supplying
the required board startup and peripheral integration. Every task stack, task
control block, semaphore, queue control block, and queue buffer has fixed
storage. The kernel never calls `malloc`, `calloc`, or `free`, so memory usage
is explicit and bounded before the scheduler starts.

> aRTOS is an educational and portfolio project. It is not currently intended
> for safety-critical or production deployment.

## Highlights

- Completely static memory model with no heap allocation
- Preemptive fixed-priority scheduler with round-robin time slicing
- Cortex-M4F context switching through SVC and PendSV
- Caller-owned, 8-byte-aligned task stacks
- Intrusive ready, delayed, suspended, and event wait lists
- Relative and absolute blocking delays with tick-wrap handling
- Binary and counting semaphores with finite or infinite waits
- Fixed-capacity FIFO queues with caller-provided storage
- Nonblocking queue and semaphore operations for interrupt handlers
- Idle task using `WFI` when no user task is ready
- HardFault capture for registers, fault status, and current task context
- Integrated hardware showcase with GPIO, UART, EXTI, queues, and semaphores

## Static By Design

aRTOS makes allocation visible at the call site. There is no hidden heap and no
runtime allocation failure after initialization.

| Resource | Storage model |
| --- | --- |
| Task control blocks | Fixed kernel pool sized by `RTOS_MAX_TASKS` |
| Task stacks | Arrays supplied by the application |
| Semaphore state | `rtos_semaphore_storage_t` supplied by the application |
| Queue state | `rtos_queue_control_storage_t` supplied by the application |
| Queue items | Fixed application-owned byte buffer |
| Idle task | Fixed kernel-owned stack and control block |

This model makes the maximum memory footprint deterministic and keeps ownership
clear. Static objects must remain alive and must not be copied or moved while
the kernel uses them.

## Kernel Design

### Portability

The scheduler, task model, intrusive lists, queues, and semaphores are kept
separate from low-level context switching and interrupt control. An architecture
port provides task stack initialization, critical sections, scheduler startup,
context-switch requests, and exception handlers. Board code supplies clock and
peripheral setup for the application.

Only the Cortex-M4F port is implemented today. Its current reference target is
the STM32F446RE, using CMSIS, SysTick, SVC, PendSV, and the hardware floating
point context. Supporting another board requires adapting the architecture port
where necessary and adding that board's build and hardware initialization.

### Scheduling

The scheduler is preemptive and fixed-priority. Priority `0` is the lowest, and
larger values represent higher priorities. The scheduler always selects a ready
task at the highest priority. Tasks at the same priority execute round-robin in
FIFO order.

SysTick advances the kernel tick, wakes expired tasks, and requests scheduling
when an equal- or higher-priority task is ready. PendSV saves the current task
context, selects the next ready task, and restores its context. SVC performs the
transition into the highest-priority task at startup. When no user task can run,
the idle task executes `WFI` until an interrupt arrives.

Creating a task makes it ready but does not immediately request a context
switch. A priority must be in the range `0` through
`RTOS_PRIORITY_COUNT - 1`.

### Blocking And Time

Tasks can yield, sleep for a relative duration, or sleep until an absolute tick:

```c
rtos_yield();
rtos_wait(RTOS_MS_TO_TICKS(250));
rtos_wait_until(next_release_tick);
```

Delayed tasks are kept in sorted intrusive lists. A second delayed list handles
the transition across the natural 32-bit tick wrap. Finite absolute waits have
a valid future horizon of `1` through `0x7fffffff` ticks.

### Semaphores

Binary and counting semaphores share the same API. A take can be nonblocking,
bounded by a timeout, or infinite. A signal either stores a token or hands it
directly to the highest-priority waiting task. Equal-priority waiters are served
in FIFO order.

```c
static rtos_semaphore_storage_t lock_storage;
static rtos_semaphore_t *lock;

lock = rtos_binary_semaphore_init(&lock_storage, true);

if (rtos_semaphore_take(lock, RTOS_DELAY_INFINITY)) {
  use_shared_resource();
  rtos_semaphore_signal(lock);
}
```

Counting semaphores use an explicit maximum and initial count:

```c
static rtos_semaphore_storage_t slots_storage;
static rtos_semaphore_t *slots;

slots = rtos_counting_semaphore_init(&slots_storage, 8U, 8U);
```

### Message Queues

Queues are bounded FIFO channels with a fixed item size. Their control block and
item buffer are both supplied by the application.

```c
typedef struct {
  uint32_t id;
  uint32_t value;
} message_t;

enum { QUEUE_CAPACITY = 8 };

static rtos_queue_control_storage_t queue_control;
static message_t queue_items[QUEUE_CAPACITY];
static rtos_queue_t *queue;

queue = rtos_queue_init(&queue_control, (uint8_t *)queue_items,
                        sizeof(message_t), QUEUE_CAPACITY);
```

Task operations support zero, finite, and infinite timeouts. Readers and writers
waiting on a queue are ordered by priority, with FIFO ordering among tasks at
the same priority. ISR operations are always nonblocking and report whether an
higher-priority task was unblocked.

### Interrupt-Safe Wakeups

ISR wake flags accumulate across multiple kernel operations. Initialize the
flag once, pass it to each operation, finish hardware cleanup, then request a
context switch if necessary. An operation sets the flag only when it unblocks a
task whose priority is higher than the interrupted task. It never clears a flag
that is already true.

```c
void EXTI15_10_IRQHandler(void) {
  bool task_woken = false;

  clear_interrupt_source();
  rtos_queue_enqueue_from_isr(button_queue, (uint8_t *)&message,
                              &task_woken);
  rtos_semaphore_signal_isr(event_semaphore, &task_woken);

  if (task_woken)
    rtos_yield_from_isr();
}
```

The wake pointer is optional. Passing `NULL` performs the operation without
reporting whether scheduling should be requested. Equal- and lower-priority
waiters still become ready, but they do not set the wake flag.

## Showcase Application

`src/main.c` runs a hardware demonstration on the Nucleo-F446RE:

```text
SysTick -> preemptive fixed-priority scheduler
LED task -> timed blink pattern
Heartbeat task -> shared queue -> three competing logger tasks
PC13 button ISR -> shared queue -> logger tasks
                 -> button queue -> dedicated button logger
UART semaphore -> complete, non-interleaved log rows
Load task -> continuous CPU pressure with explicit yields
```

The demo exercises seven user tasks with different blocking patterns:

| Component | Behavior |
| --- | --- |
| LED task | Runs a periodic multi-pulse pattern using relative and absolute waits |
| Heartbeat producer | Publishes a message every two seconds |
| Logger C1 | Consumes shared messages, then waits 2.5 seconds |
| Logger C2 | Consumes shared messages, then waits 3.5 seconds |
| Logger C3 | Consumes shared messages, then waits 4.5 seconds |
| Button logger | Immediately consumes the dedicated button queue |
| Load task | Applies CPU pressure and yields explicitly |

All showcase tasks currently use `DEFAULT_PRIORITY`, so the application also
demonstrates FIFO round-robin scheduling among equal-priority tasks.

The blue user button on PC13 is handled by EXTI with a 50 ms software debounce.
Each accepted event is copied into both queues from the ISR. USART2 runs at
115200 baud and is protected by a binary semaphore so each row remains intact.

Example output:

```text
aRTOS showcase: 0.5 Hz heartbeat + button -> shared C1-C3 queue
Button events are also copied to the immediate BUTTON-ONLY queue.
TASK    EVENT           MSG     CREATED RECEIVED        DETAIL
C1      HEARTBEAT       #1      created=@0      received=@0     age=0
BUTTON-ONLY     BUTTON  #1      created=@1537   received=@1537  age=0
```

## Current Reference Target

- Nucleo-F446RE development board
- USB cable with access to the onboard ST-LINK interface
- Python and PlatformIO Core, or Visual Studio Code with PlatformIO
- A serial terminal capable of 115200 baud

## Build And Run

Clone the project and build the firmware:

```sh
pio run -e nucleo_f446re
```

Flash it through the onboard ST-LINK:

```sh
pio run -e nucleo_f446re -t upload
```

Open the UART output:

```sh
pio device monitor -b 115200
```

Start a debug session from the command line:

```sh
pio debug -e nucleo_f446re --interface=gdb -- -x .pioinit
```

## Minimal Application

All application memory is declared before startup:

```c
#include "rtos.h"

enum {
  TASK_STACK_WORDS = 128,
  WORKER_PRIORITY = 1
};

static rtos_stack_word_t worker_stack[TASK_STACK_WORDS]
    __attribute__((aligned(8)));

static void worker(void *argument) {
  (void)argument;

  for (;;) {
    do_work();
    rtos_wait(RTOS_MS_TO_TICKS(100));
  }
}

int main(void) {
  hardware_init();
  rtos_init();

  if (rtos_task_create(worker, NULL, worker_stack, TASK_STACK_WORDS,
                       WORKER_PRIORITY) != RTOS_OK)
    for (;;) {
    }

  rtos_start();

  for (;;) {
  }
}
```

Task functions must not return. Returning enters a kernel trap with interrupts
disabled.

## Configuration

Kernel configuration lives in `include/rtos_config.h`:

| Setting | Current value | Purpose |
| --- | ---: | --- |
| `RTOS_MAX_TASKS` | `16` | Maximum number of user tasks in the static TCB pool |
| `RTOS_TICK_HZ` | `1000` | SysTick frequency and kernel time base |
| `RTOS_PRIORITY_COUNT` | `2` | Number of task priority levels, from `0` through `RTOS_PRIORITY_COUNT - 1` |
| `RTOS_MIN_STACK_WORDS` | `64` | Minimum accepted task stack size in machine words |

Task stack tops must be 8-byte aligned. The current Cortex-M4F port saves the
floating-point high registers when an extended exception frame is active.

## Project Layout

```text
include/
  rtos.h                 Public kernel API and static storage types
  rtos_config.h          Compile-time kernel configuration
src/
  main.c                 Nucleo-F446RE showcase application
  rtos/
    tasks.c              Task lifecycle, scheduler, delays, and tick handling
    list.c               Intrusive list implementation
    queue.c              Static bounded message queues
    semaphore.c          Binary and counting semaphores
    rtos.c               Public kernel entry points
    ports/
      rtos_port.c        Cortex-M4F context switching and exception handlers
test/
  test_rtos_port/        Unity tests for port-level stack initialization
TODO.md                  Milestones and planned work
platformio.ini           PlatformIO build and test environments
```

## Current Scope

Implemented:

- Static task creation and startup
- Preemptive fixed-priority scheduling with round-robin among equal priorities
- Priority-ordered semaphore and queue waiters
- Relative, absolute, finite, and infinite blocking
- Binary and counting semaphores
- Bounded message queues
- Task and ISR synchronization paths
- Idle sleep and HardFault state capture

Not yet implemented:

- Configurable preemption and time-slicing policies
- Mutex ownership and priority inheritance
- Stack watermark and overflow detection
- Task inspection APIs and production-level diagnostics
- Additional architecture ports

See `TODO.md` for the complete milestone history and roadmap.

## Tests

The repository includes Unity-based target tests for Cortex-M stack-frame
initialization. Run the configured test environment with:

```sh
pio test -e nucleo_f446re_test
```

Queue, semaphore, scheduler, and long-duration hardware coverage are planned as
part of the stabilization work.
