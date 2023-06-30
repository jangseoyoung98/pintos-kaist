#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/palloc.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/init.h"
#include "threads/flags.h"
#include "intrinsic.h"
#include "filesys/filesys.h"
#include "user/syscall.h"
#include "threads/vaddr.h"
#include "threads/synch.h"
#include "filesys/file.h"
#include "userprog/process.h"
#include <string.h>


void syscall_entry(void);
void syscall_handler(struct intr_frame *);
void check_address(void *addr);
// struct page* check_address(void *addr);

void get_argument(void *rsp, int *arg, int count);
void halt(void);
void exit(int status);
pid_t fork(const char *thread_name);
int exec(const char *cmd_line);
int wait(pid_t pid);
bool create(const char *file, unsigned initial_size);
bool remove(const char *file);
int open(const char *file);
int filesize(int fd);
int read(int fd, void *buffer, unsigned size);
int write(int fd, const void *buffer, unsigned size);
void seek(int fd, unsigned position);
unsigned tell(int fd);
void close(int fd);
// void check_address(void *addr);
int process_add_file(struct file *f);
struct file *process_get_file(int fd);
void check_valid_string (const void* str);
void check_valid_buffer (void* buffer, unsigned size);
void *mmap (void *addr, size_t length, int writable, int fd, off_t offset); 
void munmap(void* addr); //🔥 MMF : 헤더 빠트리지 않기..!!!

/* System call.
 *
 * Previously system call services was handled by the interrupt handler
 * (e.g. int 0x80 in linux). However, in x86-64, the manufacturer supplies
 * efficient path for requesting the system call, the `syscall` instruction.
 *
 * The syscall instruction works by reading the values from the the Model
 * Specific Register (MSR). For the details, see the manual. */

#define MSR_STAR 0xc0000081         /* Segment selector msr */
#define MSR_LSTAR 0xc0000082        /* Long mode SYSCALL target */
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
   lock_init(&filesys_lock);
}

/* The main system call interface */
void syscall_handler(struct intr_frame *f)
{
   // TODO: Your implementation goes here.
   check_address(f->rsp);
   struct thread *cur = thread_current();
   memcpy(&cur->tf, f, sizeof(struct intr_frame));
   int syscall_num = f->R.rax;
   switch (syscall_num)
   {
   case SYS_HALT: /* Halt the operating system. */
      halt();
      break;
   case SYS_EXIT: /* Terminate this process. */
      exit(f->R.rdi);
      break;
   case SYS_FORK: /* Clone current process. */
      f->R.rax = fork(f->R.rdi);
      break;
   case SYS_EXEC: /* Switch current process. */
      f->R.rax = exec(f->R.rdi);
      if(f->R.rax == -1)
         exit(-1);
      break;
   case SYS_WAIT: /* Wait for a child process to die. */
      f->R.rax = wait(f->R.rdi);
      break;
   case SYS_CREATE: /* Create a file. */
      f->R.rax = create(f->R.rdi, f->R.rsi);
      break;
   case SYS_REMOVE: /* Delete a file. */
      f->R.rax = remove(f->R.rdi);
      break;
   case SYS_OPEN: /* Open a file. */
      f->R.rax = open(f->R.rdi);
      break;
   case SYS_FILESIZE: /* Obtain a file's size. */
      f->R.rax = filesize(f->R.rdi);
      break;
   case SYS_READ: /* Read from a file. */
      f->R.rax = read(f->R.rdi, f->R.rsi, f->R.rdx);
      break;
   case SYS_WRITE: /* Write to a file. */
      f->R.rax = write(f->R.rdi, f->R.rsi, f->R.rdx);
      break;
   case SYS_SEEK: /* Change position in a file. */
      seek(f->R.rdi, f->R.rsi);
      break;
   case SYS_TELL: /* Report current position in a file. */
      f->R.rax = tell(f->R.rdi);
      break;
   case SYS_CLOSE: /* Close a file. */
      close(f->R.rdi);
      break;
   case SYS_MMAP:
      f->R.rax = mmap(f->R.rdi, f->R.rsi, f->R.rdx, f->R.r10, f->R.r8);
      break;
   case SYS_MUNMAP:
      munmap(f->R.rdi);
      break;
   default:
      thread_exit();
   }
}

