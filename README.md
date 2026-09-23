# aRTOS

**aRTOS is a completely static RTOS kernel built from scratch in C and a small
amount of port-specific assembly.** It demonstrates the mechanics behind task
scheduling, context switching, blocking, synchronization, and interrupt-safe
communication on real microcontrollers.

The repository currently provides a device-header-independent Cortex-M4F port,
demonstrated on the STM32F446RE and Nucleo-F446RE board. Another Cortex-M4F
device can reuse the port by supplying compatible startup code, vector-table
entries, clock configuration, CMSIS compiler intrinsics, and build integration.
Another architecture requires a new implementation of the port interface.
Every task stack, task control block, semaphore, queue control block, and queue
buffer has fixed storage. The kernel never calls `malloc`, `calloc`, or `free`,
so memory usage is explicit and bounded before the scheduler starts.

## Highlights

- Completely static memory model with no heap allocation
- Preemptive fixed-priority scheduler with round-robin time slicing
- Cortex-M4F context switching through SVC and PendSV
- Caller-owned, 8-byte-aligned task stacks
- Intrusive ready, delayed, suspended, and event wait lists
- Relative and absolute blocking delays with tick-wrap handling
- Binary and counting semaphores with finite or infinite waits
- Nonrecursive mutexes with simplified priority inheritance
- Fixed-capacity FIFO queues with caller-provided storage
- Nonblocking queue and semaphore operations for interrupt handlers
- Idle task using `WFI` when no user task is ready
- HardFault capture for registers, fault status, and current task context
- Failure records for assertions, task returns, stack overflow, and port faults
- Selectable showcase, priority-scheduling, and mutex-inheritance demos

## Static By Design

aRTOS makes allocation visible at the call site. There is no hidden heap and no
runtime allocation failure after initialization.

| Resource | Storage model |
| --- | --- |
| Task control blocks | Fixed kernel pool sized by `ARTOS_MAX_TASKS` |
| Task stacks | Arrays supplied by the application |
| Semaphore state | `artos_semaphore_storage_t` supplied by the application |
| Mutex state | `artos_mutex_storage_t` supplied by the application |
| Queue state | `artos_queue_control_storage_t` supplied by the application |
| Queue items | Fixed application-owned byte buffer |
| Idle task | Fixed kernel-owned stack and control block |

This model makes the maximum memory footprint deterministic and keeps ownership
clear. Static objects must remain alive and must not be copied or moved while
the kernel uses them.

## Kernel Design

### Portability

The scheduler, task model, intrusive lists, queues, semaphores, and mutexes are
kept separate from low-level context switching and interrupt control. An
architecture port provides task stack initialization, critical sections,
scheduler startup, context-switch requests, and exception handlers. Board code
supplies clock and peripheral setup for the application.

Only the Cortex-M4F port is implemented today. It uses ARM-defined System
Control Space registers, CMSIS compiler intrinsics, SysTick, SVC, PendSV, and
hardware floating-point context handling. It does not include an STM32 device
header. The application configuration supplies the processor clock used by
SysTick, while startup code supplies the exception vectors and device setup.

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

Creating a task before scheduler startup makes it ready without requesting a
context switch. Creating one later from task context requests a context switch
if the new task has strictly higher priority than the calling task. Equal- and
lower-priority tasks become ready without an immediate switch. Task creation is
not supported from interrupt context. A priority must be in the range `0`
through `ARTOS_PRIORITY_COUNT - 1`.

### Blocking And Time

Tasks can yield, sleep for a relative duration, or sleep until an absolute tick:

```c
artos_yield();
artos_wait(ARTOS_MS_TO_TICKS(250));
artos_wait_until(next_release_tick);
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
static artos_semaphore_storage_t lock_storage;
static artos_semaphore_t *lock;

lock = artos_binary_semaphore_init(&lock_storage, true);

if (artos_semaphore_take(lock, ARTOS_DELAY_INFINITY)) {
  use_shared_resource();
  artos_semaphore_signal(lock);
}
```

Counting semaphores use an explicit maximum and initial count:

```c
static artos_semaphore_storage_t slots_storage;
static artos_semaphore_t *slots;

slots = artos_counting_semaphore_init(&slots_storage, 8U, 8U);
```

### Mutexes

Mutexes provide task ownership and simplified FreeRTOS-style priority
inheritance. They are nonrecursive and task-context-only. A lock can be
nonblocking, bounded by a timeout, or infinite. Only the owning task can unlock
the mutex.

```c
static artos_mutex_storage_t mutex_storage;
static artos_mutex_t *mutex;

mutex = artos_mutex_init(&mutex_storage);

if (artos_mutex_lock(mutex, ARTOS_DELAY_INFINITY)) {
  use_shared_resource();
  artos_mutex_unlock(mutex);
}
```

