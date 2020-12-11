#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"
#include "filesys/buffer-cache.h"
#include <stdio.h>

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44
#define DIRECT_BLOCK_COUNTS 123
#define INDIRECT_BLOCK_COUNTS 128


enum inode_state
{
  FILE,
  DIRECTORY
};

/* On-disk inode.
   Must be exactly BLOCK_SECTOR_SIZE bytes long. */
struct inode_disk
  {
    //block_sector_t start;               /* First data sector. */
    enum inode_state state;
    off_t length;                       /* File size in bytes. */
    //uint32_t unused[125];               /* Not used. */
    block_sector_t direct_blocks[DIRECT_BLOCK_COUNTS];
    block_sector_t indirect_blocks;
    block_sector_t double_indirect_blocks;
    unsigned magic;                     /* Magic number. */
  
  };

/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
static inline size_t
bytes_to_sectors (off_t size)
{
  return DIV_ROUND_UP (size, BLOCK_SECTOR_SIZE);
}

/* In-memory inode. */
struct inode 
  {
    struct list_elem elem;              /* Element in inode list. */
    block_sector_t sector;              /* Sector number of disk location. */
    int open_cnt;                       /* Number of openers. */
    bool removed;                       /* True if deleted, false otherwise. */
    int deny_write_cnt;                 /* 0: writes ok, >0: deny writes. */
    //struct inode_disk data;             /* Inode content. */
  };

/* Returns the block device sector that contains byte offset POS
   within INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */

static block_sector_t
byte_to_sector (const struct inode *inode, off_t pos) 
{
  ASSERT (inode != NULL);
  size_t sector_idx;
  block_sector_t return_sector = -1;
  struct inode_disk *disk_inode;
  disk_inode = malloc(sizeof(struct inode_disk));
  bcache_read(inode -> sector, disk_inode, 0, BLOCK_SECTOR_SIZE);

  if (pos < disk_inode -> length)
  {
    sector_idx = pos / BLOCK_SECTOR_SIZE;
    if (sector_idx < DIRECT_BLOCK_COUNTS) {
      return_sector =  (disk_inode -> direct_blocks) [sector_idx];
    }
    else if (sector_idx < DIRECT_BLOCK_COUNTS + INDIRECT_BLOCK_COUNTS) 
    {
      size_t indirect_blocks[INDIRECT_BLOCK_COUNTS];
      bcache_read(disk_inode -> indirect_blocks, indirect_blocks, 0, BLOCK_SECTOR_SIZE);
      return_sector = indirect_blocks[sector_idx - DIRECT_BLOCK_COUNTS];
    }
    else 
    {
      size_t double_indirect_blocks[INDIRECT_BLOCK_COUNTS];
      size_t double_indirect_blocks_lv2[INDIRECT_BLOCK_COUNTS];
      size_t table_idx = sector_idx - DIRECT_BLOCK_COUNTS - INDIRECT_BLOCK_COUNTS;
      size_t offset = table_idx % INDIRECT_BLOCK_COUNTS;
      table_idx /= INDIRECT_BLOCK_COUNTS;
      bcache_read(disk_inode->double_indirect_blocks, double_indirect_blocks, 0, BLOCK_SECTOR_SIZE);
      bcache_read(double_indirect_blocks[table_idx], double_indirect_blocks_lv2, 0, BLOCK_SECTOR_SIZE);
      return_sector = double_indirect_blocks_lv2[offset]; 
    }
    //return inode->data.start + pos / BLOCK_SECTOR_SIZE;
  }
    free(disk_inode);
    return return_sector;
}

/* List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */
static struct list open_inodes;

/* Initializes the inode module. */
void
inode_init (void) 
{
  list_init (&open_inodes);
}


bool
create_empty_inode(block_sector_t sector, bool is_file)
{
  bool success;
  struct inode_disk *disk_inode = NULL;
  disk_inode = calloc(1, sizeof *disk_inode);
  if (disk_inode != NULL)
  {
    disk_inode -> length = 0;
    disk_inode -> magic = INODE_MAGIC;   
    if (is_file) disk_inode -> state = FILE;
    else disk_inode -> state = DIRECTORY;
    bcache_write(sector, disk_inode, 0, BLOCK_SECTOR_SIZE); 
    success = true;
  }
  free(disk_inode);
  return success;
}

