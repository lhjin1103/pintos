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


struct fte *
frame_alloc(struct list *frame_table, enum palloc_flags flags, struct spte *spte)
{
    lock_acquire(&frame_table_lock);
    void *vaddr = palloc_get_page(flags);
    if (vaddr == NULL) 
    {
        lock_release(&frame_table_lock);
        struct fte *victim_fte = find_victim();
        vaddr = victim_fte -> frame;
        swap_out(vaddr);
        spte_update(victim_fte->spte);
        frame_table_update(victim_fte, spte, thread_current());

        return victim_fte;
    }
    else 
    {   
        struct fte *return_fte = create_fte(vaddr, spte);
        list_push_back(frame_table, &(return_fte -> elem));
        lock_release(&frame_table_lock);
        return return_fte;
    }
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
frame_table_update(struct fte *fte UNUSED , struct spte *spte UNUSED, struct thread *t UNUSED)
{
    /* Not yet impelemented*/
    pagedir_clear_page(t->pagedir, fte->spte->upage);
    fte->spte = spte;
    fte->thread = thread_current();

}

struct fte *
create_fte(void *vaddr, struct spte *spte)
{
    struct fte *new_fte;
    new_fte = malloc(sizeof(struct fte));
    new_fte -> frame = vaddr;
    new_fte -> spte = spte;
    new_fte -> thread = thread_current();

    spte -> state = MEMORY;
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