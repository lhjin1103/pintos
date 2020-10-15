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

static void syscall_handler (struct intr_frame *);


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

int allocate_fd(struct list *fd_list);
struct file* file_from_fd(int fd);
static void check_valid_pointer(void *p);

static void
syscall_handler (struct intr_frame *f) 
{
  void *esp = f -> esp;

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
      check_valid_pointer(buffer);
      unsigned int size = *(unsigned int *) (esp + 12);

      int return_val = syscall_read(fd, buffer, size);
      f -> eax = return_val;
      break;
    }
    case 9:   //SYS_WRITE
    {
      check_valid_pointer(esp+12);
      int fd = *(int *) (esp + 4);
      const void *buffer = *(void **) (esp + 8);
      check_valid_pointer(buffer);
      unsigned int size = *(unsigned int *) (esp + 12);

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
    default:
      syscall_exit(-1);
  }

  //printf ("system call!\n");

  //thread_exit ();
}

int
syscall_exit(int status)
{

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
      file_close(f->file);
      list_remove(e);
      free(f);
      break;
      }
  }
}

static void
check_valid_pointer(void *p)
{
  if (p==NULL) syscall_exit(-1);
  bool user = is_user_vaddr(p);
  if (!user) syscall_exit(-1);
  uint32_t *addr = lookup_page(thread_current()->pagedir, p, false);
  if (addr == NULL) syscall_exit(-1);
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