static bool extend_inode_of_sector(block_sector_t sector, off_t pos);
/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   device.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool
inode_create (block_sector_t sector, off_t length, bool is_file)
{
  ASSERT (length >= 0);
  ASSERT (sizeof (struct inode_disk) == BLOCK_SECTOR_SIZE);
  return create_empty_inode(sector, is_file) && extend_inode_of_sector(sector, length);

  /*
  struct inode_disk *disk_inode = NULL;
  bool success = false;

  ASSERT (sizeof *disk_inode == BLOCK_SECTOR_SIZE);
  disk_inode = calloc (1, sizeof *disk_inode);
  
  if (disk_inode != NULL)
  {
    bool indirect_flag = false;
    bool double_indirect_flag = false;

    size_t sector_idx = 0;
    disk_inode -> length = length;
    disk_inode -> magic = INODE_MAGIC;
    
    static char zeros[BLOCK_SECTOR_SIZE];
    size_t indirect_blocks[INDIRECT_BLOCK_COUNTS];
    size_t double_indirect_blocks[INDIRECT_BLOCK_COUNTS];
    size_t double_indirect_blocks_lv2[INDIRECT_BLOCK_COUNTS];
    
    size_t sectors = bytes_to_sectors(length);
    size_t i;
    size_t indirect_i;
    size_t double_indirect_i;
    size_t double_indirect_offset;
    for (i = 0; i < sectors; i++)
    {
      if (free_map_allocate(1, &sector_idx))
      {
        if (i < DIRECT_BLOCK_COUNTS)
        {
          (disk_inode -> direct_blocks)[i] = sector_idx;
        }
        else if (i < DIRECT_BLOCK_COUNTS + INDIRECT_BLOCK_COUNTS)
        {
          indirect_i = i - DIRECT_BLOCK_COUNTS;
          if (indirect_i == 0) {
              free_map_allocate(1, &(disk_inode -> indirect_blocks));
              indirect_flag = true;
          }
          indirect_blocks[indirect_i] = sector_idx;
        }
        else
        {
          double_indirect_i = i - DIRECT_BLOCK_COUNTS - INDIRECT_BLOCK_COUNTS;
          double_indirect_offset = double_indirect_i % INDIRECT_BLOCK_COUNTS;
          double_indirect_i /= INDIRECT_BLOCK_COUNTS;
          if (double_indirect_offset == 0 && double_indirect_i == 0)
          {
            free_map_allocate(1, &(disk_inode -> double_indirect_blocks));
            double_indirect_flag = true;
          }
          if (double_indirect_offset == 0)
          {
            free_map_allocate(1, &(double_indirect_blocks[double_indirect_i]));
            if (double_indirect_i) bcache_write(double_indirect_blocks[double_indirect_i - 1], double_indirect_blocks_lv2, 0, BLOCK_SECTOR_SIZE);
          }
          double_indirect_blocks_lv2[double_indirect_offset] = sector_idx;
        }
        //block_write(fs_device, sector_idx, zeros);
        bcache_write(sector_idx, zeros, 0, BLOCK_SECTOR_SIZE);
      }
      else printf("NO FREE MAP ALLOCATED\n");
    } 
    if (indirect_flag) bcache_write(disk_inode -> indirect_blocks, indirect_blocks, 0, BLOCK_SECTOR_SIZE);
    if (double_indirect_flag) 
    {
      bcache_write(double_indirect_blocks[double_indirect_i], double_indirect_blocks_lv2, 0, BLOCK_SECTOR_SIZE);
      bcache_write(disk_inode -> double_indirect_blocks, double_indirect_blocks, 0, BLOCK_SECTOR_SIZE);
    }
    bcache_write(sector, disk_inode, 0, BLOCK_SECTOR_SIZE);
    success = true;
  }
  free(disk_inode);
  return success;
  */

}

/* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
struct inode *
inode_open (block_sector_t sector)
{
  struct list_elem *e;
  struct inode *inode;

  /* Check whether this inode is already open. */
  for (e = list_begin (&open_inodes); e != list_end (&open_inodes);
       e = list_next (e)) 
    {
      inode = list_entry (e, struct inode, elem);
      if (inode->sector == sector) 
        {
          inode_reopen (inode);
          return inode; 
        }
    }

  /* Allocate memory. */
  inode = malloc (sizeof *inode);
  if (inode == NULL)
    return NULL;

  /* Initialize. */
  list_push_front (&open_inodes, &inode->elem);
  inode->sector = sector;
  inode->open_cnt = 1;
  inode->deny_write_cnt = 0;
  inode->removed = false;
  //block_read (fs_device, inode->sector, &inode->data);
  return inode;
}

/* Reopens and returns INODE. */
struct inode *
inode_reopen (struct inode *inode)
{
  if (inode != NULL)
    inode->open_cnt++;
  return inode;
}

/* Returns INODE's inode number. */
block_sector_t
inode_get_inumber (const struct inode *inode)
{
  return inode->sector;
}

/* Closes INODE and writes it to disk.
   If this was the last reference to INODE, frees its memory.
   If INODE was also a removed inode, frees its blocks. */
void
inode_close (struct inode *inode) 
{
  /* Ignore null pointer. */
  if (inode == NULL)
    return;
  /* Release resources if this was the last opener. */
  if (--inode->open_cnt == 0)
    {
      /* Remove from inode list and release lock. */
      list_remove (&inode->elem);
 
      /* Deallocate blocks if removed. */
      if (inode->removed) 
        {
          /*
          struct inode_disk *disk_inode;
          disk_inode = malloc(sizeof(struct inode_disk));
          block_sector_t indirect_blocks[INDIRECT_BLOCK_COUNTS];
          
          
          free_map_release (inode->sector, 1);
          */
          /*
          free_map_release (inode->data.start,
                            bytes_to_sectors (inode->data.length)); 
          */
          /*
          bcache_read(inode -> sector, disk_inode, 0, BLOCK_SECTOR_SIZE);
          block_sector_t sector_idx;
          off_t length = disk_inode -> length;
          off_t i;
          struct bte *bte;
          for (i = 0; i < length; i++)
          {
            if (i < DIRECT_BLOCK_COUNTS)
            {
              sector_idx = (disk_inode -> direct_blocks)[i];
              bte = bcache_find(sector_idx);
              if (bte) bcache_clean(bte);
              free_map_release(sector_idx, 1);
            }
            else if (i < DIRECT_BLOCK_COUNTS + INDIRECT_BLOCK_COUNTS)
            {
              if (i == DIRECT_BLOCK_COUNTS)
              {
                bcache_read(disk_inode -> indirect_blocks, indirect_blocks, 0, BLOCK_SECTOR_SIZE);
                free_map_release(disk_inode -> indirect_blocks, 1);
              }
              sector_idx = (indirect_blocks[i - DIRECT_BLOCK_COUNTS]);
              bte = bcache_find(sector_idx);
              if (bte) bcache_clean(bte);
              free_map_release(sector_idx, 1);
            }
            else 
            {

            }printf("NOT YET IMPLEMENTED : cleaning double indirect blocks\n");
          }
          if (disk_inode -> indirect_blocks){
            bte = bcache_find(disk_inode -> indirect_blocks);
            if (bte) bcache_clean(bte);
          
          }
          free(disk_inode);*/
        }
        free (inode); 
    }
}

/* Marks INODE to be deleted when it is closed by the last caller who
   has it open. */
void
inode_remove (struct inode *inode) 
{
  ASSERT (inode != NULL);
  inode->removed = true;
}

