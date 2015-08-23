#ifndef CACHE_H
#define CACHE_H

#include "filesys/off_t.h"
#include "devices/block.h"
#include "threads/synch.h"

#define CACHE_SIZE 64

enum type
{
	METAD, REG
};

enum state
{
	READ = 0, REQ, WRITE, CLOCK, READY
};

enum accessed
{
	CLEAN = 0x00, ACCESSED = 0x01, DIRTY = 0x02, META = 0x04
}

struct entry
{
	enum cache_state state;
	enum cache_accessed accessed;
	enum type type;
	void *addr;
	int accessors;
	block_sector_t sector;
	block_sector_t next;
	struct condition wait;
};

bool init_cache (const size_t size);
int read_cache (const block_sector_t sector, enum type type, const int ofs, const off_t size, void *buf, const block_sector_t next);
int write_cache (const block_sector_t sector, enum type type, const int ofs, const off_t size, void *buf, const block_sector_t next);
void flush_cache (const bool await);

#endif