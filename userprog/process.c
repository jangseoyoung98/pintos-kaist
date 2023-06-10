#include "userprog/process.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "userprog/gdt.h"
#include "userprog/tss.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/mmu.h"
#include "threads/vaddr.h"
#include "intrinsic.h"
#include "threads/synch.h"
#ifdef VM
#include "vm/vm.h"
#endif

static void process_cleanup(void);
static bool load(const char *file_name, struct intr_frame *if_);
static void initd(void *f_name);
static void __do_fork(void *);
struct thread *get_child(int pid);

/* initd 및 기타 프로세스를 위한 일반 프로세스 초기화 프로그램입니다. */
static void
process_init(void)
{
	struct thread *current = thread_current();
}

/* FILE_NAME에서 로드한 "initd"라는 첫 번째 유저랜드 프로그램을 시작합니다.
 * 새 스레드가 스케줄링될 수 있습니다(그리고 종료될 수도 있습니다).
 * 종료할 수도 있습니다.
 * initd의 스레드 아이디를 반환하거나, 스레드를 생성할 수 없는 경우 TID_ERROR를 반환합니다.
 * 이 함수는 한 번만 호출해야 합니다. */
tid_t process_create_initd(const char *file_name)
{
	char *fn_copy;
	tid_t tid;

	/* Make a copy of FILE_NAME.
	 * Otherwise there's a race between the caller and load(). */
	fn_copy = palloc_get_page(0);
	if (fn_copy == NULL)
		return TID_ERROR;
	strlcpy(fn_copy, file_name, PGSIZE);

	// ✅ filename만 따로 파싱함 (에러 수정)
	char *token, *save_ptr;
	token = strtok_r(file_name, " ", &save_ptr);

	/* Create a new thread to execute FILE_NAME. */
	tid = thread_create(token, PRI_DEFAULT, initd, fn_copy);
	if (tid == TID_ERROR)
		palloc_free_page(fn_copy);
	return tid;
}

/* 첫 번째 사용자 프로세스를 시작하는 스레드 함수입니다. */
static void
initd(void *f_name)
{
#ifdef VM
	supplemental_page_table_init(&thread_current()->spt);
#endif

	process_init();

	if (process_exec(f_name) < 0)
		PANIC("Fail to launch initd\n");
	NOT_REACHED();
}

/* 현재 프로세스를 `name`으로 복제합니다. 새 프로세스의 스레드 ID를 반환합니다.
 * 스레드를 생성할 수 없으면 TID_ERROR를 반환합니다. */
tid_t process_fork(const char *name, struct intr_frame *if_ UNUSED)
{
	/* Clone current thread to new thread.*/
	// fork 추가
	struct thread *cur = thread_current();
	memcpy(&thread_current()->parent_if, if_, sizeof(struct intr_frame));
	tid_t tid = thread_create(name, PRI_DEFAULT, __do_fork, cur);

	if (tid == TID_ERROR)
	{
		return TID_ERROR;
	}
	struct thread *t = get_child(tid);
	sema_down(&t->fork_sema); // 자식이 메모리에 load될때까지 기다림
	if (t->exit_status == -1)
	{
		return TID_ERROR;
	}
	return tid;
}

struct thread *get_child(int pid)
{
	struct thread *par = thread_current();
	struct list_elem *current_elem;

	if (list_empty(&par->child_list))
		return NULL;

	for (current_elem = list_front(&par->child_list); current_elem != list_tail(&par->child_list); current_elem = list_next(current_elem))
	{
		if (list_entry(current_elem, struct thread, child_elem)->tid == pid)
		{
			return list_entry(current_elem, struct thread, child_elem);
		}
	}
	return NULL;
}
#ifndef VM
/* 📌 fork() 구현!
 * 이 함수를 전달하여 부모의 주소 공간을 복제합니다.
 * pml4_for_each. 이것은 프로젝트 2에만 해당됩니다. */
