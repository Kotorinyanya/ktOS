//
// types.h @ ktOS
//
// Created by Kotorinyanya.
//

#ifndef KTOS_TYPES_H
#define KTOS_TYPES_H

#include "stm32f10x.h"

#define SYSTICK_FREQUENCY_HZ 1000
#define SYSTICK_INTERVAL_MS 1000 / SYSTICK_FREQUENCY_HZ  // Please make sure that SYSTICK_INTERVAL_MS is an interger.

#define MAX_TASKS_SIZE 10
#define MAX_QUEUE_SIZE 5
#define MAX_QUEUE_CONTROL_BLOCK_SIZE 5
#define MEM_POOL_SIZE 3000

typedef void(*TaskFunction)(void *);

typedef enum STATUS_CODE {
/*******  Task Status Code Definitions **************************************************************/
    TASK_STATE_READY                   = 1,    /*!< task is ready to be loaded                      */
    TASK_STATE_DELAYED                 = 2,    /*!< task delayed(sleeping)                          */
    TASK_STATE_KILLED                  = 3,    /*!< task killed                                     */
    TASK_STATE_WAIT_TO_SENT_QUEUE      = 4,    /*!< task was blocked on pushing data to queue       */
    TASK_STATE_WAIT_TO_RECEIVE_QUEUE   = 5,    /*!< task was blocked on pulling data from queue     */
    TASK_STATE_RUNNING                 = 6,    /*!< task executing                                  */

/*******  Queue Status Code Definitions *************************************************************/
    QUEUE_EMPTY                        = 7,    /*!< queue empty                                     */
    QUEUE_FILLED                       = 8,    /*!< queue filled                                    */
    QUEUE_SENT_OK                      = 9,    /*!< succeeded to push data to queue                 */
    QUEUE_SENT_FAILED                  = 10,   /*!< failed to push data to queue                    */
    QUEUE_RECEIVE_OK                   = 11,   /*!< succeeded to pull data from queue               */
    QUEUE_RECEIVE_FAILED               = 12    /*!< failed to pull data from queue                  */
} STATUS_CODE_DEF;

typedef enum RETURN_CODE {
/*******  Return Code Definitions *******************************************************************/
    SYSCALL_UNDEFINED                  = 13,   /*!< undefined syscall                               */
    SYSCALL_OK                         = 14,   /*!< syscall successful                              */
    SYSCALL_FAILED                     = 15,   /*!< syscall failed                                  */
    OS_ALREADY_STARTED                 = 16,   /*!< the operating system is already started         */
    OS_START_FAILED                    = 17,   /*!< failed to start the operating system            */
    TASK_OK                            = 18,   /*!< succeeded to create task                        */
    TASK_AMOUNT_MAXIMUM_EXCEEDED       = 19,   /*!< too many tasks                                  */
    TASK_STACK_SIZE_NOT_ALIGNED        = 20,   /*!< bad stack size(not aligned to 8-byte)           */
    MEM_POOL_MAXIMUM_EXCEEDED          = 21    /*!< not enough memory                               */
} RETURN_CODE_DEF;

typedef enum SYSCALL_CODE {
/*******  Syscall Code Definitions *******************************************************************/
    SYSCALL_START_OS                   = 22,
    SYSCALL_TASK_SLEEP                 = 23,
    SYSCALL_TASK_KILL                  = 24,
    SYSCALL_SEND_TO_QUEUE              = 25,
    SYSCALL_RECEIVE_FROM_QUEUE         = 26
} SYSCALL_CODE_DEF;


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
    uint8_t queue_id;
    software_stack_frame_t software_stack_frame;
} task_control_block_t;

typedef struct {
    uint8_t status;
    uint32_t * item_ptr;
} queue_block_t;

typedef struct {
    uint8_t id;
    queue_block_t queues[MAX_QUEUE_SIZE];
} queue_control_block_t;


#endif //KTOS_TYPES_H