/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
off_t
inode_read_at (struct inode *inode, void *buffer_, off_t size, off_t offset) 
{
  uint8_t *buffer = buffer_;
  off_t bytes_read = 0;
  //uint8_t *bounce = NULL;

  while (size > 0) 
    {
      /* Disk sector to read, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually copy out of this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      //여기서 bcache_read
      bcache_read(sector_idx, buffer + bytes_read, sector_ofs, chunk_size);

      // if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
      //   {
      //     /* Read full sector directly into caller's buffer. */
      //     block_read (fs_device, sector_idx, buffer + bytes_read);
      //   }
      // else 
      //   {
      //     /* Read sector into bounce buffer, then partially copy
      //        into caller's buffer. */
      //     if (bounce == NULL) 
      //       {
      //         bounce = malloc (BLOCK_SECTOR_SIZE);
      //         if (bounce == NULL)
      //           break;
      //       }
      //     block_read (fs_device, sector_idx, bounce);
      //     memcpy (buffer + bytes_read, bounce + sector_ofs, chunk_size);
      //   }
      
      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_read += chunk_size;
    }
  //free (bounce);

  return bytes_read;
}

/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
   Returns the number of bytes actually written, which may be
   less than SIZE if end of file is reached or an error occurs.
   (Normally a write at end of file would extend the inode, but
   growth is not yet implemented.) */
off_t
inode_write_at (struct inode *inode, const void *buffer_, off_t size,
                off_t offset) 
{
  const uint8_t *buffer = buffer_;
  off_t bytes_written = 0;
  //uint8_t *bounce = NULL;

  if (inode->deny_write_cnt)
    return 0;

  if (inode_length(inode) < offset + size) extend_inode_of_sector(inode -> sector, offset + size);

  while (size > 0) 
    {
      /* Sector to write, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually write into this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      bcache_write(sector_idx, buffer + bytes_written, sector_ofs, chunk_size);
      /*
      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
          // Write full sector directly to disk. 
          block_write (fs_device, sector_idx, buffer + bytes_written);
        }
      else 
        {
          // We need a bounce buffer. 
          if (bounce == NULL) 
            {
              bounce = malloc (BLOCK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }

          // If the sector contains data before or after the chunk
          //   we're writing, then we need to read in the sector
          //   first.  Otherwise we start with a sector of all zeros.
          if (sector_ofs > 0 || chunk_size < sector_left) 
            block_read (fs_device, sector_idx, bounce);
          else
            memset (bounce, 0, BLOCK_SECTOR_SIZE);
          memcpy (bounce + sector_ofs, buffer + bytes_written, chunk_size);
          block_write (fs_device, sector_idx, bounce);
        }
      */  

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_written += chunk_size;
    }
  //free (bounce);

  return bytes_written;
}

/* Disables writes to INODE.
   May be called at most once per inode opener. */
void
inode_deny_write (struct inode *inode) 
{
  inode->deny_write_cnt++;
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
}

/* Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode. */
void
inode_allow_write (struct inode *inode) 
{
  ASSERT (inode->deny_write_cnt > 0);
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
  inode->deny_write_cnt--;
}

/* Returns the length, in bytes, of INODE's data. */
off_t
inode_length (const struct inode *inode)
{
  struct inode_disk *disk_inode;
  disk_inode = malloc(sizeof(struct inode_disk));
  bcache_read(inode -> sector, disk_inode, 0, BLOCK_SECTOR_SIZE);
  off_t length = disk_inode -> length;  
  free(disk_inode);
  //return inode->data.length;
  return length;
}