duplicate_pte(uint64_t *pte, void *va, void *aux)
{
	struct thread *current = thread_current();
	struct thread *parent = (struct thread *)aux;
	void *parent_page;
	void *newpage;
	bool writable;

	/* 1. TODO: If the parent_page is kernel page, then return immediately. */
	/* 1. 할 일: parent_page가 커널 페이지인 경우 즉시 반환합니다. */
	if (is_kernel_vaddr(va))
	{
		return true;
	}
	/* 2. Resolve VA from the parent's page map level 4. */
	parent_page = pml4_get_page(parent->pml4, va);
	if (parent_page == NULL)
	{
		return false;
	}
	/* 3. TODO: Allocate new PAL_USER page for the child and set result to
	 *    TODO: NEWPAGE. */
	newpage = palloc_get_page(PAL_USER);

	/* 4. TODO: Duplicate parent's page to the new page and
	 *    TODO: check whether parent's page is writable or not (set WRITABLE
	 *    TODO: according to the result). */
	/* 4. 할 일: 상위 페이지를 새 페이지로 복제합니다
	 * TODO: 부모 페이지의 쓰기 가능 여부 확인(쓰기 가능으로 설정)
	 * TODO: 결과에 따름). */
	memcpy(newpage, parent_page, PGSIZE);
	writable = is_writable(pte);
	/* 5. Add new page to child's page table at address VA with WRITABLE
	 *    permission. */
	if (!pml4_set_page(current->pml4, va, newpage, writable))
	{
		/* 6. TODO: if fail to insert page, do error handling. */
		return false;
	}
	return true;
}
#endif
/* 📌 fork() 구현!
 * 부모의 실행 컨텍스트를 복사하는 스레드 함수입니다.
 * 힌트) parent->tf는 프로세스의 유저랜드 컨텍스트를 보유하지 않습니다.
 * 즉, 프로세스_포크의 두 번째 인자를 이 함수에 전달해야 합니다.
 * 이 함수에 전달해야 합니다. */
static void
__do_fork(void *aux)
{
	struct intr_frame if_;
	struct thread *parent = (struct thread *)aux;
	struct thread *current = thread_current();
	current->parent = parent;
	/* TODO: somehow pass the parent_if. (i.e. process_fork()'s if_) */
	struct intr_frame *parent_if = &parent->parent_if;
	bool succ = true;

	/* 1. Read the cpu context to local stack. */
	memcpy(&if_, parent_if, sizeof(struct intr_frame));

	/* 2. Duplicate PT */
	current->pml4 = pml4_create();
	if (current->pml4 == NULL)
		goto error;

	process_activate(current);
#ifdef VM
	supplemental_page_table_init(&current->spt);
	if (!supplemental_page_table_copy(&current->spt, &parent->spt))
		goto error;
#else
	if (!pml4_for_each(parent->pml4, duplicate_pte, parent))
		goto error;
#endif

	/* TODO: Your code goes here.
	 * TODO: Hint) To duplicate the file object, use `file_duplicate`
	 * TODO:       in include/filesys/file.h. Note that parent should not return
	 * TODO:       from the fork() until this function successfully duplicates
	 * TODO:       the resources of parent.*/

	if (parent->next_fd == FD_MAX)
		goto error;

	for (int i = 0; i < FD_MAX; i++)
	{
		struct file *file = parent->fdt[i];
		if (file == NULL)
			continue;
		struct file *new_file;
		if (file > 2)
			new_file = file_duplicate(file);
		else
			new_file = file;
		current->fdt[i] = new_file;
	}
	if_.R.rax = 0;
	current->next_fd = parent->next_fd;
	process_init();
	sema_up(&current->fork_sema);
	/* Finally, switch to the newly created process. */
	if (succ)
		do_iret(&if_);
error:
	sema_up(&current->fork_sema);
	thread_exit();
}

/* 현재 실행 컨텍스트를 f_name으로 전환합니다.
 * 실패하면 -1을 반환합니다. */
int process_exec(void *f_name)
{
	char *file_name = f_name;
	bool success;

	// ✅ 1. Argument Passing - 문자열 파싱
	char *token, *save_ptr;
	char *token_list[128];
	int count = 0;

	token = strtok_r(file_name, " ", &save_ptr);
	token_list[count] = token;
	while (token != NULL)
	{
		token = strtok_r(NULL, " ", &save_ptr);
		count++;
		token_list[count] = token;
	}

	/* We cannot use the intr_frame in the thread structure.
	 * This is because when current thread rescheduled,
	 * it stores the execution information to the member. */
	struct intr_frame _if;
	_if.ds = _if.es = _if.ss = SEL_UDSEG;
	_if.cs = SEL_UCSEG;
	_if.eflags = FLAG_IF | FLAG_MBS;

	/* We first kill the current context */
	process_cleanup();

	/* And then load the binary */
	// ✅ 1. Argument Passing - load() 호출로 스택 셋팅 확인
	success = load(file_name, &_if);

	// ✅ 1. Argument Passing - argument_stack() 구현 및 호출 -> 유저 프로그램 실행 전, argc argv 인자들 스택에 셋팅b
	argument_stack(token_list, count, &_if);
	// hex_dump(_if.rsp, _if.rsp, USER_STACK - _if.rsp, true);

	/* If load failed, quit. */
	palloc_free_page(file_name);
	if (!success)
		return -1;

	/* Start switched process. */
	do_iret(&_if);
	NOT_REACHED();
}

