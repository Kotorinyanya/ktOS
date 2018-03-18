//
// helper.c @ ktOS
//
// Created by Kotorinyanya.
//

#include "helper.h"

void *memset(void *destination, int value, uint32_t n) {
    const unsigned char v = (unsigned char) value;
    unsigned char *dst;
    for (dst = destination; n > 0; ++dst, --n)
        *dst = v;
    return destination;
}

void *memcpy(void *dest, const void *src, uint32_t len) {
    char *d = dest;
    const char *s = src;
    while (len--)
        *d++ = *s++;
    return dest;
}

char *strcpy(char *destination, const char *source) {
    char *rt = destination;
    while (*source != '\0') {
        *(destination++) = *(source++);
    }
    *destination = '\0';
    return rt;
}