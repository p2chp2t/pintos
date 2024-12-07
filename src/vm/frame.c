#include "vm/frame.h"
#include "userprog/pagedir.h"
#include <list.h>
#include "threads/synch.h"
#include "threads/malloc.h"
#include <string.h>
#include "threads/vaddr.h"
#include "filesys/file.h"
#include "vm/swap.h"

struct list frame_table;
struct lock frame_lock;
struct list_elem *frame_clock;

extern struct lock f_lock;

void frame_table_init(void)
{
    list_init(&frame_table);
    lock_init(&frame_lock);
    frame_clock = NULL;
}

void add_frame_to_ft(struct frame *frame)
{   
    // add frame in the frame table
    list_push_back(&frame_table, &frame->frame_table_elem);
}

void del_frame_to_ft(struct frame *frame)
{   
    // delete frame from the frame table
    if(frame_clock == &frame->frame_table_elem) {
        frame_clock = list_remove(frame_clock);
    }
    else {
        list_remove(&frame->frame_table_elem);
    }
}

struct frame *allocate_frame(enum palloc_flags flags)
{
    if((flags & PAL_USER) == 0) {
        return NULL;
    }
    
    // allocate and initialize frame
    struct frame *f = (struct frame *)malloc(sizeof(struct frame));
    if(!f) {
        return NULL;
    }
    memset(f, 0, sizeof(struct frame));
    f->phy_addr = palloc_get_page(flags);
    while(!(f->phy_addr)) {
        evict_frame();
        f->phy_addr = palloc_get_page(flags);
    }
    ASSERT(pg_ofs(f->phy_addr) == 0);
    f->thread = thread_current();
    
    // add new frame to frame table
    add_frame_to_ft(f);
    return f;
}

void free_frame(void *kaddr)
{
    struct frame *f;
    struct list_elem *e;
    // find the frame in the frame table, then free
    for(e = list_begin(&frame_table); e != list_end(&frame_table); e = list_next(e)) {
        f = list_entry(e, struct frame, frame_table_elem);
        if(f->phy_addr == kaddr) {
            if(f != NULL) {
                f->frame_mapped_page->is_loaded = false;
                pagedir_clear_page(f->thread->pagedir, f->frame_mapped_page->vaddr);
                palloc_free_page(f->phy_addr);
                del_frame_to_ft(f);
                free(f);
            }
            break;
        }
    }
}

struct frame *get_victim(void)
{
    struct frame *f;
    struct list_elem *e;
    while(true) {
        if(!frame_clock || (frame_clock == list_end(&frame_table))) {
            if(list_empty(&frame_table)) {
                return NULL;
            }
            else{
                frame_clock = list_begin(&frame_table);
                e = list_begin(&frame_table);
            }
        }
        else {
            frame_clock = list_next(frame_clock);
            if(frame_clock == list_end(&frame_table)) {
                continue;
            }
            e = frame_clock;
        }
        f = list_entry(e, struct frame, frame_table_elem);
        if(!f->pinned) {
            if(!pagedir_is_accessed(f->thread->pagedir, f->frame_mapped_page->vaddr)) {
                return f;
            }
            else {
                pagedir_set_accessed(f->thread->pagedir, f->frame_mapped_page->vaddr, false);
            }
        }
    }
}

void evict_frame(void)
{
    struct frame *f = get_victim();
    bool dirty = pagedir_is_dirty(f->thread->pagedir, f->frame_mapped_page->vaddr);

    switch (f->frame_mapped_page->type)
    {
    case VM_FILE:
        if(dirty) {
            lock_acquire(&f_lock);
            file_write_at(f->frame_mapped_page->file, f->phy_addr, f->frame_mapped_page->read_bytes, f->frame_mapped_page->offset);
            lock_release(&f_lock);
        }
        break;
    case VM_BIN:
        if(dirty) {
            f->frame_mapped_page->swap_slot = swap_out(f->phy_addr);
            f->frame_mapped_page->type = VM_ANON;
        }
        break;
    case VM_ANON:
        f->frame_mapped_page->swap_slot = swap_out(f->phy_addr);
        break;
    default:
        break;
    }
    pagedir_clear_page(f->thread->pagedir, f->frame_mapped_page->vaddr);
    palloc_free_page(f->phy_addr);
    f->frame_mapped_page->is_loaded = false;
    del_frame_to_ft(f);
    free(f);
}

// for pinning
struct frame* find_frame(void* vaddr)
{
    struct list_elem *e;
	for (e = list_begin(&frame_table); e != list_end(&frame_table); e = list_next(e))
	{
		struct frame *f = list_entry(e, struct frame, frame_table_elem);
		if ((f->frame_mapped_page->vaddr) == vaddr)
			return f;
	}
	return NULL;
}

void pin_frame(void *addr)
{
    struct list_elem *e;
    for (e = list_begin(&frame_table); e != list_end(&frame_table); e = list_next(e)) {
        struct frame *f = list_entry(e, struct frame, frame_table_elem);
        if ((f->phy_addr) == addr) {
            f->pinned = true;
            break;
        }
    }
}

void unpin_frame(void *addr)
{
	struct list_elem *e;
    for (e = list_begin(&frame_table); e != list_end(&frame_table); e = list_next(e)) {
		struct frame *f = list_entry(e, struct frame, frame_table_elem);
		if ((f->phy_addr) == addr) {
			f->pinned = false;
            break;
		}
	}
}