/* 스레드 TID가 종료될 때까지 기다렸다가 종료 상태를 반환합니다.
 * 만약 커널에 의해 종료되면 (즉, 예외로 인해 종료된 경우) -1을 반환합니다.
 * TID가 유효하지 않거나 호출 프로세스의 자식이 아니거나, process_wait()가 성공적으로 호출된 경우, -1을 즉시 반환합니다.
 * 이 함수는 문제 2-2에서 구현될 예정입니다. */
int process_wait(tid_t child_tid UNUSED)
{
	/* XXX: Hint) The pintos exit if process_wait (initd), we recommend you
	 * XXX:       to add infinite loop here before
	 * XXX:       implementing the process_wait. */
	for (int i = 0; i < 1 << 25; i++)
	{
		continue;
	}
	return -1;
}

/* 프로세스를 종료하는 함수로, thread_exit ()에 의해 호출된다. */
void process_exit(void)
{
	struct thread *curr = thread_current();
	/* TODO: Your code goes here.
	 * TODO: Implement process termination message (see
	 * TODO: project2/process_termination.html).
	 * TODO: We recommend you to implement process resource cleanup here. */

	process_cleanup();
}

/* 현재 프로세스의 리소스를 해제합니다. */
static void
process_cleanup(void)
{
	struct thread *curr = thread_current();

#ifdef VM
	supplemental_page_table_kill(&curr->spt);
#endif

	uint64_t *pml4;
	/* Destroy the current process's page directory and switch back
	 * to the kernel-only page directory. */
	pml4 = curr->pml4;
	if (pml4 != NULL)
	{
		/* Correct ordering here is crucial.  We must set
		 * cur->pagedir to NULL before switching page directories,
		 * so that a timer interrupt can't switch back to the
		 * process page directory.  We must activate the base page
		 * directory before destroying the process's page
		 * directory, or our active page directory will be one
		 * that's been freed (and cleared). */
		curr->pml4 = NULL;
		pml4_activate(NULL);
		pml4_destroy(pml4);
	}
}

/* nest 스레드에서 사용자 코드를 실행하기 위한 CPU를 설정합니다.
 * 이 함수는 모든 컨텍스트 전환 시 호출됩니다. */
void process_activate(struct thread *next)
{
	/* Activate thread's page tables. */
	pml4_activate(next->pml4);

	/* Set thread's kernel stack for use in processing interrupts. */
	tss_update(next);
}

/* We load ELF binaries.  The following definitions are taken
 * from the ELF specification, [ELF1], more-or-less verbatim.  */

/* ELF types.  See [ELF1] 1-2. */
#define EI_NIDENT 16

#define PT_NULL 0			/* Ignore. */
#define PT_LOAD 1			/* Loadable segment. */
#define PT_DYNAMIC 2		/* Dynamic linking info. */
#define PT_INTERP 3			/* Name of dynamic loader. */
#define PT_NOTE 4			/* Auxiliary info. */
#define PT_SHLIB 5			/* Reserved. */
#define PT_PHDR 6			/* Program header table. */
#define PT_STACK 0x6474e551 /* Stack segment. */

#define PF_X 1 /* Executable. */
#define PF_W 2 /* Writable. */
#define PF_R 4 /* Readable. */

/* Executable header.  See [ELF1] 1-4 to 1-8.
 * This appears at the very beginning of an ELF binary. */
struct ELF64_hdr
{
	unsigned char e_ident[EI_NIDENT];
	uint16_t e_type;
	uint16_t e_machine;
	uint32_t e_version;
	uint64_t e_entry;
	uint64_t e_phoff;
	uint64_t e_shoff;
	uint32_t e_flags;
	uint16_t e_ehsize;
	uint16_t e_phentsize;
	uint16_t e_phnum;
	uint16_t e_shentsize;
	uint16_t e_shnum;
	uint16_t e_shstrndx;
};

