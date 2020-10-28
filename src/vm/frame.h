#include "lib/kernel/list.h"
#include "threads/thread.h"
#include "vm/page.h"

struct fte
{
    void *frame;
    struct spte *spte;
    struct thread *thread;
    struct list_elem elem;
};


struct fte *frame_alloc(enum palloc_flags flags, struct spte *spte);
void * frame_destroy(struct fte *fte);
/*
find_victim();

create_fte();
frame_table_update();

destroy_frame_table();
*/