#include "helpers.h"
#include <stdio.h>

/* Helper function definitions go here */
void initialize_block(){
        starting_address = ics_inc_brk();
        ics_header* cursor = (ics_header*)starting_address;
        cursor->block_size = 0|0x1;
        cursor+=(4088/8);
        cursor->block_size = 0|0x1;
        cursor-=(4080/8); //move to address right after prologue;
        ics_free_header* free_header = (ics_free_header*)cursor;
        freelist_head = free_header;
        free_header->header.block_size = 4096 - 8 - 8;//block size = 4080 after excluding the prologue and epilogue
        free_header->header.unused = UNUSED_HEADER_FIELD;
        free_header->prev = NULL;
        free_header->next = NULL;
        cursor+= free_header->header.block_size/8;
        cursor --;
        ics_footer* footer = (ics_footer*)cursor;
        footer -> block_size = free_header->header.block_size;
        footer -> unused = UNUSED_FOOTER_FIELD;
}

int allocate_new_page(){
        ics_header* old_epilogue = (ics_header*)ics_get_brk();
        old_epilogue --; //move to the starting address of old epilogue
        if(ics_inc_brk()!=(void*)-1){
            ics_header* new_epilogue = (ics_header*)ics_get_brk();
            new_epilogue --;
            new_epilogue -> block_size = 0|0x1; 
            ics_free_header* new_block_header = (ics_free_header*)old_epilogue;
            new_block_header->next = NULL;
            new_block_header->prev = NULL;
            new_block_header->header.block_size = 4096 ;//blocksize = incr brk size - new epilogue +  old epilogue
            new_block_header->header.unused = UNUSED_HEADER_FIELD;
            ics_footer* new_block_footer = (ics_footer*)new_block_header;
            new_block_footer += (new_block_header->header.block_size)/8;
            new_block_footer --; //move to the starting address of footer
            new_block_footer -> block_size = new_block_header->header.block_size;
            new_block_footer -> unused = UNUSED_FOOTER_FIELD;
            ics_free_header* header_after_coalescing = coalescing_checker((ics_header*)new_block_header);
            insert(header_after_coalescing);
            return 0;
        }
        return -1;
}

void* find_first_fit(size_t block_size, size_t padding, bool payload_aligned){
        void* to_return = NULL;
        ics_free_header* cursor = freelist_head;
        while(cursor!=NULL){
            if(cursor->header.block_size>= block_size){
                //find a block that is equal to or larger than the total required block size.
                size_t original_block_size = (cursor->header.block_size&-4);
                ics_header* allocated_header = (ics_header*)cursor;
                size_t remaining_size =  original_block_size - block_size;
                to_return = (void*)(allocated_header+1);// move to the payload
                if(payload_aligned)
                    if(remaining_size>=MINIMUM_BLOCK_SIZE)
                        allocated_header->block_size = block_size|0x3;
                    else
                        allocated_header->block_size = (block_size+remaining_size)|0x3;
                else
                    if(remaining_size>=MINIMUM_BLOCK_SIZE)
                        allocated_header->block_size = block_size|0x1;
                    else
                        allocated_header->block_size = (block_size+remaining_size)|0x1;
                allocated_header->unused = UNUSED_HEADER_FIELD;
                ics_footer *footer = (ics_footer*)allocated_header;
                footer += (allocated_header->block_size& -4)/8;
                footer --;// move to the starting address of footer
                footer->block_size = allocated_header->block_size;
                
                footer->unused = UNUSED_FOOTER_FIELD;
                footer->padding_amount = padding%16;

                if(cursor->next!=NULL&&cursor->prev!=NULL){
                        cursor->prev->next = cursor->next;
                        cursor->next->prev = cursor->prev;
                }
                else if(cursor->next == NULL && cursor->prev == NULL)
                    freelist_head = NULL;
                else if(cursor->next == NULL)
                    cursor->prev->next = NULL;
                else if(cursor->prev == NULL){
                    cursor->next->prev = NULL;
                    freelist_head = cursor->next;
                   }       
                
                if(remaining_size>=MINIMUM_BLOCK_SIZE){
                    ics_free_header* new_head = split_block(remaining_size, footer);
                    insert(new_head);
                }
                break;
            }
            cursor = cursor->next;
        }
    if(cursor == NULL){
        if(allocate_new_page()== -1)
            return NULL;
        return find_first_fit(block_size, padding, payload_aligned);
    }
    return to_return; 
}

