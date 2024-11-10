#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

/* Lab 2-3 Header & Type definition & Function added */
#include <stdbool.h>

typedef int pid_t;

void addr_check(void *addr);//check whether the addr is valid. (in user stack?)
void get_args(void *esp, int *arg, int count);//get arguments
struct file *get_fd_file(int fd);

void syscall_halt(void);
void syscall_exit(int status);
pid_t syscall_exec(const char *cmd_line);
int syscall_wait(pid_t pid);
bool syscall_create(const char *file, unsigned initial_size);
bool syscall_remove(const char *file);
int syscall_open(const char *file);
int syscall_filesize(int fd);
int syscall_read (int fd, void *buffer, unsigned size, void* esp);
int syscall_write (int fd, const void *buffer, unsigned size, void* esp);
void syscall_seek(int fd, unsigned position);
unsigned syscall_tell(int fd);
void syscall_close(int fd);
/* END Lab 2-3 */

void syscall_init (void);

#endif /* userprog/syscall.h */
