//
// ktos.c @ ktOS
//
// Created by Kotorinyanya.
//

#include "ktos.h"

// Queue block.
static queue_block_t queues[MAX_QUEUE_SIZE];

// Task block.
static task_control_block_t tasks[MAX_TASKS_SIZE];
// Task var.
static uint8_t task_count = 0;
static uint8_t default_task = 0;  // the idle task
uint8_t current_task = 0;

// Memory pool.
uint8_t *pool[MEM_POOL_SIZE];
uint32_t pool_count = 0;

// OS var.
uint32_t systicks = 0;
static uint8_t is_os_started = 0;



// Yield is to relinquish control of the current task.
// In this case, PendSV_Handler will be called.
static inline void _Yield(void)
{
    SCB->ICSR = SCB_ICSR_PENDSVSET;
}

// Set the CPU to idle state
void _IdleTask(void)
{
    while(1) {
        // "wfe" (A.K.A. wait for event)
        __asm__ ("wfe");
    }
}

void _InitTicker(void)
{
    SysTick_Config(SystemCoreClock / SYSTICK_FREQUENCY_HZ);

    NVIC_SetPriorityGrouping(0);
    NVIC_SetPriority(SysTick_IRQn, NVIC_EncodePriority(0, 0, 0));
    NVIC_SetPriority(SVCall_IRQn, NVIC_EncodePriority(0, 1, 0));
    NVIC_SetPriority(PendSV_IRQn, NVIC_EncodePriority(0, 2, 0));
}

// The System Tick Time (SysTick) generates interrupt requests on a regular basis.
// This allows an OS to carry out context switching to support multiple tasking.
void SysTick_Handler(void)
{
    if(!is_os_started) return;
    systicks++;
    
    for(int i = 0; i < task_count; i++) {

        task_control_block_t * this_task = tasks + i;

        // Deal with sleeping tasks
        if(this_task->sleep_time > 0 && this_task->sleep_time != NO_TIMEOUT) {
            this_task->sleep_time -= SYSTICK_INTERVAL_MS;
        }
        if(this_task->sleep_time <= 0) {
            this_task->status = TASK_STATE_READY;
            this_task->sleep_time = 0;
            continue;
        }

        // Deal with tasks blocked by queue
        switch(this_task->status) {
            case TASK_STATE_WAIT_TO_SENT_QUEUE:
                if(GetEmptyQueue() != NULL) {
                    this_task->status = TASK_STATE_READY;
                    this_task->sleep_time = 0;
                }
                break;
            case TASK_STATE_WAIT_TO_RECEIVE_QUEUE:
                if(GetFilledQueue() != NULL) {
                    this_task->status = TASK_STATE_READY;
                    this_task->sleep_time = 0;
                }
                break;
        }

    }

    _Yield();
}


static uint32_t _ktSvcStartOs(void)
{
    if(is_os_started) {
        return OS_ALREADY_STARTED;
    }

    // Set all queues to be empty.
    InitQueue();

    _InitTicker();

    // Create idle task as the default task.
    uint8_t result = TaskCreate((TaskFunction)_IdleTask, 0, 512, 0xff);
    if(result != TASK_OK) {
        return OS_START_FAILED;
    }

    current_task = task_count - 1; //the idle task
    // Load idle task, return to thread mode, other tasks will be loaded upon context switch.
    uint32_t stack_top = tasks[current_task].stack_top;
    register int r1 asm("r1") = (int)&is_os_started;
    register int r2 asm("r2") = 1;
    __asm__ __volatile__ (
        R"(
        ldmia %0!, {r4-r11}
        msr psp, %0
        str %2, [%1]
        ldr pc, =0xFFFFFFFD
        )"
        :
        : "r" (stack_top), "r" (r1), "r" (r2)
    );

    // Should not be here
    is_os_started = 0;
    return OS_START_FAILED;
}

static uint32_t _ktSvcTaskKill(uint8_t task_id)
{
    EnterCritical();
    tasks[task_id].status = TASK_STATE_KILLED;
    LeaveCritical();
    _Yield();
    return 0;
}

static uint32_t _ktSvcTaskSleep(uint8_t task_id, uint32_t sleep_time)
{
    EnterCritical();
    task_control_block_t * this_task = tasks + task_id;
    if(this_task->status == TASK_STATE_DELAYED) {
        LeaveCritical();
        _Yield();
        return 0;
    }
    this_task->status = TASK_STATE_DELAYED;
    this_task->sleep_time = sleep_time;
    LeaveCritical();
    _Yield();
    return 0;
}

