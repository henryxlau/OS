#include "vm/page.h"
#include <string.h>
#include <stdio.h>
#include "threads/palloc.h"
#include "threads/malloc.h"
#include "threads/vaddr.h"
#include "threads/thread.h"
#include "threads/synch.h"
#include "userprog/syscall.h"
#include "userprog/pagedir.h"
#include "vm/frame.h"
#include "vm/swap.h"

static bool file_load(uint8_t *kpage, struct page *page);
static bool swap_load(uint8_t *kpage, struct page *page);
static bool zero_load(uint8_t *kpage);

static int count = 0;
static struct lock lock_list;
static struct lock load;
static struct list pages;

void initial_page(void)
{
	lock_init(&lock_list);
	lock_init(&load);
	list_init(&pages);
}

struct page *file_new(void *addr, struct file *f, off_t ofs, uint32_t read, uint32_t zero, bool write)
{
	struct page *p;

	p = (struct page *) malloc(sizeof(struct page));

	if (p == NULL)
	{
		return NULL;
	}
	else
	{
		p->type = F;
		p->dir = thread_current()->pagedir;
		p->ofs = ofs;
		p->f_addr = addr;
		p->f = f;
		p->load = false;
		p->read = read;
		p->zero = zero;
		p->write = write;

		lock_acquire(&lock_list);
		list_push_back(&pages, &p->elem);
		lock_release(&lock_list);

		return p;
	}
}

struct page *swap_new(void *addr, size_t i, bool write)
{
	struct page *p;

	p = (struct page *) malloc(sizeof(struct page));

	if(p == NULL)
	{
		return NULL;
	}

	p->type = S;
	p->dir = thread_current()->pagedir;
	p->f_addr = addr;
	p->load = false;
	p->swap_index = i;
	p->write = write;

	lock_acquire(&lock_list);
	list_push_back(&pages, &p->elem);
	lock_release(&lock_list);

	return p;
}

struct page *zero_new(void *addr, bool write)
{
	struct page *p;

	p = (struct page *) malloc(sizeof(struct page));

	if(p == NULL)
	{
		return NULL;
	}

	p->type = Z;
	p->dir = thread_current()->pagedir;
	p->f_addr = addr;
	p->load = false;
	p->write = write;

	lock_acquire(&lock_list);
	list_push_back(&pages, &p->elem);
	lock_release(&lock_list);

	return p;
}

bool page_load(struct page *p, void *fp_addr)
{
	lock_acquire(&load);
	p->kpage = get_frame(PAL_USER);

	lock_release(&load);
	set_page_frame(p->kpage, p);

	pin_frame(p->kpage);

	if(p->kpage == NULL)
	{
		unpin_frame(p->kpage);
		return false;
	}

	bool temp = true;

	if(p->type == S)
	{
		temp = swap_load(p->kpage, p);
	}
	else if(p->type == F)
	{
		temp = file_load(p->kpage, p);
	}
	else
	{
		temp = zero_load(p->kpage);
	}

	if(!temp)
	{
		unpin_frame(p->kpage);
		return false;
	}

	pagedir_clear_page(p->dir, fp_addr);
	if(!pagedir_set_page(p->dir, fp_addr, p->kpage, p->write))
	{
		unpin_frame(p->kpage);
		return false;
	}
	else if(!pagedir_get_page(p->dir, fp_addr))
	{
		unpin_frame(p->kpage);
		return false;
	}

	p->load = true;

	pagedir_set_dirty(p->dir, fp_addr, false);
	pagedir_set_accessed(p->dir, fp_addr, true);

	unpin_frame(p->kpage);

	return true;
}

void page_unload(struct page *p, void *fp_addr)
{
	if (p->type == F)
	{
		if(pagedir_is_dirty(p->dir, p->f_addr))
		{
			pin_frame(p->kpage);

			file_seek(p->f, p->ofs);
			file_write(p->f, fp_addr, p->read);

			unpin_frame(p->kpage);
		}
	}
	else if(p->type == S)
	{
		p->type = S;
		p->swap_index = store_swap(fp_addr);
	}
	else if(pagedir_is_dirty(p->dir, p->f_addr))
	{
		p->swap_index = store_swap(fp_addr);
		p->type = S;
	}

	p->load = false;
	p->kpage = NULL;

	pagedir_clear_page(p->dir, p->f_addr);
}

bool page_delete(struct page *p)
{
	lock_acquire(&lock_list);

	list_remove(&p->elem);
	--count;

	lock_release(&lock_list);

	return true;
}

struct page *increase_stack(void * fp_addr)
{
	struct page * p;

	p = zero_new(fp_addr, true);

	if(page_load(p, fp_addr))
	{
		return p;
	}
	else
	{
		return NULL;
	}
}

void pin_page(struct page *p)
{
	ASSERT(p->kpage != NULL);
	pin_frame(p->kpage);
}

void unpin_page(struct page *p)
{
	ASSERT(p->kpage != NULL);
	unpin_frame(p->kpage);
}

struct page *find_page(void * f_addr)
{
	uint32_t *dir;
	struct page * p;

	dir = thread_current()->pagedir;

	lock_acquire(&lock_list);

	struct list_elem *it_elem;
	for (it_elem = list_begin(&pages); it_elem != list_end(&pages); it_elem = list_next(it_elem))
	{
		p = list_entry(it_elem, struct page, elem);

		if(p->f_addr == f_addr)
		{
			if(p->dir == dir)
			{
				lock_release(&lock_list);
				return p;
			}
		}
	}

	lock_release(&lock_list);

	return NULL;
}

static bool file_load(uint8_t *kpage, struct page *p)
{
	file_seek(p->f, p->ofs);

	if(file_read(p->f, kpage, p->read) != (int) p->read)
	{
		free_frame(kpage);
		return false;
	}
	else
	{
		memset(kpage + p->read, 0, p->zero);
		return true;
	}
}

static bool swap_load(uint8_t *kpage, struct page *p)
{
	load_swap(p->swap_index, kpage);
	free_swap(p->swap_index);

	return true;
}

static bool zero_load(uint8_t *kpage)
{
	memset(kpage, 0, PGSIZE);
	
	return true;
}