#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
/* Lab 2-3 Header added */
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "devices/shutdown.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "userprog/process.h"
#include "devices/input.h"
/* Lab 3-5 Header added */
#include "vm/frame.h"
#include "vm/page.h"
#include <list.h>

static void syscall_handler (struct intr_frame *);

/* Lab 2-3 Variable added */
struct lock f_lock;
/* Lab 3-5 Variable added */
extern struct lock frame_lock;

/* Lab 2-3 Function added & 3-4 modified */
void addr_check(void *addr) { 
  if(!addr || !is_user_vaddr(addr)) {
    syscall_exit(-1);
  }
}

void get_args(void *esp, int *arg, int count) {
  for (int i=0; i<count; i++) {
    addr_check(esp + 4*i);
    arg[i] = *(int*)(esp + 4*i);
  }
}

struct file *get_fd_file(int fd) {
  if( (2 <= fd) && (fd < thread_current()->fd_num) ) {
    return thread_current()->fd_table[fd];
  }
  else {
    return NULL; 
  }
}

void syscall_halt(void)
{
  shutdown_power_off();
}

void syscall_exit(int status)
{
  thread_current()->exit_status = status;
  printf("%s: exit(%d)\n", thread_name(), status);  /* Lab 2-1 */
  thread_exit();
}

pid_t syscall_exec(const char *cmd_line, void* esp)
{ 
  char *c = cmd_line;
  while (true) {
    addr_check(c);
    if(*c == '\0') {
      break;
    }
    c++;
  } // cmd_line check

  pid_t pid = process_execute(cmd_line);
  if(pid == -1) {
    return -1;
  } // make child process

  struct thread* child = get_pd_child(pid); // pd of child process
  sema_down(&(child->sema_load)); 
  if(!child->is_loaded) {
    return -1;
  }
  return pid; // wait and return pid of child
}

int syscall_wait(pid_t pid)
{
  return process_wait(pid);
}

bool syscall_create(const char *file, unsigned initial_size)
{ 
  addr_check((void*)file);
  if(file == NULL) {
    syscall_exit(-1);
  }  
  lock_acquire(&f_lock);
  bool success = filesys_create(file, initial_size);
  lock_release(&f_lock);
  return success;
}

bool syscall_remove(const char *file)
{ 
  addr_check((void*)file);
  lock_acquire(&f_lock);
  bool success = filesys_remove(file);
  lock_release(&f_lock);
  return success;
}

int syscall_open(const char *file)
{
  addr_check((void*)file);
  lock_acquire(&f_lock);

  struct file* f = filesys_open(file);
  if(!f) {
    lock_release(&f_lock);
    return -1;
  }

  struct thread* now = thread_current();
  if(!strcmp(now->name, file)) {
    file_deny_write(f); /* Lab 2-4 */
  }

  int fd = now->fd_num; // file descriptor allocation
  now->fd_num++;
  now->fd_table[fd] = f;

  lock_release(&f_lock);
  return fd;
}

int syscall_filesize(int fd)
{ 
  struct file *f = get_fd_file(fd);
  if(f == NULL) {
    return -1;
  }
  lock_acquire(&f_lock);
  int size = file_length(f);  // return filesize
  lock_release(&f_lock);
  return size;
}

int syscall_read(int fd, void *buffer, unsigned size, void *esp)
{
  for (int i = 0; i < size; i++){
    addr_check(buffer+i);
  }
  // pin & unpin
  pin_buffer(buffer, size, esp);
  int r_bytes = 0; // bytes read
  if(fd==0) { // if fd is 0, not file. console
    for (int j = 0; j < size; j++) {
      ((char*)buffer)[j] = input_getc();
      if(((char*)buffer)[j] == '\0') { 
        break; 
      }
      r_bytes = j;
    }
  } 
  else { // if fd>0, read from file
    struct file *f = get_fd_file(fd);
    if(!f) {
      return -1;
    }
    lock_acquire(&f_lock);
    r_bytes = file_read(f, buffer, size);
    lock_release(&f_lock);
  }
  unpin_buffer(buffer, size);

  return r_bytes;
}

int syscall_write(int fd, const void *buffer, unsigned size, void *esp)
{
  for (int i = 0; i < size; i++) {
    addr_check(buffer+i);
  }
  // pin & unpin
  pin_buffer(buffer, size, esp);
  int w_bytes = 0;
  if(fd == 1)
  {
    lock_acquire(&f_lock);
    putbuf(buffer, size);
    lock_release(&f_lock);
    w_bytes = size;
  }
  else if(fd > 1)
  {
    struct file *f = get_fd_file(fd);
    if(!f) {
      return -1;
    }
    lock_acquire(&f_lock);
    w_bytes += file_write(f, buffer, size);
    lock_release(&f_lock);
  }
  unpin_buffer(buffer, size);

  return w_bytes;

}