ics_free_header* split_block(size_t remaining_size, ics_footer* prev_footer){
    ics_free_header* to_return;
    ics_header* cursor = (ics_header*)prev_footer+1;
    ics_free_header*free_header = (ics_free_header*)cursor;
    to_return = free_header;
    free_header->header.block_size = remaining_size;
    free_header->header.unused = UNUSED_HEADER_FIELD;
    free_header->next = NULL;
    free_header->prev = NULL;
    ics_footer* footer = (ics_footer*) free_header;
    footer += (free_header->header.block_size/8);
    footer --;
    footer->block_size = remaining_size;
    footer->unused = UNUSED_FOOTER_FIELD;
    return to_return;
}

bool valid_ptr(void* ptr){
    //check if a given ptr is a valid ptr to be free
    ics_header* prologue = (ics_header*)starting_address;
    ics_header* epilogue = (ics_header*)ics_get_brk();
    prologue ++; //end of prologue
    epilogue --;//start of epilogue
    if(ptr<(void*)prologue && ptr>(void*)epilogue)
        return false;
    ics_header* head = (ics_header*)ptr;
    head--;
    ics_footer* foot = (ics_footer*)head;
    foot += (head->block_size&-2)/8; //move footer to the end of the address of footer with allocated bit masked out
    foot --;// move to the starting address of footer
    
    if(head->block_size == (head->block_size&-2) || foot->block_size == (foot->block_size&-2))// no allocated bit
        return false;
    if(head->unused != UNUSED_HEADER_FIELD || foot->unused != UNUSED_FOOTER_FIELD)
        return false;
    if((head->block_size&-2) != (foot->block_size&-2))
        return false;
    if((head->block_size&0x2) == 0x2){
        if (foot->padding_amount<=0)
            return false;
    }
    if((foot->block_size&0x2) == 0x2){
        if (foot->padding_amount<=0)
            return false;
    }
    return true;
}

