#include "vm/frame.h"
#include "threads/synch.h"
#include "threads/malloc.h"
#include "userprog/syscall.h"
#include "userprog/pagedir.h"
#include <stdio.h>
#include <random.h>

static struct hash frames;
static struct lock f_lock;
static struct lock e_lock;

static int count = 0;

static struct frame *f_find(void * p);
static bool delete_frame(void * p);
static bool evict(void);

unsigned f_hash(const struct hash_elem *f, void *aux UNUSED)
{
	const struct frame * frame;

	frame = hash_entry(f, struct frame, h_elem);

	return hash_int((unsigned)frame->f_addr);
}

bool compare_frame(const struct hash_elem *first, const struct hash_elem *second, void *aux UNUSED)
{
	struct frame *first_elem;
	struct frame *second_elem;

	first_elem = hash_entry(first, struct frame, h_elem);
	second_elem = hash_entry(second, struct frame, h_elem);

	return first_elem->f_addr < second_elem->f_addr;
}

void initial_frame()
{
	lock_init(&f_lock);
	lock_init(&e_lock);
	random_init(0);
	hash_init(&frames, f_hash, compare_frame, NULL);
}

void *get_frame(enum palloc_flags f)
{
	void * p;

	p = palloc_get_page(f);

	if (p != NULL)
	{
		struct frame *v_frame;
		v_frame = (struct frame *) malloc(sizeof(struct frame));

		if (v_frame == NULL)
		{
			return false;
		}

		v_frame->f_addr = p;
		v_frame->pin = false;
		v_frame->fp_addr = v_frame->dir = NULL;
		v_frame->p = NULL;

		lock_acquire(&f_lock);
		hash_insert(&frames, &v_frame->h_elem);
		lock_release(&f_lock);
	}
	else
	{
		evict();
		return get_frame(f);
	}
}

void free_frame(void * p)
{
	delete_frame(p);
	palloc_free_page(p);
}

void free_page(void * p)
{
	struct frame *v_frame;

	v_frame = f_find(p);

	if(v_frame != NULL)
	{
		if (v_frame->p != NULL)
		{
			page_delete(v_frame->p);
		}
	}
}

bool add_page_frame(void * f, void *fp_addr, uint32_t *dir)
{
	struct frame * v_frame;

	v_frame = f_find(f);

	if (v_frame == NULL)
	{
		return false;
	}
	else
	{
		v_frame->dir = dir;
		v_frame->fp_addr = fp_addr;

		return true;
	}
}

bool set_page_frame(void * f, struct page * p)
{
	struct frame *v_frame;

	v_frame = f_find(f);

	if (v_frame == NULL)
	{
		return false;
	}
	else
	{
		v_frame->p = p;
		return true;
	}
}

void pin_frame(void * p)
{
	struct frame * v_frame;

	v_frame = f_find(p);

	if(v_frame != NULL)
	{
		v_frame->pin = true;
	}
}

void unpin_frame(void * p)
{
	struct frame * v_frame;

	v_frame = f_find(p);

	if(v_frame != NULL)
	{
		v_frame->pin = false;
	}
}

static bool delete_frame(void * p)
{
	struct frame * v_frame;

	v_frame = f_find(p);

	if(v_frame != NULL)
	{
		lock_acquire(&f_lock);
		hash_delete(&frames, &v_frame->h_elem);
		free(v_frame);
		lock_release(&f_lock);

		--count;

		return true;
	}
	else
	{
		return false;
	}
}

static bool evict(void)
{
	struct frame * frame;
	struct frame * temp;

	frame = NULL;
	temp = NULL;

	lock_acquire(&e_lock);

	while(temp == NULL)
	{
		struct hash_iterator h_it;
		hash_first(&h_it, &frames);

		while(hash_next(&h_it))
		{
			frame = hash_entry(hash_cur(&h_it), struct frame, h_elem);
			if(frame->pin == true)
			{
				continue;
			}

			ASSERT(frame->dir != NULL);
			ASSERT(frame->fp_addr != NULL);

			temp = frame;
			break;
		}
	}

	page_unload(temp->p, temp->f_addr);

	delete_frame(temp->f_addr);
	palloc_free_page(temp->f_addr);

	lock_release(&e_lock);

	return true;
}

static struct frame *f_find(void * p)
{
	struct frame v_frame;

	v_frame.f_addr = p;

	struct hash_elem *elem = hash_find(&frames, &v_frame.h_elem);

	if (elem != NULL)
	{
		return hash_entry(elem, struct frame, h_elem);
	}
	else
	{
		return NULL;
	}
}