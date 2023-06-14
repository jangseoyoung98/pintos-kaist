#ifndef VM_VM_H
#define VM_VM_H
#include <stdbool.h>
#include "threads/palloc.h"

enum vm_type { /* 가상 메모리 타입들 */
	/* page not initialized : 초기화 되지 않은 페이지들 (디폴트) */
	VM_UNINIT = 0,
	/* page not related to the file, aka anonymous page : anonymous page */
	VM_ANON = 1,
	/* page that realated to the file : file-backed page */
	VM_FILE = 2,
	/* page that hold the page cache, for project 4 */
	VM_PAGE_CACHE = 3,

	/* Bit flags to store state */

	/* Auxillary bit flag marker for store information. You can add more
	 * markers, until the value is fit in the int. */
	VM_MARKER_0 = (1 << 3),
	VM_MARKER_1 = (1 << 4),

	/* DO NOT EXCEED THIS VALUE. */
	VM_MARKER_END = (1 << 31),
};

#include "vm/uninit.h"
#include "vm/anon.h"
#include "vm/file.h"
#ifdef EFILESYS
#include "filesys/page_cache.h"
#endif

struct page_operations;
struct thread;

#define VM_TYPE(type) ((type) & 7)

/*
 * This is kind of "parent class", which has four "child class"es, which are
 * uninit_page, file_page, anon_page, and page cache (project4).
 * DO NOT REMOVE/MODIFY PREDEFINED MEMBER OF THIS STRUCTURE. */
 //💡가상 메모리 공간의 페이지
struct page {
	const struct page_operations *operations; // 페이지 작업용
	void *va;              /* Address in terms of user space : 유저 영역의 가상 메모리 */
	struct frame *frame;   /* Back reference for frame : 물리 메모리 */

	/* Your implementation */
	
	/* Per-type data are binded into the union.
	 * Each function automatically detects the current union */
	// 페이지의 속성은 아래 4개 중 하나가 됨
	union {
		struct uninit_page uninit; 
		struct anon_page anon; 
		struct file_page file;
#ifdef EFILESYS
		struct page_cache page_cache;
#endif
	};
};


/* The representation of "frame" */
struct frame {
	void *kva; 			// 커널 가상 주소
	struct page *page;	// 페이지 구조체를 담기 위한 멤버
	// 💬 프레임 관리 인터페이스 구현 과정에서 다른 멤버들 추가 가능
};

/* The function table for page operations.
 * This is one way of implementing "interface" in C.
 * Put the table of "method" into the struct's member, and
 * call it whenever you needed. */
//💡 3개의 함수 포인터를 포함한 하나의 함수 테이블
struct page_operations {
	bool (*swap_in) (struct page *, void *);
	bool (*swap_out)- (struct page *);
	void (*destroy) (struct page *);
	enum vm_type type;
};

#define swap_in(page, v) (page)->operations->swap_in ((page), v)
#define swap_out(page) (page)->operations->swap_out (page)
#define destroy(page) \
	if ((page)->operations->destroy) (page)->operations->destroy (page)

/* Representation of current process's memory space.
 * We don't want to force you to obey any specific design for this struct.
 * All designs up to you for this. */

struct supplemental_page_table {
	// 각각의 페이지에 대해 데이터가 존재하는 곳 (frame, disk, swap 중)
	// 위에 상응하는 커널 가상 주소를 가리키는 포인터 정보
	// active인지 inactive인지

	// vm_entry

};

#include "threads/thread.h"
void supplemental_page_table_init (struct supplemental_page_table *spt);
bool supplemental_page_table_copy (struct supplemental_page_table *dst,
		struct supplemental_page_table *src);
void supplemental_page_table_kill (struct supplemental_page_table *spt);
struct page *spt_find_page (struct supplemental_page_table *spt,
		void *va);
bool spt_insert_page (struct supplemental_page_table *spt, struct page *page);
void spt_remove_page (struct supplemental_page_table *spt, struct page *page);

void vm_init (void);
bool vm_try_handle_fault (struct intr_frame *f, void *addr, bool user,
		bool write, bool not_present);

#define vm_alloc_page(type, upage, writable) \
	vm_alloc_page_with_initializer ((type), (upage), (writable), NULL, NULL)
bool vm_alloc_page_with_initializer (enum vm_type type, void *upage,
		bool writable, vm_initializer *init, void *aux);
void vm_dealloc_page (struct page *page);
bool vm_claim_page (void *va);
enum vm_type page_get_type (struct page *page);

#endif  /* VM_VM_H */