Waiters are ordered by effective priority and FIFO among equal priorities.
Unlock transfers ownership directly to the selected waiter. A lower-priority
owner inherits the highest waiter's effective priority, and releasing its final
held mutex restores its base priority. If a waiter times out, priority is
recalculated only when the owner holds exactly one mutex; otherwise the inherited
priority is retained conservatively.

Inheritance is intentionally simplified. A priority increase is applied to the
direct mutex owner but is not propagated transitively through an existing chain
of blocked mutex owners. Mutex operations are not supported from interrupt
context; use a semaphore, queue, or another ISR-safe mechanism instead.

### Message Queues

Queues are bounded FIFO channels with a fixed item size. Their control block and
item buffer are both supplied by the application.

```c
typedef struct {
  uint32_t id;
  uint32_t value;
} message_t;

enum { QUEUE_CAPACITY = 8 };

static artos_queue_control_storage_t queue_control;
static message_t queue_items[QUEUE_CAPACITY];
static artos_queue_t *queue;

queue = artos_queue_init(&queue_control, (uint8_t *)queue_items,
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
  artos_queue_enqueue_from_isr(button_queue, (uint8_t *)&message,
                               &task_woken);
  artos_semaphore_signal_isr(event_semaphore, &task_woken);

  if (task_woken)
    artos_yield_from_isr();
}
```

The wake pointer is optional. Passing `NULL` performs the operation without
reporting whether scheduling should be requested. Equal- and lower-priority
waiters still become ready, but they do not set the wake flag.

## Demo Applications

Three selectable demos run on both supported boards:

| Environment | Source | Demonstrates |
| --- | --- | --- |
| `f446re` | `src/demos/showcase/main.c` | Tasks, waits, queues, semaphores, ISR wakeups, and equal-priority scheduling |
| `f446re_priority` | `src/demos/priority/main.c` | Round-robin workers and strict higher-priority preemption |
| `f446re_mutex` | `src/demos/mutex/main.c` | Mutex ownership, direct handoff, and priority inheritance |
| `l4s5i` | `src/demos/showcase/main.c` | Full showcase on the B-L4S5I-IOT01A |
| `l4s5i_priority` | `src/demos/priority/main.c` | Priority scheduling on the B-L4S5I-IOT01A |
| `l4s5i_mutex` | `src/demos/mutex/main.c` | Mutex inheritance on the B-L4S5I-IOT01A |

The showcase demo runs this hardware demonstration:

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

The STM32L4S5I board implementation switches the B-L4S5I-IOT01A from its reset
MSI clock to the 16 MHz HSI clock before starting the kernel, matching
`ARTOS_CPU_CLOCK_HZ`. It uses the PA5 user LED and USART1 TX on PB6 through the
ST-LINK virtual COM port at 115200 baud.

## Current Reference Target

- Nucleo-F446RE development board
- B-L4S5I-IOT01A Discovery kit for the STM32L4S5I environments
- USB cable with access to the onboard ST-LINK interface
- Python and PlatformIO Core, or Visual Studio Code with PlatformIO
- A serial terminal capable of 115200 baud

## Build And Run

Clone the project and build the firmware:

```sh
pio run -e f446re
```

Select a focused demo by changing the environment:

```sh
pio run -e f446re_priority
pio run -e f446re_mutex
pio run -e l4s5i
pio run -e l4s5i_priority
pio run -e l4s5i_mutex
```

Flash a selected environment through the onboard ST-LINK:

```sh
pio run -e f446re -t upload
pio run -e f446re_priority -t upload
pio run -e f446re_mutex -t upload
pio run -e l4s5i -t upload
pio run -e l4s5i_priority -t upload
pio run -e l4s5i_mutex -t upload
```

Open the UART output:

```sh
pio device monitor -b 115200
```

Start a debug session from the command line:

```sh
pio debug -e f446re --interface=gdb -- -x .pioinit
```

## API Documentation

The root `Doxyfile` generates HTML documentation for the public headers and uses
this README as its landing page:

```sh
doxygen Doxyfile
```

Open `build/doxygen/html/index.html` after generation. Doxygen is a documentation
tool dependency and is not installed automatically by PlatformIO.

## Minimal Application

All application memory is declared before startup:

