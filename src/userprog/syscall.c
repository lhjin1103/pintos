#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
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

static void syscall_handler (struct intr_frame *);


void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static int syscall_exit(void *esp);
static int syscall_exec(void *esp);
static int syscall_wait(void *esp);
static bool syscall_create(void *esp);
static bool syscall_remove(void *esp);
static int syscall_open(void *esp);
static int syscall_filesize(void *esp);
static int syscall_read(void *esp);
static int syscall_write(void *esp);
static void syscall_seek(void *esp);
static void syscall_tell(void *esp);
static void syscall_close(void *esp);

int allocate_fd(struct list *fd_list);
struct file* file_from_fd(int fd);
//static void check_valid_pointer(void *p);

static void
syscall_handler (struct intr_frame *f) 
{
  void *esp = f -> esp;

  int syscall_no = *(int *) esp;
  switch (syscall_no){
    case 0:   //SYS_HALT
      shutdown_power_off();
      break;
    case 1:   //SYS_EXIT
      {
      int return_val = syscall_exit(esp);
      f -> eax = return_val;
      break;
      }
    case 2:   //SYS_EXEC
    {
      tid_t return_val = syscall_exec(esp);
      f -> eax = return_val;
      break;
    }
    case 3:   //SYS_WAIT
    {
      tid_t return_val = syscall_wait(esp);
      f -> eax = return_val;
      break;
    }
    case 4:   //SYS_CREATE
    {
      bool return_val = syscall_create(esp);
      f -> eax = return_val;
      break;
    }
    case 5:   //SYS_REMOVE
    {
      bool return_val = syscall_remove(esp);
      f -> eax = return_val;
      break;
    }
    case 6:   //SYS_OPEN
    {
      int return_val = syscall_open(esp);
      f -> eax = return_val;
      break;
    }
    case 7:   //SYS_FILESIZE
    {
      int return_val = syscall_filesize(esp);
      f -> eax = return_val;
      break;
    }
    case 8:   //SYS_READ
    {
      int return_val = syscall_read(esp);
      f -> eax = return_val;
      break;
    }
    case 9:   //SYS_WRITE
    {
      int return_val = syscall_write(esp);
      f -> eax = return_val;
      break;
    }
    case 10:  //SYS_SEEK
      syscall_seek(esp);
      break;
    case 11:  //SYS_TELL
      syscall_tell(esp);
      break;
    case 12:  //SYS_CLOSE
      syscall_close(esp);
      break;
  }

  //printf ("system call!\n");

  //thread_exit ();
}

static int
syscall_exit(void *esp)
{
  //void *arg0 = esp+4;
  //check_valid_pointer(arg0);
  int status = *(int *) (esp + 4);
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
syscall_exec(void *esp)
{
  const char *cmd_line = *(char **) (esp+4);
  tid_t tid = process_execute(cmd_line);
  struct thread *child = get_from_tid(tid);
  sema_down(&(child -> load_sema));
  if (child->load_success) return tid;
  else return TID_ERROR;
}

static int 
syscall_wait(void *esp)
{
  tid_t tid = *(tid_t *) (esp+4);
  int status = process_wait(tid);
  return status;
}

static bool 
syscall_create(void *esp)
{
  const char *file = *(char **) (esp+4);
  if (file==NULL) return -1;
  unsigned initial_size = *(unsigned *) (esp+8);
  return filesys_create(file, initial_size);
}

static bool 
syscall_remove(void *esp)
{
  const char *file = *(char **) (esp+4);
  return filesys_remove(file);
}

static int 
syscall_open(void *esp)
{
  const char *filename = *(char **) (esp+4);
  if (filename==NULL) return -1;
  if (filename[0]=='\0') return -1;
  struct file *opened_file = filesys_open(filename);
  if (opened_file == NULL) return -1;
  else{
    struct fd_struct new_fd;
    new_fd.fd = allocate_fd(&(thread_current()->fd_list));
    new_fd.file = opened_file;
    list_push_back(&(thread_current()->fd_list), &(new_fd.fileelem));
    return new_fd.fd;
  }

}

static int 
syscall_filesize(void *esp UNUSED)
{
  int fd = *(int *) (esp+4);
  struct file *f = file_from_fd(fd);
  if (f==NULL) return -1;
  return file_length(f);
}

static int 
syscall_read(void *esp UNUSED)
{
  int fd = *(int *) (esp + 4);
  void *buffer = *(void **) (esp + 8);
  unsigned int size = *(unsigned int *) (esp + 12);
  if (fd==0)
  {
    return input_getc();
  }
  else if (fd==1) return -1;
  else{
    struct file *f = file_from_fd(fd);
    if (f==NULL) return -1;
    return file_read(f, buffer, size);
  }
}

static int 
syscall_write(void *esp)
{
  int fd = *(int *) (esp + 4);
  const void *buffer = *(void **) (esp + 8);
  unsigned int size = *(unsigned int *) (esp + 12);

  if (fd == 1)
  {
    putbuf((char *) buffer, size);
    return size;
  } 
  else if (fd==0) return -1;
  else
  {
  struct file *f = file_from_fd(fd);
  if (f==NULL) return -1;
  return file_write(f, buffer, size);
  }
}
static void 
syscall_seek(void *esp)
{
  int fd = *(int *) (esp + 4);
  unsigned position = *(unsigned *) (esp + 8); 
  struct file *f = file_from_fd(fd);
  //if (f==NULL) return -1;
  file_seek(f, position);
}

static void 
syscall_tell(void *esp UNUSED)
{
  int fd = *(int *) (esp + 4);
  struct file *f = file_from_fd(fd);
  file_tell(f);
}

static void
syscall_close(void *esp UNUSED)
{
  int fd = *(int *) (esp + 4);
  struct list_elem *e;
  struct list *fd_list = &(thread_current()->fd_list);
  for (e = list_begin(fd_list); e != list_end(fd_list); e = list_next(e))
  {
    struct fd_struct *f = list_entry(e, struct fd_struct, fileelem);
    if (f->fd == fd) {
      list_remove(e);
      break;
      }
  }
}

/*
static void
check_valid_pointer(void *p)
{
  bool user = is_user_vaddr(p);
  if (!user) exit(-1);
  void *addr = lookup_page(thread_current()->pagedir, p, false);
  if (addr == NULL) exit(-1);
}
*/

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