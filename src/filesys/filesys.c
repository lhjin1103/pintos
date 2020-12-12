#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "filesys/buffer-cache.h"
#include "threads/thread.h"
#include "threads/malloc.h"

/* Partition that contains the file system. */
struct block *fs_device;

static void do_format (void);

/* Initializes the file system module.
   If FORMAT is true, reformats the file system. */
void
filesys_init (bool format) 
{
  fs_device = block_get_role (BLOCK_FILESYS);
  if (fs_device == NULL)
    PANIC ("No file system device found, can't initialize file system.");

  inode_init ();
  free_map_init ();

  if (format) 
    do_format ();
  free_map_open (); 

  thread_current() -> dir = dir_open_root();
}

/* Shuts down the file system module, writing any unwritten data
   to disk. */
void
filesys_done (void) 
{
  bcache_destroy();
  free_map_close ();
}

struct dir* path_parser(const char *input_path_, char **return_name, bool is_make);

/* Creates a file named NAME with the given INITIAL_SIZE.
   Returns true if successful, false otherwise.
   Fails if a file named NAME already exists,
   or if internal memory allocation fails. */
bool
filesys_create (const char *name, off_t initial_size, bool is_file) 
{
  block_sector_t inode_sector_idx = 0;
  //struct dir *dir = dir_open_root ();
  char *file_name;

  //struct dir *dir = dir_reopen(thread_current() -> dir);
  struct dir *dir = path_parser(name, &file_name, true);
  
  bool success = (dir != NULL
                  && free_map_allocate (1, &inode_sector_idx)
                  && inode_create (inode_sector_idx, initial_size, is_file)
                  && dir_add (dir, file_name, inode_sector_idx));
                  
  if (success && !is_file)
  {
    struct dir* new_dir = dir_open(inode_open(inode_sector_idx));
    dir_add(new_dir, ".", inode_sector_idx);
    dir_add(new_dir, "..", inode_sector(dir_get_inode(thread_current()->dir)));
    dir_close(new_dir); 
  }

  if (!success && inode_sector_idx != 0) 
    free_map_release (inode_sector_idx, 1);

  dir_close (dir);

  return success;
}

/* Opens the file with the given NAME.
   Returns the new file if successful or a null pointer
   otherwise.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
struct file *
filesys_open (const char *name)
{
  char *file_name = NULL;
  struct dir *dir = path_parser(name, &file_name, true);
  //struct dir *dir = dir_open_root ();
  struct inode *inode = NULL;

  if (dir != NULL && file_name != NULL)
    dir_lookup (dir, file_name, &inode);
  else if (dir != NULL && file_name == NULL)
    inode = dir_get_inode(dir);

  dir_close (dir);

  return file_open (inode);
}

/* Deletes the file named NAME.
   Returns true if successful, false on failure.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
bool
filesys_remove (const char *name) 
{
  //struct dir *dir = dir_open_root ();

  char *file_name = NULL;
  struct dir *dir = path_parser(name, &file_name, true);

  struct inode *rm_inode;

  if (!file_name)
  {
    dir_close(dir);
    return false;
  }
  if (!dir_lookup(dir, file_name, &rm_inode))
  {
    dir_close(dir);
    inode_close(rm_inode);
    return false;
  }
  if (inode_is_dir(rm_inode) && !dir_inode_is_empty(rm_inode))
  {
    dir_close(dir);
    inode_close(rm_inode);
    return false;
  } 


  bool success = dir != NULL && dir_remove (dir, file_name);
  dir_close (dir); 
  //inode_remove(rm_inode);
  inode_close(rm_inode);

  return success;
}

/* Formats the file system. */
static void
do_format (void)
{
  printf ("Formatting file system...");
  free_map_create ();
  if (!dir_create (ROOT_DIR_SECTOR, 16))
    PANIC ("root directory creation failed");

  struct dir *root_dir = dir_open_root();
  dir_add(root_dir, ".", ROOT_DIR_SECTOR);
  dir_add(root_dir, "..", ROOT_DIR_SECTOR);
  dir_close(root_dir);
  
  free_map_close ();
  printf ("done.\n");
}

struct dir*
path_parser(const char *input_path_, char **return_name, bool is_make)
{
  if (!input_path_) return NULL;
  if (strlen(input_path_) == 0) return NULL;
  
  char *input_path;
  input_path = malloc(strlen(input_path_) + 1);
  strlcpy (input_path, input_path_, strlen(input_path_)+1);

  char **argv = malloc(sizeof(char*) * 20);
  int argc = 0;

  struct dir *cur_dir;

  if (input_path [0] == '/') 
  {
    cur_dir = dir_open_root();
  }

  else
  {
    cur_dir = dir_reopen(thread_current() -> dir);
  }

  struct inode *cur_inode;
  
  char *token, *save_ptr;

  for (token = strtok_r(input_path, "/", &save_ptr); token != NULL; token = strtok_r(NULL, "/", &save_ptr))
  {
    argv[argc] = token;
    argc ++;
  }
  if (argc == 0) 
  {
    free(argv);
    return cur_dir;
  }
  *return_name = argv[argc-1];

  if (!is_make) argc ++;
  int i;
  for (i=0; i<argc-1; i++)
  {
    if (!strcmp(argv[i], ".")) continue;
    dir_lookup(cur_dir, argv[i], &cur_inode);
    if ((!cur_inode) || (!inode_is_dir(cur_inode)))
    {
      dir_close(cur_dir);
      cur_dir = NULL;
      break;
    }
    dir_close(cur_dir);
    cur_dir = dir_open(cur_inode);
  }

  free(argv);

  return cur_dir;
}


bool
mv_dir(const char *input_path)
{
  char *not_used;
  struct dir* return_dir;
  return_dir = path_parser(input_path, &not_used, false);
  if (!return_dir) return false;
  dir_close(thread_current() -> dir);
  thread_current() -> dir = return_dir;
  return true;
}

