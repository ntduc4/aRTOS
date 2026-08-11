# STM32 RTOS Milestone Checklist

## Milestone Targets

- [ ] Week 4: cooperative task switching works with PendSV
- [ ] Week 5: preemptive scheduling works with SysTick
- [ ] Week 6: blocking delay works
- [ ] Week 7: semaphore and button ISR demo work
- [ ] Week 8: queue-based final demo works
- [ ] Week 9-12: debug, priority scheduling, docs, polish, and slack

## Week 0: Reading

- [ ] Review old course on linux kernel

## Week 1: Board Bring-Up

- [ ] Create PlatformIO project for the target STM32 Nucleo board
- [ ] Configure `platformio.ini`
- [ ] Build firmware successfully with `pio run`
- [ ] Flash firmware with ST-Link
- [ ] Blink onboard LED
- [ ] Configure UART output
- [ ] Print `boot ok` over serial
- [ ] Verify `pio device monitor` works
- [ ] Verify debugger connection with `pio debug`
- [ ] Document build, flash, monitor, and debug commands

## Week 2: Kernel Skeleton and Task Model

- [ ] Create RTOS folder structure
- [ ] Create public RTOS header
- [ ] Create `rtos_config.h`
- [ ] Define maximum task count
- [ ] Define default tick rate
- [ ] Define task function type
- [ ] Define task states
- [ ] Define task control block
- [ ] Add task stack pointer field to TCB
- [ ] Add task stack base field to TCB
- [ ] Add task stack size field to TCB
- [ ] Implement static task table
- [ ] Implement `rtos_init()`
- [ ] Implement basic `rtos_task_create()`
- [ ] Validate task count limit
- [ ] Validate stack pointer input
- [ ] Validate stack size input
- [ ] Return clear error codes from task creation

## Week 3: Task Stack Initialization and First Task Start

- [ ] Define Cortex-M exception stack frame layout
- [ ] Initialize task stack with xPSR
- [ ] Initialize task stack with PC
- [ ] Initialize task stack with LR
- [ ] Initialize task stack with R0 argument
- [ ] Initialize task stack with R1, R2, R3, and R12
- [ ] Reserve space for R4 to R11
- [ ] Store initial stack pointer in TCB
- [ ] Add task exit trap handler
- [ ] Fill task stack with debug pattern
- [ ] Verify 8-byte stack alignment
- [ ] Configure first task to run using PSP
- [ ] Keep exceptions running on MSP
- [ ] Implement initial task launch path
- [ ] Start first task manually
- [ ] Confirm first task runs from its own stack

## Week 4: PendSV Cooperative Context Switch

- [ ] Implement `rtos_start()`
- [ ] Implement `rtos_yield()`
- [ ] Trigger PendSV from `rtos_yield()`
- [ ] Set PendSV to lowest interrupt priority
- [ ] Implement round-robin next-task selection
- [ ] Implement `PendSV_Handler`
- [ ] Save R4 to R11 of current task
- [ ] Store current task stack pointer
- [ ] Select next ready task
- [ ] Restore next task stack pointer
- [ ] Restore R4 to R11 of next task
- [ ] Return from exception into next task
- [ ] Run two cooperative tasks
- [ ] Confirm tasks switch when calling `rtos_yield()`
- [ ] Confirm UART output alternates between two tasks
- [ ] Confirm each task uses its own stack

## Week 5: SysTick Preemption

- [ ] Configure SysTick timer
- [ ] Set RTOS tick rate to 1 kHz
- [ ] Implement global tick counter
- [ ] Implement `SysTick_Handler`
- [ ] Increment tick count in SysTick
- [ ] Trigger PendSV from SysTick
- [ ] Ensure PendSV has lower priority than SysTick
- [ ] Ensure SysTick does not perform heavy scheduling work
- [ ] Verify automatic task switching
- [ ] Run two tasks without manual yield
- [ ] Confirm a busy task does not starve other tasks
- [ ] Implement `rtos_get_tick()`
- [ ] Confirm tick counter increments correctly
- [ ] Confirm preemption still works with UART logging disabled

## Week 6: Blocking Delay and Idle Task

- [ ] Add blocked task state handling
- [ ] Add delay tick field to TCB
- [ ] Implement `rtos_delay_ticks()`
- [ ] Implement `rtos_delay_ms()`
- [ ] Move delayed task to blocked state
- [ ] Trigger scheduler after delay call
- [ ] Decrement task delays from SysTick
- [ ] Move expired blocked tasks back to ready state
- [ ] Add idle task
- [ ] Run idle task when no user task is ready
- [ ] Run LED task with `rtos_delay_ms(500)`
- [ ] Run logger task with `rtos_delay_ms(1000)`
- [ ] Confirm delayed tasks do not busy wait
- [ ] Confirm preemption works while some tasks are blocked
- [ ] Confirm system keeps running when all user tasks are delayed

## Week 7: Critical Sections and Binary Semaphore

- [ ] Implement `rtos_irq_save()`
- [ ] Implement `rtos_irq_restore()`
- [ ] Preserve previous interrupt state
- [ ] Protect task table updates
- [ ] Protect scheduler state updates
- [ ] Protect blocked task updates
- [ ] Define semaphore type
- [ ] Add semaphore count field
- [ ] Add semaphore waiting task storage
- [ ] Implement `rtos_sem_init()`
- [ ] Implement `rtos_sem_wait()`
- [ ] Block task when semaphore is unavailable
- [ ] Implement `rtos_sem_signal()`
- [ ] Wake one waiting task on signal
- [ ] Add ISR-safe semaphore signal path
- [ ] Configure button interrupt
- [ ] Signal semaphore from button interrupt
- [ ] Wake button task from semaphore
- [ ] Print button event over UART

