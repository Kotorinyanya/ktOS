//
// helper.h @ ktOS
//
// Created by Kotorinyanya.
//

#ifndef KTOS_HELPER_H
#define KTOS_HELPER_H

#include "types.h"

void *memset(void *destination, int value, uint32_t n);

void *memcpy(void *dest, const void *src, uint32_t len);

char *strcpy(char *destination, const char *source);

#endif //KTOS_HELPER_H