bool 
extend_inode_of_sector(block_sector_t sector, off_t pos)
{
    bool indirect_flag = false;
    bool double_indirect_flag = false;

    struct inode_disk *disk_inode = NULL;
    bool success = false;
    disk_inode = malloc(sizeof *disk_inode);
    bcache_read(sector, disk_inode, 0, BLOCK_SECTOR_SIZE);
    off_t length = disk_inode -> length;
    if (pos == length) {
      free(disk_inode);
      return true;
    }

    static char zeros[BLOCK_SECTOR_SIZE];
    size_t start_sector = bytes_to_sectors(length);
    size_t sectors = bytes_to_sectors(pos);

    size_t indirect_blocks[INDIRECT_BLOCK_COUNTS];
    size_t double_indirect_blocks[INDIRECT_BLOCK_COUNTS];
    size_t double_indirect_blocks_lv2[INDIRECT_BLOCK_COUNTS];   

    size_t indirect_i;
    size_t double_indirect_i;
    size_t double_indirect_offset; 

    size_t sector_idx = 0;
    if (start_sector == 0);
    else if (start_sector < DIRECT_BLOCK_COUNTS); 
    else if (start_sector < DIRECT_BLOCK_COUNTS + INDIRECT_BLOCK_COUNTS)
    {
        bcache_read(disk_inode -> indirect_blocks, indirect_blocks, 0, BLOCK_SECTOR_SIZE);
        indirect_flag = true;
    }
    else
    {
        double_indirect_i = start_sector - DIRECT_BLOCK_COUNTS - INDIRECT_BLOCK_COUNTS;
        double_indirect_offset = double_indirect_i % INDIRECT_BLOCK_COUNTS;
        double_indirect_i /= INDIRECT_BLOCK_COUNTS;
        
        bcache_read(disk_inode -> double_indirect_blocks, double_indirect_blocks, 0, BLOCK_SECTOR_SIZE);
        bcache_read(double_indirect_blocks[double_indirect_i], double_indirect_blocks_lv2, 0, BLOCK_SECTOR_SIZE);
    }
    off_t i;
    for (i = start_sector; i < sectors; i++)
    {
      if (free_map_allocate(1, &sector_idx))
      {
        if (i < DIRECT_BLOCK_COUNTS)
        {
          (disk_inode -> direct_blocks)[i] = sector_idx;
        }
        else if (i < DIRECT_BLOCK_COUNTS + INDIRECT_BLOCK_COUNTS)
        {
          indirect_i = i - DIRECT_BLOCK_COUNTS;
          if (indirect_i == 0) {
              free_map_allocate(1, &(disk_inode -> indirect_blocks));
              indirect_flag = true;
          }
          indirect_blocks[indirect_i] = sector_idx;
        }
        else
        {
          double_indirect_i = i - DIRECT_BLOCK_COUNTS - INDIRECT_BLOCK_COUNTS;
          double_indirect_offset = double_indirect_i % INDIRECT_BLOCK_COUNTS;
          double_indirect_i /= INDIRECT_BLOCK_COUNTS;
          if (double_indirect_offset == 0 && double_indirect_i == 0)
          {
            free_map_allocate(1, &(disk_inode -> double_indirect_blocks));
            double_indirect_flag = true;
          }
          if (double_indirect_offset == 0)
          {
            free_map_allocate(1, &(double_indirect_blocks[double_indirect_i]));
            if (double_indirect_i) bcache_write(double_indirect_blocks[double_indirect_i - 1], double_indirect_blocks_lv2, 0, BLOCK_SECTOR_SIZE);
          }
          double_indirect_blocks_lv2[double_indirect_offset] = sector_idx;
        }
        //block_write(fs_device, sector_idx, zeros);
        bcache_write(sector_idx, zeros, 0, BLOCK_SECTOR_SIZE);
      }
      else printf("NO FREE MAP ALLOCATED\n");        
    }
    if (indirect_flag) bcache_write(disk_inode -> indirect_blocks, indirect_blocks, 0, BLOCK_SECTOR_SIZE);
    if (double_indirect_flag) 
    {
      bcache_write(double_indirect_blocks[double_indirect_i], double_indirect_blocks_lv2, 0, BLOCK_SECTOR_SIZE);
      bcache_write(disk_inode -> double_indirect_blocks, double_indirect_blocks, 0, BLOCK_SECTOR_SIZE);
    }
    disk_inode -> length = pos;
    bcache_write(sector, disk_inode, 0, BLOCK_SECTOR_SIZE);
    success = true;    
    free(disk_inode);
    return success;
}

bool
inode_is_dir(struct inode *inode)
{
  struct inode_disk *disk_inode = malloc(BLOCK_SECTOR_SIZE);
  bcache_read(inode -> sector, disk_inode, 0, BLOCK_SECTOR_SIZE);
  enum inode_state return_state = disk_inode -> state;
  free(disk_inode);
  return (return_state == DIRECTORY);
}

int
inode_sector(struct inode *inode)
{
  return inode -> sector;
}