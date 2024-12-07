#include "vm/swap.h"
#include <bitmap.h>
#include "threads/synch.h"
#include "devices/block.h"
#include "threads/vaddr.h"
#include "threads/interrupt.h"

#define SECTOR_NUM (PGSIZE/BLOCK_SECTOR_SIZE)

struct lock swap_lock;
struct block *swap_block;
struct bitmap *swap_bitmap;

void swap_init(void)
{
    lock_init(&swap_lock);
    swap_block = block_get_role(BLOCK_SWAP);
    if(!swap_block) return;
    swap_bitmap = bitmap_create(block_size(swap_block) / SECTOR_NUM);
    if(!swap_bitmap) return;
}

size_t swap_out(void *kaddr)
{   
    swap_block = block_get_role(BLOCK_SWAP);
    lock_acquire(&swap_lock);
    size_t slot_index = bitmap_scan_and_flip(swap_bitmap, 0, 1, false);
    if(slot_index == BITMAP_ERROR) {
        lock_release(&swap_lock);
        NOT_REACHED();
        return BITMAP_ERROR;
    }

    for(int i = 0; i < SECTOR_NUM; i++) {
        block_write(swap_block, slot_index*SECTOR_NUM + i, kaddr + BLOCK_SECTOR_SIZE*i);
    }
    lock_release(&swap_lock);
    return slot_index;
}

bool swap_in(size_t used_index, void *kaddr)
{
    swap_block = block_get_role(BLOCK_SWAP);
    lock_acquire(&swap_lock);
    for(int i = 0; i < SECTOR_NUM; i++) {
        block_read(swap_block, used_index*SECTOR_NUM + i, kaddr + BLOCK_SECTOR_SIZE*i);
    }
    bitmap_set(swap_bitmap, used_index, false);
    lock_release(&swap_lock);
    return true;
}

