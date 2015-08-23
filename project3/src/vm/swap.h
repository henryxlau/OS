#ifndef SWAP_H
#define SWAP_H

#include <stddef.h>

void initial_swap(void);
void load_swap(size_t, void *);
void free_swap(size_t);
size_t store_swap(void *);

#endif