/*
power_off()를 호출해서 Pintos를 종료합니다. (power_off()는 src/include/threads/init.h에 선언되어 있음.)
*/
void halt(void)
{
   power_off();
}
/*
현재 동작중인 유저 프로그램을 종료합니다. 커널에 상태를 리턴하면서 종료합니다.
만약 부모 프로세스가 현재 유저 프로그램의 종료를 기다리던 중이라면,
그 말은 종료되면서 리턴될 그 상태를 기다린다는 것 입니다.
관례적으로, 상태 = 0 은 성공을 뜻하고 0 이 아닌 값들은 에러를 뜻 합니다.
*/
void exit(int status)
{
   struct thread *cur = thread_current();
   cur->exit_flag = status;
   printf("%s: exit(%d)\n", cur->name, status);
   thread_exit();
}
/*
위의 함수는 file(첫 번째 인자)를 이름으로 하고 크기가 initial_size(두 번째 인자)인 새로운 파일을 생성합니다.
*/
bool create(const char *file, unsigned initial_size)
{
   check_valid_string(file);
   return filesys_create(file, initial_size);
}

/*
위의 함수는 file(첫 번째)라는 이름을 가진 파일을 삭제합니다.
성공적으로 삭제했다면 true를 반환하고, 그렇지 않으면 false를 반환합니다.
파일은 열려있는지 닫혀있는지 여부와 관계없이 삭제될 수 있고,
파일을 삭제하는 것이 그 파일을 닫았다는 것을 의미하지는 않습니다.
*/
bool remove(const char *file)
{
   check_address(file);
   return filesys_remove(file);
}

