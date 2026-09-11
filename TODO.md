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
- [ ] Implement Cortex-M task stack initialization and basic stack diagnostics `[C]`
- [ ] Implement the first-task launch path using the process stack `[C]`
- [ ] Verify a task starts and runs from its own stack `[C]`

## Week 4: Cooperative Scheduling
- [ ] Implement kernel startup, yielding, and round-robin scheduling `[C]`
- [ ] Implement the Cortex-M PendSV context-switch port `[C]`
- [ ] Verify two cooperative tasks alternate correctly `[C]`

## Week 5: Preemption and Kernel Foundations
- [ ] Implement SysTick-based timekeeping and preemptive scheduling `[C]`
- [ ] Implement interrupt-safe critical sections for kernel state `[C]`
- [ ] Add an idle task for periods with no runnable user tasks `[C]`
- [ ] Add a basic HardFault handler with useful fault context `[C]`
- [ ] Verify preemption, starvation prevention, and idle behavior without UART logging `[C]`

## Week 6: Blocking Delays
- [ ] Implement tick- and millisecond-based blocking delays `[C]`
- [ ] Integrate delayed-task wake-up with the scheduler and system tick `[C]`
- [ ] Verify delayed tasks do not busy-wait and the idle task runs when appropriate `[C]`
- [ ] Demonstrate concurrent LED and logger tasks using blocking delays `[C]`

## Week 7: Binary Semaphore and Button ISR
- [ ] Implement binary semaphore initialization, wait, and signal operations `[C]`
- [ ] Support task blocking and ISR-safe semaphore signaling `[C]`
- [ ] Integrate the board button interrupt with a semaphore-driven task `[C]`
- [ ] Verify button events over UART `[C]`

## Week 8: Message Queue and Showcase Demo
- [ ] Implement a bounded message queue with blocking receive and full-queue handling `[C]`
- [ ] Make queue operations safe under preemption and interrupt concurrency `[C]`
- [ ] Build the heartbeat, button, and logger producer-consumer demo `[C]`
- [ ] Verify the complete LED, heartbeat, button, logger, and load-task demo `[C]`

## Week 9: Debugging and Stability
- [ ] Add task inspection and diagnostic output `[S]`
- [ ] Add stack watermark and overflow detection `[S]`
- [ ] Add kernel assertions and improve fault reporting `[S]`
- [ ] Fix concurrency, context-switching, and timing defects found during testing `[C]`
- [ ] Run the complete demo for at least 10 minutes without failure `[C]`

## Week 10: Optional Priority Scheduling
- [ ] Implement priority scheduling with round-robin behavior among equal priorities `[S]`
- [ ] Integrate priorities with blocking and synchronization wake-ups `[S]`
- [ ] Demonstrate high-, medium-, and low-priority task behavior `[S]`
- [ ] Document priority inversion as a known limitation `[S]`

## Week 11: Polish and Cleanup
- [ ] Clean up public APIs, naming, structure, and low-level documentation `[C]`
- [ ] Verify a clean build, flash, debug, and serial workflow `[C]`
- [ ] Fix remaining stability and integration issues `[C]`
- [ ] Run the complete demo for at least 30 minutes without failure `[C]`
- [ ] Record known limitations and future work `[C]`

## Week 12: Final Demo and Documentation
- [ ] Finalize the README with workflows, architecture, scheduling, and synchronization notes `[C]`
- [ ] Document the demo, sample output, limitations, and future work `[C]`
- [ ] Record a short demo video or GIF `[C]`
- [ ] Complete final build and reliability verification `[C]`
- [ ] Tag the final version after repository cleanup `[C]`

---

## Critical Path Summary

If time runs short, land features in this order:

1. **Weeks 1-4:** Board bring-up, task model, task startup, and cooperative scheduling
2. **Week 5:** Preemption, critical sections, idle task, and fault handling
3. **Week 6:** Blocking delays
4. **Week 7:** Binary semaphore and button integration
5. **Week 8:** Message queue and integrated demo
6. **Weeks 11-12:** Stabilization, documentation, and final demo

Everything tagged `[S]` can be deferred without compromising the core demonstration.
