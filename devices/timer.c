// 2차 수정
#include "devices/timer.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include "threads/interrupt.h"
#include "threads/io.h"
#include "threads/synch.h"
#include "threads/thread.h"
// #include "kernel/list.h"
// #include "threads/thread.c"

/* See [8254] for hardware details of the 8254 timer chip. */

#if TIMER_FREQ < 19
#error 8254 timer requires TIMER_FREQ >= 19
#endif
#if TIMER_FREQ > 1000
#error TIMER_FREQ <= 1000 recommended
#endif

/* Number of timer	ticks since OS booted. */
static int64_t ticks;

/* Number of loops per timer tick.
   Initialized by timer_calibrate(). */
static unsigned loops_per_tick; // ★★★ timer tick이 어떻게 증가하는지

static intr_handler_func timer_interrupt;
static bool too_many_loops(unsigned loops);
static void busy_wait(int64_t loops);
static void real_time_sleep(int64_t num, int32_t denom);

/* Inserts ELEM in the proper position in LIST, which must be
   sorted according to LESS given auxiliary data AUX.
   Runs in O(n) average case in the number of elements in LIST. */

// 리스트 우선순위 함수 사용용 비교함수
// static bool thread_less(const struct list_elem *a_, const struct list_elem *b_, void *aux)
// {
// 	const struct thread *a = list_entry(a_, struct thread, elem);
// 	const struct thread *b = list_entry(b_, struct thread, elem);

// 	return a->wakeup_tick < b->wakeup_tick;
// }

// static void list_insert_ordered2(struct list *list, struct list_elem *elem,
// 								 list_less_func *less, void *aux)
// {
// 	struct list_elem *e;

// 	ASSERT(list != NULL);
// 	ASSERT(elem != NULL);
// 	ASSERT(less != NULL);

// 	for (e = list_begin(list); e != list_end(list); e = list_next(e))
// 		if (less(elem, e, aux))
// 			break;
// 	return list_insert(e, elem);
// }
/* Sets up the 8254 Programmable Interval Timer (PIT) to
   interrupt PIT_FREQ times per second, and registers the
   corresponding interrupt. */
void timer_init(void)
{
	/* 8254 input frequency divided by TIMER_FREQ, rounded to
	   nearest. */
	uint16_t count = (1193180 + TIMER_FREQ / 2) / TIMER_FREQ;

	outb(0x43, 0x34); /* CW: counter 0, LSB then MSB, mode 2, binary. */
	outb(0x40, count & 0xff);
	outb(0x40, count >> 8);

	intr_register_ext(0x20, timer_interrupt, "8254 Timer");
}

/* Calibrates(보정) loops_per_tick, used to implement brief delays. */
void timer_calibrate(void)
{
	unsigned high_bit, test_bit;

	ASSERT(intr_get_level() == INTR_ON);
	printf("Calibrating timer...  ");

	/* Approximate loops_per_tick as the largest power-of-two
	   still less than one timer tick. */
	loops_per_tick = 1u << 10;
	while (!too_many_loops(loops_per_tick << 1))
	{
		loops_per_tick <<= 1;
		ASSERT(loops_per_tick != 0);
	}

	/* Refine the next 8 bits of loops_per_tick. */
	high_bit = loops_per_tick;
	for (test_bit = high_bit >> 1; test_bit != high_bit >> 10; test_bit >>= 1)
		if (!too_many_loops(high_bit | test_bit))
			loops_per_tick |= test_bit;

	printf("%'" PRIu64 " loops/s.\n", (uint64_t)loops_per_tick * TIMER_FREQ);
}

/* Returns the number of timer	ticks since the OS booted. */
int64_t
timer_ticks(void)
{
	enum intr_level old_level = intr_disable();
	int64_t t = ticks;
	intr_set_level(old_level);
	barrier();
	return t;
}

/* Returns the number of timer	ticks elapsed since THEN, which
   should be a value once returned by timer	ticks(). */
int64_t
timer_elapsed(int64_t then)
{
	return timer_ticks() - then;
}

/* Suspends execution for approximately	ticks timer	ticks. */
/* timer_sleep()을 호출하는 thread의 실행을 최소	ticks 값 만큼의 시간이 지난 이후까지 일시 중단한다.
 *  thread를 해당 시간 동안 기다린 후 ready queue에 넣는다.
 */
void timer_sleep(int64_t ticks)
{
	int64_t start = timer_ticks();

	ASSERT(intr_get_level() == INTR_ON);
	// while (timer_elapsed (start) <	ticks)
	// 	thread_yield ();
	if (timer_elapsed(start) < ticks)
		thread_sleep(start + ticks);
}
// // thread_sleep 구현
// void thread_sleep(int64_t ticks)
// { // 입력 : wake up 할 스레드의 wakeup_tick
// 	/*
// 	1) 현재 스레드가 idle 스레드인지 검사
// 	* 인터럽트 비활성화
// 	2) 호출된 스레드의 상태를 BLOCKED로 바꿈 -> sleep_list에 넣기
// 	3) (wake up 할 스레드의 local tick) 매개변수로 받은 ticks를 저장
// 	4) if(조건문) -> global tick 업데이트 + do_schedule() -> ★ global tick을 언제 업데이트 해야 하는지?
// 	* 인터럽트 활성화
// 	*/
// 	struct thread *curr = thread_current(); // RUNNING 상태에 있는 스레드를 반환
// 	enum intr_level old_level;
// 	// ASSERT (!intr_context ());
// 	old_level = intr_disable();

