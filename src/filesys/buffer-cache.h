#include "lib/kernel/list.h"
#include "devices/block.h"

void *bcache_pointer;

void bcache_init(void);

struct bte
{
    bool dirty;
    int clock_bit;
    block_sector_t disk_sector;
    void *file_sector_pointer;
};