void syscall_seek(int fd, unsigned position)
{
  struct file* f = get_fd_file(fd);
  if(f == NULL) {return;}
  lock_acquire(&f_lock);
  file_seek(f, position);
  lock_release(&f_lock);
}

unsigned syscall_tell(int fd)
{
  unsigned position;
  struct file *f = get_fd_file(fd);
  lock_acquire(&f_lock);
  if (f) {
    position = file_tell(f);
  }
  else {
    position = 0;
  }
  lock_release(&f_lock);
  return position;
}

void syscall_close(int fd)
{
  struct file *f = get_fd_file(fd);
  if(f == NULL) { 
    return;
  }
  if((fd < thread_current()->fd_num)) {
    file_close(f);
    thread_current()->fd_table[fd] = NULL;
  }
  return;
}
/* END Lab 2-3 */

/* Lab 2-3 Function modified */
void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  lock_init(&f_lock);
}

/* Lab 2-3 & 3-5 Function modified */
static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  //printf ("system call!\n");
  //thread_exit ();
  /* check if the stack pointer is valid */
  for(int i = 0; i < 3; i++) {
    addr_check(f->esp + 4*i);
  }
  int argv[3];
  switch(*(uint32_t *)(f->esp)) {
    case SYS_HALT:
      syscall_halt();
      break;
    case SYS_EXIT:
      get_args(f->esp+4, &argv[0], 1);
      syscall_exit((int)argv[0]);
      break;
    case SYS_EXEC:
      get_args(f->esp+4, &argv[0], 1);
      f->eax = syscall_exec((const char*)argv[0], f->esp);
      break;
    case SYS_WAIT:
      get_args(f->esp+4, &argv[0], 1);
      f->eax = syscall_wait((pid_t)argv[0]);
      break;
    case SYS_CREATE:
      get_args(f->esp+4, &argv[0], 2);
      f->eax = syscall_create((const char*)argv[0], (unsigned)argv[1]);
      break;
    case SYS_REMOVE:
      get_args(f->esp+4, &argv[0], 1);
      f->eax = syscall_remove((const char*)argv[0]);
      break;
    case SYS_OPEN:
      get_args(f->esp+4, &argv[0], 1);
      f->eax = syscall_open((const char*)argv[0]);
      break;
    case SYS_FILESIZE:
      get_args(f->esp+4, &argv[0], 1);
      f->eax = syscall_filesize((int)argv[0]);
      break;
    case SYS_READ:
      get_args(f->esp+4, &argv[0], 3);
      f->eax = syscall_read((int)argv[0], (void *)argv[1], (unsigned)argv[2], f->esp);
      break;
    case SYS_WRITE:
      get_args(f->esp+4, &argv[0], 3);
      f->eax = syscall_write((int)argv[0], (const void *)argv[1], (unsigned)argv[2], f->esp);
      break;
    case SYS_SEEK:
      get_args(f->esp+4, &argv[0], 2);
      syscall_seek((int)argv[0], (unsigned)argv[1]);
      break;
    case SYS_TELL:
      get_args(f->esp+4, &argv[0], 1);
      f->eax = syscall_tell((int)argv[0]);
      break;
    case SYS_CLOSE:
      get_args(f->esp+4, &argv[0], 1);
      syscall_close((int)argv[0]);
      break;
    /* Lab 3-5 */
    case SYS_MMAP:
      get_args(f->esp+4, &argv[0], 2);
      f->eax = syscall_mmap(argv[0], (void*)argv[1]);
      break;
    case SYS_MUNMAP:
      get_args(f->esp+4, argv, 1);
      syscall_munmap(argv[0]);
      break;
    /* END Lab 3-5 */
    default:
      syscall_exit(-1);
  }
}

