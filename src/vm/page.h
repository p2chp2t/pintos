#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <hash.h>
#include <list.h>
#include "filesys/off_t.h"

#define VM_BIN 0
#define VM_FILE 1
#define VM_ANON 2

/* Lab 3-3 */
struct vm_entry
{
    uint8_t type;
    void *vaddr;
    bool writable;
    bool is_loaded;
    struct file *file;
    size_t offset;
    size_t read_bytes;
    size_t zero_bytes;
    struct hash_elem elem;
    struct list_elem mmap_elem;
    size_t swap_slot;
};

void vm_init(struct hash *vm);
static unsigned vm_hash_func(const struct hash_elem *e, void *aux);
static bool vm_less_func(const struct hash_elem *a, const struct hash_elem *b, void *aux);
bool insert_vme(struct hash *vm, struct vm_entry *vme);
bool delete_vme(struct hash *vm, struct vm_entry *vme);
struct vm_entry *find_vme(void *vaddr);
static void vm_destroy_func(struct hash_elem *e, void *aux);
void vm_destroy(struct hash *vm);

/* Lab 3-5 */
struct mmap_file
{
    int mapid;
    struct file *file;
    struct list_elem elem;
    struct list vme_list;
};

#endif
