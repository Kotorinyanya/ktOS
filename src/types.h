//
// types.h @ ktOS
//
// Created by Kotorinyanya.
//

#ifndef KTOS_TYPES_H
#define KTOS_TYPES_H

#include "../CMSIS/CM3/DeviceSupport/ST/STM32F10x/stm32f10x.h"
#include "heap.h"
#include "config.h"

#define NO_TIMEOUT 0xffffffff
#define NULL 0x0
#define HEADER_SIZE sizeof(mem_block_header_t)

typedef void(*TaskFunction)(void *);

typedef enum STATUS_CODE {
/*******  Task Status Code Definitions **************************************************************/
            TASK_STATE_READY = 1,    /*!< task is ready to be loaded */
    TASK_STATE_DELAYED = 2,    /*!< task delayed(sleeping) */
    TASK_STATE_KILLED = 3,    /*!< task killed */
    TASK_STATE_WAIT_TO_SENT_QUEUE = 4,    /*!< task was blocked on pushing data to queue */
    TASK_STATE_WAIT_TO_RECEIVE_QUEUE = 5,    /*!< task was blocked on pulling data from queue */
    TASK_STATE_RUNNING = 6,    /*!< task executing */

/*******  Queue Status Code Definitions *************************************************************/
            QUEUE_EMPTY = 7,    /*!< queue empty */
    QUEUE_FILLED = 8,    /*!< queue filled */
    QUEUE_SENT_OK = 9,    /*!< succeeded to push data to queue */
    QUEUE_SENT_FAILED = 10,   /*!< failed to push data to queue */
    QUEUE_RECEIVE_OK = 11,   /*!< succeeded to pull data from queue */
    QUEUE_RECEIVE_FAILED = 12,   /*!< failed to pull data from queue */
    TASK_WITH_NO_QUEUE = 255,    /*!< task do not have a queue with it */
    QUEUE_CONTROL_BLOCK_NOT_BEING_USED = 255
} STATUS_CODE_DEF;

typedef enum RETURN_CODE {
/*******  Return Code Definitions *******************************************************************/
            SYSCALL_UNDEFINED = 13,   /*!< undefined syscall */
    SYSCALL_OK = 14,   /*!< syscall successful */
    SYSCALL_FAILED = 15,   /*!< syscall failed */
    OS_ALREADY_STARTED = 16,   /*!< the operating system is already started */
    OS_START_FAILED = 17,   /*!< failed to start the operating system */
    TASK_OK = 18,   /*!< succeeded to create task */
    TASK_AMOUNT_MAXIMUM_EXCEEDED = 19,   /*!< too many tasks */
    TASK_ALLOCATE_STACK_FAILED = 20,   /*!< bad stack size(not aligned to 8-byte) */
    MEM_POOL_MAXIMUM_EXCEEDED = 21    /*!< not enough memory */
} RETURN_CODE_DEF;

typedef enum SYSCALL_CODE {
/*******  Syscall Code Definitions *******************************************************************/
            SYSCALL_START_OS = 22,
    SYSCALL_TASK_SLEEP = 23,
    SYSCALL_TASK_KILL = 24,
    SYSCALL_SEND_TO_QUEUE = 25,
    SYSCALL_RECEIVE_FROM_QUEUE = 26
} SYSCALL_CODE_DEF;


// Stack frame that is saved by the hardware (automatically)
typedef struct _hardware_stack_frame_t {
    uint32_t r0;
    uint8_t r1;
    uint32_t r2;
    uint32_t r3;
    uint32_t r12;
    uint32_t lr;
    uint32_t pc;
    uint32_t psr;
} hardware_stack_frame_t;

// Stack frame that is saved by the software (manually)
typedef struct _software_stack_frame_t {
    uint32_t r4;
    uint32_t r5;
    uint32_t r6;
    uint32_t r7;
    uint32_t r8;
    uint32_t r9;
    uint32_t r10;
    uint32_t r11;
} software_stack_frame_t;

// Memory block for the stack of task.
typedef struct _mem_block_header_t mem_block_header_t;
struct _mem_block_header_t {
    uint32_t size;
    struct _mem_block_header_t *next_block;
    struct _mem_block_header_t *past_block;
    uint32_t *stack_bottom;
};

// Task control block definitions.
typedef struct _task_control_block_t {
    char name[TASK_NAME_SIZE];
    uint8_t pid;
    uint8_t status;
    uint8_t priority;
    uint32_t sleep_time;
    uint32_t stack_size;
    mem_block_header_t *mem_block;
    uint32_t *stack_top;
    uint32_t *stack_bottom;
    uint8_t queue_id;
    //software_stack_frame_t software_stack_frame;
} task_control_block_t;
//const task_control_block_t task_control_block_default = {
//        .name = "",
//        .pid = (uint8_t) -1,
//        .status = TASK_STATE_KILLED,
//        .priority = (uint8_t) -1,
//        .sleep_time = 0,
//        .stack_size = 0,
//        .mem_block = NULL,
//        .queue_id = TASK_WITH_NO_QUEUE,
//};

// Queue definitions.
typedef struct _queue_t {
    uint8_t status;
    uint32_t *item_ptr;
} queue_t;
//const queue_t queue_block_default = {
//        .status = QUEUE_EMPTY,
//        .item_ptr = NULL
//};


// Queue control block definitions.
typedef struct _queue_control_block_t {
    uint8_t id;
    queue_t queues[QUEUE_SIZE];
} queue_control_block_t;

#endif //KTOS_TYPES_H
