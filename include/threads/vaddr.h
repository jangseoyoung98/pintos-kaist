#ifndef THREADS_VADDR_H
#define THREADS_VADDR_H

#include <debug.h>
#include <stdint.h>
#include <stdbool.h>

#include "threads/loader.h"

/* Functions and macros for working with virtual addresses.
 *
 * See pte.h for functions and macros specifically for x86
 * hardware page tables. */

#define BITMASK(SHIFT, CNT) (((1ul << (CNT)) - 1) << (SHIFT))

/* Page offset (bits 0:12). */
#define PGSHIFT 0                          /* Index of first offset bit. : 가상 주소 오프셋 부분의 인덱스 */
#define PGBITS  12                         /* Number of offset bits. : 가상 주소 오프셋 부분의 bit 수 */
#define PGSIZE  (1 << PGBITS)              /* Bytes in a page. : 바이트 단위에서 페이지 크기 */
#define PGMASK  BITMASK(PGSHIFT, PGBITS)   /* Page offset bits (0:12). : 페이지 오프셋 부분의 비트들에 1, 나머지는 0으로 설정되어 있는 비트마스킹 매크로 */

/* Offset within a page. */
#define pg_ofs(va) ((uint64_t) (va) & PGMASK) /* 가상 주소 va의 페이지 오프셋을 뽑아내 반환 */

#define pg_no(va) ((uint64_t) (va) >> PGBITS) /* 가상 주소 va의 페이지 번호를 뽑아내 반환 */

/* Round up to nearest page boundary. */
#define pg_round_up(va) ((void *) (((uint64_t) (va) + PGSIZE - 1) & ~PGMASK)) /* 가장 근접한 페이지 경계 값으로 올림된 va 반환*/

/* Round down to nearest page boundary. */
#define pg_round_down(va) (void *) ((uint64_t) (va) & ~PGMASK) /* 내부에서 va가 가리키는 가상 페이지의 시작 (페이지 오프셋이 0으로 설정된 va) 반환*/

/* Kernel virtual address start */
#define KERN_BASE LOADER_KERN_BASE /* 유저 가상 메모리(0~KERN_BASE), 커널 가상 메모리(나머지 부분)의 경계 */

/* User stack start */
#define USER_STACK 0x47480000

/* Returns true if VADDR is a user virtual address. */
#define is_user_vaddr(vaddr) (!is_kernel_vaddr((vaddr))) /* va(가상주소)가 유저 가상 주소라면 true 리턴, 커널이면 false */

/* Returns true if VADDR is a kernel virtual address. */
#define is_kernel_vaddr(vaddr) ((uint64_t)(vaddr) >= KERN_BASE) /* va(가상주소)가 커널 가상 주소라면 true 리턴, 유저면 false */

// FIXME: add checking
/* Returns kernel virtual address at which physical address PADDR
 *  is mapped. */
#define ptov(paddr) ((void *) (((uint64_t) paddr) + KERN_BASE)) /* 0 ~ (물리 주소 크기) 내 존재하는 물리 주소(pa)에 대응되는 커널 가상 주소 반환 */

/* Returns physical address at which kernel virtual address VADDR
 * is mapped. */ 
#define vtop(vaddr) \ /* 커널 가상 주소(va)와 대응되는 물리 주소 반환 */
({ \
	ASSERT(is_kernel_vaddr(vaddr)); \
	((uint64_t) (vaddr) - (uint64_t) KERN_BASE);\
})

#endif /* threads/vaddr.h */
