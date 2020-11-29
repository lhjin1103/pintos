#include "lib/kernel/hash.h"
#include "devices/block.h"
#include "lib/kernel/list.h"
#include "threads/synch.h"


enum page_status
{
    SWAP_DISK,
    MEMORY,
    FILE
};

enum file_status
{
    MMAP_FILE,
    EXEC_FILE,
    NOT_FILE
};

struct spte
{
    struct hash_elem elem;

    struct list_elem listelem;

    enum page_status state;
    enum file_status file_state;
    void *upage;

    block_sector_t swap_location;

    bool writable;

    struct file *file;
    int read_bytes;
    int zero_bytes;
    int offset;

    struct lock spte_lock;
};


void spt_init(struct hash *spt);
void spte_update(struct spte *spte, block_sector_t swap_location);
struct spte *spte_create(enum page_status state, void *upage, block_sector_t swap_location);
void spte_destroy(struct spte *spte);
struct spte *spte_from_addr(void *addr);
