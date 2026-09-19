# aRTOS - Milestone Checklist

**Priority key:** `[C]` = Critical path, `[S]` = Stretch (nice to have)

## Week 1: Board Bring-Up
- [x] Establish the PlatformIO build, flash, debug, and serial workflow `[C]`
- [x] Bring up the onboard LED and UART output `[C]`
- [x] Print a boot message over serial `[C]`
- [x] Document the development commands `[C]`

## Week 2: Kernel Skeleton and Task Model
- [x] Establish the public API, configuration, kernel, and architecture-port structure `[C]`
- [x] Define the task model and task control block `[C]`
- [x] Implement static task storage and kernel initialization `[C]`
- [x] Implement validated static task creation with clear status codes `[C]`

## Week 3: Task Initialization and Startup
- [x] Implement Cortex-M task stack initialization and basic stack diagnostics `[C]`
- [x] Implement the first-task launch path using the process stack `[C]`
- [x] Verify a task starts and runs from its own stack `[C]`

## Week 4: Cooperative Scheduling
- [x] Implement kernel startup, yielding, and round-robin scheduling `[C]`
- [x] Implement the Cortex-M PendSV context-switch port `[C]`
- [x] Verify two cooperative tasks alternate correctly `[C]`

## Week 5: Preemption and Kernel Foundations
- [x] Implement SysTick-based timekeeping and preemptive scheduling `[C]`
- [x] Implement interrupt-safe critical sections for kernel state `[C]`
- [x] Add an idle task for periods with no runnable user tasks `[C]`
- [x] Add a basic HardFault handler with useful fault context `[C]`
- [x] Verify preemption, starvation prevention, and idle behavior without UART logging `[C]`

## Week 6: Blocking Delays
- [x] Implement tick- and millisecond-based blocking delays `[C]`
- [x] Integrate delayed-task wake-up with the scheduler and system tick `[C]`
- [x] Verify delayed tasks do not busy-wait and the idle task runs when appropriate `[C]`
- [x] Demonstrate concurrent LED and logger tasks using blocking delays `[C]`

## Week 7: Binary Semaphore and Button ISR
- [x] Implement binary semaphore initialization, wait, and signal operations `[C]`
- [x] Support task blocking and ISR-safe semaphore signaling `[C]`
- [x] Integrate the board button interrupt with a semaphore-driven task `[C]`
- [x] Verify button events over UART `[C]`

## Week 8: Message Queue and Showcase Demo
- [x] Implement a bounded message queue with blocking receive and full-queue handling `[C]`
- [x] Make queue operations safe under preemption and interrupt concurrency `[C]`
- [x] Build the heartbeat, button, and logger producer-consumer demo `[C]`
- [x] Verify the complete LED, heartbeat, button, logger, and load-task demo `[C]`

## Week 9: Debugging and Stability
- [ ] Add task inspection and diagnostic output `[S]`
- [ ] Add stack watermark and overflow detection `[S]`
- [ ] Add kernel assertions and improve fault reporting `[S]`
- [ ] Fix concurrency, context-switching, and timing defects found during testing `[C]`
- [ ] Run the complete demo for at least 10 minutes without failure `[C]`

## Week 10: Optional Priority Scheduling
- [ ] Add compile-time scheduling configuration (max priorities, preemption, time slicing), FreeRTOS-style `[S]`
- [ ] Extend task creation and the TCB with a priority field `[S]`
- [ ] Implement highest-priority-ready selection with round-robin among equal priorities `[S]`
- [ ] Add optional O(1) selection via ready mask + count-leading-zeros instead of a flat scan `[S]`
- [ ] Integrate priorities with blocking and synchronization wake-ups `[S]`
- [ ] Demonstrate high-, medium-, and low-priority task behavior `[S]`

## Week 11: Optional Mutex
- [ ] Implement mutex create, take, and give with ownership enforcement `[S]`
- [ ] Implement priority inheritance for tasks blocked on a mutex `[S]`
- [ ] Ensure mutex operations are preemption-safe and task-only `[S]`
- [ ] Demonstrate a mutex guarding a shared resource between tasks `[S]`

## Week 12: Polish, Final Demo, and Documentation
- [ ] Polish APIs, naming, and low-level documentation; verify the full workflow `[C]`
- [ ] Finalize the README with architecture, scheduling, and synchronization notes `[C]`
- [ ] Run the final reliability test and tag the release `[C]`

---

Everything tagged `[S]` (Weeks 9-11) can be deferred without compromising the core demonstration.
