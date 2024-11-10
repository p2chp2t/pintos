#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"

/* Lab 2-3 Type definition added */
typedef int pid_t;

/* Lab 2-2 Function added */
void argument_stack(char *file_name, void **esp);

/* Lab 2-3 Function added */
struct thread* get_pd_child(pid_t pid);

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);

#endif /* userprog/process.h */