// 	if (curr != idle_thread)
// 	{ // ★ RUNNING 상태에 있는 스레드가 있어야 하는지, 없어야 하는지..? -> 일단 running이어야 했다고 가정
// 		// curr->status = THREAD_BLOCKED;
// 		// list_push_back (&sleep_list, &curr->elem);
// 		list_insert_ordered2(&sleep_list, &curr->elem, thread_less, NULL);
// 		// ((uint8_t *) &(LIST_ELEM)->next
// 	}
// 	do_schedule(THREAD_BLOCKED);
// 	intr_set_level(old_level);
// }

/* Suspends execution for approximately MS milliseconds. */
void timer_msleep(int64_t ms)
{
	real_time_sleep(ms, 1000);
}

/* Suspends execution for approximately US microseconds. */
void timer_usleep(int64_t us)
{
	real_time_sleep(us, 1000 * 1000);
}

/* Suspends execution for approximately NS nanoseconds. */
void timer_nsleep(int64_t ns)
{
	real_time_sleep(ns, 1000 * 1000 * 1000);
}

/* Prints timer statistics. */
void timer_print_stats(void)
{
	printf("Timer: %" PRId64 "	ticks\n", timer_ticks());
}

/* Timer interrupt handler. */
static void
timer_interrupt(struct intr_frame *args UNUSED)
{
	ticks++;
	thread_tick();
	// * 인터럽트 비활성화
	// 1) (추가 구현 4번 함수) 최솟값 tick을 리턴 받는다. -> wake up 할 스레드의 wakeup_tick을 찾는다.
	// 2) sleep_list로부터 해당 스레드를 제거하고 -> (추가 구현 2번 함수)
	// 3) 해당 스레드를 ready_list로 옮긴다.
	// 4) ★ 스레드의 멤버 state를 READY로 변경
	// 5) global tick 업데이트 -> sleep list에 있는 minimum tick을 업데이트
	// * 인터럽트 활성화

	// struct thread *curr = thread_current(); // RUNNING 상태에 있는 스레드를 반환

	// enum intr_level old_level;
	// old_level = intr_disable(); // 비활성화
	// timer_ticks();
	wake_up(ticks);
	// struct list_elem *sselem = NULL;
	// struct list *sleep_list = NULL;

	// struct thread *minimum_tick = list_entry(list_begin(&sleep_list), struct thread, elem);
	// int min_tick = list_entry(list_begin(&sleep_list), struct thread, elem)->wakeup_tick;
	// // list_entry(list_begin(&sleep_list), struct thread, elem)->wakeup_tick;
	// // sleep_list.head.next = list_next(list_entry(list_begin(&sleep_list), struct thread, elem));
	// while (min_tick <= ticks)
	// {
	// 	thread_unblock(list_entry(list_begin(&sleep_list), struct thread, elem)); // 언블락
	// 	// list_insert_ordered2(&ready_list, list_entry(list_begin(&sleep_list), struct thread, elem), less, NULL); // readylist에 추가
	// 	list_remove(&minimum_tick->elem);										 // sleeplist에서 지워
	// 	minimum_tick = list_entry(list_begin(&sleep_list), struct thread, elem); // 최소값 틱 갱신후 반복
	// }

	// // ticks = sleep_list.head.next; // 틱값 최소값으로
	// intr_set_level(old_level); // 활성화
}

/* Returns true if LOOPS iterations waits for more than one timer
   tick, otherwise false. */
static bool
too_many_loops(unsigned loops)
{
	/* Wait for a timer tick. */
	int64_t start = ticks;
	while (ticks == start)
		barrier();

	/* Run LOOPS loops. */
	start = ticks;
	busy_wait(loops);

	/* If the tick count changed, we iterated too long. */
	barrier();
	return start != ticks;
}

/* Iterates through a simple loop LOOPS times, for implementing
   brief delays.

   Marked NO_INLINE because code alignment can significantly
   affect timings, so that if this function was inlined
   differently in different places the results would be difficult
   to predict. */
static void NO_INLINE
busy_wait(int64_t loops)
{
	while (loops-- > 0)
		barrier();
}

/* Sleep for approximately NUM/DENOM seconds. */
static void
real_time_sleep(int64_t num, int32_t denom)
{
	/* Convert NUM/DENOM seconds into timer	ticks, rounding down.

	   (NUM / DENOM) s
	   ---------------------- = NUM * TIMER_FREQ / DENOM	ticks.
	   1 s / TIMER_FREQ	ticks
	   */
	int64_t ticks = num * TIMER_FREQ / denom;

	ASSERT(intr_get_level() == INTR_ON);
	if (ticks > 0)
	{
		/* We're waiting for at least one full timer tick.  Use
		   timer_sleep() because it will yield the CPU to other
		   processes. */
		timer_sleep(ticks);
	}
	else
	{
		/* Otherwise, use a busy-wait loop for more accurate
		   sub-tick timing.  We scale the numerator and denominator
		   down by 1000 to avoid the possibility of overflow. */
		ASSERT(denom % 1000 == 0);
		busy_wait(loops_per_tick * num / 1000 * TIMER_FREQ / (denom / 1000));
	}
}