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

static void syscall_handler (struct intr_frame *);

/* Lab 2-3 Variable added */
struct lock f_lock;

/* Lab 2-3 Function added */
void addr_check(void *addr) { 
  if (!is_user_vaddr(addr)|| !pagedir_get_page(thread_current()->pagedir, addr)) 
    syscall_exit(-1);
}

void get_args(void *esp, int *arg, int count) {
  for (int i=0; i<count; i++) {
    addr_check(esp + 4*i);
    arg[i] = *(int*)(esp + 4*i);
  }
}

struct file *get_fd_file(int fd) {
  struct file *f;
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

pid_t syscall_exec(const char *cmd_line)
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
  addr_check(file);
  if(file == NULL) {
    syscall_exit(-1);
  }
  return filesys_create(file, initial_size);
}

bool syscall_remove(const char *file)
{
  addr_check((void*)file);
  return filesys_remove(file);
}

int syscall_open(const char *file)
{
  addr_check(file);
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
  return file_length(f);  // return filesize
}

int syscall_read(int fd, void *buffer, unsigned size, void *esp)
{
  for (int i = 0; i < size; i++){
    addr_check(buffer+i);
  }
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
  return r_bytes;
}

int syscall_write(int fd, const void *buffer, unsigned size, void *esp)
{
  for(int i = 0; i < size; i++) {
    addr_check(buffer + i);
  }
  int w_bytes = 0;
  if(fd == 1) {
    lock_acquire(&f_lock);
    putbuf(buffer, size);
    lock_release(&f_lock);
    w_bytes = size;
  }
  else if(fd > 1) {
    struct file* f = get_fd_file(fd);
    if(f==NULL) {
      return -1;
    }
    lock_acquire(&f_lock);
    w_bytes = file_write(f, buffer, size);
    lock_release(&f_lock);
 }
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
  struct file *f = get_fd_file(fd);
  if (f) {
    return file_tell(f);
  }
  return 0;
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

/* Lab 2-3 Function modified */
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
      f->eax = syscall_exec((const char*)argv[0]);
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
    default:
      syscall_exit(-1);
  }
}
