/**
 * Do not submit your assignment with a main function in this file.
 * If you submit with a main function in this file, you will get a zero.
 * If you want to make helper functions, put them in helpers.c
 */
#include "icsmm.h"
#include "helpers.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

ics_free_header *freelist_head = NULL;
void* starting_address = NULL;

void* ics_malloc(size_t size){
    if(size == 0){
        errno = EINVAL;
        return NULL;
    }
    if(size>MAX_SIZE){
        errno = ENOMEM;
        return NULL;
    }
    size_t padding = PADDING(size);
    size_t payload_size = size;
    bool payload_aligned = true;
    if(padding == 16)
        payload_aligned = false;
    if(payload_aligned)
        payload_size += padding;
    size_t block_size = payload_size+16;
    
    if(starting_address == NULL){
           initialize_block();   
    }
    void* to_return = (void*)find_first_fit(block_size, padding, payload_aligned);
    if (to_return == NULL){
        errno = ENOMEM;
        return NULL;
    }     
    return to_return;

}

void *ics_realloc(void *ptr, size_t size) {

    if(size == 0){
        ics_free(ptr);
        return NULL;
    }
    ics_header* old_header = (ics_header*)ptr;
    old_header --;
    size_t old_size = (old_header->block_size&-4);
    if(size<=old_size){
        size_t new_size = ((old_header->block_size&-4)-16) - size;
        size_t padding = PADDING(new_size);
        size_t payload_size = new_size;
        bool payload_aligned = true;
        if(padding == 16)
            payload_aligned = false;
        if(payload_aligned)
            payload_size += padding;
        size_t block_size = payload_size+16;
        if(payload_aligned)
            old_header->block_size = block_size|0x3;
        else
            old_header->block_size = block_size|0x1;
        ics_footer* old_footer = (ics_footer*)old_header;
        old_footer += block_size;
        old_footer --;
        old_footer->block_size = old_header->block_size;
        size_t remainning_size = old_size - block_size;
        if(remainning_size>=MINIMUM_BLOCK_SIZE){
            ics_free_header* new_head = split_block(remainning_size, old_footer);
            insert(new_head);
        }
        
        return ptr;
            
            
        
        
    }
    else{
        int to_copy_bytes = old_header->block_size;
        char *old_ptr = ptr;
        void *new_payload = ics_malloc(size);
        char *cursor = new_payload;
        int i = 0;
        while(i<to_copy_bytes){
            *cursor = *old_ptr;
            cursor++;
            old_ptr++;
            i++;   
        }
        ics_free(ptr);   
        return new_payload;
        
    }
    
}

int ics_free(void *ptr) {
    if (!valid_ptr(ptr)){
        errno = EINVAL;
        return -1;
    }
    ics_header* header = (ics_header*)ptr;
    header --;
    ics_free_header* free_header = coalescing_checker(header);
    insert(free_header);
    return 0;
    
}
