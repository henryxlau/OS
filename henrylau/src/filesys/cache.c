#include <debug.h>
#include <stdio.h>
#include <string.h>

#include "threads/thread.h"
#include "threads/vaddr.h"
#include "filesys/cache.h"
#include "filesys/filesys.h"
#include "filesys/inode.h"
#include "devices/block.h"
#include "devices/timer.h"
#include "threads/malloc.h"
#include "threads/palloc.h"

static struct entry *cache;
static struct lock lock_cache;
static struct condition ready;

struct next_entry
{
  block_sector_t s;
  struct list_elem elem;
}

static int size;
static int hand;

static struct list list_block;
static struct lock block_lock;

static struct condition block_data;

bool init_cache (const size_t size)
{
  void *addr;
  tid_t writer;
  tid_t reader;

  cache = malloc(size * sizeof (struct entry));

  if (cache == NULL)
  {
    return false;
  }
  else 
  {
    lock_init(&lock_cache);
    cond_init(&ready);

    int i;
    for (i = 0; i < cache_size; i++)
    {
      if (i % (PGSIZE/BLOCK_SECTOR_SIZE) == 0)
      {
        addr = palloc_get_page(0);
        if (addr == NULL)
        {
          return false
        }
      }
      else
      {
        addr = addr + BLOCK_SECTOR_SIZE;
      }
    }
  }
}

int read_cache (const block_sector_t sector, enum type type, const int ofs, const off_t size, void *buf, const block_sector_t next)
{

}

int write_cache (const block_sector_t sector, enum type type, const int ofs, const off_t size, void *buf, const block_sector_t next)
{

}

void flush_cache (const bool await)
{
  int i;

  for (i = 0; i < size; i++)
  {
    lock_acquire(&lock_cache);
    lock_release(&lock_cache);
  }
}