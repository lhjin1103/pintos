#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include <string.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "userprog/process.h"
#include "devices/shutdown.h"
#include "threads/synch.h"
#include "threads/palloc.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "lib/kernel/list.h"
#include "devices/input.h"
#include "userprog/pagedir.h"
#include "threads/vaddr.h"
#include "threads/malloc.h"
//#include "vm/page.h"
#include "vm/frame.h"
#include "vm/swap.h"

static void syscall_handler (struct intr_frame *);

#define STACK_HEURISTIC 32
typedef int mapid_t;

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

int syscall_exit(int status);
static int syscall_exec(char* cmd_line);
static int syscall_wait(tid_t tid);
static bool syscall_create(char *file, unsigned initial_size);
static bool syscall_remove(char *file);
static int syscall_open(char *filename);
static int syscall_filesize(int fd);
static int syscall_read(int fd, void *buffer, unsigned int size);
static int syscall_write(int fd, void *buffer, unsigned int size);
static void syscall_seek(int fd, unsigned position);
static void syscall_tell(int fd);
static void syscall_close(int fd);
static mapid_t syscall_mmap(int fd, void *addr);
static void syscall_munmap(mapid_t mapid);

int allocate_fd(struct list *fd_list);
struct file* file_from_fd(int fd);
static void check_valid_pointer(void *p);
static void check_writable_pointer(void *p);
static void check_buffer(void *buffer, unsigned int size, void *esp);


struct mte
{
  struct file *file;
  struct list_elem elem;
  mapid_t mapid;
  struct list spte_list;
};

static struct mte * create_mte(mapid_t mapid);
static mapid_t new_mapid(void);
static void clear_mte(struct mte *mte);

static void
syscall_handler (struct intr_frame *f) 
{
  void *esp = f -> esp;
  thread_current() -> esp = esp;
  
  check_valid_pointer(esp);

  int syscall_no = *(int *) esp;
  switch (syscall_no){
    case 0:   //SYS_HALT
      shutdown_power_off();
      break;
    case 1:   //SYS_EXIT
    {
      check_valid_pointer(esp+4);
      int status = *(int *) (esp + 4);
      int return_val = syscall_exit(status);
      f -> eax = return_val;
      break;
    }
    case 2:   //SYS_EXEC
    {
      check_valid_pointer(esp+4);
      const char *cmd_line = *(char **) (esp+4);
      
      check_valid_pointer(cmd_line);

      tid_t return_val = syscall_exec(cmd_line);
      f -> eax = return_val;
      break;
    }
    case 3:   //SYS_WAIT
    {
      check_valid_pointer(esp+4);
      tid_t tid = *(tid_t *) (esp+4);
      tid_t return_val = syscall_wait(tid);
      f -> eax = return_val;
      break;
    }
    case 4:   //SYS_CREATE
    {
      check_valid_pointer(esp+8);
      char *file = *(char **) (esp+4);
      check_valid_pointer(file);
      unsigned initial_size = *(unsigned *) (esp+8);

      bool return_val = syscall_create(file, initial_size);
      f -> eax = return_val;
      break;
    }
    case 5:   //SYS_REMOVE
    {
      check_valid_pointer(esp+4);
      char *file = *(char **) (esp+4);

      bool return_val = syscall_remove(file);
      f -> eax = return_val;
      break;
    }
    case 6:   //SYS_OPEN
    {
      check_valid_pointer(esp+4);
      const char *filename = *(char **) (esp+4);
      check_valid_pointer(filename);

      int return_val = syscall_open(filename);
      f -> eax = return_val;
      break;
    }
    case 7:   //SYS_FILESIZE
    {
      check_valid_pointer(esp+4);
      int fd = *(int *) (esp+4);

      int return_val = syscall_filesize(fd);
      f -> eax = return_val;
      break;
    }
    case 8:   //SYS_READ
    {
      check_valid_pointer(esp+12);
      int fd = *(int *) (esp + 4);
      void *buffer = *(void **) (esp + 8);
      //check_valid_pointer(buffer);
      unsigned int size = *(unsigned int *) (esp + 12);
      check_buffer(buffer, size, esp);
      check_writable_pointer(buffer);
      int return_val = syscall_read(fd, buffer, size);
      f -> eax = return_val;
      break;
    }
    case 9:   //SYS_WRITE
    {
      check_valid_pointer(esp+12);
      int fd = *(int *) (esp + 4);
      const void *buffer = *(void **) (esp + 8);
      //check_valid_pointer(buffer);
      unsigned int size = *(unsigned int *) (esp + 12);
      check_buffer(buffer, size, esp);
      int return_val = syscall_write(fd, buffer, size);
      f -> eax = return_val;
      break;
    }
    case 10:  //SYS_SEEK
    {
      check_valid_pointer(esp+8);
      int fd = *(int *) (esp + 4);
      unsigned position = *(unsigned *) (esp + 8); 

      syscall_seek(fd, position);
      break;
    }
    case 11:  //SYS_TELL
    {
      check_valid_pointer(esp+4);
      int fd = *(int *) (esp + 4);
      syscall_tell(fd);
      break;
    }
    case 12:  //SYS_CLOSE
    {
      check_valid_pointer(esp+4);
      int fd = *(int *) (esp + 4);
      syscall_close(fd);
      break;
    }
    case 13:  //SYS_MMAP
    {
      check_valid_pointer(esp+4);
      int fd = *(int *) (esp + 4);
      check_valid_pointer(esp+8);
      void *addr = *(void**) (esp + 8);
      mapid_t return_val = syscall_mmap(fd, addr);
      f -> eax = return_val;
      break;
    }
    case 14:  //SYS_MUNMAP
    {
      check_valid_pointer(esp+4);
      mapid_t mapid= *(int *) (esp + 4);
      syscall_munmap(mapid);
      break;
    }
  }

  //printf ("system call!\n");

  //thread_exit ();
}

