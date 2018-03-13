//
// ktos.h @ ktOS
//
// Created by Kotorinyanya.
//

#ifndef KTOS_KTOS_H
#define KTOS_KTOS_H

#include "types.h"
#inlcude "config.h"
#include "stm32f10x.h"
#include "helper.h"

#define NO_TIMEOUT 0xffffffff
#define NULL 0

uint32_t ktOSStart(void);

uint8_t TaskCreate(TaskFunction entry, void * arg, uint32_t stack_size, uint8_t priority);
void TaskKill(void);
void TaskSleep(uint32_t sleep_time);

void InitQueue(void);
uint8_t QueueSendToBlock(uint8_t qcb_id, uint32_t item, uint32_t timeout);
uint8_t QueueReciveFromBlock(uint8_t qcb_id, uint32_t *item_ptr, uint32_t timeout);

void EnterCritical(void);
void LeaveCritical(void);

#endif //KTOS_KTOS_H
