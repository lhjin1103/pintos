#include "vm/page.h"
#include "lib/kernel/list.h"
#include "threads/thread.h"

struct fte
{
    void *frame;
    struct spte *spte;
    struct thread *thread;
    struct list_elem elem;
}