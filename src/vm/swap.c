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


block_sector_t
swap_out(void *victim_frame)
{
    lock_acquire(&swap_lock);
    block_sector_t free_index = bitmap_scan_and_flip(swap_table, 0, 8, false); 
    if (free_index == BITMAP_ERROR) ASSERT("NO free index in swap disk");
    
    for (int i = 0; i < 8; i++)
    { 
        block_write(swap_disk, free_index + i, (uint8_t *)victim_frame + i * BLOCK_SECTOR_SIZE);
    }
    lock_release(&swap_lock);
    return free_index;
}

void
swap_in(block_sector_t swap_location,void *frame)
{
    lock_acquire(&swap_lock);

    for (int i = 0; i < 8; i++) 
    {        
        if (bitmap_test(swap_table, swap_location + i) == false) ASSERT ("Trying to swap in a free block.");
        bitmap_flip(swap_table, swap_location + i);
        block_read(swap_disk, swap_location + i, (uint8_t *) frame + i * BLOCK_SECTOR_SIZE);    
    }
    lock_release(&swap_lock);
}

void
swap_clear(block_sector_t swap_location)
{
    lock_acquire(&swap_lock);
    for (int i = 0; i < 8; i++){
        if (bitmap_test(swap_table, swap_location + i) == false) ASSERT ("Trying to clear a free block.");
        bitmap_flip(swap_table, swap_location + i); 
    }
    lock_release(&swap_lock);
}