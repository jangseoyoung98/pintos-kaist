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
// #include "user/syscall.h"

void syscall_entry(void);
void syscall_handler(struct intr_frame *);
void halt(void);
void exit(int status);
bool create(const char *file, unsigned initial_size);
bool remove(const char *file);

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
	case SYS_EXIT:
		exit(f->R.rdi);
	// case SYS_FORK:
	// 	fork(f->R.rdi, f->R.rsi);
	// case SYS_EXEC:
	// 	exec(f->R.rdi);
	// case SYS_WAIT:
	// 	wait(f->R.rdi);
	case SYS_CREATE:
		create(f->R.rdi, f->R.rsi);
	case SYS_REMOVE:
		remove(f->R.rdi);
		// case SYS_OPEN:
		// 	open(f->R.rdi);
		// case SYS_FILESIZE:
		// 	filesize(f->R.rdi);
		// case SYS_READ:
		// 	read(f->R.rdi, f->R.rsi);
		// case SYS_WRITE:
		// 	write(f->R.rdi, f->R.rsi);
		// case SYS_SEEK:
		// 	seek(f->R.rdi, f->R.rsi);
		// case SYS_TELL:
		// 	tell(f->R.rdi);
		// case SYS_CLOSE:
		// 	close(f->R.rdi);
	}
	printf("system call!\n");
	thread_exit();
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
	// syscall0(SYS_HALT);
	// NOT_REACHED();
}
void exit(int status)
{
	thread_exit();
	// syscall1(SYS_EXIT, status);
	// NOT_REACHED();
}
bool create(const char *file, unsigned initial_size)
{
	if (filesys_create(file, initial_size))
	{
		return true;
	}
	else
	{
		return false;
	}
	// return syscall2(SYS_CREATE, file, initial_size);
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