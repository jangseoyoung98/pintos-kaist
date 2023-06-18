#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"
// 06.18 : 디버깅 1차
#include "filesys/file.h"
struct file;

tid_t process_create_initd (const char *file_name);
tid_t process_fork (const char *name, struct intr_frame *if_);
int process_exec (void *f_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (struct thread *next);
int process_add_file (struct file *f);
struct file *process_get_file(int fd);
void process_close_file(int fd);
void remove_child_process(struct thread *cp);
// 06.18 : 디버깅 2차
static bool load_segment(struct file *file, off_t ofs, uint8_t *upage, uint32_t read_bytes, uint32_t zero_bytes, bool writable);
static bool setup_stack(struct intr_frame *if_);

// 06.18 : 디버깅 1차
struct lazy_load_file{
    size_t page_read_bytes;
    size_t page_zero_bytes;
    struct file* file;
};

#endif /* userprog/process.h */