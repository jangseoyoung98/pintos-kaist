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
#include "kernel/stdio.h"
#include "threads/synch.h"
// #include "user/syscall.h"
typedef int pid_t;

void syscall_entry(void);
void syscall_handler(struct intr_frame *);
void halt(void);
void exit(int status);
pid_t fork(const char *thread_name);
int exec(const char *file);
bool create(const char *file, unsigned initial_size);
bool remove(const char *file);
int open(const char *file);
int filesize(int fd);
int write(int fd, const void *buffer, unsigned size);
void close(int fd);
int process_add_file(struct file *file);
struct file *process_get_file(int fd);
unsigned tell(int fd);
int wait(pid_t temp);



/* 시스템 콜 실행 전반적인 흐름
 * int main() : pintos 부팅 시작
 * thread_init() : main 스레드 실행
 * palloc_init() : 스레드, 콘솔, malloc, 페이지 할당 초기화
 * tss_init() : 유저 프로세스에 대응하는 커널 프로세스에 대해 해당 커널 스택 포인터 끝
 * fsutil_put() : 하드 디스크 내 파일 시스템에 명령어 복사 후 넣어줌
 * run_action() -> run_task() : args-single-onearg에 대한 프로세스 생성
 * process_create_initd() -> initd() : 첫 유저 프로세스 실행
 *
 * process_exec() : Argiment Passing
 * 
 * do_iret()
 * args.c -> main() 
 * msg() ->vmsg() ->write() : 첫 시스템 콜
 * syscall()
 * syscall-entry.S
 * syscall_handler() : 시스템 콜 처리
*/

#define MSR_STAR 0xc0000081			/* Segment selector msr */
#define MSR_LSTAR 0xc0000082		/* Long mode SYSCALL target */
#define MSR_SYSCALL_MASK 0xc0000084 /* Mask for the eflags */

struct lock filesys_lock;

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
	 lock_init(&filesys_lock);
}

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
    case SYS_CREATE:
		f->R.rax = create(f->R.rdi, f->R.rsi);
        break;
    case SYS_OPEN:
        f->R.rax = open(f->R.rdi);
        break;
    case SYS_READ:
        f->R.rax = read(f->R.rdi, f->R.rsi, f->R.rdx);
        break;
    case SYS_WRITE:
        f->R.rax = write(f->R.rdi, f->R.rsi, f->R.rdx);
        break;
    case SYS_CLOSE:
        close(f->R.rdi);
        break;
    case SYS_FILESIZE:
        f->R.rax = filesize(f->R.rdi);
        break;
    case SYS_REMOVE:
        if(!(remove(f->R.rdi))){ // 수정
            exit(-1);
        }
        break;
    case SYS_SEEK:
        seek(f->R.rdi, f->R.rsi);
        break;
    case SYS_TELL:
        f->R.rax = tell(f->R.rdi);
        break;
    }
    // case SYS_FORK:
    //  fork(f->R.rdi, f->R.rsi);
    //  break;
    // case SYS_EXEC: // :벌레: 일부 성공..?
    //  exec(f->R.rdi);
    //  break;
    // case SYS_WAIT:
    //  wait(f->R.rdi);
    //  break;
}

// 1) ✅ pass 완료 : halt/exit
void halt(void) // ▶ 핀토스 종료
{
	power_off();
}
void exit(int status) // ▶ running 스레드 종료
{
	struct thread *t = thread_current();
	t->exit_status = status;
	printf("%s: exit(%d)\n", t->name, status);
	thread_exit();
}