/*
THREAD_NAME이라는 이름을 가진 현재 프로세스의 복제본인 새 프로세스를 만듭니다
피호출자(callee) 저장 레지스터인 %RBX, %RSP, %RBP와 %R12 - %R15를 제외한 레지스터 값을 복제할 필요가 없습니다.
 자식 프로세스의 pid를 반환해야 합니다.
*/
pid_t fork(const char *thread_name)
{
   struct thread *cur = thread_current();
   return process_fork(thread_name, &cur->tf);
}
/*
현재의 프로세스가 cmd_line에서 이름이 주어지는 실행가능한 프로세스로 변경됩니다. 이때 주어진 인자들을 전달합니다.
성공적으로 진행된다면 어떤 것도 반환하지 않습니다.
*/
int exec(const char *cmd_line)
{
   char *fn_copy;
   tid_t tid;

   // // 06.22 : exec-missing 디버깅
   // struct thread* temp = thread_current();
   // if(temp == NULL)
   //    return -1;

   check_valid_string(cmd_line);

   fn_copy = palloc_get_page(PAL_ZERO);
   if (fn_copy == NULL)
      return TID_ERROR;
   strlcpy(fn_copy, cmd_line, PGSIZE);
   tid = process_exec(fn_copy);
   if (tid == -1)
   {
      return TID_ERROR;
   }
   palloc_free_page(fn_copy);

   // 페이지 폴트 시에 -1을 리턴해야 함

   return tid;
}
/*
자식 프로세스 (pid) 를 기다려서 자식의 종료 상태(exit status)를 가져옵니다.
만약 pid (자식 프로세스)가 아직 살아있으면, 종료 될 때 까지 기다립니다.
종료가 되면 그 프로세스가 exit 함수로 전달해준 상태(exit status)를 반환합니다.
*/
int wait(pid_t pid)
{
   // return 81;
   return process_wait(pid);
}
/*
file(첫 번째 인자)이라는 이름을 가진 파일을 엽니다. 해당 파일이 성공적으로 열렸다면,
파일 식별자로 불리는 비음수 정수(0또는 양수)를 반환하고, 실패했다면 -1를 반환합니다.
*/
int open(const char *file)
{
   check_valid_string(file);
   struct file *open_file = filesys_open(file);
   if (open_file == NULL)
   {
      return -1;
   }
   int fd = process_add_file(open_file);
   if (fd == -1)
   {
      file_close(open_file);
      exit(-1);
   }
   return fd;
}
/*
fd(첫 번째 인자)로서 열려 있는 파일의 크기가 몇 바이트인지 반환합니다.
*/
int filesize(int fd)
{

   struct file *find_file = process_get_file(fd);
   if (find_file == NULL)
      return -1;
   return file_length(find_file);
}
/*
buffer 안에 fd 로 열려있는 파일로부터 size 바이트를 읽습니다.
실제로 읽어낸 바이트의 수 를 반환합니다 (파일 끝에서 시도하면 0).
파일이 읽어질 수 없었다면 -1을 반환합니다.
*/
int read(int fd, void *buffer, unsigned size)
{
   check_valid_buffer(buffer, size);

   int file_size;
   char *read_buffer = buffer;
   if (fd == 0)
   {
      char key;
      for (file_size = 0; file_size < size; file_size++)
      {
         key = input_getc();
         *read_buffer++ = key;
         if (key == '\0')
         {
            break;
         }
      }
   }
   else if (fd == 1)
   {
      return -1;
   }
   else
   {
      struct file *read_file = process_get_file(fd);
      if (read_file == NULL)
      {
         return -1;
      }
      lock_acquire(&filesys_lock);
      file_size = file_read(read_file, buffer, size);
      lock_release(&filesys_lock);
   }
   return file_size;
}
/*
buffer로부터 open file fd로 size 바이트를 적어줍니다.
실제로 적힌 바이트의 수를 반환해주고,
일부 바이트가 적히지 못했다면 size보다 더 작은 바이트 수가 반환될 수 있습니다.
*/
int write(int fd, const void *buffer, unsigned size)
{
   // check_valid_string(buffer);
   check_valid_buffer(buffer, size);

   int file_size;
   // 06.22 : 디버깅 추가
   // 1) fd가 -1인지 확인 -> 앞서, open이 제대로 안 열린 것임
   // if(fd == -1)
   //    exit(-1);
   // 2) buffer에 내용이 담겨 있는지 확인 (size 만큼의 정보가 있는지!)
   // printf("####################버퍼 내용 : %s ###################\n\n", buffer);
   // printf("####################버퍼 주소 : %p ###################\n\n", buffer);
   // printf("####################fd : %d ###################\n\n", fd);

   if (fd == STDOUT_FILENO)
   {
      putbuf(buffer, size);
      file_size = size;
   }
   else if (fd == STDIN_FILENO)
   {
      return -1;
   }
   else
   {
      // 06.21
      if (fd < FD_MIN || fd >= FD_MAX)
      {
         exit(-1);
         return -1;
      }
      lock_acquire(&filesys_lock);
      file_size = file_write(process_get_file(fd), buffer, size);
      lock_release(&filesys_lock);
   }
   return file_size;
}

/*
open file fd에서 읽거나 쓸 다음 바이트를 position으로 변경합니다.
position은 파일 시작부터 바이트 단위로 표시됩니다.
(따라서 position 0은 파일의 시작을 의미합니다).
*/
void seek(int fd, unsigned position)
{
   /*
   Changes the next byte to be read or written in open file fd to position.
   Use void file_seek(struct file *file, off_t new_pos).
   */
   struct file *seek_file = process_get_file(fd);
   if (fd < 2)
   {
      return;
   }
   return file_seek(seek_file, position);
}
/*
열려진 파일 fd에서 읽히거나 써질 다음 바이트의 위치를 반환합니다.
파일의 시작지점부터 몇바이트인지로 표현됩니다.
*/
unsigned tell(int fd)
{
   // Use off_t file_tell(struct file *file).
   struct file *tell_file = process_get_file(fd);
   return file_tell(tell_file);
}
/*
파일 식별자 fd를 닫습니다. 프로세스를 나가거나 종료하는 것은 묵시적으로 그 프로세스의 열려있는 파일 식별자들을 닫습니다.
마치 각 파일 식별자에 대해 이 함수가 호출된 것과 같습니다.
*/
void close(int fd)
{
   struct file *close_file = process_get_file(fd);
   if (close_file == NULL)
   {
      return -1;
   }
   process_close_file(fd);
   return file_close(close_file);
}
/*
주소 값이 유저 영역 주소 값인지 확인
유저 영역을 벗어난 영역일 경우 프로세스 종료(exit(-1)
*/
void check_address(void *addr)
{
   if (is_kernel_vaddr(addr) || !addr || addr >= (void *)0xc0000000) // || pml4_get_page(curr->pml4, addr) == NULL + 추후 숫자 확인!!!
   {
      exit(-1);
   }

   // 06.23
   struct thread* cur_thread = thread_current();
   struct page* temp = spt_find_page(&cur_thread->spt, addr);
   if(!temp)
      exit(-1);
}

