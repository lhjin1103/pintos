#include "lib/kernel/list.h"
#include "devices/block.h"
#include "threads/synch.h"

void *bcache_pointer;



struct bte
{
    //struct inode *inode; 
    bool dirty;
    //bool occupied;
    int clock_bit;
    block_sector_t disk_sector;
    void *block_pointer;
    struct list_elem elem;
    struct lock bte_lock;
};

void bcache_init(void);
void bcache_read(block_sector_t sector, void *user_buffer, unsigned offset, int read_bytes);
void bcache_write(block_sector_t sector, void *user_buffer, unsigned offset, int write_bytes);
struct bte *bcache_find(block_sector_t sector);
void bcache_clean(struct bte *bte);
void bcache_destroy(void);

void bcache_find_and_clean(block_sector_t sector_idx);