// Supervisor Calls
//   called by syscall
void _ktSvcHandler(hardware_stack_frame_t * hw_ctx)
{
    uint8_t service_no = *(uint8_t *)(hw_ctx->pc - 2);
    if(service_no != 0x80) {
        return;
    }

    switch(hw_ctx->r0) {
        case SYSCALL_START_OS:
            hw_ctx->r0 = _ktSvcStartOs();
            break;
        case SYSCALL_TASK_KILL:
            hw_ctx->r0 = _ktSvcTaskKill(hw_ctx->r1);
            break;
        case SYSCALL_TASK_SLEEP:
            hw_ctx->r0 = _ktSvcTaskSleep(hw_ctx->r1, hw_ctx->r2);
            break;
        default:
            hw_ctx->r0 = SYSCALL_UNDEFINED;
    }
}

__attribute__((naked)) void SVC_Handler(void)
{
    __asm__(
        R"(
        tst lr, #4
        ite eq
        mrseq r0, msp
        mrsne r0, psp

        push {lr}
        bl _ktSvcHandler
        pop {pc}
        )"
    );
}

// "syscall" wrapper
static inline uint32_t _Syscall(uint32_t r0, uint32_t r1, uint32_t r2, uint32_t r3)
{
    uint32_t result;
    __asm__ __volatile__ (
        R"(
        mov r0, %1
        mov r1, %2
        mov r2, %3
        mov r3, %4
        svc #0x80
        mov %0, r0
        )"
        : "=r" (result)
        : "r" (r0), "r" (r1), "r" (r2), "r" (r3)
        : "r0", "r1", "r2", "r3"
    );
    return result;
}

// Context switch methods
//   called by PendSV_Handler.
uint32_t _ContextSwitcher(uint32_t stack_top)
{
    task_control_block_t * this_task = tasks + current_task;
    task_control_block_t * next_task = tasks + current_task;

    // Save the stack pointer passed by r0
    this_task->stack_top = stack_top;
    
    if(this_task->status == TASK_STATE_RUNNING) {
        this_task->status = TASK_STATE_READY;
    }

    // Sreach for the task with highest priority
    uint8_t highest_priority = 0xff;
    uint8_t current_task_i = current_task;
    uint8_t i = task_count;

    // if task_count_i == 1, this loop won't be triggered.
    while(--i) {
        current_task_i++;
        if(current_task_i >= task_count) {
            current_task_i = 0;
        }
        
        this_task = tasks + current_task_i;

        // Switch to the task with highest priority,
        // if there were multiple tasks at the highest,
        // switch between the highest ones.
        if(this_task->status == TASK_STATE_READY
                && this_task->priority <= highest_priority) {
            next_task = this_task;
            current_task = current_task_i;
            highest_priority = next_task->priority;
        }
    }

    next_task->status = TASK_STATE_RUNNING;
    
    // Load the stack pointer back to r0
    return next_task->stack_top;

}

// PendSV will be called by Yield every SysTick.
//   this is to do the context switch
__attribute__((naked)) void PendSV_Handler(void)
{
    __asm__ __volatile__(
        R"(
        push {lr}
        mrs r0, psp
        stmdb r0!, {r4-r11}

        bl _ContextSwitcher

        ldmia r0!, {r4-r11}
        msr psp, r0
        pop {pc}
        )"
    );
}

uint32_t ktOSStart(void)
{
    return _Syscall(SYSCALL_START_OS, 0, 0, 0);
}

// Task methods
uint8_t TaskCreate(
        TaskFunction entry,
        void * arg,
        uint32_t stack_size,
        uint8_t priority
)
{
    EnterCritical();

    // Check if the stack is aligned to 8 bytes
    if(stack_size % 2 != 0) {
        LeaveCritical();
        return TASK_STACK_SIZE_NOT_ALIGNED;
    }

    // Check if reached max tasks amount
    if(task_count == MAX_TASKS_SIZE) {
        LeaveCritical();
        return TASK_AMOUNT_MAXIMUM_EXCEEDED;
    }

    // Allocate memory form the memory pool
    if(pool_count + stack_size > MEM_POOL_SIZE) {
        LeaveCritical();
        return MEM_POOL_MAXIMUM_EXCEEDED;
    }
    pool_count += stack_size;
    void * stack_bottom = pool + pool_count;

    // Init task control block
    task_control_block_t * this_task = tasks + task_count++;
    this_task->id = (uint8_t)(task_count - 1); //task id increases by numerical order
    this_task->priority = priority;
    this_task->status = TASK_STATE_READY;
    this_task->stack_size = stack_size;
    this_task->stack_bottom = (uint32_t)stack_bottom;
    this_task->stack_top = this_task->stack_bottom - (16 * sizeof(uint32_t));

    // Init stack frame
    hardware_stack_frame_t * hardware_stack_frame = (hardware_stack_frame_t*)(this_task->stack_bottom - (8 * sizeof(uint32_t)));
    software_stack_frame_t * software_stack_frame = (software_stack_frame_t*)this_task->stack_top;
    //memset(hardware_stack_frame, 0, sizeof(hardware_stack_frame));
    //memset(software_stack_frame, 0, sizeof(software_stack_frame));

    hardware_stack_frame->r0 = (uint32_t)arg;
    hardware_stack_frame->pc = (uint32_t)entry;
    hardware_stack_frame->lr = (uint32_t)TaskKill;
    hardware_stack_frame->psr = 0x21000000; //default PSR value

    LeaveCritical();
    return TASK_OK;

}