void check_valid_string (const void* str) 
{
   check_address(str);

   /* str에 대한 page의 존재 여부를 확인*/
   struct thread* curr = thread_current();
   struct page* is_page = spt_find_page(&curr->spt, pg_round_down(str));
   if(is_page == NULL){
      exit(-1);
   }

}

// 06.22 : 디버깅! -> 수정 필요!! 🔥MMF : 이거 아직 안 고쳤는데, 수정 필요한가..? 
void check_valid_buffer (void* buffer, unsigned size)
{
   // 버퍼를 통해 접근한 주소 + size가 PGSIZE와 동일한지 검사
   struct thread* curr = thread_current();
   struct page* is_page;
   
   // for문 돌면서 size마다 buffer 유효성 확인
   // buffer 안에 fd 로 열려있는 파일로부터 size 바이트를 읽습니다.
   void* temp_buffer = buffer;
   while(temp_buffer <= buffer + size){
      check_address(temp_buffer);
      is_page = spt_find_page(&curr->spt, temp_buffer);
      if(!is_page){ //|| is_page->writable != true
         exit(-1); 
      }
      temp_buffer++;
   }            
}


// 06.27
void *
mmap (void *addr, size_t length, int writable, int fd, off_t offset) {
   // ★ mmap 입장에서 length와 file_length는 어떤 역할인지 고민!!
   // ▶ length는 읽을 범위, file_length는 실제 파일의 길이 -> length가 file_length보다 작을 경우도 OK지만, length가 file_length보다 큰 경우는 file_length만큼까지 읽기..?

   // PANIC("################## %zu ########################\n\n", length);
   // PANIC("addr : %zu \n\n", addr);
   // PANIC("addr + length : %zu \n\n", addr + length);


	// 인자들에 대한 유효성 검사
	if(offset % PGSIZE != 0) {
      // PANIC("offset %% PGSIZE != 0\n\n");
      return NULL;
   }
	
   if(!addr || (uintptr_t)addr % PGSIZE != 0 || is_kernel_vaddr(addr)) {
      // PANIC("!addr || (uintptr_t)addr % PGSIZE != 0 || is_kernel_vaddr(addr)");
      return NULL;
   }
	
	struct thread* cur_thread = thread_current();
	if(spt_find_page(&cur_thread->spt, addr)){
      // PANIC("spt_find_page(&cur_thread->spt, addr)\n\n");
      return NULL;
   }

   struct file* open_file = process_get_file(fd);
   if(!open_file){
      // PANIC("!open_file\n\n");
      return NULL;
   }
	if(length <= 0) {
      // PANIC("length <= 0\n\n");
      return NULL;
   }

   // mmap-kernel 예외 처리 (06.29) -> Physical
   // if(LOADER_KERN_BASE - (uintptr_t)addr <= length) return NULL;
   if(addr + length == 0) { // unsigned long long 유효 범위를 오버 플로우 하게 됨!!!
      // PANIC("addr : %zu \n\n", addr);
      // PANIC("length : %zu \n\n", length);      
      // PANIC("addr + length : %zu \n\n", addr + length);
      return NULL;
   }

   int file_len = file_length(open_file);
   length = ((length < file_len) ? length : file_len);

	return do_mmap(addr, length, writable, open_file, offset);
}

void 
munmap(void* addr){
   do_munmap(addr);
}