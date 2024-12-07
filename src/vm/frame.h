#ifndef VM_FRAME_H
#define VM_FRAME_H

#include "vm/page.h"
#include "threads/thread.h"
#include "threads/palloc.h"
#include <list.h>
#include "threads/synch.h"

struct frame
{
    struct vm_entry *frame_mapped_page;
    struct thread *thread;
    void *phy_addr;
    struct list_elem frame_table_elem;
	bool pinned;
};

void frame_table_init(void);
void add_frame_to_ft(struct frame *frame);
void del_frame_to_ft(struct frame *frame);
struct frame *allocate_frame(enum palloc_flags flags);
void free_frame(void *kaddr);

struct frame *get_victim(void);
void evict_frame(void);

// for pinning
struct frame* find_frame(void* vaddr);
void pin_frame(void *addr);
void unpin_frame(void *addr);

#endif
