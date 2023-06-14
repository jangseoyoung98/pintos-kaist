#ifndef THREADS_PALLOC_H
#define THREADS_PALLOC_H

#include <stdint.h>
#include <stddef.h>

/* How to allocate pages. */
enum palloc_flags {
	PAL_ASSERT = 001,           /* Panic on failure. : 페이지가 할당될 수 없는 경우 커널 패닉 발생 */
	PAL_ZERO = 002,             /* Zero page contents. : 모든 바이트가 0인 페이지 할당 */
	PAL_USER = 004              /* User page. : 유저풀로부터 페이지 할당 */
};

/* Maximum number of pages to put in user pool. */
extern size_t user_page_limit;

uint64_t palloc_init (void);
void *palloc_get_page (enum palloc_flags);
void *palloc_get_multiple (enum palloc_flags, size_t page_cnt);
void palloc_free_page (void *);
void palloc_free_multiple (void *, size_t page_cnt);

#endif /* threads/palloc.h */
