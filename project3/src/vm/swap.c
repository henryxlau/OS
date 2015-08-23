#include "vm/swap.h"
#include <bitmap.h>
#include "threads/vaddr.h"
#include "devices/block.h"
#include "threads/palloc.h"
#include "threads/synch.h"

#define BLOCKS (PGSIZE/512)

static struct lock lock;
static struct bitmap *map;

static size_t size = 0;

static struct block *block;

void assert_funct(size_t i);

void initial_swap(void)
{
	lock_init(&lock);

	block = block_get_role(BLOCK_SWAP);

	size = block_size(block);

	map = bitmap_create(size);
}

void load_swap(size_t i, void *f_addr)
{
	lock_acquire(&lock);

	size_t iter;

	for(iter = 0; iter < BLOCKS; ++iter)
	{
		assert_funct(i);

		block_read(block, i, f_addr+iter* 512);

		++i;
	}
	lock_release(&lock);
}

void free_swap(size_t i)
{
	lock_acquire(&lock);

	size_t iter;

	for(iter = 0; iter < BLOCKS; ++iter)
	{
		assert_funct(i);

		bitmap_reset(map, i);

		++i;
	}
	lock_release(&lock);
}

size_t store_swap(void * f_addr)
{
	lock_acquire(&lock);

	size_t i;

	i = bitmap_scan_and_flip(map, 0, BLOCKS, false);

	ASSERT(i != BITMAP_ERROR);

	size_t iter;
	size_t temp;

	temp = i;

	for (iter = 0; iter < BLOCKS; ++iter)
	{
		ASSERT(i < size);
		ASSERT(bitmap_test(map, temp));

		block_write(block, temp, f_addr+iter * 512);

		++temp;
	}

	lock_release(&lock);

	return i;
}

void assert_funct(size_t i)
{
	ASSERT(i < size);

	ASSERT(bitmap_test(map,i));
}