int
syscall_exit(int status)
{
  struct list *map_table = &(thread_current() -> map_table);

  thread_current()->exit_status = status;
  printf("%s: exit(%d)\n", thread_current()->name, status);

  struct list_elem *e;
  struct list *child_list = &(thread_current() -> child_list);
  for (e = list_begin (child_list); e != list_end (child_list); e = list_next (e))
  {
    struct thread *c = list_entry (e, struct thread, childelem);
    if (c->status == THREAD_DYING) palloc_free_page(c);
    else c->parent = NULL;
  }

  /* Unmap every mapped files */
  
  while (!list_empty(map_table))
  { 
    struct mte *mte = list_entry(list_pop_front(map_table), struct mte, elem);
    clear_mte(mte);
  }
  
  thread_exit();
  return status;
}

static int
syscall_exec(char *cmd_line)
{
  tid_t tid = process_execute(cmd_line);
  struct thread *child = get_from_tid(tid);
  sema_down(&(child -> load_sema));

  if (child->load_success) return tid;
  else {
      struct list_elem *e;
      struct list *child_list = &(thread_current() -> child_list);
      for (e = list_begin (child_list); e != list_end (child_list); e = list_next (e))
      { 
        struct thread *c = list_entry (e, struct thread, childelem);
        if (c -> tid == tid) list_remove(e);
      }
  }return TID_ERROR;
}

static int 
syscall_wait(tid_t tid)
{
  int status = process_wait(tid);
  return status;
}

static bool 
syscall_create(char *file, unsigned initial_size)
{
  if (file[0]=='\0') return 0;
  lock_acquire(&file_lock);
  bool created = filesys_create(file, initial_size);
  lock_release(&file_lock);
  return created;
}

static bool 
syscall_remove(char *file)
{
  lock_acquire(&file_lock);
  bool removed = filesys_remove(file);
  lock_release(&file_lock);
  return removed;
}

static int 
syscall_open(char *filename)
{

  if (filename==NULL) return -1;
  if (filename[0]=='\0') return -1;

  lock_acquire(&file_lock);
  struct file *opened_file = filesys_open(filename);
  
  if (opened_file == NULL) {
    lock_release(&file_lock);
    return -1;
  }
  else{
    if (!strcmp(filename, thread_current()->name)) file_deny_write(opened_file);

    struct fd_struct* new_fd;

    new_fd = malloc(sizeof(struct fd_struct));
    new_fd->fd = allocate_fd(&(thread_current()->fd_list));
    new_fd->file = opened_file;
    list_push_back(&(thread_current()->fd_list), &(new_fd->fileelem));
    
    lock_release(&file_lock); //here..???
    return new_fd->fd;
  }
}

