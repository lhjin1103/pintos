#include "filesys/buffer-cache.h"
#include "lib/kernel/list.h"
#include "threads/malloc.h"
#include "devices/block.h"
#include "threads/synch.h"
#include "threads/thread.h"

#include <stdio.h>


#define BCACHE_SECTORS 64

void *bcache_pointer;
struct list bcache_table;
//struct lock bcache_table_lock;
struct list_elem *clock_hand; //or int clock_hand (index)

struct semaphore bcache_sema;

int empty_bcache = 64;

/* Block device that contains the file system. */
extern struct block *fs_device;

static struct bte *bcache_find_victim();
static struct bte *bcache_find();

void
bcache_init()
{
    //bcache_pointer = malloc(BLOCK_SECTOR_SIZE * BCACHE_SECTORS);
    list_init(&bcache_table);
    sema_init(&bcache_sema, 64);
    //lock_init(&bcache_table_lock);


    /*used for asynchronous write behind*/ 
    //thread_create("write_behind", PRI_MIN, async_write_behind, NULL);
}

void
bcache_read(block_sector_t sector, void *user_buffer, unsigned offset, int read_bytes)
{
    //printf("read: %d\n", sector);
    struct bte *bte = bcache_find(sector);
    if (!bte){
        if (sema_try_down(&bcache_sema))
        {
            void *addr = malloc(BLOCK_SECTOR_SIZE);
            bte = malloc(sizeof(struct bte));
            if (list_empty(&bcache_table)) 
            {
                list_push_back(&bcache_table, &(bte->elem));
                clock_hand = &(bte -> elem);
            }
            else list_insert(clock_hand, &(bte -> elem));
            bte -> block_pointer = addr;
        }
        else
        {
            bte = bcache_find_victim();
            
        }
        block_read(fs_device, sector, bte -> block_pointer);
        bte -> disk_sector = sector;
        bte -> dirty = false;
    }
    //block_read(fs_device, sector, bte -> block_pointer);
    memcpy(user_buffer, bte -> block_pointer + offset, read_bytes);
    bte -> clock_bit = 1;
}


void
bcache_write(block_sector_t sector, void *user_buffer, unsigned offset, int write_bytes)
{
    //printf("write: %d\n", sector);
    struct bte *bte = bcache_find(sector);
    if (!bte){
        if (sema_try_down(&bcache_sema))
        {
            void *addr = malloc(BLOCK_SECTOR_SIZE);
            bte = malloc(sizeof(struct bte));
            if (list_empty(&bcache_table)) 
            {
                list_push_back(&bcache_table, &(bte->elem));
                clock_hand = &(bte -> elem);
            }
            else list_insert(clock_hand, &(bte -> elem));
            bte -> block_pointer = addr;
        }
        else
        {
            bte = bcache_find_victim();
            
        }
        block_read(fs_device, sector, bte -> block_pointer);
        bte -> disk_sector = sector;
    }
    //block_read(fs_device, sector, bte -> block_pointer);
    memcpy(bte -> block_pointer + offset, user_buffer, write_bytes);
    bte -> clock_bit = 1;
    bte -> dirty = true;
}
void
bcache_flush(struct bte *bte)
{
    /*flush dirty bte back to the disk memory*/
    
    block_write(fs_device, bte ->disk_sector, bte -> block_pointer);
    bte -> dirty = false;
    
}

void
bcache_destroy()
{
    /* iterate over the buffer cache table. (bcache_table)
        flush to disk if ditry bit is 1
        free bte */
        struct list_elem *e;
        for (e = list_begin (&bcache_table); e != list_end (&bcache_table); e = list_next (e)){
            struct bte *bte = list_entry(e, struct bte, elem);
            if (bte -> dirty) bcache_flush(bte);
        }    

    /* free the allocated buffer cache */
    free(bcache_pointer);
}

static struct bte *
bcache_find(block_sector_t sector)
{
    /* find the file sector in the buffer cache.
        returns NULL if not cached.*/
    struct list_elem *e;
    struct bte *bte;
    for (e = list_begin (&bcache_table); e != list_end (&bcache_table); e = list_next (e)){
        bte = list_entry(e, struct bte, elem);
        if (bte -> disk_sector == sector) {
            return bte;
        }
    }
    return NULL;
}

static struct bte *
bcache_find_victim()
{
    /* find victim, returns struct bte */
    //clock algorithm
    struct bte *bte = list_entry(clock_hand, struct bte, elem);
    while (bte -> clock_bit == 1){
        bte -> clock_bit = 0;
        if (list_back(&bcache_table) == clock_hand) clock_hand = list_front(&bcache_table);
        else clock_hand = list_next(clock_hand);
        bte = list_entry(clock_hand ,struct bte, elem);
    }
    if (list_back(&bcache_table) == clock_hand) clock_hand = list_front(&bcache_table);
    else clock_hand = list_next(clock_hand);

    if (bte -> dirty) bcache_flush(bte);
    return bte;
}

void
write_behind()
{
    //lock_acquire(&&bcache_table_lock);
    struct list_elem *e;
    for (e = list_begin (&bcache_table); e != list_end (&bcache_table); e = list_next (e)){
        struct bte *bte = list_entry(e, struct bte, elem);
        if (bte -> dirty) bcache_flush(bte);
    } 
}
/*
void
async_write_behind(void *aux UNUSED)
{
    while(true)
    {
        timer_sleep(50); //??????? how long???
        write_behind(false);
    }
}
*/