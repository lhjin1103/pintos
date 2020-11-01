#include "vm/frame.h"
#include "threads/malloc.h"
#include "vm/swap.h"
#include "threads/synch.h"
#include "lib/kernel/list.h"
#include <stdio.h>
#include "userprog/pagedir.h"

struct fte *find_victim(void);
void frame_table_update(struct fte *fte, struct spte *spte, struct thread *t);
struct fte * create_fte(void *vaddr, struct spte *spte);
static void pte_clear(struct fte *fte);

struct list frame_table;
struct lock frame_table_lock;

void
frame_init()
{
    list_init (&frame_table);
    lock_init (&frame_table_lock);
}

struct fte *
frame_alloc(enum palloc_flags flags, struct spte *spte)
{
    struct fte *fte;
    void *vaddr = palloc_get_page(flags);
    
    if (!vaddr) 
    {
        /* No empty page in user pool. Need swapping. */
        lock_acquire(&frame_table_lock);
        fte = find_victim();
        vaddr = fte -> frame;
        ASSERT(vaddr != NULL);

        /*clear victim fte. */
        block_sector_t swap_location = swap_out(vaddr);
        spte_update(fte->spte, swap_location);
        pte_clear(fte);
        
        /*update fte with current process. */
        frame_table_update(fte, spte, thread_current());
        lock_release(&frame_table_lock);
    }
    else 
    {   
        /* There is an empty page in user pool */
        fte = create_fte(vaddr, spte);  
    }
    
    //spte -> state = MEMORY; this is done in spte_create
    return fte;
}

void
frame_destroy(struct fte *fte)
{
    lock_acquire(&frame_table_lock);
    list_remove(&(fte->elem));
    free(fte);
    lock_release(&frame_table_lock);
}

struct fte *
find_victim()
{
    /* Currently implemented in FIFO.
       Need to change to a better algorithm. */
    struct list_elem *evict_elem = list_pop_back(&frame_table);
    list_push_front(&frame_table, evict_elem);    
    return list_entry (evict_elem, struct fte, elem);
}

void 
frame_table_update(struct fte *fte, struct spte *spte , struct thread *t)
{
    //pagedir_clear_page(t->pagedir, fte->spte->upage);
    fte->spte = spte;
    fte->thread = t;
}

struct fte *
create_fte(void *addr, struct spte *spte)
{
    struct fte *new_fte;
    new_fte = malloc(sizeof(struct fte));
    new_fte -> frame = addr;
    new_fte -> spte = spte;
    new_fte -> thread = thread_current();
    lock_acquire(&frame_table_lock);
    list_push_back(&frame_table, &(new_fte -> elem));
    lock_release(&frame_table_lock);
    return new_fte;
}

struct fte *
fte_from_spte(struct list *frame_table, struct spte *spte)
{
    struct list_elem *e;
    for (e = list_begin(frame_table); e != list_end(frame_table); e = list_next(e))
    {
        struct fte *fte = list_entry(e, struct fte, elem);
        if (fte -> spte == spte) return fte;
    }
    return NULL;
}

static void
pte_clear(struct fte *fte){
    pagedir_clear_page(fte -> thread -> pagedir, fte -> spte -> upage);
}