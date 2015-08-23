#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "lib/kernel/hash.h"

static void syscall_handler (struct intr_frame *);
typedef void (*call) (struct intr_frame *f);
static call syscall[20];

struct entry
{
	struct file *file;
	struct thread *thread;
	struct hash_elem elem;
	int file_des;
};

struct hash entries;
static struct lock file_lock;

static void exit(int status);
static struct file *get_file(int file_des);
static struct entry *get_entry(int file_des);

static int generate_descriptor(void);

static bool validate_address(void *ptr);
bool cmp_entries(const struct hash_elem *first, const struct hash_elem *second, void *aux);
unsigned hash_entries(const struct hash_elem *ptr, void *aux);

static void sys_halt(void)
{
	shutdown_power_off();
}

static void sys_exit(struct intr_frame *f)
{
	int *t_ptr;
	int file_des;

	t_ptr = f->esp;
	file_des = (int) *(t_ptr+1);

	exit(file_des);
}

static void sys_exec(struct intr_frame *f)
{
	int *t_ptr;
	char *fn;

	t_ptr = f->esp;
	fn = *(t_ptr+1);

	if (!validate_address(fn))
	{
		f->eax = -1;
	}
	else
	{
		f->eax = process_execute(fn);
	}
}

static void sys_wait(struct intr_frame *f)
{
	int *t_ptr;
	tid_t tid;

	t_ptr = f->esp;
	tid = *(t_ptr+1);

	if (validate_address(t_ptr))
	{
		f->eax = process_wait(tid);
	}
	else
	{
		exit(-1);
	}
}

static void sys_create(struct intr_frame *f)
{
	int *t_ptr;
	char *fn;

	t_ptr = f->esp;
	fn = *(t_ptr+1);

	if(validate_address(fn))
	{
		f->eax = filesys_create(fn, *(t_ptr+2));
	}
	else
	{
		exit(-1);
	}
}

static void sys_remove(struct intr_frame *f)
{
	int *t_ptr;
	char *fn;

	t_ptr = f->esp;
	fn = *(t_ptr+1);

	if (validate_address(fn))
	{
		f->eax = filesys_remove(fn);
	}
	else
	{
		exit(-1);
	}
}

static void sys_open(struct intr_frame *f)
{
	int *t_ptr;
	char *fn;
	struct file *temp_file;
	struct entry *fd_entry;

	t_ptr = f->esp;
	fn = *(t_ptr+1);

	if(!fn)
	{
		f->eax = -1;
		return;
	}
	else
	{
		if(!validate_address(fn))
		{
			exit(-1);
		}
		else
		{
			temp_file = filesys_open(fn);
			if (temp_file == NULL)
			{
				f->eax = -1;
				return;
			}
			else
			{
				fd_entry = (struct entry *) malloc(sizeof(struct entry));
				if (fd_entry)
				{
					fd_entry->file = temp_file;
					fd_entry->file_des = generate_descriptor();
					fd_entry->thread = thread_current();
					hash_insert(&entries, &fd_entry->elem);

					f->eax = fd_entry->file_des;
				}
				else
				{
					file_close(temp_file);
					exit(-1);
				}
			}
		}
	}
}

static void sys_filesize(struct intr_frame *f)
{
	int *t_ptr;
	int file_des;
	struct file *temp_file;

	t_ptr = f->esp;
	file_des = *(t_ptr+1);
	temp_file = get_file(file_des);

	if (!temp_file)
	{
		f->eax = -1;
	}
	else
	{
		f->eax = file_length(temp_file);
	}
}

static void sys_read(struct intr_frame *f)
{
	int *t_ptr;
	void *queue;
	int file_des;
	unsigned len;
	struct file *temp_file;

	t_ptr = f->esp;
	file_des = *(t_ptr+1);
	queue = *(t_ptr+2);
	len = *(t_ptr+3);

	lock_acquire(&file_lock);

	if (file_des == STDIN_FILENO)
	{
		unsigned i;
		for (i = 0; i < len; i++)
		{
			*(uint8_t *)(queue+i) = input_getc();
		}
		f->eax = len;
	}
	else if (file_des == STDOUT_FILENO)
	{
		f->eax = -1;
	}
	else if (!validate_address(queue) || !validate_address(queue+len))
	{
		lock_release(&file_lock);
		exit(-1);
		return;
	}
	else
	{
		temp_file = get_file(file_des);
		if(!temp_file)
		{
			f->eax = -1;
		}
		else
		{
			f->eax = file_read(temp_file, queue, len);
		}
	}
	lock_release(&file_lock);
}

