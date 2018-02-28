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
    while(1) {
        uint8_t result = QueueSendToBlock(count++, 0);
        switch (result) {
            case QUEUE_SENT_OK:
                printf("Hello bar, I have a message to you, %d\n", count);
                break;
            case QUEUE_SENT_FAILED:
                printf("Sorry bar, I can't send you the message now\n");
                break;
        }
        TaskSleep(2000);
    }
}

void bar(void)
{
    uint32_t count = 0;
    while(1) {
        uint8_t result = QueueReciveFromBlock(&count, 0);
        switch (result) {
            case QUEUE_RECEIVE_OK:
                printf("Hello foo, I have received your message, %d\n", count);
                break;
            case QUEUE_RECEIVE_FAILED:
                printf("Sorry foo, I can't receive your message now\n");
                break;
        }
        TaskSleep(3000);
    }
}

int main(void)
{
    TaskCreate((TaskFunction)foo, 0, 1000, 3);
    TaskCreate((TaskFunction)bar, 0, 1000, 3);

    ktOSStart();

    while(1) {
        printf("Oops! What happened ???\n");
    }

}