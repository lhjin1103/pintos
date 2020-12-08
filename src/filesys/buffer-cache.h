#include "lib/kernel/list.h"
#include "devices/block.h"

void *bcache_pointer;

void bcache_init(void);

struct bte
{
    struct inode *inode; 
    bool dirty;
    bool occupied;
    int clock_bit;
    block_sector_t disk_sector;
    void *block_pointer;
    struct list_elem elem;
};

void bcache_read(block_sector_t sector, void *user_buffer, unsigned offset, int read_bytes);