static int 
syscall_filesize(int fd)
{
  struct file *f = file_from_fd(fd);
  if (f==NULL) return -1;

  lock_acquire(&file_lock);
  int l = file_length(f);
  lock_release(&file_lock);
  
  return l;
}

static int 
syscall_read(int fd, void *buffer, unsigned int size)
{
  int real_size;
  lock_acquire(&file_lock);
  if (fd==0)
  {
    real_size = input_getc();
    lock_release(&file_lock);
    return real_size;
  }
  else if (fd==1) {
    lock_release(&file_lock);
    return -1;
  }
  else{
    struct file *f = file_from_fd(fd);
    if (f==NULL) {
      lock_release(&file_lock);
      return -1;
    }
    real_size = file_read(f, buffer, size);
    lock_release(&file_lock);
    return real_size;
  }
}

static int 
syscall_write(int fd, void *buffer, unsigned int size)
{
  lock_acquire(&file_lock);
  if (fd == 1)
  {
    putbuf((char *) buffer, size);
    lock_release(&file_lock);
    return size;
  } 
  else if (fd==0) {
    lock_release(&file_lock);
    return -1;
  }
  else
  {
    struct file *f = file_from_fd(fd);
    if (f==NULL) {
      lock_release(&file_lock);
      return -1;
    }
    int real_size = file_write(f, buffer, size);
    lock_release(&file_lock);
    return real_size;
    }
}

static void 
syscall_seek(int fd, unsigned position)
{
  struct file *f = file_from_fd(fd);
  lock_acquire(&file_lock);
  file_seek(f, position);
  lock_release(&file_lock);
}

static void 
syscall_tell(int fd)
{
  struct file *f = file_from_fd(fd);
  lock_acquire(&file_lock);
  file_tell(f);
  lock_release(&file_lock);
}

static void
syscall_close(int fd)
{
  struct list_elem *e;
  struct list *fd_list = &(thread_current()->fd_list);
  for (e = list_begin(fd_list); e != list_end(fd_list); e = list_next(e))
  {
    struct fd_struct *f = list_entry(e, struct fd_struct, fileelem);
    if (f->fd == fd) {
      list_remove(e);
      lock_acquire(&file_lock);
      file_close(f->file);
      lock_release(&file_lock);
      //free(f);
      break;
      }
  }
}

static mapid_t
syscall_mmap(int fd, void *addr)
{
  if (!addr) return -1;
  if (pg_round_down(addr) != addr) return -1;
  struct file *original_file = file_from_fd(fd);
  if (!original_file) return -1;
  
  lock_acquire(&file_lock);

  struct file *file = file_reopen(original_file);
  int l = file_length(file);

  lock_release(&file_lock);
  int offset = 0;

  int check_l = l;
  void *check_addr = addr;
  
  while (check_l>0)
  {
    if (spte_from_addr(check_addr)) return -1;
    check_l -= PGSIZE;
    check_addr += PGSIZE;
  }

  mapid_t mapid = new_mapid();
  struct mte *mte = create_mte(mapid);
  mte -> file = file;

  while (l>0)
  {
    int page_read_bytes = (l < PGSIZE)? l: PGSIZE;
    int page_zero_bytes = PGSIZE - page_read_bytes;
    struct spte *spte = spte_create(FILE, addr, 0);
    if (spte == NULL) return false;
    spte -> writable = true;
    spte -> file = file;
    spte -> read_bytes = page_read_bytes;
    spte -> zero_bytes = page_zero_bytes;
    spte -> offset = offset;
    spte -> file_state = MMAP_FILE;

    list_push_back(&(mte->spte_list), &(spte->listelem));

    l -= page_read_bytes;
    l -= page_zero_bytes;
    addr += PGSIZE;
    offset += page_read_bytes;
  }
  return mapid;
}
static void 
syscall_munmap(mapid_t mapid)
{
  struct list_elem *e;
  struct list *map_table = &(thread_current()->map_table);
  
  for (e = list_begin(map_table); e != list_end(map_table); e = list_next(e))
  {
    struct mte *mte = list_entry(e, struct mte, elem);
    
    if (mte -> mapid == mapid)
    {
      list_remove(e);
      clear_mte(mte);
      return;
    }
    
  }
  
}

