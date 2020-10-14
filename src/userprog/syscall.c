#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "userprog/process.h"
#include "devices/shutdown.h"
#include "threads/synch.h"

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
      syscall_create(esp);
      break;
    case 5:   //SYS_REMOVE
      syscall_remove(esp);
      break;
    case 6:   //SYS_OPEN
      syscall_open(esp);
      break;
    case 7:   //SYS_FILESIZE
      syscall_filesize(esp);
      break;
    case 8:   //SYS_READ
      syscall_read(esp);
      break;
    case 9:   //SYS_WRITE
      syscall_write(esp);
      break;
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
  int status = *(int *) (esp + 4);
  thread_current()->exit_status = status;
  printf("%s: exit(%d)\n", thread_current()->name, status);
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
syscall_wait(void *esp UNUSED)
{
  tid_t tid = *(tid_t *) (esp+4);
  int status = process_wait(tid);
  return status;
}

static bool 
syscall_create(void *esp UNUSED)
{
  return 0;
}

static bool 
syscall_remove(void *esp UNUSED)
{
  return 0;
}

static int 
syscall_open(void *esp UNUSED)
{
  return 0;
}

static int 
syscall_filesize(void *esp UNUSED)
{
  return 0;
}

static int 
syscall_read(void *esp UNUSED)
{
  return 0;
}

static int 
syscall_write(void *esp)
{
  int fd = *(int *) (esp + 4);
  const void *buffer = *(void **) (esp + 8);
  unsigned int size = *(unsigned int *) (esp + 12);

  if (fd == 1)
  {
    putbuf(buffer, size);
    return 0;
  } 
  else
  {
    // one day we will make a file descriptor table..
    return 0;
  }
}
static void 
syscall_seek(void *esp UNUSED)
{

}

static void 
syscall_tell(void *esp UNUSED)
{

}

static void
syscall_close(void *esp UNUSED)
{

}
