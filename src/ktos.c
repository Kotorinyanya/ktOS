//
// ktos.c @ ktOS
//
// Created by Kotorinyanya.
//

#include "ktos.h"
#include "../CMSIS/CM3/CoreSupport/core_cm3.h"

// Queue control block.
static queue_control_block_t queue_control_blocks[MAX_QUEUE_CONTROL_BLOCK_COUNT];
// Task control block.
static task_control_block_t task_control_blocks[MAX_TASKS_COUNT];
// Task var.
static uint8_t task_count = 0;
static uint8_t current_task = 0;
// OS var.
static uint32_t systicks = 0;
static uint8_t is_os_started = 0;


// Queue methods
void InitQueueControlBlock(void) {
    queue_control_block_t *this_qcb;
    for (int i = 0; i < MAX_QUEUE_CONTROL_BLOCK_COUNT; i++) {
        this_qcb = queue_control_blocks + i;
        this_qcb->id = QUEUE_CONTROL_BLOCK_NOT_BEING_USED; //set all queue block id to 255 at initial state.
        for (int j = 0; j < QUEUE_SIZE; j++) {
            this_qcb->queues[j].status = QUEUE_EMPTY;
            this_qcb->queues[j].item_ptr = NULL;
        }
    }
}


static queue_t *GetEmptyQueueBlock(uint8_t queue_id) {
    queue_control_block_t *this_qcb;
    queue_t *this_queue;
    
    //Try to find the queue with the specified id.
    for (int i = 0; i < MAX_QUEUE_CONTROL_BLOCK_COUNT; i++) {
        this_qcb = queue_control_blocks + i;
        if (this_qcb->id == queue_id) {
            for (int j = 0; j < QUEUE_SIZE; j++) {
                this_queue = this_qcb->queues + j;
                if (this_queue->status == QUEUE_EMPTY) { //check if the queue has empty blocks
                    return this_queue;
                }
            }
            return NULL;
        }
    }
    //Try to create a new queue with the specified id.
    for (int i = 0; i < MAX_QUEUE_CONTROL_BLOCK_COUNT; i++) {
        this_qcb = queue_control_blocks + i;
        if (this_qcb->id == QUEUE_CONTROL_BLOCK_NOT_BEING_USED) {
            this_qcb->id = queue_id;
            for (int j = 0; j < QUEUE_SIZE; j++) {
                this_queue = this_qcb->queues + j;
                if (this_queue->status == QUEUE_EMPTY) { //check if the queue has empty blocks
                    return this_queue;
                }
            }
            return NULL;
        }
    }
    
    return NULL;
}

static queue_t *GetFilledQueueBlock(uint8_t queue_id) {
    queue_control_block_t *this_qcb;
    
    //Try to find the queue with the specified id.
    for (int i = 0; i < MAX_QUEUE_CONTROL_BLOCK_COUNT; i++) {
        this_qcb = queue_control_blocks + i;
        if (this_qcb->id == queue_id) {
            queue_t *this_queue;
            for (int j = 0; j < QUEUE_SIZE; j++) {
                this_queue = this_qcb->queues + j;
                if (this_queue->status == QUEUE_FILLED) {
                    return this_queue;
                }
            }
            return NULL;
        }
    }
    return NULL;
}


// Yield is to relinquish control of the current task.
// In this case, PendSV_Handler will be called.
static inline void Yield(void) {
    SCB->ICSR = SCB_ICSR_PENDSVSET;
}

// Set the CPU to idle state
void _IdleTask(void) {
    while (1) {
        // "wfe" (A.K.A. wait for event)
        __asm__ ("wfe");
    }
}

static void InitTicker(void) {
    SysTick_Config(SystemCoreClock / SYSTICK_FREQUENCY_HZ);
    
    NVIC_SetPriorityGrouping(0);
    NVIC_SetPriority(SysTick_IRQn, NVIC_EncodePriority(0, 0, 0));
    NVIC_SetPriority(SVCall_IRQn, NVIC_EncodePriority(0, 1, 0));
    NVIC_SetPriority(PendSV_IRQn, NVIC_EncodePriority(0, 2, 0));
}


