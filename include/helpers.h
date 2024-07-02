#ifndef HELPERS_H
#define HELPERS_H
#include "icsmm.h"
#include <stdbool.h>
/* Helper function declarations go here */
#define UNUSED_HEADER_FIELD 0x1999999999999
#define UNUSED_FOOTER_FIELD 0x333333333333
#define MINIMUM_BLOCK_SIZE 32
#define MAX_SIZE 4*4096
#define PADDING(size) (16*(size/16+1)-size)

extern ics_free_header *freelist_head;
extern void* starting_address;
void initialize_block();
void* find_first_fit(size_t block_size, size_t padding, bool payload_aligned);
ics_free_header* split_block(size_t remaining_size, ics_footer* prev_footer);


bool valid_ptr(void* ptr);
ics_free_header* coalescing_checker(ics_header* header);
void insert(ics_free_header* free_header);
int allocate_new_page();
unsigned int coalescing_case(ics_footer* prev_footer, ics_header* next_header);


#endif
