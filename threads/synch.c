/* This file is derived from source code for the Nachos
   instructional operating system.  The Nachos copyright notice
   is reproduced in full below. */

/* Copyright (c) 1992-1996 The Regents of the University of California.
   All rights reserved.

   Permission to use, copy, modify, and distribute this software
   and its documentation for any purpose, without fee, and
   without written agreement is hereby granted, provided that the
   above copyright notice and the following two paragraphs appear
   in all copies of this software.

   IN NO EVENT SHALL THE UNIVERSITY OF CALIFORNIA BE LIABLE TO
   ANY PARTY FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR
   CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OF THIS SOFTWARE
   AND ITS DOCUMENTATION, EVEN IF THE UNIVERSITY OF CALIFORNIA
   HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

   THE UNIVERSITY OF CALIFORNIA SPECIFICALLY DISCLAIMS ANY
   WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
   WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
   PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS ON AN "AS IS"
   BASIS, AND THE UNIVERSITY OF CALIFORNIA HAS NO OBLIGATION TO
   PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR
   MODIFICATIONS.
   */

#include "threads/synch.h"
#include <stdio.h>
#include <string.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

static struct list ready_list;

/* Initializes semaphore SEMA to VALUE.  A semaphore is a
   nonnegative integer along with two atomic operators for
   manipulating it:

   - down or "P": wait for the value to become positive, then
   decrement it.

   - up or "V": increment the value (and wake up one waiting
   thread, if any). */

/* One semaphore in a list. */
struct semaphore_elem
{
	struct list_elem elem;		/* List element. */
	struct semaphore semaphore; /* This semaphore. */
};

static bool
cmp_priority(const struct list_elem *a_, const struct list_elem *b_, void *aux UNUSED)
{
	const struct thread *a = list_entry(a_, struct thread, elem);
	const struct thread *b = list_entry(b_, struct thread, elem);

	return a->priority > b->priority;
}

static bool
cmp_sem_priority(const struct list_elem *a_, const struct list_elem *b_, void *aux UNUSED)
{
	const struct semaphore_elem *sa = list_entry(a_, struct semaphore_elem, elem);
	const struct semaphore_elem *sb = list_entry(b_, struct semaphore_elem, elem);

	const struct list_elem *lsa = list_begin(&sa->semaphore.waiters);
	const struct list_elem *lsb = list_begin(&sb->semaphore.waiters);

	const struct thread *tlsa = list_entry(lsa, struct thread, elem);
	const struct thread *tlsb = list_entry(lsb, struct thread, elem);

	return tlsa->priority > tlsb->priority;
}

static bool
cmp_donation_priority(const struct list_elem *a_, const struct list_elem *b_, void *aux UNUSED)
{
	const struct thread *a = list_entry(a_, struct thread, d_elem);
	const struct thread *b = list_entry(b_, struct thread, d_elem);

	return a->priority > b->priority;
}

//🔥새로운 세마포어 구조체를 초기화 한다.
void sema_init(struct semaphore *sema, unsigned value)
{
	ASSERT(sema != NULL);

	sema->value = value;
	list_init(&sema->waiters);
}

//🔥down 연산을 sema에 실행한다. -> 세마 값이 양수가 될 때까지 기다렸다가 양수가 되면 1을 뺌
/* Down or "P" operation on a semaphore.  Waits for SEMA's value
   to become positive and then atomically decrements it.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but if it sleeps then the next scheduled
   thread will probably turn interrupts back on. This is
   sema_down function. */
void sema_down(struct semaphore *sema)
{
	enum intr_level old_level;

	ASSERT(sema != NULL);
	ASSERT(!intr_context());

	old_level = intr_disable();
	while (sema->value == 0)
	{
		list_insert_ordered(&sema->waiters, &thread_current()->elem, cmp_priority, NULL);
		thread_block();
	}
	sema->value--;
	intr_set_level(old_level);
}

/* Down or "P" operation on a semaphore, but only if the
   semaphore is not already 0.  Returns true if the semaphore is
   decremented, false otherwise.

   This function may be called from an interrupt handler. */
