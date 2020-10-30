#include "lib/kernel/list.h"
#include "threads/thread.h"
#include "vm/page.h"
#include "threads/palloc.h"

struct fte
{
    void *frame;
    struct spte *spte;
    struct thread *thread;
    struct list_elem elem;
};

struct list frame_table;
struct lock frame_table_lock;


void frame_init();
struct fte *frame_alloc(enum palloc_flags flags, struct spte *spte);
void frame_destroy(struct fte *fte);

struct fte * fte_from_spte(struct list *frame_table, struct spte *spte);
/*
find_victim();

create_fte();
frame_table_update();

destroy_frame_table();
*/