#ifndef FRAME_H
#define FRAME_H

#include "vm/page.h"
#include "threads/palloc.h"
#include <hash.h>
#include "threads/thread.h"

struct frame
{
	struct page *p;
	struct hash_elem h_elem;
	bool pin;
	void * f_addr;
	void * fp_addr;
	uint32_t *dir;
};

void initial_frame(void);
void *get_frame(enum palloc_flags f);

void free_frame(void *);
void free_page(void *);
bool add_page_frame(void*, void*, uint32_t *);
bool set_page_frame(void *, struct page *);

void pin_frame(void *);
void unpin_frame(void *);

#endif