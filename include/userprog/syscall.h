#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

void syscall_init(void);
void check_address(void *addr);
// 06/07 추가 함수
// struct lock *filesys_lock;
// void syscall_handler(struct intr_frame *);
#endif
