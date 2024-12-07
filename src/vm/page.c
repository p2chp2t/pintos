#include "vm/page.h"
#include "vm/swap.h"
#include "vm/frame.h"
#include "threads/thread.h"
#include "threads/synch.h"
#include "userprog/pagedir.h"
#include "threads/vaddr.h"
#include "filesys/file.h"
#include <hash.h>
#include <stdlib.h>
#include <bitmap.h>

extern struct lock frame_lock;
extern struct lock swap_lock;
extern struct bitmap *swap_bitmap;

void vm_init(struct hash *vm)
{
    hash_init(vm, vm_hash_func, vm_less_func, NULL);
}

static unsigned vm_hash_func(const struct hash_elem *e, void *aux)
{   
    struct vm_entry *vme = hash_entry(e, struct vm_entry, elem);
    return hash_int((int)vme->vaddr);
}

static bool vm_less_func(const struct hash_elem *a, const struct hash_elem *b, void *aux)
{   
    // return true if a address < b address, otw false
    void *a_vaddr = hash_entry(a, struct vm_entry, elem)->vaddr;
    void *b_vaddr = hash_entry(b, struct vm_entry, elem)->vaddr;
    return (a_vaddr < b_vaddr);
}

bool insert_vme(struct hash *vm, struct vm_entry *vme)
{
    if(!hash_insert(vm, &vme->elem)) {
        return true;
    } 
    else {
        return false;
    }
}

bool delete_vme(struct hash *vm, struct vm_entry *vme)
{   
    if(hash_delete(vm, &vme->elem)) {
        lock_acquire(&frame_lock);
        free_frame(pagedir_get_page(thread_current()->pagedir, vme->vaddr));
        free(vme);
        lock_release(&frame_lock);
        return true;
    }
    else {
        return false;
    }
}

struct vm_entry *find_vme(void *vaddr)
{
    struct vm_entry vme;
    vme.vaddr = pg_round_down(vaddr);
    struct hash *vm;
    vm = &thread_current()->vm;
    struct hash_elem *e;
    e = hash_find(vm, &vme.elem);
    if(e != NULL) {
        return hash_entry(e, struct vm_entry, elem);
    }
    else {
        return NULL;
    }
}

void vm_destroy(struct hash *vm)
{
    hash_destroy(vm, vm_destroy_func);
}

static void vm_destroy_func(struct hash_elem *e, void *aux)
{
    // destroy memory of vm_entry
    struct vm_entry *vme = hash_entry(e, struct vm_entry, elem);
    lock_acquire(&frame_lock);
    if(vme != NULL) {
        if(vme->is_loaded) {
            free_frame(pagedir_get_page(thread_current()->pagedir, vme->vaddr));
        }
        free(vme);
    }
    lock_release(&frame_lock);
}