bool sema_try_down(struct semaphore *sema)
{
	enum intr_level old_level;
	bool success;

	ASSERT(sema != NULL);

	old_level = intr_disable();
	if (sema->value > 0)
	{
		sema->value--;
		success = true;
	}
	else
		success = false;
	intr_set_level(old_level);

	return success;
}

//🔥세마 값을 증가 시킨다. -> 세마 중에 기다리는 스레드가 있으면, 하나 깨운다.
/* Up or "V" operation on a semaphore.  Increments SEMA's value
   and wakes up one thread of those waiting for SEMA, if any.

   This function may be called from an interrupt handler. */
// FIXME: 세마포어 해제 후 priority preemption 기능 추가
void sema_up(struct semaphore *sema)
{
	enum intr_level old_level;

	ASSERT(sema != NULL);
	old_level = intr_disable();
	if (!list_empty(&sema->waiters))
	{
		list_sort(&sema->waiters, cmp_priority, NULL);
		thread_unblock(list_entry(list_pop_front(&sema->waiters), struct thread, elem));
	}
	sema->value++;
	test_max_priority();
	intr_set_level(old_level);
}

static void sema_test_helper(void *sema_);

/* Self-test for semaphores that makes control "ping-pong"
   between a pair of threads.  Insert calls to printf() to see
   what's going on. */
void sema_self_test(void)
{
	struct semaphore sema[2];
	int i;

	printf("Testing semaphores...");
	sema_init(&sema[0], 0);
	sema_init(&sema[1], 0);
	thread_create("sema-test", PRI_DEFAULT, sema_test_helper, &sema);
	for (i = 0; i < 10; i++)
	{
		sema_up(&sema[0]);
		sema_down(&sema[1]);
	}
	printf("done.\n");
}

/* Thread function used by sema_self_test(). */
static void
sema_test_helper(void *sema_)
{
	struct semaphore *sema = sema_;
	int i;

	for (i = 0; i < 10; i++)
	{
		sema_down(&sema[0]);
		sema_up(&sema[1]);
	}
}

//🔥새로운 lock 구조체를 초기화 한다. (어떤 스레드도 소유하지 않음)
/* Initializes LOCK.  A lock can be held by at most a single
   thread at any given time.  Our locks are not "recursive", that
   is, it is an error for the thread currently holding a lock to
   try to acquire that lock.
   A lock is a specialization of a semaphore with an initial
   value of 1.  The difference between a lock and such a
   semaphore is twofold.  First, a semaphore can have a value
   greater than 1, but a lock can only be owned by a single
   thread at a time.  Second, a semaphore does not have an owner,
   meaning that one thread can "down" the semaphore and then
   another one "up" it, but with a lock the same thread must both
   acquire and release it.  When these restrictions prove
   onerous, it's a good sign that a semaphore should be used,
   instead of a lock. */
void lock_init(struct lock *lock)
{
	ASSERT(lock != NULL);

	lock->holder = NULL;
	sema_init(&lock->semaphore, 1);
}

/* Acquires LOCK, sleeping until it becomes available if
   necessary.  The lock must not already be held by the current
   thread.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but interrupts will be turned back on if
   we need to sleep. */

/* priority donation을 수행 */
void donate_priority(void)
{
	/* 현재 스레드가 기다리고 있는 lock과 연결된 모든 스레드들을 순회하며,
	   현재 스레드의 우선순위를 lock을 보유하고 있는 스레드에게 기부
	   (Nested donation 그림 참고, nested depth 는 8로 제한) */
	struct thread *cur = thread_current();
	struct lock *cur_lock = cur->wait_on_lock;

	for (int i = 0; i < 8; i++)
	{
		if (cur_lock == NULL)
		{ // cur->wait_on_lock이 없을시 break
			break;
		}
		else
		{
			if (cur->priority > cur_lock->holder->priority)
			{
				cur_lock->holder->priority = cur->priority;
			}
			cur = cur_lock->holder;
			cur_lock = cur->wait_on_lock;
		}
	}
}

