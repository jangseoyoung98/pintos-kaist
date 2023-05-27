// 1차 수정
#ifndef THREADS_SYNCH_H
#define THREADS_SYNCH_H

#include <list.h>
#include <stdbool.h>

/* A counting semaphore. */
struct semaphore {
	unsigned value;             /* Current value. */
	struct list waiters;        /* List of waiting threads. */
};

void sema_init (struct semaphore *, unsigned value); 	// value 값으로 sema를 새로운 세마포로 초기화 한다.
void sema_down (struct semaphore *);				    // sema에서 "down" 또는 "P" 작업을 실행한다. (-1)
bool sema_try_down (struct semaphore *);				// (사용 X) 기다리지 않고 sema에서 "down" 또는 "P" 작업을 실행하려고 시도한다. 
void sema_up (struct semaphore *); 						// sema에서 "up" 또는 "V" 작업을 실행한다. (+1)
void sema_self_test (void);

/*****************************************************/

/* Lock. -> 초기값 1인 세마포어와 같다!(down-aquire / up-release) */
struct lock {
	struct thread *holder;      /* Thread holding lock (for debugging). */
	struct semaphore semaphore; /* Binary semaphore controlling access. */
};

void lock_init (struct lock *);							// lock을 초기화 한다. (처음에는, lock 된 스레드가 없다.)
void lock_acquire (struct lock *);						// 현재 스레드에 대해 lock을 건다. (필요한 경우 현재 소유자(스레드)가 잠금 해제하기를 먼저 기다린다.)
bool lock_try_acquire (struct lock *);					// (사용 X!!) 현재 스레드에 대해 lock을 건다. (기다리지 않는다.)
void lock_release (struct lock *);						// 현재 스레드가 소유해야 하는 lock을 해제한다.
bool lock_held_by_current_thread (const struct lock *);	// 실행 중인 스레드가 lock을 소유하고 있으면 true를 반환하고, 그렇지 않으면 false를 반환한다.

/****************************************************/

/* 앞선 방법으로 먼저 시도해 보고, 추후 하기..! https://casys-kaist.github.io/pintos-kaist/appendix/synchronization.html#Semaphores */
/* Condition variable. -> Monitor */
struct condition {
	struct list waiters;        /* List of waiting threads. */
};

void cond_init (struct condition *); 						
void cond_wait (struct condition *, struct lock *);			
void cond_signal (struct condition *, struct lock *);
void cond_broadcast (struct condition *, struct lock *);

/* Optimization barrier.
 *
 * The compiler will not reorder operations across an
 * optimization barrier.  See "Optimization Barriers" in the
 * reference guide for more information.*/
#define barrier() asm volatile ("" : : : "memory")

#endif /* threads/synch.h */
