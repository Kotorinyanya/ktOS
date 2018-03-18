//
// config.h @ ktOS
//
// Created by Kotorinyanya.
//

#include "heap.h"

static uint32_t mem_pool[MEM_POOL_SIZE];

mem_block_header_t *RequestFreeBlock(uint32_t size) {
    
    uint32_t available_size;
    mem_block_header_t *this_block;
    mem_block_header_t *last_block;
    
    //if there is enough space between two blocks inside the chain.
    mem_block_header_t *this_block_i = (mem_block_header_t *) mem_pool;
    while (this_block_i->next_block != NULL) {
        available_size =
                (uint32_t) (this_block_i->next_block) - (uint32_t) (this_block_i) - this_block_i->size - HEADER_SIZE;
        if (available_size >= size) {
            last_block = this_block_i;
            this_block = (struct _mem_block_header_t *) ((uint32_t) last_block + HEADER_SIZE + last_block->size);
            this_block->next_block = last_block->next_block;
            this_block->past_block = last_block;
            this_block->past_block->next_block = this_block;
            return this_block;
        }
        this_block_i = this_block_i->next_block;
    }

    this_block = this_block_i;
    
    //if there is enough space on the 'top'.
    available_size = (uint32_t) (mem_pool + MEM_POOL_SIZE) - (uint32_t) this_block - HEADER_SIZE * 2;//with one additionally new header for linking.
    if (available_size >= size) {
        this_block->size = size;
        if (this_block->past_block != NULL) { //deal with initial block.
            this_block->past_block->next_block = this_block;
        }
        this_block->next_block = (struct _mem_block_header_t *) ((uint32_t) (this_block) + HEADER_SIZE +
                                                                 this_block->size); //create a empty block just for linking.
        this_block->next_block->past_block = this_block;
        return this_block;
    }
    
    return NULL;
}


struct _mem_block_header_t *AllocateMemBlock(uint32_t size) {
    
    //re-align stack size to 8 bytes.
    if (size % 2 != 0) {
        size = (size / 2) * 2;
    }
    
    mem_block_header_t *mem_block = RequestFreeBlock(size);
    if (mem_block != NULL) {
        mem_block->size = size;
        mem_block->stack_bottom = (uint32_t *) ((uint32_t) mem_block + HEADER_SIZE + mem_block->size) - 1;
        return mem_block;
    } else {
        return NULL;
    }
    
}


void FreeMemBlock(struct _mem_block_header_t *this_block) {
    this_block->past_block->next_block = this_block->next_block;
    this_block = NULL;
}