static void sys_write(struct intr_frame *f)
{
	int *t_ptr;
	void *queue;
	int file_des;
	unsigned len;
	struct file *temp_file;

	t_ptr = f->esp;
	file_des = *(t_ptr+1);
	queue = *(t_ptr+2);
	len = *(t_ptr+3);

	lock_acquire(&file_lock);

	if (file_des == STDIN_FILENO)
	{
		f->eax = 0;
	}
	else if (file_des == STDOUT_FILENO)
	{
		putbuf(queue, len);
		f->eax = 1;
	}
	else if (!validate_address(queue) || !validate_address(queue+len))
	{
		lock_release(&file_lock);
		exit(-1);
		return;
	}
	else
	{
		temp_file = get_file(file_des);
		if(!temp_file)
		{
			f->eax = -1;
		}
		else
		{
			f->eax = file_write(temp_file, queue, len);
		}
	}
	lock_release(&file_lock);
}

static void sys_seek(struct intr_frame *f)
{
	int *t_ptr;
	int file_des;
	unsigned loc;
	struct file *temp_file;

	file_des = *(t_ptr+1);
	loc = *(t_ptr+2);

	if(temp_file)
	{
		file_seek(temp_file, loc);
		f->eax = 0;
	}
	else
	{
		f->eax = -1;
	}
}

static void sys_tell(struct intr_frame *f)
{
	int *t_ptr;
	int file_des;
	struct file *temp_file;

	t_ptr = f->esp;
	file_des = (t_ptr+1);
	temp_file = get_file(file_des);

	if(temp_file)
	{
		f->eax = 0;
		file_tell(temp_file);
	}
	else
	{
		f->eax = -1;
	}
}

static void sys_close(struct intr_frame *f)
{
	int *t_ptr;
	struct thread *thrd;
	int file_des;
	struct entry *fd_entry;

	t_ptr = f->esp;
	file_des = (int) *(t_ptr+1);
	fd_entry = get_entry(file_des);
	thrd = thread_current();

	if (fd_entry)
	{
		if (fd_entry->thread == thrd)
		{
			file_close(fd_entry->file);
			hash_delete(&entries, &fd_entry->elem);
		}
	}
	f->eax = 0;
}

static void exit(int status)
{
	struct thread *thrd;
  	thrd = thread_current();

  	file_close(thrd->file);
  	thrd->s_exit = status;
  	get_child(thrd->parent, thrd->tid)->s_exit = thrd->s_exit;

  	thread_exit();
}

static struct file *get_file(int file_des)
{
	struct entry *fd_entry;
	fd_entry = get_entry(file_des);

	if (!fd_entry)
	{
		return NULL;
	}
	else
	{
		return fd_entry->file;
	}
}

static struct entry *get_entry(int file_des)
{
	struct entry tmp;
	struct hash_elem *elem;
	struct entry *temp_item;

	tmp.file_des = file_des;
	elem = hash_find(&entries, &tmp.elem);

	if (!elem)
	{
		return NULL;
	}
	else
	{
	    temp_item = hash_entry(elem, struct entry, elem);
	    return temp_item;
	}
}

static int generate_descriptor(void)
{
	static int file_des = 3;
	return file_des++;
}

static bool validate_address(void *ptr)
{
	struct thread *thrd;
	thrd = thread_current();
	if(ptr != NULL)
	{
		if(is_user_vaddr(ptr))
		{
			if(pagedir_get_page(thrd->pagedir, ptr))
			{
				return true;
			}
		}
	}
	return false;
}

bool cmp_entries(const struct hash_elem *first, const struct hash_elem *second, void *aux)
{
	struct entry *first_entry;
	struct entry *second_entry;

	first_entry = hash_entry(first, struct entry, elem);
	second_entry = hash_entry(second, struct entry, elem);

	if (first_entry->file_des < second_entry->file_des)
	{
		return true;
	}
	return false;
}

unsigned hash_entries(const struct hash_elem *ptr, void *aux)
{
	struct entry *temp;
	temp = hash_entry(ptr, struct entry, elem);

	return hash_int(temp->file_des);
}

void syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  memset(syscall, 0, 20);

  syscall[SYS_HALT] = sys_halt;
  syscall[SYS_EXIT] = sys_exit;
  syscall[SYS_EXEC] = sys_exec;
  syscall[SYS_WAIT] = sys_wait;
  syscall[SYS_CREATE] = sys_create;
  syscall[SYS_REMOVE] = sys_remove;
  syscall[SYS_OPEN] = sys_open;
  syscall[SYS_FILESIZE] = sys_filesize;
  syscall[SYS_READ] = sys_read;
  syscall[SYS_WRITE] = sys_write;
  syscall[SYS_SEEK] = sys_seek;
  syscall[SYS_TELL] = sys_tell;
  syscall[SYS_CLOSE] = sys_close;

  lock_init(&file_lock);
  hash_init(&entries, hash_entries, cmp_entries, NULL);
}

static void syscall_handler (struct intr_frame *f UNUSED) 
{
  int *code;
  code = f->esp;

  if (!validate_address(code) || !(validate_address(code+1) && validate_address(code+2) && validate_address(code+3)) || *code < SYS_HALT && *code > SYS_CLOSE)
  {
  	exit(-1);
  }
  else
  {
  	syscall[*code](f);
  }
}
