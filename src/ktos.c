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
static uint8_t current_task = (uint8_t)-1;
static uint8_t default_task = 0;  // the idle task

// Memory pool.
uint32_t *pool[MEM_POOL_SIZE];
uint32_t pool_count = 0;

// OS var.
static uint32_t systicks = 0;
static uint8_t is_os_started = 0;



// Yield is to relinquish control of the current task.
// In this case, PendSV_Handler will be called.
static void _Yield(void)
{
    SCB->ICSR = SCB_ICSR_PENDSVSET;
}

// Set the CPU to idle state
static void _IdleTask(void)
{
    while(1) {
        // "wfe" (A.K.A. wait for event)
        __asm__ __volatile("wfe");
    }
}


// The System Tick Time (SysTick) generates interrupt requests on a regular basis.
// This allows an OS to carry out context switching to support multiple tasking.
void SysTick_Handler(void)
{
    if(!is_os_started) return;
    systicks++;
    
    for(int i = 0; i <= task_count; i++) {

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
            case TASK_STATE_WAIT_TO_SENT_QUEUE: {
                if(GetEmptyQueue() != NULL) {
                    this_task->status = TASK_STATE_READY;
                    this_task->sleep_time = 0;
                }
                break;
            }
            case TASK_STATE_WAIT_TO_RECEIVE_QUEUE: {
                if(GetFilledQueue() != NULL) {
                    this_task->status = TASK_STATE_READY;
                    this_task->sleep_time = 0;
                }
                break;
            }
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

    // Create idle task as the default task.
    uint8_t idle_task = TaskCreate((TaskFunction) _IdleTask, (void *)0, 0x100, 0xff);

    // Load idle task, return to thread mode, other tasks will be loaded upon context switch.
    register int r0 asm("r0") = tasks[idle_task].stack_top;
    __asm__ __volatile__ (
        R"(
        ldmia %0!, {r4-r11}
        msr psp, %0
        ldr r0, =0xFFFFFFFD
        bx r0
        )"
        :
        : "r" (r0)
    );

    // Should not be here
    is_os_started = 0;
    return OS_START_FAILED;
}

static uint32_t _ktSvcTaskKill(uint8_t task_id)
{
    tasks[task_id].status = TASK_STATE_KILLED;
    _Yield(); //switch the context.
    while(1); //once context changed, the program will no longer return to this thread.
    return 0;
}

static uint32_t _ktSvcTaskSleep(uint8_t task_id, uint32_t timeout)
{
    tasks[task_id].status = TASK_STATE_DELAYED;
    tasks[task_id].sleep_time = timeout;
    _Yield();
    return 0;
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

// Context switch methods
//   called by PendSV_Handler.
uint32_t _ContextSwitcher(uint32_t stack_top)
{
    EnterCritical();

    task_control_block_t * this_task = tasks + current_task;
    task_control_block_t * next_task = tasks + default_task;

    // Save the stack pointer passed by r0
    this_task->stack_top = stack_top;

    // Sreach for the task with highest priority
    uint8_t highest_priority = 0xff;
    uint8_t i = task_count;
    while(--i) {
        current_task++;
        if(current_task > task_count) {
            current_task = 0;
        }
        this_task = tasks + current_task;

        // Switch to the task with highest priority,
        // if there were multiple tasks at the highest,
        // switch between the highest ones.
        if(this_task->status == TASK_STATE_READY
                && this_task->priority <= highest_priority) {
            next_task = this_task;
            highest_priority = this_task->priority;
        }
    }
    
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
    if(stack_size % 8 != 0) {
        LeaveCritical();
        return TASK_STACK_SIZE_NOT_ALIGNED;
    }

    // Check if reached max tasks amount
    if(task_count == MAX_TASKS_SIZE - 1) {
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

    //memset(this_task->hardware_stack_frame, 0, sizeof(hardware_stack_frame_t));
    //memset(this_task->software_stack_frame, 0, sizeof(software_stack_frame_t));

    this_task->hardware_stack_frame.r0 = (uint32_t)(uint32_t*)arg;
    this_task->hardware_stack_frame.pc = (uint32_t)(uint32_t*)entry;
    this_task->hardware_stack_frame.lr = (uint32_t)(uint32_t*)TaskKill;
    this_task->hardware_stack_frame.psr = 0x21000000; //default PSR value

    this_task->id = (uint8_t)(task_count - 1); //task id increases by numerical order
    this_task->stack_size = stack_size;
    this_task->stack_bottom = (uint32_t)stack_bottom;
    this_task->priority = priority;
    this_task->sleep_time = 0;
    this_task->status = TASK_STATE_READY;

    this_task->stack_top = this_task->stack_bottom - sizeof(software_stack_frame_t) - sizeof(hardware_stack_frame_t);

    LeaveCritical();
    return this_task->id;

}

void TaskKill(void)
{
    tasks[current_task].status = TASK_STATE_KILLED;
    _Yield(); //switch the context.
    while(1); //once context changed, the program will no longer return to this thread.
}

void TaskSleep(uint32_t sleep_time)
{
    tasks[current_task].status = TASK_STATE_DELAYED;
    tasks[current_task].sleep_time = sleep_time;
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
        this_queue->item_ptr = &item;
        this_queue->status = QUEUE_FILLED;
        LeaveCritical();
        return QUEUE_SENT_OK;
    }

    // If there is no queue empty, check timeout
    if(!timeout) {
    // '0' for no timeout
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
        item_ptr = this_queue->item_ptr;
        this_queue->status = QUEUE_EMPTY;
        LeaveCritical();
        return QUEUE_RECEIVE_OK;
    }

    // If there is no queue filled, check timeout
    if(!timeout) {
    // '0' for no timeout
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
    critical_depth++;
    __set_PRIMASK(1);
}

void LeaveCritical(void)
{
    critical_depth--;
    if(critical_depth == 0) {
        __set_PRIMASK(0);
    }
}