struct ELF64_PHDR
{
	uint32_t p_type;
	uint32_t p_flags;
	uint64_t p_offset;
	uint64_t p_vaddr;
	uint64_t p_paddr;
	uint64_t p_filesz;
	uint64_t p_memsz;
	uint64_t p_align;
};

/* 약어 */
#define ELF ELF64_hdr
#define Phdr ELF64_PHDR

static bool setup_stack(struct intr_frame *if_);
static bool validate_segment(const struct Phdr *, struct file *);
static bool load_segment(struct file *file, off_t ofs, uint8_t *upage,
						 uint32_t read_bytes, uint32_t zero_bytes,
						 bool writable);

/* FILE_NAME에서 현재 스레드로 ELF 실행 파일을 로드합니다.
 * 실행 파일의 진입점을 *RIP에, 초기 스택 포인터를 *RSP에 저장합니다.
 * 성공하면 참을 반환하고, 그렇지 않으면 거짓을 반환합니다. */
static bool
load(const char *file_name, struct intr_frame *if_)
{
	struct thread *t = thread_current();
	struct ELF ehdr;
	struct file *file = NULL;
	off_t file_ofs;
	bool success = false;
	int i;

	/* Allocate and activate page directory. */
	t->pml4 = pml4_create();
	if (t->pml4 == NULL)
		goto done;
	process_activate(thread_current());

	/* Open executable file. */
	file = filesys_open(file_name);
	if (file == NULL)
	{
		printf("load: %s: open failed\n", file_name);
		goto done;
	}

	/* Read and verify executable header. */
	if (file_read(file, &ehdr, sizeof ehdr) != sizeof ehdr || memcmp(ehdr.e_ident, "\177ELF\2\1\1", 7) || ehdr.e_type != 2 || ehdr.e_machine != 0x3E // amd64
		|| ehdr.e_version != 1 || ehdr.e_phentsize != sizeof(struct Phdr) || ehdr.e_phnum > 1024)
	{
		printf("load: %s: error loading executable\n", file_name);
		goto done;
	}

	/* Read program headers. */
	file_ofs = ehdr.e_phoff;
	for (i = 0; i < ehdr.e_phnum; i++)
	{
		struct Phdr phdr;

		if (file_ofs < 0 || file_ofs > file_length(file))
			goto done;
		file_seek(file, file_ofs);

		if (file_read(file, &phdr, sizeof phdr) != sizeof phdr)
			goto done;
		file_ofs += sizeof phdr;
		switch (phdr.p_type)
		{
		case PT_NULL:
		case PT_NOTE:
		case PT_PHDR:
		case PT_STACK:
		default:
			/* Ignore this segment. */
			break;
		case PT_DYNAMIC:
		case PT_INTERP:
		case PT_SHLIB:
			goto done;
		case PT_LOAD:
			if (validate_segment(&phdr, file))
			{
				bool writable = (phdr.p_flags & PF_W) != 0;
				uint64_t file_page = phdr.p_offset & ~PGMASK;
				uint64_t mem_page = phdr.p_vaddr & ~PGMASK;
				uint64_t page_offset = phdr.p_vaddr & PGMASK;
				uint32_t read_bytes, zero_bytes;
				if (phdr.p_filesz > 0)
				{
					/* Normal segment.
					 * Read initial part from disk and zero the rest. */
					read_bytes = page_offset + phdr.p_filesz;
					zero_bytes = (ROUND_UP(page_offset + phdr.p_memsz, PGSIZE) - read_bytes);
				}
				else
				{
					/* Entirely zero.
					 * Don't read anything from disk. */
					read_bytes = 0;
					zero_bytes = ROUND_UP(page_offset + phdr.p_memsz, PGSIZE);
				}
				if (!load_segment(file, file_page, (void *)mem_page,
								  read_bytes, zero_bytes, writable))
					goto done;
			}
			else
				goto done;
			break;
		}
	}

	/* Set up stack. */
	if (!setup_stack(if_))
		goto done;

	/* Start address. */
	if_->rip = ehdr.e_entry;

	success = true;

done:
	/* We arrive here whether the load is successful or not. */
	file_close(file);
	return success;
}