static int _ktSvcStartOs(void) {
    if (is_os_started) {
        return OS_ALREADY_STARTED;
    }
    
    InitTicker();
    
    // Create idle task as the default task.
    int result = TaskCreate((TaskFunction) _IdleTask, 0, 512, 0xff, "Idle");
    if (result != TASK_OK) {
        return OS_START_FAILED;
    }
    
    // current_task = (uint8_t) (task_count - 1); //the idle task
    // Load idle task, return to thread mode, other tasks will be loaded upon context switch.
    current_task = task_count - 1;//the idle task.
    task_control_block_t *this_tcb = task_control_blocks + current_task;
    uint32_t stack_top = (uint32_t) this_tcb->stack_top;
    register int r1 asm("r1") = (int) &is_os_started;
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

static int _ktSvcTaskKill(uint8_t task_pid) {
    EnterCritical();
    task_control_block_t *this_task = task_control_blocks + task_pid;
    
    this_task->status = TASK_STATE_KILLED;
    FreeMemBlock(this_task->mem_block);
    
    LeaveCritical();
    Yield();
    return 0;
}

static int _ktSvcTaskSleep(uint8_t task_id, uint32_t sleep_time) {
    EnterCritical();
    task_control_block_t *this_task = task_control_blocks + task_id;
    this_task->status = TASK_STATE_DELAYED;
    this_task->sleep_time = sleep_time;
    LeaveCritical();
    Yield();
    return 0;
}

static int _ktSvcSendToQueue(uint8_t queue_id, uint32_t item, uint32_t timeout) {
    EnterCritical();
    
    queue_t *this_queue = GetEmptyQueueBlock(queue_id);
    
    // Try to push item to the queue
    if (this_queue != NULL) {
        this_queue->item_ptr = (uint32_t *) item;
        this_queue->status = QUEUE_FILLED;
        LeaveCritical();
        return QUEUE_SENT_OK;
    }
    
    // If there is no queue empty, check timeout
    if (timeout == 0) {
        LeaveCritical();
        return QUEUE_SENT_FAILED;
    }
    
    // Set the task to wait until timeout
    task_control_block_t *this_task = task_control_blocks + current_task;
    this_task->status = TASK_STATE_WAIT_TO_SENT_QUEUE;
    this_task->sleep_time = timeout;
    this_task->queue_id = queue_id;
    LeaveCritical();
    Yield();
    
    // The task is ready again, let's try to push item to the queue again
    EnterCritical();
    this_queue = GetEmptyQueueBlock(queue_id);
    if (this_queue != NULL) {
        this_queue->item_ptr = &item;
        this_queue->status = QUEUE_FILLED;
        LeaveCritical();
        return QUEUE_SENT_OK;
    }
    
    LeaveCritical();
    return QUEUE_SENT_FAILED;
}

static int _ktSvcReceiveFromQueue(uint8_t queue_id, uint32_t *item_ptr, uint32_t timeout) {
    EnterCritical();
    
    queue_t *this_queue = GetFilledQueueBlock(queue_id);
    
    // Try to pull item from the queue
    if (this_queue != NULL) {
        *item_ptr = this_queue->item_ptr;
        this_queue->status = QUEUE_EMPTY;
        LeaveCritical();
        return QUEUE_RECEIVE_OK;
    }
    
    // If there is no queue filled, check timeout
    if (!timeout) {
        LeaveCritical();
        return QUEUE_RECEIVE_FAILED;
    }
    
    // Set the task to wait until timeout
    task_control_block_t *this_task = task_control_blocks + current_task;
    this_task->status = TASK_STATE_WAIT_TO_RECEIVE_QUEUE;
    this_task->sleep_time = timeout;
    this_task->queue_id = queue_id;
    LeaveCritical();
    Yield();
    
    // The task is ready again, let's try to pull item from the queue again
    EnterCritical();
    this_queue = GetFilledQueueBlock(queue_id);
    if (this_queue != NULL) {
        *item_ptr = this_queue->item_ptr;
        this_queue->status = QUEUE_EMPTY;
        LeaveCritical();
        return QUEUE_RECEIVE_OK;
    }
    
    LeaveCritical();
    return QUEUE_RECEIVE_FAILED;
}


// Supervisor Calls
//   called by syscall
void _ktSvcHandler(hardware_stack_frame_t *hw_ctx) {
    uint8_t service_no = *(uint8_t *) (hw_ctx->pc - 2);
    if (service_no != 0x80) {
        return;
    }
    
    switch (hw_ctx->r0) {
        case SYSCALL_START_OS:
            hw_ctx->r0 = _ktSvcStartOs();
            break;
        case SYSCALL_TASK_KILL:
            hw_ctx->r0 = _ktSvcTaskKill(hw_ctx->r1);
            break;
        case SYSCALL_TASK_SLEEP:
            hw_ctx->r0 = _ktSvcTaskSleep(hw_ctx->r1, hw_ctx->r2);
            break;
        case SYSCALL_SEND_TO_QUEUE:
            hw_ctx->r0 = _ktSvcSendToQueue(hw_ctx->r1, hw_ctx->r2, hw_ctx->r3);
            break;
        case SYSCALL_RECEIVE_FROM_QUEUE:
            hw_ctx->r0 = _ktSvcReceiveFromQueue(hw_ctx->r1, hw_ctx->r2, hw_ctx->r3);
            break;
        default:
            hw_ctx->r0 = SYSCALL_UNDEFINED;
    }
}

__attribute__((naked)) void SVC_Handler(void) {
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
static inline int32_t syscall(int32_t r0, int32_t r1, int32_t r2, int32_t r3) {
    int32_t result;
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
uint32_t _ContextSwitcher(uint32_t stack_top) {
    task_control_block_t *this_task = task_control_blocks + current_task;
    task_control_block_t *next_task = task_control_blocks + current_task;
    
    // Save the stack pointer passed by r0
    this_task->stack_top = (uint32_t *) stack_top;
    
    if (this_task->status == TASK_STATE_RUNNING) {
        this_task->status = TASK_STATE_READY;
    }
    
    // Search for the task with highest priority
    uint8_t highest_priority = 0xff;
    uint8_t current_task_i = current_task;
    
    uint8_t i = task_count;
    // if task_count_i == 1, this loop won't be triggered.
    while (--i) {
        current_task_i++;
        if (current_task_i >= task_count) {
            current_task_i = 0;
        }
        
        this_task = task_control_blocks + current_task_i;
        
        // Switch to the task with highest priority,
        // if there were multiple tasks at the highest,
        // switch between the highest ones.
        if (this_task->status == TASK_STATE_READY
            && this_task->priority <= highest_priority) {
            next_task = this_task;
            current_task = current_task_i;
            highest_priority = next_task->priority;
        }
    }
    
    next_task->status = TASK_STATE_RUNNING;
    
    // Load the stack pointer back to r0
    return (uint32_t) next_task->stack_top;
    
}

// PendSV will be called by Yield every SysTick.
//   this is to do the context switch
__attribute__((naked)) void PendSV_Handler(void) {
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

int ktOSStart(void) {
    return syscall(SYSCALL_START_OS, 0, 0, 0);
}

// Task methods

void InitTaskControlBlock(void) {
    task_control_block_t *this_tcb;
    for (int i = 0; i < MAX_TASKS_COUNT; i++) {
        this_tcb = task_control_blocks + i;
        this_tcb->pid = (uint8_t) -1;
        this_tcb->status = TASK_STATE_KILLED;
        this_tcb->priority = (uint8_t) -1;
        this_tcb->queue_id = TASK_WITH_NO_QUEUE;
    }
}


int TaskCreate(
        TaskFunction entry,
        void *arg,
        uint32_t stack_size,
        uint8_t priority,
        const char *name
) {
    EnterCritical();
    
    // Find a available task control block.
    task_control_block_t *this_task;
    for (int i = 0; i < MAX_TASKS_COUNT; i++) {
        this_task = task_control_blocks + i;
        if (this_task->status == TASK_STATE_KILLED) {
            break;
        }
    }
    
    // Allocate memory space for stack.
    this_task->mem_block = AllocateMemBlock(stack_size);
    if (this_task->mem_block == NULL) {
        LeaveCritical();
        return TASK_ALLOCATE_STACK_FAILED;
    }
    
    strcpy(this_task->name, name);
    this_task->pid = task_count++; //task id for operation.
    this_task->priority = priority;
    this_task->status = TASK_STATE_READY;
    this_task->stack_size = stack_size;
    this_task->stack_bottom = this_task->mem_block->stack_bottom;
    this_task->stack_top = (uint32_t *) ((uint32_t) this_task->stack_bottom - (16 * sizeof(uint32_t)));
    
    // Init stack frame.
    hardware_stack_frame_t *hardware_stack_frame;
    hardware_stack_frame = (hardware_stack_frame_t *) ((uint32_t) this_task->stack_bottom - (8 * sizeof(uint32_t)));
    //memset(hardware_stack_frame, 0, sizeof(hardware_stack_frame));
    //software_stack_frame_t * software_stack_frame;
    //software_stack_frame = (software_stack_frame_t*)this_task->stack_top;
    //memset(software_stack_frame, 0, sizeof(software_stack_frame));
    
    hardware_stack_frame->r0 = (uint32_t) arg;
    hardware_stack_frame->lr = (uint32_t) TaskKill;
    hardware_stack_frame->pc = (uint32_t) entry;
    hardware_stack_frame->psr = 0x21000000; //default PSR value
    
    LeaveCritical();
    return TASK_OK;
    
}

void TaskKill(void) {
    syscall(SYSCALL_TASK_KILL, current_task, 0, 0);
}

void TaskSleep(uint32_t sleep_time) {
    syscall(SYSCALL_TASK_SLEEP, current_task, sleep_time, 0);
}


int QueueSendToBlock(uint8_t qcb_id, int32_t item, uint32_t timeout) {
    return syscall(SYSCALL_SEND_TO_QUEUE, qcb_id, item, timeout);
}

int QueueReceiveFromBlock(uint8_t qcb_id, uint32_t *item_ptr, uint32_t timeout) {
    return syscall(SYSCALL_RECEIVE_FROM_QUEUE, qcb_id, item_ptr, timeout);
}


// The System Tick Time (SysTick) generates interrupt requests on a regular basis.
// This allows an OS to carry out context switching to support multiple tasking.
void SysTick_Handler(void) {
    if (!is_os_started) return;
    systicks++;
    
    task_control_block_t *this_task;
    for (int i = 0; i < task_count; i++) {
        
        this_task = task_control_blocks + i;
        
        // Deal with killed tasks
        if (this_task->status == TASK_STATE_KILLED) {
            continue;
        }
        
        // Deal with sleeping tasks
        if (this_task->sleep_time > 0 && this_task->sleep_time != NO_TIMEOUT) {
            this_task->sleep_time -= SYSTICK_INTERVAL_MS;
        }
        if (this_task->sleep_time <= 0) {
            this_task->status = TASK_STATE_READY;
            this_task->sleep_time = 0;
            continue;
        }
        
        // Deal with tasks blocked by queue
        switch (this_task->status) {
            case TASK_STATE_WAIT_TO_SENT_QUEUE:
                if (GetEmptyQueueBlock(this_task->queue_id) != NULL) {
                    this_task->status = TASK_STATE_READY;
                    this_task->sleep_time = 0;
                    this_task->queue_id = (uint8_t) -2;
                }
                break;
            case TASK_STATE_WAIT_TO_RECEIVE_QUEUE:
                if (GetFilledQueueBlock(this_task->queue_id) != NULL) {
                    this_task->status = TASK_STATE_READY;
                    this_task->sleep_time = 0;
                    this_task->queue_id = (uint8_t) -2;
                }
                break;
        }
        
    }
    
    Yield();
}


// Critical region methods
//   Setting PRIMASK to 1 raises the execution priority to 0.
//   This prevents any exceptions with configurable priority from
//   becoming active, except through the fault escalation mechanism
static uint8_t critical_depth = 0;

void EnterCritical(void) {
    if (critical_depth++ == 0) {
        __set_PRIMASK(1);
    }
}

void LeaveCritical(void) {
    if (--critical_depth == 0) {
        __set_PRIMASK(0);
    }
}