// 2) 🐛 FAIL + 수정 필요
bool create(const char *file, unsigned initial_size) // ▶ 파일 생성
{	// FAIL : empty/bad-ptr/long/exits
	
	/* 
	 * 필요 : check_address(), filesys_create()
	 * 동작 : 성공 true, 실패 false 리턴
	*/

	// check_address(file);
	// return filesys_create(file, initial_size);
	check_address(file);
	bool isCreated = filesys_create(file, initial_size);
	// if(!isCreated){
	// 	printf("파일 생성 실패");
	// }
	return isCreated;
}
int open(const char *file) // ▶ 파일을 열기
{	// FAIL : missing/empty/bad-ptr/twice

	/* 
	 * 필요 : check_address(), filesys_create()
	 * 성공 시 fd를 생성하고 반환, 실패 시 -1 반환
	 * File : 파일의 이름 및 경로 정보
	*/

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
int read(int fd, void *buffer, unsigned length) // ▶ 해당 파일로부터 값을 읽어 버퍼에 넣음
{
	/* 
	 * 매개변수 : 파일 디스크립터, 버퍼, 버퍼 사이즈
	 * 필요 : check_address(), filesys_read(), input_getc/putbuf -> fd 0 또는 1 
	 * 버퍼가 유효한 주소인지 체크
	 * fd로 파일 객체를 찾음
	 * fd가 0과 1인 경우에 대해 처리
	 * (fd가 0과 1이 아닌 경우) length 바이트 크기 만큼 파일을 읽어 버퍼에 넣음
	 * lock으로 protection 수행
	*/
	check_address(buffer);

	char *buf = buffer;
	if (fd == 0)
	{
		char key;
		char **pbuf = &buf;
		int len= 0;
		for(int i = 0; i < length; i++) {
			key = input_getc();
			*pbuf = key;
			pbuf++;
			if (key == NULL)
			{
				*pbuf = '\0';
				break;
			}
			len++;
		}
		return len+1;
	}
	struct file *file = process_get_file(fd);
	if(file == NULL) 
	{
		return -1;
	}
	else
	{
		lock_acquire(&filesys_lock);
		return file_read(file, buffer, length);
		lock_release(&filesys_lock);
	}

}
int write(int fd, const void *buffer, unsigned size) // ▶ 파일에 쓰기 
{	// FAIL : 전부

	/* fd가 1인 경우, 화면에 출력 처리 (버퍼에 있는 값에서 size만큼 출력)
	 * fd가 0인 경우, -1 리턴
	 * fd가 1과 0이 아닌 경우, 버퍼로부터 size 만큼 값을 읽어와 해당 파일에 작성
	 * lock으로 protection 수행
	*/
	// struct thread *t = thread_current();
	// struct file *file = process_get_file(fd);

	// if (fd == 1)
	// {
	// 	putbuf(buffer, size);
	// 	return size;
	// }
	// else
	// {
	// 	return file_write(file, buffer, size);
	// }
	if (fd == 1)
    {
        putbuf(buffer, size);
        return size;
    }
    else if (fd == 0)
    {
        return -1;
    }
    else
    {
        struct file *file = process_get_file(fd);
        if (file == NULL)
        {
            return -1;
        }
        lock_acquire(&filesys_lock);
        return file_write(file, buffer, size);
        lock_release(&filesys_lock);
    }
    return size;
}

void close(int fd) // :앞쪽_화살표: 해당 파일을 닫기 // :불:전체 수정
{   // FAIL : 전부
    /* 필요 : filesys_close()
     * fd로 file 포인터를 찾아 해당 파일을 종료
     * 이때, 파일 디스크립터 테이블 내에 파일 포인터를 제거하는 방향으로 진행
    */
    struct thread *t = thread_current();
    struct file *file = process_get_file(fd);
    // (추가) fd에 위치한 파일을 fdt에서 제거한다.
    t->fdt[fd] = NULL;
    t->next_fd--;
    // check_address(file);
    file_close(file);
}

// 3) 📌 구현 필요 : fork/exec/wait
pid_t fork(const char *thread_name)
{
/*
현재의 프로세스가 cmd_line에서 이름이 주어지는 실행가능한 프로세스로 변경됩니다. 이때 주어진 인자들을 전달합니다.
성공적으로 진행된다면 어떤 것도 반환하지 않음
*/ 
	return 1;
}
// int exec(const char *cmd_line)
// {
// 	check_address(cmd_line);
// 	return process_exec(cmd_line);
// }
int exec(const char *file)
{
	check_address(file);
	return process_exec(file);
}

int wait(pid_t temp)
{
	return 1;
}

/* ****************** 함수 구현 명시 O + p/f 상관 X 함수 ****************** */

int filesize(int fd)
{
    //  파일의 크기를 알려주는 시스템 콜
    //  성공 시 파일의 크기를 반환, 실패 시 -1 반환
    struct file *file = process_get_file(fd);
    int size = file_length(file);
    if (size == NULL)
    {
        return -1;
    }
    return size;
}
bool remove(const char *file) // ▶ 파일을 제거
{
	/* 
	 * 필요 : check_address(), filesys_remove()
	 * 동작 : 성공 true, 실패 false 리턴
	*/

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
void seek (int fd, unsigned position) // ▶ 파일을 작성할 position을 찾음
{
    /* 필요 : file_seek()
     * fd로 파일을 찾고
     * 파일 객체의 pos를 입력받은 position으로 변경한다.
     */
    struct file *file = process_get_file(fd);
    return file_seek(file, tell(fd));
}

unsigned tell(int fd) // ▶ 파일을 읽어야 할 위치를 찾음
{
	/* 필요 : file_tell()
	 * fd로 파일을 찾아 pos를 리턴한다.
	*/
}

/* ****************** 함수 구현 명시 X + p/f 상관 X 함수 ****************** */

void check_address(void *addr)
{
	// 주소값이 유저 영역 주소값인지 확인, 유저 영역 벗어나면 프로세스 종료 -1
	if (is_user_vaddr(addr) == false || addr == NULL)
	{
		return exit(-1);
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