// ✅ 1. Argument Passing - argument_stack구현
void argument_stack(char **argument_list, int cnt, struct intr_frame *if_)
{
	// rdi : argc 저장
	if_->R.rdi = cnt;
	char *argu_addr[128];

	// 1. 깃북 argv[4][..] ~ argv[0][..]
	for (int i = cnt - 1; i >= 0; i--)
	{
		// 스택 공간 확보하기 (해당 원소의 길이만큼 주소 내리기) -> 역방향
		int argv_len = strlen(argument_list[i]); // '\0' 넣을 공간 확보 : +1
		if_->rsp -= (argv_len + 1);
		// 스택의 rsp에서 위로 쌓아 올리기 (원소 한 개씩) -> 순방향
		memcpy(if_->rsp, argument_list[i], (argv_len + 1));
		argu_addr[i] = if_->rsp;
	}
	// 2. 깃북 word-align
	// 패딩이 필요한지 확인한다.
	// size_t paddings = (((uint8_t)if_->rsp % 8));

	if (((uint8_t)if_->rsp % 8) != 0)
	{ // 0이 아닌 경우(8의 배수 X) 패딩을 실행한다.
		while ((uint8_t)if_->rsp % 8 != 0)
		{
			if_->rsp--;
			*(uint8_t *)if_->rsp = 0;
		}
	}

	// 3. 깃북 argv[4]
	if_->rsp -= 8;
	memset(if_->rsp, 0, 8);
	// 3. 깃북 argv[3] ~ argv[0]
	for (int i = cnt - 1; i >= 0; i--)
	{
		if_->rsp -= 8;
		memcpy(if_->rsp, &argu_addr[i], 8);
	}

	// 4. 깃북 return address
	if_->rsp -= 8;
	if_->R.rsi = if_->rsp;
	memset(if_->rsp, 0, 8);
	if_->R.rsi += 8;
}

/* 파일에 유효하고 로드 가능한 세그먼트가 있는지 확인합니다.
 * FILE에 유효한 세그먼트를 기술하고 있는지 확인하고, 유효하면 참을, 그렇지 않으면 거짓을 반환합니다. */
static bool
validate_segment(const struct Phdr *phdr, struct file *file)
{
	/* p_offset and p_vaddr must have the same page offset. */
	if ((phdr->p_offset & PGMASK) != (phdr->p_vaddr & PGMASK))
		return false;

	/* p_offset must point within FILE. */
	if (phdr->p_offset > (uint64_t)file_length(file))
		return false;

	/* p_memsz must be at least as big as p_filesz. */
	if (phdr->p_memsz < phdr->p_filesz)
		return false;

	/* The segment must not be empty. */
	if (phdr->p_memsz == 0)
		return false;

	/* The virtual memory region must both start and end within the
	   user address space range. */
	if (!is_user_vaddr((void *)phdr->p_vaddr))
		return false;
	if (!is_user_vaddr((void *)(phdr->p_vaddr + phdr->p_memsz)))
		return false;

	/* The region cannot "wrap around" across the kernel virtual
	   address space. */
	if (phdr->p_vaddr + phdr->p_memsz < phdr->p_vaddr)
		return false;

	/* Disallow mapping page 0.
	   Not only is it a bad idea to map page 0, but if we allowed
	   it then user code that passed a null pointer to system calls
	   could quite likely panic the kernel by way of null pointer
	   assertions in memcpy(), etc. */
	if (phdr->p_vaddr < PGSIZE)
		return false;

	/* It's okay. */
	return true;
}

#ifndef VM
/* 이 블록의 코드는 프로젝트 2에서만 사용됩니다.
 * 프로젝트 2 전체에 대해 함수를 구현하려면 매크로 외부에서 구현하세요. */

/* load() helpers. */
static bool install_page(void *upage, void *kpage, bool writable);

/* Loads a segment starting at offset OFS in FILE at address
 * UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
 * memory are initialized, as follows:
 *
 * - READ_BYTES bytes at UPAGE must be read from FILE
 * starting at offset OFS.
 *
 * - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.
 *
 * The pages initialized by this function must be writable by the
 * user process if WRITABLE is true, read-only otherwise.
 *
 * Return true if successful, false if a memory allocation error
 * or disk read error occurs. */