void TaskKill(void)
{
    _Syscall(SYSCALL_TASK_KILL, current_task, 0, 0);
}

void TaskSleep(uint32_t sleep_time)
{
    _Syscall(SYSCALL_TASK_SLEEP, current_task, sleep_time, 0);
}

// Queue methods
//  this is a FIFO(A.K.A. First In First Out) style queue
void InitQueue(void)
{
    for(int i = 0; i < MAX_QUEUE_SIZE; i++) {
        queue_block_t * this_queue = queues + i;
        this_queue->status = QUEUE_EMPTY;
    }
}

queue_block_t * GetEmptyQueue(void)
{
    // FIFO style
    for(int i = 0; i < MAX_QUEUE_SIZE; i++) {
        queue_block_t * this_queue = queues + i;
        if(this_queue->status == QUEUE_EMPTY) {
            return this_queue;
        }
    }
    return NULL;
}

queue_block_t * GetFilledQueue(void)
{
    // FIFO style
    for(int i = 0; i < MAX_QUEUE_SIZE; i++) {
        queue_block_t * this_queue = queues + i;
        if(this_queue->status == QUEUE_FILLED) {
            return this_queue;
        }
    }
    return NULL;
}

uint8_t QueueSendToBlock(uint32_t item, uint32_t timeout)
{
    EnterCritical();

    queue_block_t * this_queue = GetEmptyQueue();
    
    // Try to push item to the queue
    if(this_queue != NULL) {
        this_queue->item_ptr = item;
        this_queue->status = QUEUE_FILLED;
        LeaveCritical();
        return QUEUE_SENT_OK;
    }

    // If there is no queue empty, check timeout
    if(timeout == 0) {
        LeaveCritical();
        return QUEUE_SENT_FAILED;
    }

    // Set the task to wait until timeout
    task_control_block_t * this_task = tasks + current_task;
    this_task->status = TASK_STATE_WAIT_TO_SENT_QUEUE;
    this_task->sleep_time = timeout;
    LeaveCritical();
    _Yield();

    // The task is ready again, let's try to push item to the queue again
    EnterCritical();
    this_queue = GetEmptyQueue();
    if(this_queue != NULL) {
        this_queue->item_ptr = &item;
        this_queue->status = QUEUE_FILLED;
        LeaveCritical();
        return QUEUE_SENT_OK;
    }

    LeaveCritical();
    return QUEUE_SENT_FAILED;
}

uint8_t QueueReciveFromBlock(uint32_t *item_ptr, uint32_t timeout)
{
    EnterCritical();

    queue_block_t * this_queue = GetFilledQueue();

    // Try to pull item from the queue
    if(this_queue != NULL) {
        *item_ptr = this_queue->item_ptr;
        this_queue->status = QUEUE_EMPTY;
        LeaveCritical();
        return QUEUE_RECEIVE_OK;
    }

    // If there is no queue filled, check timeout
    if(!timeout) {
        LeaveCritical();
        return QUEUE_RECEIVE_FAILED;
    }

    // Set the task to wait until timeout
    task_control_block_t * this_task = tasks + current_task;
    this_task->status = TASK_STATE_WAIT_TO_RECEIVE_QUEUE;
    this_task->sleep_time = timeout;
    LeaveCritical();
    _Yield();

    // The task is ready again, let's try to pull item from the queue again
    EnterCritical();
    this_queue = GetFilledQueue();
    if(this_queue != NULL) {
        item_ptr = this_queue->item_ptr;
        this_queue->status = QUEUE_EMPTY;
        LeaveCritical();
        return QUEUE_RECEIVE_OK;
    }

    LeaveCritical();
    return QUEUE_RECEIVE_FAILED;
}

// Critical region methods
//   Setting PRIMASK to 1 raises the execution priority to 0.
//   This prevents any exceptions with configurable priority from
//   becoming active, except through the fault escalation mechanism
static uint8_t critical_depth = 0;

void EnterCritical(void)
{
    if(critical_depth++ == 0){
        __set_PRIMASK(1);
    }
}

void LeaveCritical(void)
{
    if(--critical_depth == 0) {
        __set_PRIMASK(0);
    }
}



