# aRTOS — Milestone Checklist

**Priority key:** `[C]` = Critical path (must land for demo), `[S]` = Stretch (nice to have)

## Week 1: Board Bring-Up
- [x] Create PlatformIO project for target STM32 Nucleo board
- [x] Configure `platformio.ini`
- [x] Build firmware successfully with `pio run`
- [x] Flash firmware with ST-Link
- [x] Verify `pio device monitor` works
- [x] Verify debugger connection with `pio debug`
- [x] Blink onboard LED `[C]`
- [ ] Configure UART output `[C]`
- [ ] Print `boot ok` over serial `[C]`
- [ ] Document build, flash, monitor, and debug commands `[C]`

## Week 2: Kernel Skeleton and Task Model
- [ ] Create RTOS folder structure `[C]`
- [ ] Create public RTOS header `[C]`
- [ ] Create `rtos_config.h` `[C]`
- [ ] Define maximum task count `[C]`
- [ ] Define default tick rate `[C]`
- [ ] Define task function type `[C]`
- [ ] Define task states `[C]`
- [ ] Define task control block `[C]`
- [ ] Add task stack pointer, base, and size fields to TCB `[C]`
- [ ] Implement static task table `[C]`
- [ ] Implement `rtos_init()` `[C]`
- [ ] Implement basic `rtos_task_create()` `[C]`
- [ ] Validate task count limit, stack pointer, stack size `[C]`
- [ ] Return clear error codes `[C]`

## Week 3: Stack Initialization and First Task Start
- [ ] Define Cortex-M exception stack frame layout `[C]`
- [ ] Initialize task stack: xPSR, PC, LR, R0–R3, R12 `[C]`
- [ ] Reserve space for R4–R11 `[C]`
- [ ] Store initial stack pointer in TCB `[C]`
- [ ] Add task exit trap handler `[C]`
- [ ] Fill task stack with debug pattern `[C]`
- [ ] Verify 8-byte stack alignment `[C]`
- [ ] Configure PSP for first task; keep MSP for exceptions `[C]`
- [ ] Implement initial task launch path `[C]`
- [ ] Confirm first task runs from its own stack `[C]`

## Week 4: PendSV Cooperative Context Switch
- [ ] Implement `rtos_start()` `[C]`
- [ ] Implement `rtos_yield()` → triggers PendSV `[C]`
- [ ] Set PendSV to lowest interrupt priority `[C]`
- [ ] Implement round-robin next-task selection `[C]`
- [ ] Implement `PendSV_Handler`: save R4–R11 of current task `[C]`
- [ ] Store current SP in TCB; restore next SP from TCB `[C]`
- [ ] Restore R4–R11 of next task; return into next task `[C]`
- [ ] Run two cooperative tasks; confirm alternation via `rtos_yield()` `[C]`

## Week 5: SysTick Preemption + Critical Sections + Idle + Fault Handler

### Preemption
- [ ] Configure SysTick timer at 1 kHz `[C]`
- [ ] Implement global tick counter `[C]`
- [ ] `SysTick_Handler`: increment tick, pend PendSV `[C]`
- [ ] Ensure PendSV has lower priority than SysTick `[C]`
- [ ] Verify automatic task switching without manual yield `[C]`
- [ ] Confirm a busy task does not starve others `[C]`

### Critical sections (needed here, not later)
- [ ] Implement `rtos_irq_save()` / `rtos_irq_restore()` `[C]`
- [ ] Preserve previous interrupt state `[C]`
- [ ] Protect task table updates `[C]`
- [ ] Protect scheduler state updates `[C]`

### Idle task (minimal)
- [ ] Add idle task with `while(1) { __WFI(); }` `[C]`
- [ ] Ensure idle task is selected when no other task is ready `[C]`

### Fault handling (basic, saves debugging time)
- [ ] Add basic `HardFault_Handler` capturing PC, LR, SP `[C]`
- [ ] Optionally print captured fault info or breakpoint loop `[C]`

### Verification
- [ ] Confirm preemption works with UART logging disabled `[C]`
- [ ] Confirm idle task runs `[C]`