## Week 8: Message Queue and Showcase Demo Baseline

- [ ] Define queue type
- [ ] Add queue buffer pointer
- [ ] Add item size field
- [ ] Add queue capacity field
- [ ] Add head index
- [ ] Add tail index
- [ ] Add item count
- [ ] Protect queue state updates
- [ ] Implement `rtos_queue_create()`
- [ ] Implement `rtos_queue_send()`
- [ ] Implement `rtos_queue_recv()`
- [ ] Block receiver when queue is empty
- [ ] Wake receiver when item is sent
- [ ] Return error when queue is full
- [ ] Create heartbeat event producer
- [ ] Create button event producer
- [ ] Create logger task
- [ ] Send heartbeat events through queue
- [ ] Send button events through queue
- [ ] Print received events over UART
- [ ] Run LED task
- [ ] Run heartbeat task
- [ ] Run button task
- [ ] Run logger task
- [ ] Run low-priority load task
- [ ] Confirm preemption works
- [ ] Confirm blocking delays work
- [ ] Confirm semaphore works
- [ ] Confirm queue works

## Week 9: Debugging and Stability Buffer

- [ ] Add task name field to TCB
- [ ] Implement `rtos_dump_tasks()`
- [ ] Print task name
- [ ] Print task state
- [ ] Print task stack pointer
- [ ] Print task delay ticks
- [ ] Add stack watermark checking
- [ ] Print task stack usage
- [ ] Add stack overflow guard check
- [ ] Implement RTOS assert handler
- [ ] Add basic HardFault handler
- [ ] Store fault PC
- [ ] Store fault LR
- [ ] Store fault PSR
- [ ] Store fault stack pointer
- [ ] Confirm task dump works over UART
- [ ] Fix context switching bugs found during testing
- [ ] Fix queue or semaphore bugs found during testing
- [ ] Confirm demo runs for at least 10 minutes without crashing

## Week 10: Optional Priority Scheduling

- [ ] Add priority field to TCB
- [ ] Update task creation API to accept priority
- [ ] Implement highest-priority-ready task selection
- [ ] Preserve round-robin behavior among equal-priority tasks
- [ ] Create high-priority button task
- [ ] Create medium-priority logger task
- [ ] Create low-priority load task
- [ ] Confirm high-priority task preempts low-priority task
- [ ] Confirm low-priority task runs when higher-priority tasks are blocked
- [ ] Confirm equal-priority tasks share CPU
- [ ] Confirm delay wake-up respects priority
- [ ] Confirm semaphore wake-up respects priority
- [ ] Document priority inversion as a limitation

## Week 11: Polish, Cleanup, and Extra Debug Slack

- [ ] Clean up public RTOS API
- [ ] Clean up internal headers
- [ ] Remove unused code
- [ ] Rename unclear functions
- [ ] Rename unclear variables
- [ ] Add comments only where low-level behavior is not obvious
- [ ] Verify project builds from clean checkout
- [ ] Verify firmware flashes successfully
- [ ] Verify serial monitor output
- [ ] Run final demo for at least 30 minutes
- [ ] Fix timing bugs
- [ ] Fix race condition bugs
- [ ] Fix HardFault issues
- [ ] Fix UART logging issues
- [ ] Fix stack size issues
- [ ] Record known limitations
- [ ] Record future work items

## Week 12: Final Demo and Documentation

- [ ] Finalize `README.md`
- [ ] Write build instructions
- [ ] Write flash instructions
- [ ] Write serial monitor instructions
- [ ] Write debug instructions
- [ ] Write architecture notes
- [ ] Write scheduler notes
- [ ] Write context switch notes
- [ ] Write synchronization notes
- [ ] Write limitations section
- [ ] Write future work section
- [ ] Add final demo description
- [ ] Add final demo UART output sample
- [ ] Record short demo video or GIF
- [ ] Confirm final firmware builds
- [ ] Confirm final firmware flashes
- [ ] Confirm final demo runs reliably
- [ ] Tag final version in Git
- [ ] Do final repository cleanup

## Final Completion Checklist

- [ ] Firmware builds with PlatformIO
- [ ] Firmware flashes to STM32 Nucleo
- [ ] UART logging works
- [ ] Multiple tasks run
- [ ] Tasks use separate stacks
- [ ] Tasks run on PSP
- [ ] Exceptions run on MSP
- [ ] PendSV context switch works
- [ ] SysTick preemption works
- [ ] Round-robin scheduling works
- [ ] Blocking delay works
- [ ] Critical sections work
- [ ] Binary semaphore works
- [ ] Message queue works
- [ ] Idle task works
- [ ] Task dump works
- [ ] Stack watermark checking works
- [ ] Final demo runs reliably

## Optional Completion Checklist

- [ ] Priority scheduling works
- [ ] HardFault capture works
- [ ] CPU idle estimate works
- [ ] Host-side scheduler tests work
- [ ] Host-side queue tests work
- [ ] Demo video or GIF is recorded
