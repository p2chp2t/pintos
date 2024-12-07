#ifndef VM_SWAP_H
#define VM_SWAP_H

#include <stddef.h>
#include <stdbool.h>

void swap_init(void);
size_t swap_out(void *kaddr);
bool swap_in(size_t used_index, void *kaddr);

#endif
