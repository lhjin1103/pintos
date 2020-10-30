#include "lib/kernel/hash.h"
#include "devices/block.h"


enum page_status
{
    SWAP_DISK,
    MEMORY,
    EXEC_FILE
};

struct spte
{
    struct hash_elem elem;

    enum page_status state;
    void *upage;

    block_sector_t swap_location;

    bool writable;
};


void spt_init(struct hash *spt);
void spte_update(struct spte *spte, block_sector_t swap_location);
struct spte *spte_create(enum page_status state, void *upage, block_sector_t swap_location);
void spte_destroy(struct spte *spte);
struct spte *spte_from_addr(void *addr);