//
// config.h @ ktOS
//
// Created by Kotorinyanya.
//

#include "config.h"
#include "types.h"

#define NULL 0
#define HEADER_SIZE sizeof(header_t)

uint32_t mempool[MEM_POOL_SIZE];

void *global_base = mempool;

typedef struct HEADER {
    uint32_t size;
    uint32_t * next;
    uint32_t * past;
} header_t;

void *malloc(uint32_t size) {

    if(size % 8 != 0)   return NULL;

    header_t *block = RequestFreeBlock(size);
}

header_t *RequestFreeBlock(int32_t size)
{
    //if there is enough space between two blocks inside the chain.
    header_t * header_i;
    for(header_i = mempool; header_i != NULL; header_i = header_i->next) {
        uint32_t available_space = (uint32_t)(header_i->next - header_i - HEADER_SIZE - header_i->size);
        if(available_space >= size) {
            header_t * header = header_i + HEADER_SIZE + header_i->size;
            header_i->past->next = header;
            header->next = header_i;
            header->size = size;
            return header;
        }
    }

    header_t * last_block = header_i;
    //if the space inside the chain is not enough, check the 'top'.
    if((last_block + HEADER_SIZE + last_block->size + size) 
        <= (mempool + MEM_POOL_SIZE)) {
        header_t * header = last_block + HEADER_SIZE + lase->size;
        header->size = size;
        header->next = NULL;
        header->past = last_block;
        last_block->next = header;
        return header;
    }

    return NULL;

}

void free(header_t *header)
{
    header->past->next = header->next;
}

// header_t *FindFreeBlock(header_t **last_block, uint32_t size)
// {
//     header_t * current = global_base;
//     while (current && !(current->free && current->size >= size)) {
//         *last_block = current;
//         current = current->next;
//     }
//     return current;
// }


//    header_t * current_header = 0;
//    header_t * closest_header = current_header->next;
//    header_t * next_header = current_header->next;
//    do {
//        next_header = current_header->next;
//        closest_header = (closest_header < next_header)?(closest_header):(next_header);
//    } while(next_header != NULL);
//
//    uint32_t next_header_addr = current_header + sizeof(header_t) + current_header->pool_size + size;
//
//    if(closest_header == NULL)