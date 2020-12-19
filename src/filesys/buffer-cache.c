#include "filesys/buffer-cache.h"
#include "lib/kernel/list.h"
#include "threads/malloc.h"
#include "devices/block.h"

#include "threads/thread.h"
#include "devices/timer.h"

#include <stdio.h>
#include <string.h>


#define BCACHE_SECTORS 64
#define WRITE_BEHIND_TICKS 120

void *bcache_pointer;
struct list bcache_table;
struct lock bcache_table_lock;

//struct bte bcache_table[BCACHE_SECTORS];

struct list_elem *clock_hand; //or int clock_hand (index)

struct semaphore bcache_sema;

int empty_bcache = 64;

/* Block device that contains the file system. */
extern struct block *fs_device;

static struct bte *bcache_find_victim(void);
struct bte *bcache_find(block_sector_t sector);
static void bcache_flush(struct bte *bte);
void async_write_behind(void *aux UNUSED);
void async_read_ahead(void *aux);

void
bcache_init()
{
    //bcache_pointer = malloc(BLOCK_SECTOR_SIZE * BCACHE_SECTORS);
    list_init(&bcache_table);
    sema_init(&bcache_sema, BCACHE_SECTORS);
    lock_init(&bcache_table_lock);


    /*used for asynchronous write behind*/ 
    thread_create("write_behind", PRI_MIN, async_write_behind, NULL);
    //thread_create("read_ahead", PRI_MIN, async_read_ahead, arg);
}

void
bcache_read(block_sector_t sector, void *user_buffer, unsigned offset, int read_bytes)
{
    //printf("read: %d\n", sector);
    struct bte *bte;
    bte = bcache_find(sector);
    //lock_acquire(&(bte -> bte_lock));
    if (!bte){
        if (sema_try_down(&bcache_sema))
        {
            void *addr = malloc(BLOCK_SECTOR_SIZE);
            bte = malloc(sizeof(struct bte));
            lock_init(&(bte -> bte_lock));
            lock_acquire(&(bte -> bte_lock));
            lock_acquire(&bcache_table_lock);
            if (list_empty(&bcache_table)) 
            {
                list_push_back(&bcache_table, &(bte->elem));
                clock_hand = &(bte -> elem);
            }
            else 
            {
                //list_push_back(&bcache_table, &(bte->elem));
                list_insert(clock_hand, &(bte -> elem));
            }
            lock_release(&bcache_table_lock);
            bte -> block_pointer = addr;
        }
        else
        {
            bte = bcache_find_victim();
            //lock_acquire(&(bte -> bte_lock));
        }
        block_read(fs_device, sector, bte -> block_pointer);
        bte -> disk_sector = sector;
        bte -> dirty = false;
    }
    //block_read(fs_device, sector, bte -> block_pointer);
    memcpy(user_buffer, bte -> block_pointer + offset, read_bytes);
    bte -> clock_bit = 1;
    lock_release(&(bte -> bte_lock));
    //read_ahead
    // block_sector_t *arg = malloc(BLOCK_SECTOR_SIZE);
    // *arg = sector + 1;  // next block
    // thread_create("read_ahead", PRI_MIN, async_read_ahead, arg);
}


void
bcache_write(block_sector_t sector, void *user_buffer, unsigned offset, int write_bytes)
{

    struct bte *bte = bcache_find(sector);
    //lock_acquire(&(bte -> bte_lock));
    if (!bte){
        if (sema_try_down(&bcache_sema))
        {
            void *addr = malloc(BLOCK_SECTOR_SIZE);
            bte = malloc(sizeof(struct bte));
            lock_init(&(bte -> bte_lock));
            lock_acquire(&(bte -> bte_lock));
            lock_acquire(&bcache_table_lock);
            if (list_empty(&bcache_table)) 
            {
                list_push_back(&bcache_table, &(bte->elem));
                clock_hand = &(bte -> elem);
            }
            else list_insert(clock_hand, &(bte -> elem));
            bte -> block_pointer = addr;
            lock_release(&bcache_table_lock);
        }
        else
        {
            bte = bcache_find_victim();  
            //lock_acquire(&(bte->bte_lock));
            
        }
        block_read(fs_device, sector, bte -> block_pointer);
        bte -> disk_sector = sector;
    }
    //block_read(fs_device, sector, bte -> block_pointer);
    memcpy(bte -> block_pointer + offset, user_buffer, write_bytes);
    bte -> clock_bit = 1;
    bte -> dirty = true;
    lock_release(&(bte->bte_lock));
}
static void
bcache_flush(struct bte *bte)
{
    /*flush dirty bte back to the disk memory*/
    //lock_acquire(&(bte -> bte_lock));
    block_write(fs_device, bte ->disk_sector, bte -> block_pointer);
    bte -> dirty = false;
    //lock_release(&(bte -> bte_lock));
    //sema_up(&bcache_sema);
    
}

