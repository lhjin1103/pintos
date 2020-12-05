#include "filesys/buffer-cache.h"
#include "lib/kernel/list.h"
#include "threads/malloc.h"
#include "devices/block.h"
#include "threads/synch.h"


#define BCACHE_SECTORS 64

void *bcache_pointer;
struct list bcache_table;
//struct lock bcache_table_lock;
struct list_elem *clock_hand; //or int clock_hand (index)

int empty_bcache = 64;

/* Block device that contains the file system. */
extern struct block *fs_device;

static struct bte *bcache_find_victim();
static struct bte *bcache_find();

void
bcache_init()
{
    bcache_pointer = malloc(BLOCK_SECTOR_SIZE * BCACHE_SECTORS);
    list_init(&bcache_table);
    //lock_init(&bcache_table_lock);
}

void
bcache_read(block_sector_t sector, void *user_buffer, unsigned offset, int read_bytes)
{
    /* read the cached data if it is cached. 
       If not, cache the data and read. */
    struct bte *bte = bcache_find();
    if (!bte)
    {
        //lock_acquire(&bcache_table_lock);
        if (empty_bcache > 0)
        {
            /* There is an empty space in buffer cache. */
            /*put file into buffer cache*/

            empty_bcache --;
        }
        else
        {
            bte = bcache_find_victim();
            /* read into victim bte */
        }
    }
    /* copy the sector into the user buffer */
    /* update bits */
}

void
bcache_write(block_sector_t sector, void *user_buffer, unsigned offset, int write_bytes)
{
    /* write on the cached data if it is cached. 
       If not, cache the data and write. */
    struct bte *bte = bcache_find();
    if (!bte)
    {
        //lock_acquire(&bcache_table_lock);
        if (empty_bcache > 0)
        {
            /* There is an empty space in buffer cache. */
            /*put file into buffer cache*/

            empty_bcache --;
        }
        else
        {
            bte = bcache_find_victim();
            /* write into victim bte */
            
        }
    }
    /* copy the sector into the user buffer */
    /* update bits */
}

void
bcache_destroy()
{
    /* iterate over the buffe cache table. (bcache_table)
        flush to disk if ditry bit is 1
        free bte */
    /* free the allocated buffer cache */
}

static struct bte *
bcache_find()
{
    /* find the file sector in the buffer cache.
        returns NULL is not cached.*/
    if in bcache_table:
        if (bte -> clock_bit == 0) bte ->clock_bit = 1;
        return bte;
    else return NULL;
}

static struct bte *
bcache_find_victim()
{
    /* find victim, returns struct bte */
    //clock algorithm
    struct bte *bte = list_entry(clock_hand,,);
    if (bte -> clock_bit == 1)
        bte -> clock_bit = 0;
        if (list_tail(&bcache_table) == clock_hand) clock_hand = list_head(&hcache_table);
        else clock_hand = list_next(clock_hand);

    if (bte -> dirty) bcache_flush();
}