```c
#include "rtos.h"

enum {
  TASK_STACK_WORDS = 128,
  WORKER_PRIORITY = 1
};

static artos_stack_word_t worker_stack[TASK_STACK_WORDS]
    __attribute__((aligned(8)));

static void worker(void *argument) {
  (void)argument;

  for (;;) {
    do_work();
    artos_wait(ARTOS_MS_TO_TICKS(100));
  }
}

int main(void) {
  hardware_init();
  artos_init();

  if (artos_task_create(worker, NULL, worker_stack, TASK_STACK_WORDS,
                        WORKER_PRIORITY) != ARTOS_OK)
    for (;;) {
    }

  artos_start();

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
| `ARTOS_MAX_TASKS` | `16` | Maximum number of user tasks in the static TCB pool |
| `ARTOS_TICK_HZ` | `1000` | SysTick frequency and kernel time base |
| `ARTOS_PRIORITY_COUNT` | `2` | Number of task priority levels, from `0` through `ARTOS_PRIORITY_COUNT - 1` |
| `ARTOS_MIN_STACK_WORDS` | `64` | Minimum accepted task stack size in machine words |
| `ARTOS_CPU_CLOCK_HZ` | `16000000` | Processor clock used to calculate the SysTick reload value |
| `ARTOS_ENABLE_ASSERTS` | `1` | Enables internal kernel invariant checks |
| `ARTOS_DEBUG_BREAK_ON_FAILURE` | `1` | Executes a debugger breakpoint before halting on failure |

Task stack tops must be 8-byte aligned. The current Cortex-M4F port saves the
floating-point high registers when an extended exception frame is active.
`ARTOS_CPU_CLOCK_HZ` must match the processor clock when `artos_start()` is
called. The configured clock divided by `ARTOS_TICK_HZ` must fit the SysTick
24-bit reload register.

## Project Layout

```text
include/
  rtos.h                 Public kernel API and static storage types
  rtos_diagnostics.h     Public task inspection API
  rtos_config.h          Compile-time kernel configuration
src/
  boards/
    board.h              Board interface used by demo applications
    f446re/board.c       Nucleo-F446RE LED and USART implementation
    l4s5i/board.c        B-L4S5I-IOT01A clock, LED, and USART implementation
  demos/
    showcase/main.c      Full queue, semaphore, ISR, and timing demo
    priority/main.c      Fixed-priority scheduling demo
    mutex/main.c         Priority-inheritance mutex demo
  rtos/
    tasks.c              Task lifecycle, scheduler, delays, and tick handling
    list.c               Intrusive list implementation
    queue.c              Static bounded message queues
    semaphore.c          Binary and counting semaphores
    mutex.c              Nonrecursive mutexes and priority inheritance
    rtos.c               Public kernel entry points
    ports/
      cortex-m4f/
        rtos_port.c      Cortex-M4F context switching and exception handlers
test/
  test_kernel/           Deterministic list and nonblocking kernel API tests
  test_kernel_runtime/   Live scheduling and synchronization tests
  test_rtos_port/        Unity tests for port-level stack initialization
Doxyfile                 Public API documentation configuration
TODO.md                  Milestones and planned work
platformio.ini           PlatformIO build and test environments
```

## Current Scope

Implemented:

- Static task creation and startup
- Preemptive fixed-priority scheduling with round-robin among equal priorities
- Priority-ordered semaphore and queue waiters
- Nonrecursive mutexes with direct handoff and simplified priority inheritance
- Relative, absolute, finite, and infinite blocking
- Binary and counting semaphores
- Bounded message queues
- Task and ISR synchronization paths
- Task inspection and stack-bound diagnostics
- Assertions, retained failure information, and HardFault state capture
- Idle sleep through `WFI`

Not yet implemented:

- Configurable preemption and time-slicing policies
- Recursive mutexes and transitive priority-inheritance propagation
- True historical stack-watermark reporting
- Retained-record validity markers and production-level diagnostics
- Additional architecture ports

See `TODO.md` for the complete milestone history and roadmap.

## Tests

The repository includes Unity-based target tests for:

- Intrusive-list ordering, stability, removal, and link maintenance
- Queue initialization, FIFO wraparound, capacity, and ISR operations
- Binary and counting semaphore bounds and ISR operations
- Mutex initialization and task-context enforcement
- Task argument validation, creation, inspection, and pool limits
- Cortex-M initial stack-frame construction
- On-target priority preemption, blocking wakeups, direct handoff, timeouts, and
  mutex inheritance

Build and run the configured test environment with:

```sh
pio test -e nucleo_f446re_test
```

Build the test firmware without uploading or running it with:

```sh
pio test -e nucleo_f446re_test --without-uploading --without-testing
```

The `test_kernel_runtime` suite exercises live context switching and therefore
must run on the target rather than only being build-checked.

## License

aRTOS is licensed under the [MIT License](LICENSE).
