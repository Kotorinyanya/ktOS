//
// config.h @ ktOS
//
// Created by Kotorinyanya.
//

#ifndef KTOS_CONF_H
#define KTOS_CONF_H

#define SYSTICK_FREQUENCY_HZ 1000
#define SYSTICK_INTERVAL_MS 1000 / SYSTICK_FREQUENCY_HZ  // Please make sure that SYSTICK_INTERVAL_MS is an interger.

#define MAX_TASKS_SIZE 10
#define MAX_QUEUE_SIZE 5
#define MEM_POOL_SIZE 2500


#endif //KTOS_CONF_H