void
bcache_clean(struct bte *bte)
{
    lock_acquire(&bcache_table_lock);
    list_remove(&(bte->elem));
    if (bte -> dirty) bcache_flush(bte);
    if (clock_hand == &(bte->elem)) 
    {

        if (list_back(&bcache_table) == clock_hand) clock_hand = list_front(&bcache_table);
        else clock_hand = list_next(clock_hand);
    }
    free(bte -> block_pointer);
    sema_up(&bcache_sema);
    free(bte);
    lock_release(&bcache_table_lock);
}

void
bcache_destroy(void)
{
    /* iterate over the buffer cache table. (bcache_table)
        flush to disk if ditry bit is 1
        free bte */
    //printf("destroy start\n");
    struct list_elem *e;
    struct bte *bte;
    lock_acquire(&bcache_table_lock);
    while (!list_empty(&bcache_table))
    {
        e = list_pop_front(&bcache_table);
        bte = list_entry(e, struct bte, elem);
        if (bte -> dirty) bcache_flush(bte);
        sema_up(&bcache_sema);
        free(bte -> block_pointer);
        free(bte);
    }
    lock_release(&bcache_table_lock);
}

struct bte *
bcache_find(block_sector_t sector)
{
    /* find the file sector in the buffer cache.
        returns NULL if not cached.*/
    struct list_elem *e;
    struct bte *bte;
    lock_acquire(&bcache_table_lock);
    for (e = list_begin (&bcache_table); e != list_end (&bcache_table); e = list_next (e)){
        bte = list_entry(e, struct bte, elem);
        if (bte -> disk_sector == sector) {
            lock_release(&bcache_table_lock);
            lock_acquire(&(bte->bte_lock));
            return bte;
        }
    }
    lock_release(&bcache_table_lock);
    return NULL;
}

static struct bte *
bcache_find_victim(void)
{
    /* find victim, returns struct bte */
    //clock algorithm
    lock_acquire(&bcache_table_lock);
    struct bte *bte = list_entry(clock_hand, struct bte, elem);
    while (true){
        if (lock_try_acquire(&(bte->bte_lock)))
        {
            if (bte -> clock_bit == 1)
            {
                bte -> clock_bit = 0;
                lock_release(&(bte->bte_lock));
            }
            else
            {
                goto done;
            }
        }
        if (list_back(&bcache_table) == clock_hand) clock_hand = list_front(&bcache_table);
        else clock_hand = list_next(clock_hand);
        bte = list_entry(clock_hand ,struct bte, elem);
        //lock_release(&(bte -> bte_lock));
    }

    done:
    if (list_back(&bcache_table) == clock_hand) clock_hand = list_front(&bcache_table);
    else clock_hand = list_next(clock_hand);
    if (bte -> dirty) bcache_flush(bte);
    lock_release(&bcache_table_lock);
    return bte;
}

void
write_behind(void)
{
    lock_acquire(&bcache_table_lock);
    struct list_elem *e;
    struct bte *bte;

    for (e = list_begin (&bcache_table); e != list_end (&bcache_table); e = list_next (e)){
        bte = list_entry(e, struct bte, elem);
        if (lock_try_acquire(&(bte->bte_lock)))
        {
            if (bte -> dirty) bcache_flush(bte);
            lock_release(&(bte->bte_lock));
        }
    }
    lock_release(&bcache_table_lock);
}

void
bcache_find_and_clean(block_sector_t sector_idx)
{
    /* find the file sector in the buffer cache.
        returns NULL if not cached.*/
    struct list_elem *e;
    struct bte *bte;
    lock_acquire(&bcache_table_lock);
    for (e = list_begin (&bcache_table); e != list_end (&bcache_table); e = list_next (e)){
        bte = list_entry(e, struct bte, elem);
        if (bte -> disk_sector == sector_idx) {
            lock_acquire(&(bte->bte_lock));
            list_remove(&(bte->elem));
            if (bte -> dirty) bcache_flush(bte);
            if (clock_hand == &(bte->elem)) 
            {
                if (list_back(&bcache_table) == clock_hand) clock_hand = list_front(&bcache_table);
                else clock_hand = list_next(clock_hand);
            }
            free(bte -> block_pointer);
            lock_release(&(bte->bte_lock));
            sema_up(&bcache_sema);
            free(bte);
            break;            
        }
    }
    lock_release(&bcache_table_lock);  

}

void
async_write_behind(void *aux UNUSED)
{
    while(true)
    {
        //printf("write_behind \n");
        timer_sleep(WRITE_BEHIND_TICKS);
        write_behind();
    }
}

void
async_read_ahead(void *aux){
    block_sector_t sector = *(block_sector_t *)aux;
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
    free(aux);
}