## Week 6: Blocking Delay
- [ ] Add blocked state handling `[C]`
- [ ] Add delay tick field to TCB `[C]`
- [ ] Implement `rtos_delay_ticks()` and `rtos_delay_ms()` `[C]`
- [ ] Move delayed task to blocked state; trigger scheduler `[C]`
- [ ] Decrement task delays from SysTick; move expired tasks back to ready `[C]`
- [ ] Refine idle task (optional: measure idle cycles) `[C]`
- [ ] Run LED task (`delay_ms(500)`) + logger task (`delay_ms(1000)`) `[C]`
- [ ] Confirm delayed tasks do not busy-wait `[C]`
- [ ] Confirm system keeps running when all user tasks are blocked `[C]`

## Week 7: Binary Semaphore + Button ISR
- [ ] Define semaphore type: count + waiting task storage `[C]`
- [ ] Implement `rtos_sem_init()`, `rtos_sem_wait()`, `rtos_sem_signal()` `[C]`
- [ ] Block task when semaphore unavailable; wake one on signal `[C]`
- [ ] Add ISR-safe semaphore signal path `[C]`
- [ ] Configure button EXTI interrupt `[C]`
- [ ] Signal semaphore from button ISR; wake button task `[C]`
- [ ] Print button event over UART `[C]`

## Week 8: Message Queue + Showcase Demo
- [ ] Define queue type: buffer, item size, capacity, head, tail, count `[C]`
- [ ] Protect queue state with critical sections `[C]`
- [ ] Implement `rtos_queue_create()`, `rtos_queue_send()`, `rtos_queue_recv()` `[C]`
- [ ] Block receiver on empty queue; wake on send `[C]`
- [ ] Return error on full queue `[C]`
- [ ] Create heartbeat producer, button producer, logger consumer `[C]`
- [ ] Send heartbeat and button events through queue `[C]`
- [ ] Print received events over UART `[C]`
- [ ] Run final demo: LED + heartbeat + button + logger + low-pri load `[C]`
- [ ] Confirm preemption, blocking delays, semaphore, and queue all work `[C]`

## Week 9: Debugging and Stability
- [ ] Add task name field to TCB (for debugging) `[S]`
- [ ] Implement `rtos_dump_tasks()` printing name, state, SP, delay ticks `[S]`
- [ ] Add stack watermark and overflow guard checking `[S]`
- [ ] Implement RTOS assert handler `[S]`
- [ ] Refine `HardFault_Handler` with full register capture `[S]`
- [ ] Fix context switching, queue, or semaphore bugs found in testing `[C]`
- [ ] Confirm demo runs for ≥10 minutes without crashing `[C]`

## Week 10: Optional — Priority Scheduling
- [ ] Add priority field to TCB; update `rtos_task_create()` to accept priority `[S]`
- [ ] Implement highest-priority-ready task selection `[S]`
- [ ] Preserve round-robin among equal-priority tasks `[S]`
- [ ] Create high-pri button / med-pri logger / low-pri load tasks `[S]`
- [ ] Confirm high-pri preempts low-pri; low-pri runs when higher tasks block `[S]`
- [ ] Confirm delay and semaphore wake-up respect priority `[S]`
- [ ] Document priority inversion as a known limitation `[S]`

## Week 11: Polish and Cleanup
- [ ] Clean up API headers; remove unused code `[C]`
- [ ] Rename unclear functions/variables `[C]`
- [ ] Add comments where low-level behavior is non-obvious `[C]`
- [ ] Verify clean build, flash, and serial monitor `[C]`
- [ ] Run final demo for ≥30 minutes `[C]`
- [ ] Fix any remaining timing, race, HardFault, stack, or UART issues `[C]`
- [ ] Record known limitations and future work `[C]`

## Week 12: Final Demo and Documentation
- [ ] Finalize `README.md` with build/flash/monitor/debug instructions `[C]`
- [ ] Write architecture, scheduler, context switch, and sync notes `[C]`
- [ ] Write limitations and future work sections `[C]`
- [ ] Add final demo description and sample UART output `[C]`
- [ ] Record short demo video or GIF `[C]`
- [ ] Confirm final firmware builds, flashes, and runs reliably `[C]`
- [ ] Tag final version in Git; final repository cleanup `[C]`

---

## Critical path summary

If time runs short, land in this order:

1. **Weeks 1–4**: boot, UART, TCB, stack init, cooperative PendSV
2. **Week 5**: preemption + critical sections + idle + fault handler
3. **Week 6**: blocking delay
4. **Week 7**: semaphore + button
5. **Week 8**: queue + all-tasks demo
6. **Week 11–12**: polish + docs + demo video

Everything tagged `[S]` is safe to cut or defer. The demo is still a compelling resume piece without them.
