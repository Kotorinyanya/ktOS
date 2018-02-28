//
// helper.c @ ktOS
//
// Created by Kotorinyanya.
//

#include "helper.h"

void * memset(void *destination, int value, uint32_t n)
{
    const unsigned char v = (unsigned char)value;
    unsigned char *dst;
    for(dst = destination; n > 0; ++dst, --n)
        *dst = v;
    return destination;
}