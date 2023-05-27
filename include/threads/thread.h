// 1차 수정
#ifndef THREADS_THREAD_H
#define THREADS_THREAD_H

#include <debug.h>
#include <list.h>
#include <stdint.h>
#include "threads/interrupt.h"
#ifdef VM
#include "vm/vm.h"
#endif

enum thread_status {
	THREAD_RUNNING,     /* 2) 실행중 -> thread_current() 실행 중인 스레드 반환 */
	THREAD_READY,       /* 1) 실행 준비 완료 -> ready_list에 보관됨 */
	THREAD_BLOCKED,     /* 3) 인터럽트(event to trigger) -> READY 상태로 전환되기를 기다림 */
	THREAD_DYING        /* 4) 종료 */
};

/* 스레드 식별자 타입 (재정의 및 에러 검출)*/
typedef int tid_t; 
#define TID_ERROR ((tid_t) -1)          /* Error value for tid_t. */

/* 스레드 우선순위 */
#define PRI_MIN 0                       /* Lowest priority. */
#define PRI_DEFAULT 31                  /* Default priority. */
#define PRI_MAX 63                      /* Highest priority. */

/* 스레드 (구조체) */
struct thread {
	tid_t tid;                          /* 스레드 식별자 */
	int64_t wakeup_tick;				// local tick thread랑 ticks 값을 합친값이 wakeup_tick 그중 가장 작은값이 gloabal tick? 매번 갱신
	enum thread_status status;          /* 스레드 상태 */
	char name[16];                      /* 스레드 이름 (for debugging purposes). */
	int priority;                       /* 스레드 우선순위 */

	struct list_elem elem;              /* 어느 리스트에 들어가 있는지 표시 -> wait list(ready_list) 혹은 run queue(sema_down()) */

#ifdef USERPROG
	/* Owned by userprog/process.c. */
	uint64_t *pml4;                     /* Page map level 4 */
#endif
#ifdef VM
	/* Table for whole virtual memory owned by thread. */
	struct supplemental_page_table spt;
#endif
	
	struct intr_frame tf;               /* context switch 정보 (레지스터, 스택 포인터) */
	unsigned magic;                     /* 스택오버플로우 검사 확인용 */
};

extern bool thread_mlfqs;				/* cpu 스케줄러 채택용 - false면 RR, true면 mlfp 채택*/

/* thread.c에 있는 함수들 */
void thread_init (void);
void thread_start (void);

void thread_tick (void);
void thread_print_stats (void);

typedef void thread_func (void *aux);
tid_t thread_create (const char *name, int priority, thread_func *, void *);

void thread_block (void);
void thread_unblock (struct thread *);

struct thread *thread_current (void);
tid_t thread_tid (void);
const char *thread_name (void);

void thread_exit (void) NO_RETURN;
void thread_yield (void);

int thread_get_priority (void);
void thread_set_priority (int);

int thread_get_nice (void);
void thread_set_nice (int);
int thread_get_recent_cpu (void);
int thread_get_load_avg (void);

void do_iret (struct intr_frame *tf);

#endif 