static void
check_valid(void *p)
{
  if (p==NULL) syscall_exit(-1);
  bool user = is_user_vaddr(p);
  if (!user) syscall_exit(-1);
  struct spte *spte = spte_from_addr(p);
  if (!spte) syscall_exit(-1);
}

static void
check_valid_pointer(void *p)
{
  check_valid(p);
  check_valid(p+1);
  check_valid(p+2);
  check_valid(p+3);
}

static void
check_writable_pointer(void *p)
{
  struct spte *spte = spte_from_addr(p);
  if (spte && (! spte -> writable)) syscall_exit(-1);
}


int 
allocate_fd(struct list *fd_list)
{
  if (list_empty(fd_list)) return 2;
  else 
    return list_entry((list_back(fd_list)), struct fd_struct, fileelem) -> fd +1;
}

struct file*
file_from_fd(int fd)
{
  struct list_elem *e;
  struct list *fd_list = &(thread_current()->fd_list);
  for (e = list_begin(fd_list); e != list_end(fd_list); e = list_next(e))
  {
    struct fd_struct *f = list_entry(e, struct fd_struct, fileelem);
    if (f->fd == fd) return f->file;
  }
  return NULL;
}

static void load_from_swap_sc(struct spte *spte);
static void stack_growth_sc(void *addr);

static void
check_buffer(void *buffer, unsigned int size, void *esp)
{
  void *addr;
  
  if (buffer==NULL) syscall_exit(-1);
  bool user = is_user_vaddr(buffer);
  if (!user) syscall_exit(-1);

  void *upage = pg_round_down(buffer);
  for (addr = upage; addr < upage + size; addr += PGSIZE)
  {
    struct spte *spte = spte_from_addr(addr);
    if (spte)
    {
      if (spte -> state == MEMORY);
      else if (spte -> state == SWAP_DISK)
      {
        load_from_swap_sc(spte);
      }
    }
    else if (addr >= esp - PGSIZE)
    {
      stack_growth_sc(addr);
      esp -=PGSIZE;
    }
    else syscall_exit(-1);
  }
}

static void
load_from_swap_sc(struct spte* spte)
{
  struct fte *fte = frame_alloc(PAL_USER, spte);
  void *frame = fte -> frame;
  if (!install_page(spte->upage, frame, spte->writable)) {
    frame_destroy(fte);
  }
  swap_in(spte -> swap_location, frame);
  spte -> state = MEMORY;
}

static void
stack_growth_sc(void *addr)
{
  void *upage = pg_round_down(addr);
  struct spte *spte = spte_create(MEMORY, upage, 0);
  spte -> writable = true;
  struct fte *fte = frame_alloc(PAL_USER, spte);
  void *kpage = fte -> frame;
  if (!install_page(upage, kpage, true)) 
  {
      spte_destroy(spte);
      frame_destroy(fte);
  }
}

static struct mte *
create_mte(mapid_t mapid)
{
  struct mte *mte;
  mte = malloc(sizeof(struct mte));
  mte -> mapid = mapid;
  list_init(&(mte -> spte_list));
  list_push_back(&(thread_current() -> map_table), &(mte -> elem));
  return mte;
}

static mapid_t
new_mapid()
{
  struct list *map_table = &(thread_current() -> map_table);
  if (list_empty(map_table)) return 1;
  else return list_entry((list_back(map_table)), struct mte, elem) -> mapid +1;
}

static void
clear_mte(struct mte *mte)
{
  while (!list_empty(&(mte -> spte_list)))
  {
    struct spte *spte = list_entry(list_pop_front(&(mte -> spte_list)), struct spte, listelem);
    //list_remove(&(spte->elem));
    hash_delete(&(thread_current()->spt), &(spte->elem));
    if (spte -> state == MEMORY)
    {
      void *kpage = pagedir_get_page(thread_current()->pagedir, spte->upage);
      if (pagedir_is_dirty(thread_current()->pagedir, spte->upage))
      {
        lock_acquire(&file_lock);
        file_write_at(spte->file, kpage, spte -> read_bytes, spte->offset);
        lock_release(&file_lock);
      }
      struct fte *fte = fte_from_spte(spte);
      pagedir_clear_page(fte -> thread -> pagedir, fte -> spte -> upage);
      frame_destroy(fte);
    }
    
    spte_destroy(spte);
    
  }
  lock_acquire(&file_lock);
  file_close(mte -> file);
  lock_release(&file_lock);
  free(mte);
}