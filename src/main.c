//
// main.c @ ktOS
//
// Created by Kotorinyanya.
//
#include "ktos.h"
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
    uint32_t count = 0;
    uint8_t result;
    uint8_t queue_id = 3;
    uint32_t timeout = 0;
    printf("I am foo\n");
    TaskSleep(50);
    for(;;) {
        TaskSleep(100);
        result = QueueSendToBlock(queue_id, count, timeout);
        switch (result) {
            case QUEUE_SENT_OK:
                printf("Hello bar, I have a message to you, that's number: %d\n", count++);
                break;
            case QUEUE_SENT_FAILED:
                printf("Sorry bar, I can't send you the message right now\n");
                break;
        }
        if(count == 3) {
            TaskKill();
        }
    }
}

void bar(void)
{
    uint32_t item;
    uint8_t result;
    uint8_t queue_id = 3;
    uint32_t timeout = 0;
    printf("I am bar\n");
    for(;;) {
        TaskSleep(150);
        result = QueueReciveFromBlock(queue_id, &item, timeout);
        switch (result) {
            case QUEUE_RECEIVE_OK:
                printf("OK foo, I have received your message, it's number: %d\n", item);
                break;
            case QUEUE_RECEIVE_FAILED:
                printf("Sorry foo, I can't receive your message right now\n");
                break;
        }
    }
}

int main(void)
{

    TaskCreate((TaskFunction)foo, 0, 1024, 3);
    TaskCreate((TaskFunction)bar, 0, 1024, 3);

    ktOSStart();

    while(1) {
        printf("Oops! What happened ???\n");
    }

}