bool cmp_d_priority(const struct list_elem *a_elem, const struct list_elem *b_elem, void *aux)
{
	int a = list_entry(a_elem, struct thread, d_elem)->priority;
	int b = list_entry(b_elem, struct thread, d_elem)->priority;
	return a > b;
}

//🔥현재 스레드에서 lock을 획득한다. (lock owner가 lock을 놓아주기를 기다려야 한다면 기다린다.)
// lock을 점유하고 있는 스레드와 요청 하는 스레드의 우선순위를 비교하여 priority donation을 수행하도록 수정
// NOTE: lock_acquire를 이해한 대로 최종적으로 로직을 수정했음. 지금으로썬 더 수정할 필요 없어보임
void lock_acquire(struct lock *lock)
{
	ASSERT(lock != NULL);
	ASSERT(!intr_context());
	ASSERT(!lock_held_by_current_thread(lock));

	if (lock->holder)
	{
		thread_current()->wait_on_lock = lock;
		list_insert_ordered(&lock->holder->list_donation, &thread_current()->d_elem, cmp_d_priority, NULL);
		donate_priority();
	}

	sema_down(&lock->semaphore);
	// 스레드는 sema_down에서 락을 얻을 때 까지 기다리다가, 락을 점유할 수 있는 상황이 되면 탈출하여 아래 줄을 실행함
	thread_current()->wait_on_lock = NULL;
	lock->holder = thread_current();
}

/* Tries to acquires LOCK and returns true if successful or false
   on failure.  The lock must not already be held by the current
   thread.

   This function will not sleep, so it may be called within an
   interrupt handler. */
bool lock_try_acquire(struct lock *lock)
{
	bool success;

	ASSERT(lock != NULL);
	ASSERT(!lock_held_by_current_thread(lock));

	success = sema_try_down(&lock->semaphore);
	if (success)
		lock->holder = thread_current();
	return success;
}

void refresh_priority(void)
{
	/* 현재 스레드의 우선순위를 기부받기 전의 우선순위로 변경 */
	struct thread *cur = thread_current();
	cur->priority = cur->pre_priority;

	/* 가장 우선순위가 높은 donation List의 thread와
	   현재 thread의 우선순위를 비교하여 높은 값을 현재 thread의 우선순위로 설정 */
	if (list_empty(&cur->list_donation))
		return;
	struct list_elem *first_don = list_begin(&cur->list_donation);
	struct thread *first_thread = list_entry(first_don, struct thread, d_elem);

	if (first_thread->priority > cur->priority)
	{
		cur->priority = first_thread->priority;
	}
}

/* 우선순위를 다시 계산 */
// NOTE: 가독성 개선했고, 함수에 대해 이해를 마쳤음.
void remove_with_lock(struct lock *lock)
{
	/* 현재 스레드의 donations 리스트를 확인하여 해지 할 lock 을 보유하고 있는 엔트리(그룹)를 삭제 */
	struct thread *cur = thread_current();
	struct list *cur_don = &cur->list_donation;
	struct list_elem *e;

	if (!list_empty(cur_don))
	{
		for (e = list_begin(cur_don); e != list_end(cur_don); e = list_next(e))
		{ // 순회
			struct thread *e_cur = list_entry(e, struct thread, d_elem);
			if (lock == e_cur->wait_on_lock)
			{ // 해당 Lock과 연관되어 있는 모든 thread를 donation에서 삭제함
				list_remove(&e_cur->d_elem);
			}
		}
	}
}

//🔥락을 놓아준다. (현재 스레드가 소유 중이어야 한다.)
/* Releases LOCK, which must be owned by the current thread.
   This is lock_release function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to release a lock within an interrupt
   handler. */
