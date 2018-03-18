//
// ktos.h @ ktOS
//
// Created by Kotorinyanya.
//

#ifndef KTOS_KTOS_H
#define KTOS_KTOS_H

#include "types.h"
#include "helper.h"
#include "../CMSIS/CM3/DeviceSupport/ST/STM32F10x/stm32f10x.h"

int ktOSStart(void);

void InitTaskControlBlock(void);

int TaskCreate(TaskFunction entry, void *arg, uint32_t stack_size, uint8_t priority, const char *name);

void TaskKill(void);

void TaskSleep(uint32_t sleep_time);

void InitQueueControlBlock(void);

int QueueSendToBlock(uint8_t qcb_id, int32_t item, uint32_t timeout);

int QueueReceiveFromBlock(uint8_t qcb_id, uint32_t *item_ptr, uint32_t timeout);

void EnterCritical(void);

void LeaveCritical(void);

#endif //KTOS_KTOS_H
