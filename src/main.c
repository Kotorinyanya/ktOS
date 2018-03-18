//
// main.c @ ktOS
//
// Created by Kotorinyanya.
//
#include "ktos.h"
#include "../CMSIS/CM3/CoreSupport/core_cm3.h"
#include "config.h"
#include <stdio.h>

void Init(void)
{
    NVIC_SetPriorityGrouping(0);

    SysTick_Config(SystemCoreClock / SYSTICK_FREQUENCY_HZ);

    NVIC_SetPriority(SysTick_IRQn, NVIC_EncodePriority(0, 0, 0));
    NVIC_SetPriority(SVCall_IRQn, NVIC_EncodePriority(0, 1, 0));
    NVIC_SetPriority(PendSV_IRQn, NVIC_EncodePriority(0, 1, 0));
}

void foo(void)
{
    uint32_t count = 1;
    int result;
    int queue_id = 3;
    int timeout = 0;
    printf("I am foo\n");
    for(;;) {
        TaskSleep(100);
        result = QueueSendToBlock(queue_id, count, timeout);
        switch (result) {
            case QUEUE_SENT_OK:
                printf("Hello bar %d\n", count++);
                break;
            case QUEUE_SENT_FAILED:
                printf("Sorry bar \n");
                break;
        }
        if(count == 5) {
            TaskKill();
        }
    }
}

void bar(void)
{
    uint32_t item;
    int result;
    int queue_id = 3;
    int timeout = 0;
    printf("I am bar\n");
    for(;;) {
        TaskSleep(150);
        result = QueueReceiveFromBlock(queue_id, &item, timeout);
        switch (result) {
            case QUEUE_RECEIVE_OK:
                printf("OK foo %d\n", item);
                break;
            case QUEUE_RECEIVE_FAILED:
                printf("Sorry foo \n");
                break;
        }
    }
}

int main(void)
{
    InitQueueControlBlock();
    InitTaskControlBlock();
    
    TaskCreate((TaskFunction)foo, 0, 2048, 3, "foo");
    TaskCreate((TaskFunction)bar, 0, 2048, 3, "bar");

    ktOSStart();

    while(1) {
        printf("Oops! What happened ???\n");
    }

}