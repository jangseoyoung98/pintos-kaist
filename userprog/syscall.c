#include "userprog/syscall.h"
#include <stdio.h>

#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"
// 추가 헤더 3개
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "userprog/process.h"
// #include "user/syscall.h"

void syscall_entry(void);
void syscall_handler(struct intr_frame *);
void halt(void);
void exit(int status);
// pid_t fork(const char *thread_name);
int exec(const char *cmd_line);
bool create(const char *file, unsigned initial_size);
bool remove(const char *file);
int open(const char *file);
int filesize(int fd);
int write(int fd, const void *buffer, unsigned size);
void close(int fd);
int process_add_file(struct file *file);
struct file *process_get_file(int fd);

/* System call.
 *
 * Previously system call services was handled by the interrupt handler
 * (e.g. int 0x80 in linux). However, in x86-64, the manufacturer supplies
 * efficient path for requesting the system call, the `syscall` instruction.
 *
 * The syscall instruction works by reading the values from the the Model
 * Specific Register (MSR). For the details, see the manual. */

#define MSR_STAR 0xc0000081			/* Segment selector msr */
#define MSR_LSTAR 0xc0000082		/* Long mode SYSCALL target */
#define MSR_SYSCALL_MASK 0xc0000084 /* Mask for the eflags */

void syscall_init(void)
{
	write_msr(MSR_STAR, ((uint64_t)SEL_UCSEG - 0x10) << 48 |
							((uint64_t)SEL_KCSEG) << 32);
	write_msr(MSR_LSTAR, (uint64_t)syscall_entry);

	/* The interrupt service rountine should not serve any interrupts
	 * until the syscall_entry swaps the userland stack to the kernel
	 * mode stack. Therefore, we masked the FLAG_FL. */
	write_msr(MSR_SYSCALL_MASK,
			  FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);
	// 06/07 수정
	//  lock_init(&filesys_lock);
}

/* The main system call interface */
// 우리가 써야되는거 핀토스 종료함수 power_off, 스레드 종료함수 thread_exit,
// 파일이름과 파일 사이즈 인자로 받아서 생성 함수 filesys_create(const char*name,off_t initial_size)
// 파일이름에 해당하는 파일 제거 함수 filesys_remove
void syscall_handler(struct intr_frame *f UNUSED)
{
	// number는 시스템 콜 넘버
	// rdi-rsi-rdx-r10-r8-r9 순서로 전달됨
	int number = f->R.rax;
	// TODO: Your implementation goes here.
	switch (number)
	{
	case SYS_HALT:
		halt();
		break;
	case SYS_EXIT:
		exit(f->R.rdi);
		break;
	// case SYS_FORK:
	// 	fork(f->R.rdi, f->R.rsi);
	case SYS_EXEC:
		exec(f->R.rdi);
		break;
		// case SYS_WAIT:
		// 	wait(f->R.rdi);

	case SYS_CREATE:
		create(f->R.rdi, f->R.rsi);
		break;
	case SYS_REMOVE:
		remove(f->R.rdi);
		break;
	// case SYS_OPEN:
	// 	open(f->R.rdi);
	// case SYS_FILESIZE:
	// 	filesize(f->R.rdi);
	// case SYS_READ:
	// 	read(f->R.rdi, f->R.rsi);
	case SYS_WRITE:
		write(f->R.rdi, f->R.rsi, f->R.rdx);
		break;
	// case SYS_SEEK:
	// 	seek(f->R.rdi, f->R.rsi);
	// case SYS_TELL:
	// 	tell(f->R.rdi);
	case SYS_CLOSE:
		close(f->R.rdi);
		break;
	}
}
// 주소값이 유저 영역 주소값인지 확인 , 유저 영역 벗어나면 프로세스 종료 -1
void check_address(void *addr)
{
	if (is_user_vaddr(addr) == false || addr == NULL)
	{
		return exit(-1);
	}
}
void halt(void)
{
	power_off();
}
void exit(int status)
{
	struct thread *t = thread_current();
	t->exit_status = status;
	printf("%s: exit(%d)\n", t->name, status);
	thread_exit();
}
// fork 자리
/*
현재의 프로세스가 cmd_line에서 이름이 주어지는 실행가능한 프로세스로 변경됩니다. 이때 주어진 인자들을 전달합니다.
성공적으로 진행된다면 어떤 것도 반환하지 않음
*/
int exec(const char *cmd_line)
{
	check_address(cmd_line);
	return process_exec(cmd_line);
}
bool create(const char *file, unsigned initial_size)
{
	check_address(file);
	return filesys_create(file, initial_size);
}
bool remove(const char *file)
{
	if (filesys_remove(file))
	{
		return true;
	}
	else
	{
		return false;
	}
	// return syscall1(SYS_REMOVE, file);
}
// 파일을 열 때 사용하는 시스템 콜
// 파일이 없을 경우 실패
// 성공 시 fd를 생성하고 반환, 실패 시 -1 반환
// File : 파일의 이름 및 경로 정보
int open(const char *file)
{
	check_address(file);
	struct file *file_object = filesys_open(file); // 열려고 하는 파일 정보를 filesys_open()으로 받기

	// 파일 없으면 -1
	if (file_object == NULL)
	{
		return -1;
	}
	int fd = process_add_file(file_object);
	return fd;
}
//  파일의 크기를 알려주는 시스템 콜
//  성공 시 파일의 크기를 반환, 실패 시 -1 반환
int filesize(int fd)
{
	// int size = file_length(fd);
	// if()
	// return size;
}

int write(int fd, const void *buffer, unsigned size)
{
	struct thread *t = thread_current();
	struct file *file = process_get_file(fd);

	if (fd == 1)
	{
		putbuf(buffer, size);
		return size;
	}
	else
	{
		return file_write(file, buffer, size);
	}
}
int process_add_file(struct file *file)
{
	struct thread *t = thread_current();
	struct file **fdt = t->fdt;
	int fd = t->next_fd; // fd값은 2부터 출발

	while (t->fdt[fd] != NULL)
	{
		fd++;
	}
	t->next_fd = fd;
	fdt[fd] = file;
	return fd;
}

struct file *process_get_file(int fd)
{
	struct thread *t = thread_current();
	struct file **file_dt = t->fdt;
	struct file *file = file_dt[fd];
	return file;
}
void close(int fd)
{
	struct file *file = process_get_file(fd);
	file_close(fd);
}