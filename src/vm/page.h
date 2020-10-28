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
};


struct hash *spt_init();
void spte_update(struct spte *spte);
struct spte *spte_create(enum page_status state, void *upage, block_sector_t swap_location);
void spte_destroy(struct spte *spte);