/* NOTE: 이 동작은 위의 주석을 읽어보면 CPU에서 running되는 `현재 스레드`가 점유하고 있는 lock을 놓아주는 상황을 일컬음.
그걸 상정하고 코드를 읽어본다면 refresh_priority()에서 왜 `현재 스레드`를 주축으로 동작하는 지 이해할 수 있었음.
`현재 스레드`는 이제 이 락을 놓아주는 상황이고, 그렇다면 점유하고 있는 동안 donation받았던 우선순위가 아닌 기존 본인의 우선순위로 돌아가야 알맞은 동작이 가능할 것.
아다리가 맞아떨어진다.
*/
void lock_release(struct lock *lock)
{
	ASSERT(lock != NULL);
	ASSERT(lock_held_by_current_thread(lock));

	remove_with_lock(lock);
	refresh_priority();
	lock->holder = NULL;
	sema_up(&lock->semaphore);
}

//🔥현재 스레드가 락을 갖고 있는지의 여부를 리턴
/* Returns true if the current thread holds LOCK, false
   otherwise.  (Note that testing whether some other thread holds
   a lock would be racy.) */
bool lock_held_by_current_thread(const struct lock *lock)
{
	ASSERT(lock != NULL);

	return lock->holder == thread_current();
}

//🔥새로운 컨디션 변수를 초기화 한다.
/* Initializes condition variable COND.  A condition variable
   allows one piece of code to signal a condition and cooperating
   code to receive the signal and act upon it. */
void cond_init(struct condition *cond)
{
	ASSERT(cond != NULL);

	list_init(&cond->waiters);
}

/* Atomically releases LOCK and waits for COND to be signaled by
   some other piece of code.  After COND is signaled, LOCK is
   reacquired before returning.  LOCK must be held before calling
   this function.

   The monitor implemented by this function is "Mesa" style, not
   "Hoare" style, that is, sending and receiving a signal are not
   an atomic operation.  Thus, typically the caller must recheck
   the condition after the wait completes and, if necessary, wait
   again.

   A given condition variable is associated with only a single
   lock, but one lock may be associated with any number of
   condition variables.  That is, there is a one-to-many mapping
   from locks to condition variables.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but interrupts will be turned back on if
   we need to sleep. */
//🔥뒤엉키지 않게 락(lock)을 놓아주고 컨디션 변수(cond)가 다른 코드로부터 신호 받는 걸 기다린다. -> 신호 받으면 lock을 다시 획득
void cond_wait(struct condition *cond, struct lock *lock)
{
	struct semaphore_elem waiter;

	ASSERT(cond != NULL);
	ASSERT(lock != NULL);
	ASSERT(!intr_context());
	ASSERT(lock_held_by_current_thread(lock));

	sema_init(&waiter.semaphore, 0);
	list_insert_ordered(&cond->waiters, &waiter.elem, cmp_sem_priority, NULL);
	lock_release(lock);
	sema_down(&waiter.semaphore);
	lock_acquire(lock);
}

//🔥cond를 기다리는 스레드가 있으면, 한 개의 스레드 깨운다. (이 함수 호출 전 락을 갖고 있다)
/* If any threads are waiting on COND (protected by LOCK), then
   this function signals one of them to wake up from its wait.
   LOCK must be held before calling this function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler. */
void cond_signal(struct condition *cond, struct lock *lock UNUSED)
{
	ASSERT(cond != NULL);
	ASSERT(lock != NULL);
	ASSERT(!intr_context());
	ASSERT(lock_held_by_current_thread(lock));
	if (!list_empty(&cond->waiters))
	{
		list_sort(&cond->waiters, cmp_sem_priority, NULL);
		sema_up(&list_entry(list_pop_front(&cond->waiters), struct semaphore_elem, elem)->semaphore);
	}
}

/* Wakes up all threads, if any, waiting on COND (protected by
   LOCK).  LOCK must be held before calling this function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler. */
//🔥cond를 기다리는 스레드가 있으며면, 모든 스레드를 깨운다.
void cond_broadcast(struct condition *cond, struct lock *lock)
{
	ASSERT(cond != NULL);
	ASSERT(lock != NULL);

	while (!list_empty(&cond->waiters))
		cond_signal(cond, lock);
}