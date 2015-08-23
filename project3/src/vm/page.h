#ifndef PAGE_H
#define PAGE_H

#include "threads/interrupt.h"
#include "filesys/file.h"
#include <stdbool.h>
#include <list.h>

enum page_type {S, F, Z};

struct page
{
	enum page_type type;
	uint32_t *dir;
	bool load;
	bool write;
	void * f_addr;
	void * kpage;
	struct list_elem elem;
	size_t swap_index;

	size_t read;
	size_t zero;
	off_t offset;
	struct file *f;
};

void initial_page(void);

struct page *file_new(void *addr, struct file *f, off_t offset, uint32_t read, uint32_t zero, bool write);
struct page *swap_new(void *addr, size_t i, bool write);
struct page *zero_new(void *addr, bool write);

bool page_load(struct page *p, void *fp_addr);
void page_unload(struct page *p, void *fp_addr);
bool page_delete(struct page *p);

struct page *increase_stack(void * fp_addr);

void pin_page(struct page * p);
void unpin_page(struct page * p);
struct page *find_page(void * f_addr);

#endif