ics_free_header* coalescing_checker(ics_header* header){
    //check if the block that ptr points to is subject to coalesing, if (yes) perform coalescing and return the ics_free_header after the coalesing.
    ics_free_header* to_return = NULL;
    unsigned int c;
    ics_header* cursor = header;
    ics_footer* prev_footer = (ics_footer*)cursor;
    prev_footer--;
    ics_header* next_header = (ics_header*)cursor;
    next_header += (next_header->block_size&-4)/8;
    c = coalescing_case(prev_footer, next_header);
    ics_header* temp;
    ics_footer* new_footer;
    ics_free_header* next_free_header;
    switch(c){
        case 1:
            to_return = (ics_free_header*)header;
            to_return -> next = NULL;
            to_return -> prev = NULL;
            to_return -> header.block_size = header->block_size&-4;// masked out padding and allocated bit
            ics_footer*footer = (ics_footer*)header;
            footer+= (header->block_size&-4)/8;
            footer --;
            footer->block_size = header->block_size&-4;
            return to_return;
            break;
        case 2:
            temp = (ics_header*)(prev_footer - (prev_footer->block_size&-4)/8);
            temp++;//move to the beginning of the freeheader
            to_return = (ics_free_header*)temp;
            to_return->header.block_size = (prev_footer->block_size&-4)+(header->block_size&-4);
            new_footer = (ics_footer*)to_return;
            new_footer += (to_return->header.block_size&-4)/8;
            new_footer--;
            new_footer ->block_size = to_return->header.block_size;
            if(to_return == freelist_head){
                freelist_head = to_return->next;
            }
            else{
                if(to_return->next!=NULL){
                to_return->prev->next = to_return->next;
                to_return->next->prev = to_return ->prev;
                }
                else{
                    to_return->prev->next = NULL;
                }
            }
            return to_return;
            break;
            
        case 3:
            to_return = (ics_free_header*)header;
            next_free_header  = (ics_free_header*)next_header;
            to_return -> header.block_size = (header->block_size&-4) + (next_header->block_size&-4);
            new_footer = (ics_footer*)to_return;
            new_footer+= (to_return->header.block_size&-4)/8;
            new_footer--;
            new_footer -> block_size = to_return->header.block_size;
            if(next_free_header == freelist_head){
                freelist_head = next_free_header->next;
            }
            else{
            if(next_free_header->next!=NULL){
                next_free_header->prev->next = next_free_header->next;
                next_free_header->next->prev = next_free_header ->prev;
                }
                else{
                    next_free_header->prev->next = NULL;
                }
            }
            return to_return;
            break;
        case 4:
            temp = (ics_header*)(prev_footer - (prev_footer->block_size&-4)/8);
            temp++;//move to the beginning of the freeheader
            to_return = (ics_free_header*)temp;
            next_free_header  = (ics_free_header*)next_header;
            to_return -> header.block_size = (prev_footer->block_size&-4)+(header->block_size&-4)+(next_header->block_size&-4);
            new_footer = (ics_footer*)to_return;
            new_footer+= (to_return->header.block_size&-4)/8;
            new_footer--;
            new_footer -> block_size = to_return->header.block_size;
            if(to_return == freelist_head){
                freelist_head = to_return->next;
                if(next_free_header == freelist_head)
                    freelist_head = next_free_header->next;
            }
            else{
                if(to_return->next!=NULL){
                to_return->prev->next = to_return->next;
                to_return->next->prev = to_return ->prev;
                }
                else{
                    to_return->prev->next = NULL;
                }
                if(next_free_header->next!=NULL){
                next_free_header->prev->next = next_free_header->next;
                next_free_header->next->prev = next_free_header ->prev;
                }
                else{
                    next_free_header->prev->next = NULL;
                }
            }
            return to_return;
            break;
        case -1:
            break;
        default:
            break;
            
    }
    
    
    
    return to_return;
    
}
    

void insert(ics_free_header* free_header){
    //insert a ics_free_header into the free_list
    bool inserted = false;
    ics_free_header* cursor = freelist_head;
    ics_free_header* prev = NULL;
    if(freelist_head == NULL){
        freelist_head = free_header;
        free_header->prev = NULL;
        free_header->next = NULL;
    }
    else if(free_header<cursor){
        freelist_head = free_header;
        free_header->prev = NULL;
        free_header->next = cursor;
        cursor->prev = free_header;
        inserted = true;
    }
    else{
    while(cursor!=NULL){
        if(free_header<cursor){
            free_header->prev = cursor->prev;
            free_header->next = cursor;
            cursor->prev->next = free_header;
            cursor->prev = free_header;
            inserted = true;
            break;
            }
        prev = cursor;
        cursor = cursor->next;
        }
        
    }
    
    if(inserted == false){
        if(prev != NULL){
        free_header->next = NULL;
        free_header->prev = prev;
        prev->next = free_header;
        }
        else{
            freelist_head = free_header;
        }
    }
    ics_freelist_print();

    
    
}


// 1 no coalescing, 2 prev coalescing, 3 next coalescing, 4 both coalescing
unsigned int coalescing_case(ics_footer* prev_footer, ics_header* next_header){
    if((prev_footer->block_size&-2) != prev_footer->block_size && (next_header->block_size&-2) != next_header->block_size)
        return 1;
    else if((prev_footer->block_size&-2) == prev_footer->block_size && (next_header->block_size&-2) != next_header->block_size)
        return 2;
    else if((prev_footer->block_size&-2) != prev_footer->block_size && (next_header->block_size&-2) == next_header->block_size)
        return 3;
    else if((prev_footer->block_size&-2) == prev_footer->block_size && (next_header->block_size&-2) == next_header->block_size)
        return 4;
    return -1;
}