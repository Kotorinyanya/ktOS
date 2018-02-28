//
// types.h @ ktOS
//
// Created by Kotorinyanya.
//

#ifndef KTOS_TYPES_H
#define KTOS_TYPES_H

#include "stm32f10x.h"

typedef void(*TaskFunction)(void *);

// Codes definition

enum RETURN_CODE {
    SYSCALL_UNDEFINED,
    SYSCALL_OK,
    SYSCALL_FAILED,
    OS_ALREADY_STARTED,
    OS_START_FAILED,
    TASK_OK,
    TASK_AMOUNT_MAXIMUM_EXCEEDED,
    TASK_STACK_SIZE_NOT_ALIGNED,
    MEM_POOL_MAXIMUM_EXCEEDED,
    QUEUE_OK,
    QUEUE_FAILED
};

enum TASK_STATE_CODE {
    TASK_STATE_READY,
    TASK_STATE_DELAYED,
    TASK_STATE_KILLED,
    TASK_STATE_WAIT_TO_SENT_QUEUE,
    TASK_STATE_WAIT_TO_RECEIVE_QUEUE
};

enum CALL_CODE {
    SYSCALL_START_OS,
    SYSCALL_TASK_SLEEP,
    SYSCALL_TASK_KILL,
    SYSCALL_SWITCH_CONTEXT,
    SYSCALL_FATAL_ERROR
};

enum QUEUE_CODE {
    QUEUE_EMPTY,
    QUEUE_FILLED,
    QUEUE_SENT_OK,
    QUEUE_SENT_FAILED,
    QUEUE_RECEIVE_OK,
    QUEUE_RECEIVE_FAILED
};


// Struct definition

// This defines the stack frame that is saved by the hardware (automatically)
typedef struct {
    uint32_t r0;
    uint32_t r1;
    uint32_t r2;
    uint32_t r3;
    uint32_t r12;
    uint32_t lr;
    uint32_t pc;
    uint32_t psr;
} hardware_stack_frame_t;

// This defines the stack frame that is saved by the software (manually)
typedef struct {
    uint32_t r4;
    uint32_t r5;
    uint32_t r6;
    uint32_t r7;
    uint32_t r8;
    uint32_t r9;
    uint32_t r10;
    uint32_t r11;
} software_stack_frame_t;

// This defines the task control block to manage tasks
typedef struct {
    uint8_t id;
    uint8_t status;
    uint8_t priority;
    uint32_t sleep_time;
    uint32_t stack_size;
    uint32_t stack_top;
    uint32_t stack_bottom;
    hardware_stack_frame_t hardware_stack_frame;
    software_stack_frame_t software_stack_frame;
} task_control_block_t;

// This defines the queue block for Inter Process Communication
typedef struct {
    uint8_t status;
    uint32_t * item_ptr;
} queue_block_t;


#endif //KTOS_TYPES_H
