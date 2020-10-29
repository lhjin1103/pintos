#include "vm/page.h"
#include "threads/malloc.h"
#include "threads/thread.h"
#include <stdio.h>
//#include "vm/frame.h"


unsigned hashing_func(const struct hash_elem *e, void *aux UNUSED);
bool less_func(const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED);

hash_hash_func hashing_func;
hash_less_func less_func;

struct hash *
spt_init()
{
    struct hash *hash_table;
    hash_table = malloc(sizeof(struct hash));
    if (hash_table != NULL) hash_init(hash_table, hashing_func, less_func, NULL);   
    return hash_table;
}


void 
spte_update(struct spte *spte UNUSED)
{
    /* Not yet implemented */
};


struct spte *
spte_create(enum page_status state, void *upage, block_sector_t swap_location)
{
    struct spte *new_spte;
    new_spte = malloc(sizeof(struct spte));
    if (new_spte != NULL)
    {
        new_spte -> state = state;
        new_spte -> upage = upage;
        new_spte -> swap_location = swap_location;
        hash_insert(thread_current() -> spt, &(new_spte -> elem));
    }
    return new_spte;
}

void
spte_destroy(struct spte *spte)
{
    free(spte);
}


unsigned
hashing_func(const struct hash_elem *e, void *aux UNUSED)
{
    struct spte *spte = hash_entry(e, struct spte, elem);
    int addr = (int) (spte->upage);
    return hash_int(addr);
}


bool
less_func(const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED)
{
    struct spte *spte_a = hash_entry(a, struct spte, elem);
    void *uaddr_a = spte_a -> upage;
    
    struct spte *spte_b = hash_entry(b, struct spte, elem);
    void *uaddr_b = spte_b -> upage;

    return uaddr_a < uaddr_b;
}