/* Lab 3-5 Function added */
mapid_t syscall_mmap(int fd, void* addr)
{
  if(!addr || !is_user_vaddr(addr) || pg_ofs(addr) != 0) {
    return -1;
  }

  struct file *f = get_fd_file(fd);
  if(f == NULL) {
    return -1;
  }
  size_t offset = 0;

  // allocate and initalize mmf
  struct mmap_file *mmf;
  mmf = (struct mmap_file*)malloc(sizeof(struct mmap_file));
  if(mmf == NULL) {
    return -1;
  }
  memset(mmf, 0, sizeof(struct mmap_file));

  struct thread *t = thread_current();
  mmf->mapid = t->mmap_next++;
  list_push_back(&t->mmap_list, &mmf->elem);

  lock_acquire(&f_lock);
  mmf->file = file_reopen(f);
  lock_release(&f_lock);
  
  list_init(&mmf->vme_list);
  int _file_length = file_length(mmf->file);
  while(_file_length > 0) {
    if(find_vme(addr)) {
      return -1;
    }
    struct vm_entry *vme = (struct vm_entry *)malloc(sizeof(struct vm_entry));
    if(!vme) {
      return -1;
    }
    memset(vme, 0, sizeof(struct vm_entry));

    size_t _read_bytes = _file_length < PGSIZE ? _file_length : PGSIZE;
    size_t _zero_bytes = PGSIZE - _read_bytes;

    vme->type = VM_FILE;
    vme->vaddr = addr;   
    vme->writable = true;
    vme->is_loaded = false;
    vme->file = mmf->file;
    vme->offset = offset;
    vme->read_bytes = _read_bytes;
    vme->zero_bytes = _zero_bytes;
    
    list_push_back(&mmf->vme_list, &vme->mmap_elem);
    insert_vme(&t->vm, vme);

    addr += PGSIZE;
    offset += PGSIZE;
    _file_length -= PGSIZE;
  }

  return mmf->mapid;
}

void syscall_munmap(mapid_t mapid)
{
  struct mmap_file *mmf = NULL;
  struct thread *t = thread_current();
  struct list_elem *e;
  for(e = list_begin(&t->mmap_list); e != list_end(&t->mmap_list); e = list_next(e)) {
    mmf = list_entry(e, struct mmap_file, elem);
    if(mmf->mapid == mapid) {
      break;
    }
  }
  if(mmf == NULL) { // no such mmap file
    return;
  }
  // remove all vm_entry in vme_list
  for(e = list_begin(&mmf->vme_list); e != list_end(&mmf->vme_list); ) {
    struct vm_entry *vme = list_entry(e, struct vm_entry, mmap_elem);
    if(vme->is_loaded && pagedir_is_dirty(t->pagedir, vme->vaddr)) {
      lock_acquire(&f_lock);
      file_write_at(vme->file, vme->vaddr, vme->read_bytes, vme->offset);
      lock_release(&f_lock);

      lock_acquire(&frame_lock);
      free_frame(pagedir_get_page(t->pagedir, vme->vaddr));
      lock_release(&frame_lock);
    }
    vme->is_loaded = false;
    e = list_remove(e);
    delete_vme(&t->vm, vme);
  }
  list_remove(&mmf->elem);
  free(mmf);
}
/* END Lab 3-5 */

// for pinning
void pin_buffer(void *buffer, int size, void *esp)
{
  void *ptr = buffer;     // buffer pointer
  size_t cur_size = size; 

  while (cur_size > 0) {
    struct vm_entry *vme = find_vme(pg_round_down(ptr));
    if(vme) {
      if(!vme->is_loaded) {
        if(!handle_mm_fault(vme)) {
          syscall_exit(-1);
        }
      }
    }
    else {
      if(!verify_stack(ptr, esp)) {
        syscall_exit(-1);
      }
      if(!expand_stack(ptr)) {
        syscall_exit(-1);
      }
    }
     
    lock_acquire(&frame_lock);
    struct frame *f = find_frame(pg_round_down(ptr));
    if(!f) {
      lock_release(&frame_lock);
      syscall_exit(-1);
    }
    else {
      pin_frame(f->phy_addr);
    }
    lock_release(&frame_lock);

    size_t pinned = cur_size > PGSIZE - pg_ofs(ptr) ? PGSIZE - pg_ofs(ptr) : cur_size;
    ptr += pinned;
    cur_size -= pinned;
  }
}

void unpin_buffer(void *buffer, int size)
{
  void *ptr = buffer;     // buffer pointer
  size_t cur_size = size; 

  while (cur_size > 0) {
    lock_acquire(&frame_lock);
    struct frame *f = find_frame(pg_round_down(ptr));
    if(!f) {
      lock_release(&frame_lock);
      syscall_exit(-1);
    }
    else {
      unpin_frame(f->phy_addr);
    }
    lock_release(&frame_lock);

    size_t unpinned = cur_size > PGSIZE - pg_ofs(ptr) ? PGSIZE - pg_ofs(ptr) : cur_size;
    ptr += unpinned;
    cur_size -= unpinned;
  }
}
