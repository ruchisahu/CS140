#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "threads/malloc.h"
#include "threads/thread.h"

/* Partition that contains the file system. */
struct block *fs_device;

static void do_format (void);
bool filesys_parse(const char *name, struct dir **dir, char **file_name);
inline bool is_root(struct dir *dir);

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
}

/* Shuts down the file system module, writing any unwritten data
   to disk. */
void
filesys_done (void) 
{
  free_map_close ();
}

/* Creates a file named NAME with the given INITIAL_SIZE.
   Returns true if successful, false otherwise.
   Fails if a file named NAME already exists,
   or if internal memory allocation fails. */
//TODO: syscall by _create() 
bool
filesys_create (const char *name, off_t initial_size) 
{
  //TODO: parse out the path and real name from NAME
  block_sector_t inode_sector = 0;
  //struct dir *dir = dir_open_root ();
  struct dir *dir;
  char *file_name;
  if(!filesys_parse(name, &dir, &file_name)) 
    return false;
  //printf(" name =%s, filename = %s, dir not null? %d\n", name, file_name, dir!=NULL);
  bool success = (dir != NULL
                  && free_map_allocate (1, &inode_sector)  //ask free_list给这个新文件分配一个sector存放inode
                  && inode_create (inode_sector, initial_size)  //给这个sector写上inode信息：长度，根据大小初始化14个pointer，给每个pointer分配block，给每个block写满0
                  && dir_add (dir, file_name, inode_sector));
  if (!success && inode_sector != 0) 
    free_map_release (inode_sector, 1);
  dir_close (dir);

  PRINTF("filesys creat success(%d)\n", success);
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
  struct dir *dir;
  char *file_name;
  //TODO:
  PRINTF("\nfilesys_open called. filename:%s\n", name);
  if(!filesys_parse(name, &dir, &file_name)) 
    return NULL;
  PRINTF("dir=%p, filename=%s\n", dir, file_name);
  struct inode *inode = NULL;
  if (dir != NULL)
    dir_lookup (dir, file_name, &inode);
  dir_close (dir);

  PRINTF("filesys_open: inode=%p\n", inode);
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
  struct dir *dir;
  char *file_name;
  
  if(!filesys_parse(name, &dir, &file_name)) 
    return NULL;
  PRINTF("\n\nremove called, name=%s\n", file_name);
  if ( !strcmp(file_name,".") || !strcmp(file_name, ".."))
  {
    PRINTF("can't remove . and ..");
    return false;
  }
  

  bool success = dir != NULL && dir_remove (dir, file_name);
  PRINTF("success(%d)\n", success);
  dir_close (dir); 

  return success;
}

/* Formats the file system. */
static void
do_format (void)
{
  printf ("Formatting file system...");
  free_map_create ();
  /* Creat a root directory that only contains . and .. */
  if (!dir_create (ROOT_DIR_SECTOR, 2))
    PANIC ("root directory creation failed");
  struct dir *dir = dir_open_root();
  dir_add(dir,".", ROOT_DIR_SECTOR);
  dir_add(dir,"..", ROOT_DIR_SECTOR);
  free_map_close ();
  printf ("done.\n");
}

bool
filesys_parse(const char *path, struct dir **dir, char **file_name)
{
  size_t len = strlen (path);
  if (len == 0)
    return false;
  //TODO: this buf is never freed.
  char *buf = malloc(len+1);
  memcpy(buf, path, len+1);
  char *token, *save_ptr;
  struct dir *cur_dir;
  bool is_root = false;
  bool is_new_file = false;
  char *begin = buf;
  /* Tail is the last file name */
  char *tail = buf + strlen(buf)-1;
  while ( tail >= begin && *tail != '/')
    tail--;
  tail++;
  PRINTF("begin:%s   tail: %s\n\n", begin, tail);
  /* find the first word */
  while (*begin == ' ')
    begin++;
  while (*begin == '/')
  {
    begin++;
    is_root = true;
  }
  //TODO:
  PRINTF("is root(%d), string is:%s\n", is_root, begin);
  if (is_root)
    cur_dir = dir_open_root();
  else
  {
    cur_dir = thread_current()->cur_dir;
    cur_dir = dir_open_root();
    PRINTF("cur_dir = %p\n", cur_dir);
  }
  // TODO: begin = "/a/b/c"
  for (token = strtok_r (begin, "/", &save_ptr); token != tail;
       token = strtok_r (NULL, "/", &save_ptr))
  {
    PRINTF ("token is %s\n", token);
    struct inode *inode = NULL;
    /* Find the name<->inode_sector pair in cur_dir, return false 
       if not found*/
    if (!dir_lookup(cur_dir, token, &inode))
      return false;
    dir_close(cur_dir);
    cur_dir = dir_open(inode);
  }

  PRINTF("dir=%p, extracted file name = %s\n", cur_dir, tail);
    
  *dir = cur_dir;
  *file_name = tail;
  return true;
}

/* Check if the directory is root directory */
inline bool
is_root(struct dir *dir)
{
/*
  struct inode *inode1 =dir->inode;
  return (inode1.sector == ROOT_DIR_SECTOR);*/
   return false;

}

