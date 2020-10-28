#include "lib/kernel/hash.h"
#include "devices/block.h"


enum page_status
{
    SWAP_DISK,
    MEMORRY,
    EXEC_FILE
}

struct spte
{
    struct hash_elem elem;

    enum page_status state;
    void *upage;

    block_sector_t swap_location;

}