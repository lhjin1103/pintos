#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "userprog/process.h"

static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void syscall_write(void *esp);
static void syscall_exit(void *esp);


static void
syscall_handler (struct intr_frame *f) 
{
  void *esp = f -> esp;

  int syscall_no = *(int *) esp;
  /*
  int arg_0 = *(int *) (esp + 4);
  int arg_1 = *(int *) (esp + 8);
  int arg_2 = *(int *) (esp + 12);
  
  printf("%d\n", syscall_no);
  printf("%d\n", arg_0);
  printf("%x\n", arg_1);
  printf("%d\n", arg_2);
  */
  switch (syscall_no){
    case 0:   //SYS_HALT
      break;
    case 1:   //SYS_EXIT
      syscall_exit(esp);
      break;
    case 2:   //SYS_EXEC
      break;
    case 3:   //SYS_WAIT
      break;
    case 4:   //SYS_CREATE
      break;
    case 5:   //SYS_REMOVE
      break;
    case 6:   //SYS_OPEN
      break;
    case 7:   //SYS_FILESIZE
      break;
    case 8:   //SYS_READ
      break;
    case 9:   //SYS_WRITE
      syscall_write(esp);
      break;
    case 10:  //SYS_SEEK
      break;
    case 11:  //SYS_TELL
      break;
    case 12:  //SYS_CLOSE
      break;
  }

  printf ("system call!\n");

  thread_exit ();
}



static void
syscall_write(void *esp)
{
  int fd = *(int *) (esp + 4);
  const void *buffer = *(void **) (esp + 8);
  unsigned int size = *(unsigned int *) (esp + 12);

  if (fd == 1)
    putbuf(buffer, size);
  else
    {
      // one day we will make a file descriptor table..
    }
  
}

static void
syscall_exit(void *esp)
{
  int status = *(int *) (esp + 4);
  process_exit();
  //how to return the status?

}