static bool
load_segment(struct file *file, off_t ofs, uint8_t *upage,
			 uint32_t read_bytes, uint32_t zero_bytes, bool writable)
{
	ASSERT((read_bytes + zero_bytes) % PGSIZE == 0);
	ASSERT(pg_ofs(upage) == 0);
	ASSERT(ofs % PGSIZE == 0);

	file_seek(file, ofs);
	while (read_bytes > 0 || zero_bytes > 0)
	{
		/* Do calculate how to fill this page.
		 * We will read PAGE_READ_BYTES bytes from FILE
		 * and zero the final PAGE_ZERO_BYTES bytes. */
		size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
		size_t page_zero_bytes = PGSIZE - page_read_bytes;

		/* Get a page of memory. */
		uint8_t *kpage = palloc_get_page(PAL_USER);
		if (kpage == NULL)
			return false;

		/* Load this page. */
		if (file_read(file, kpage, page_read_bytes) != (int)page_read_bytes)
		{
			palloc_free_page(kpage);
			return false;
		}
		memset(kpage + page_read_bytes, 0, page_zero_bytes);

		/* Add the page to the process's address space. */
		if (!install_page(upage, kpage, writable))
		{
			printf("fail\n");
			palloc_free_page(kpage);
			return false;
		}

		/* Advance. */
		read_bytes -= page_read_bytes;
		zero_bytes -= page_zero_bytes;
		upage += PGSIZE;
	}
	return true;
}

/* Create a minimal stack by mapping a zeroed page at the USER_STACK */
static bool
setup_stack(struct intr_frame *if_)
{
	uint8_t *kpage;
	bool success = false;

	kpage = palloc_get_page(PAL_USER | PAL_ZERO);
	if (kpage != NULL)
	{
		success = install_page(((uint8_t *)USER_STACK) - PGSIZE, kpage, true);
		if (success)
			if_->rsp = USER_STACK;
		else
			palloc_free_page(kpage);
	}
	return success;
}

/* Adds a mapping from user virtual address UPAGE to kernel
 * virtual address KPAGE to the page table.
 * If WRITABLE is true, the user process may modify the page;
 * otherwise, it is read-only.
 * UPAGE must not already be mapped.
 * KPAGE should probably be a page obtained from the user pool
 * with palloc_get_page().
 * Returns true on success, false if UPAGE is already mapped or
 * if memory allocation fails. */
static bool
install_page(void *upage, void *kpage, bool writable)
{
	struct thread *t = thread_current();

	/* Verify that there's not already a page at that virtual
	 * address, then map our page there. */
	return (pml4_get_page(t->pml4, upage) == NULL && pml4_set_page(t->pml4, upage, kpage, writable));
}
#else
/* From here, codes will be used after project 3.
 * If you want to implement the function for only project 2, implement it on the
 * upper block. */

static bool
lazy_load_segment(struct page *page, void *aux)
{
	/* TODO: Load the segment from the file */
	/* TODO: This called when the first page fault occurs on address VA. */
	/* TODO: VA is available when calling this function. */
}

/* Loads a segment starting at offset OFS in FILE at address
 * UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
 * memory are initialized, as follows:
 *
 * - READ_BYTES bytes at UPAGE must be read from FILE
 * starting at offset OFS.
 *
 * - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.
 *
 * The pages initialized by this function must be writable by the
 * user process if WRITABLE is true, read-only otherwise.
 *
 * Return true if successful, false if a memory allocation error
 * or disk read error occurs. */
static bool
load_segment(struct file *file, off_t ofs, uint8_t *upage,
			 uint32_t read_bytes, uint32_t zero_bytes, bool writable)
{
	ASSERT((read_bytes + zero_bytes) % PGSIZE == 0);
	ASSERT(pg_ofs(upage) == 0);
	ASSERT(ofs % PGSIZE == 0);

	while (read_bytes > 0 || zero_bytes > 0)
	{
		/* Do calculate how to fill this page.
		 * We will read PAGE_READ_BYTES bytes from FILE
		 * and zero the final PAGE_ZERO_BYTES bytes. */
		size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
		size_t page_zero_bytes = PGSIZE - page_read_bytes;

		/* TODO: Set up aux to pass information to the lazy_load_segment. */
		void *aux = NULL;
		if (!vm_alloc_page_with_initializer(VM_ANON, upage,
											writable, lazy_load_segment, aux))
			return false;

		/* Advance. */
		read_bytes -= page_read_bytes;
		zero_bytes -= page_zero_bytes;
		upage += PGSIZE;
	}
	return true;
}

/* Create a PAGE of stack at the USER_STACK. Return true on success. */
static bool
setup_stack(struct intr_frame *if_)
{
	bool success = false;
	void *stack_bottom = (void *)(((uint8_t *)USER_STACK) - PGSIZE);

	/* TODO: Map the stack on stack_bottom and claim the page immediately.
	 * TODO: If success, set the rsp accordingly.
	 * TODO: You should mark the page is stack. */
	/* TODO: Your code goes here */

	return success;
}
#endif /* VM */
