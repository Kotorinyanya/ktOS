//
// config.h @ ktOS
//
// Created by Kotorinyanya.
//

#ifndef KTOS_HEAP_H
#define KTOS_HEAP_H

#include "types.h"
#include "config.h"

struct _mem_block_header_t * AllocateMemBlock(uint32_t size);

void FreeMemBlock(struct _mem_block_header_t *this_block);

#endif //KOTS_HEAP_H