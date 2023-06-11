#ifndef THREADS_THREAD_H
#define THREADS_THREAD_H
#define FDT_PAGES 3
#define FDCOUNT_LIMIT FDT_PAGES * (1 << 9)

#include <debug.h>
#include <list.h>
#include <stdint.h>
#include "threads/interrupt.h"
#include "threads/synch.h"
#ifdef VM
#include "vm/vm.h"
#endif

/* States in a thread's life cycle. */
enum thread_status
{
	THREAD_RUNNING, /* Running thread. */
	THREAD_READY,	/* Not running but ready to run. */
	THREAD_BLOCKED, /* Waiting for an event to trigger. */
	THREAD_DYING	/* About to be destroyed. */
};

/* Thread identifier type.
   You can redefine this to whatever type you like. */
typedef int tid_t;
#define TID_ERROR ((tid_t)-1) /* Error value for tid_t. */

#define FD_MIN 2  /* Lowest File descriptor */
#define FD_MAX 63 /* Highest File descriptor */

#define STDIN_FILENO 0
#define STDOUT_FILENO 1

/* Thread priorities. */
#define PRI_MIN 0	   /* Lowest priority. */
#define PRI_DEFAULT 31 /* Default priority. */
#define PRI_MAX 63	   /* Highest priority. */

/* A kernel thread or user process.
 *
 * Each thread structure is stored in its own 4 kB page.  The
 * thread structure itself sits at the very bottom of the page
 * (at offset 0).  The rest of the page is reserved for the
 * thread's kernel stack, which grows downward from the top of
 * the page (at offset 4 kB).  Here's an illustration:
 *
 *      4 kB +---------------------------------+
 *           |          kernel stack           |
 *           |                |                |
 *           |                |                |
 *           |                V                |
 *           |         grows downward          |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           +---------------------------------+
 *           |              magic              |
 *           |            intr_frame           |
 *           |                :                |
 *           |                :                |
 *           |               name              |
 *           |              status             |
 *      0 kB +---------------------------------+
 *
 * The upshot of this is twofold:
 *
 *    1. First, `struct thread' must not be allowed to grow too
 *       big.  If it does, then there will not be enough room for
 *       the kernel stack.  Our base `struct thread' is only a
 *       few bytes in size.  It probably should stay well under 1
 *       kB.
 *
 *    2. Second, kernel stacks must not be allowed to grow too
 *       large.  If a stack overflows, it will corrupt the thread
 *       state.  Thus, kernel functions should not allocate large
 *       structures or arrays as non-static local variables.  Use
 *       dynamic allocation with malloc() or palloc_get_page()
 *       instead.
 *
 * The first symptom of either of these problems will probably be
 * an assertion failure in thread_current(), which checks that
 * the `magic' member of the running thread's `struct thread' is
 * set to THREAD_MAGIC.  Stack overflow will normally change this
 * value, triggering the assertion. */
/* The `elem' member has a dual purpose.  It can be an element in
 * the run queue (thread.c), or it can be an element in a
 * semaphore wait list (synch.c).  It can be used these two ways
 * only because they are mutually exclusive: only a thread in the
 * ready state is on the run queue, whereas only a thread in the
 * blocked state is on a semaphore wait list. */
struct thread
{
	/* Owned by thread.c. */
	tid_t tid;				   /* Thread identifier. */
	enum thread_status status; /* Thread state. */
	char name[16];			   /* Name (for debugging purposes). */
	int priority;			   /* Priority. */
	int64_t wakeup_tick;	   // 1 추가
	// 추가한거
	int origin_priority;	   // donation 이후 우선순위를 초기화하기 위해 초기 우선순위 값을 저장할 필드
	struct lock *wait_on_lock; // 스레드가 현재 얻기위해 기다리고있는 lock
	struct list donations;	   // 나한테 priority나눠준 애들 리스트?
	struct list_elem d_elem;   // donations 안에 들어갈 elem
	// sj추가
	struct file *fdt[64]; // 파일 디스크립트 테이블
	struct file *current_file;
	int next_fd;		  // 파일 디스크립트 인덱스
	int exit_status;	  // 종료 프로세스 상태
	/* Shared between thread.c and synch.c. */
	struct list_elem elem; /* List element. */

	struct thread *parent;
	struct intr_frame parent_if;
	struct list child_list;
	struct list_elem child_elem;
	struct semaphore fork_sema;
	struct semaphore wait_sema;
	struct semaphore free_sema;
	uint64_t *pml4;
	/* ✨ 파일 디스크립터 테이블은 각 스레드 또는 프로세스마다 독립적으로 유지됨 (스레드 : 프로그램 실행 단위 / 파일 : 데이터 저장 단위)
	 * 스레드는 파일에 대한 작업을 수행하기 위해 파일 디스크립터 사용 -> 이를 통해 스레드는 파일 시스템에서 데이터를 읽거나 쓸 수 있음
	 * ★ 스레드가 파일을 open 하면, 운영체제는 그 파일에 대한 디스크립터를 생성하고, 이를 파일 디스크립터 테이블에 추가!
	 *    이후, 스레드가 파일에서 읽기나 쓰기를 수행하려면, 그 파일의 디스크립터를 사용하여 연산을 수행
	 *    그리고, 파일을 닫을 때는 해당 파일 디스크립터를 테이블에서 제거하고 운영체제에게 해당 파일에 대한 접근을 종료한다는 것을 알린다.
	 */

#ifdef USERPROG
	/* Owned by userprog/process.c. */
	/* Page map level 4 */
#endif
#ifdef VM
	/* Table for whole virtual memory owned by thread. */
	struct supplemental_page_table spt;
#endif

	/* Owned by thread.c. */
	struct intr_frame tf; /* Information for switching */
	unsigned magic;		  /* Detects stack overflow. */
};

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
extern bool thread_mlfqs;

void thread_init(void);
void thread_start(void);

void thread_tick(void);
void thread_print_stats(void);

typedef void thread_func(void *aux);
tid_t thread_create(const char *name, int priority, thread_func *, void *);

void thread_block(void);
void thread_unblock(struct thread *);

struct thread *thread_current(void);
tid_t thread_tid(void);
const char *thread_name(void);

void thread_exit(void) NO_RETURN;
void thread_yield(void);

int thread_get_priority(void);
void thread_set_priority(int);

int thread_get_nice(void);
void thread_set_nice(int);
int thread_get_recent_cpu(void);
int thread_get_load_avg(void);

void do_iret(struct intr_frame *tf);
// 4 추가
void wake_up(int64_t ticks);
void thread_sleep(int64_t ticks);

// alarm-all-pass 클론 후 추가 수정
bool cmp_priority(const struct list_elem *a_, const struct list_elem *b_,
				  void *aux UNUSED);
void test_max_priority(void);
// firebird
void donate_priority(void);
void remove_with_lock(struct lock *lock);
void refresh_priority(void);
// 16:30
bool d_cmp_priority(const struct list_elem *a_, const struct list_elem *b_,
					void *aux);
#endif /* threads/thread.h */
