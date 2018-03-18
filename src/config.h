//
// config.h @ ktOS
//
// Created by Kotorinyanya.
//

#ifndef KTOS_CONF_H
#define KTOS_CONF_H

#define SYSTICK_FREQUENCY_HZ 1000
#define SYSTICK_INTERVAL_MS 1000 / SYSTICK_FREQUENCY_HZ  // Please make sure that SYSTICK_INTERVAL_MS is an interger.

#define MAX_TASKS_COUNT 10
#define TASK_NAME_SIZE 20

#define QUEUE_SIZE 1
#define MAX_QUEUE_CONTROL_BLOCK_COUNT 5

#define MEM_POOL_SIZE 16000


#endif //KTOS_CONF_H
