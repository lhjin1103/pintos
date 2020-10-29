#include "vm/frame.h"
#include "threads/malloc.h"
#include "vm/swap.h"
#include "threads/synch.h"

struct fte *find_victim(void);
void frame_table_update(struct fte *fte, struct spte *spte, struct thread *t);
struct fte * create_fte(void *vaddr, struct spte *spte);


struct fte *
frame_alloc(enum palloc_flags flags, struct spte *spte)
{
    lock_acquire(&frame_table_lock);
    void *vaddr = palloc_get_page(flags);
    if (vaddr == NULL) 
    {
        lock_release(&frame_table_lock);
        return NULL;
        /*
        struct fte *victim_fte = find_victim();
        vaddr = victim_fte -> frame;
        swap_out(vaddr);
        spte_update(victim_fte->spte);
        frame_table_update(victim_fte, spte, thread_current());
        */
    }
    else 
    {   
        struct fte *return_fte = create_fte(vaddr, spte);
        lock_release(&frame_table_lock);
        return return_fte;
    }
}

void*
frame_destroy(struct fte *fte)
{
    lock_acquire(&frame_table_lock);
    free(fte);
    palloc_free_page(fte -> frame);
    lock_release(&frame_table_lock);
}

struct fte *
find_victim()
{
    /* Not yet impelemented*/
}

void 
frame_table_update(struct fte *fte, struct spte *spte, struct thread *t)
{
    /*Not yet implemented */
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