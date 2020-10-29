#include "vm/swap.h"
#include "lib/kernel/bitmap.h"
#include <stdio.h>


struct bitmap *swap_table;
struct block *swap_disk;
struct lock swap_lock;



void
swap_init()
{
    lock_init(&swap_lock);
    swap_disk = block_get_role(BLOCK_SWAP);
    size_t swap_disk_size = block_size(swap_disk);
    swap_table = bitmap_create(swap_disk_size);
}


void 
swap_out(void *frame)
{
    /* Not yet impelemented */
}

void
swap_in(block_sector_t swap_location,void *frame)
{
    /* Not yet implemented */
}