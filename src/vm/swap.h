/* these are the ones I want to impelement in swap disk.

    struct bitmap *swap_table; 
    struct block *swap_disk;
    struct lock swap_lock;


    functions!

    swap_table_init() //swap_disk 사이즈만큼의 bitmap_create
    swap_out()
    swap_in()

    swap_disk = block_get_role(BLOCK_SWAP);


    I will fill these later!
*/

#include "devices/block.h"
#include "threads/synch.h"

struct bitmap *swap_table;
struct block *swap_disk;
struct lock swap_lock;


void swap_init(void);
block_sector_t swap_out(void *victim_frame);
void swap_in(block_sector_t swap_location,void *frame);
    

