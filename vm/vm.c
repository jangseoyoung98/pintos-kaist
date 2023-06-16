/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include "threads/thread.h"
#include "threads/mmu.h"
#include "lib/string.h"


/* Initializes the virtual memory subsystem by invoking each subsystem's
 * intialize codes. */
void
vm_init (void) {
	vm_anon_init ();
	vm_file_init ();
#ifdef EFILESYS  /* For project 4 */
	pagecache_init ();
#endif
	register_inspect_intr ();
	/* DO NOT MODIFY UPPER LINES. */
	/* TODO: Your code goes here. */
}

/* Get the type of the page. This function is useful if you want to know the
 * type of the page after it will be initialized.
 * This function is fully implemented now. */
enum vm_type
page_get_type (struct page *page) {
	int ty = VM_TYPE (page->operations->type);
	switch (ty) {
		case VM_UNINIT:
			return VM_TYPE (page->uninit.type);
		default:
			return ty;
	}
}

/* Helpers */
static struct frame *vm_get_victim (void);
static bool vm_do_claim_page (struct page *page);
static struct frame *vm_evict_frame (void);

/* Create the pending page object with initializer. If you want to create a
 * page, do not create it directly and make it through this function or
 * `vm_alloc_page`. */
bool
vm_alloc_page_with_initializer (enum vm_type type, void *upage, bool writable,
		vm_initializer *init, void *aux) {

	ASSERT (VM_TYPE(type) != VM_UNINIT)

	struct supplemental_page_table *spt = &thread_current ()->spt;

	/* Check wheter the upage is already occupied or not. */
	if (spt_find_page (spt, upage) == NULL) {
		/* TODO: Create the page, fetch the initialier according to the VM type,
		 * TODO: and then create "uninit" page struct by calling uninit_new. You
		 * TODO: should modify the field after calling the uninit_new. */

		/* TODO: Insert the page into the spt. */
	}
err:
	return false;
}

/* Find VA from spt and return page. On error, return NULL. */
// ▶ 인자로 넘겨진 보조 페이지 테이블에서로부터 가상 주소(va)와 대응되는 페이지 구조체를 찾아서 반환한다. (실패시 NULL)
// 06.15 : 구현
// 06.16 : 수정 1차
struct page *
spt_find_page (struct supplemental_page_table *spt UNUSED, void *va UNUSED) {
    struct page *page = NULL;
    struct hash_elem *e;
    /* TODO: Fill this function. */
	
	// struct page temp_page;
	// temp_page.va = va; // spt가 UNUSED인 경우..?
	// // 구조체 초기화?
	// page = &temp_page;

	struct hash_iterator i;
	hash_first (&i, &spt->table); // 해시 첫번째 요소로 초기화
	while (hash_next (&i)) {
		
	page = hash_entry(hash_cur(&i), struct page, hash_elem);

	if(page->va == va)
		return page;
	}
	return NULL;

    // e = hash_find(&spt->table, &page->hash_elem);
    // return e != NULL ? hash_entry(e, struct page, hash_elem) : NULL;
}

/* Insert PAGE into spt with validation. */
// ▶ 인자로 주어진 보조 페이지 테이블에 구조체를 삽입 (보충 테이블에서 가상 주소가 존재하지 않는지 검사)
// 06.15 : 구현
bool
spt_insert_page (struct supplemental_page_table *spt UNUSED, struct page *page UNUSED) {
    int succ = false;
    /* TODO: Fill this function. */
    if(hash_insert(&spt->table, &page->hash_elem) == NULL) // 삽입 성공하면 NULL 리턴 (기존에 있으면 NULL 아님)
        succ = true;
    return succ;
}

void
spt_remove_page (struct supplemental_page_table *spt, struct page *page) {
	vm_dealloc_page (page);
	return true;
}

/* Get the struct frame, that will be evicted. */
static struct frame *
vm_get_victim (void) {
	struct frame *victim = NULL;
	 /* TODO: The policy for eviction is up to you. */

	return victim;
}

/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
static struct frame *
vm_evict_frame (void) {
	struct frame *victim UNUSED = vm_get_victim ();
	/* TODO: swap out the victim and return the evicted frame. */

	return NULL;
}

/* palloc() and get frame. If there is no available page, evict the page
 * and return it. This always return valid address. That is, if the user pool
 * memory is full, this function evicts the frame to get the available memory
 * space.*/
// ▶ 메모리 풀에서 새로운 물리메모리 페이지를 가져온다.
// 06.15 : 구현
// 06.16 : 수정 1차
static struct frame *
vm_get_frame (void) {
    struct frame *frame = NULL;
    /* TODO: Fill this function. */
    // palloc_get_page()를 호출해서 새로운 물리 메모리 페이지를 가져온다.
    // 성공 : 프레임 할당 -> 프레임 구조체의 멤버들을 초기화 한 후 -> 해당 프레임을 반환한다.
    // 이후 모든 유저 공간 페이지들은 이 함수를 통해 할당한다.
    // 실패 : PANIC("todo")
    frame = palloc_get_page(PAL_USER);
    if (frame == NULL){
        PANIC("todo");
    }
    memset(frame, 0, sizeof(frame));
    frame->kva = frame;
    frame->page = NULL;
    ASSERT (frame != NULL);
    ASSERT (frame->page == NULL);
    return frame;
}

/* Growing the stack. */
static void
vm_stack_growth (void *addr UNUSED) {
}

/* Handle the fault on write_protected page */
static bool
vm_handle_wp (struct page *page UNUSED) {
}

/* Return true on success */
bool
vm_try_handle_fault (struct intr_frame *f UNUSED, void *addr UNUSED,
		bool user UNUSED, bool write UNUSED, bool not_present UNUSED) {
	struct supplemental_page_table *spt UNUSED = &thread_current ()->spt;
	struct page *page = NULL;
	/* TODO: Validate the fault */
	/* TODO: Your code goes here */
	// 1. 페이지 폴트 발생 시, 해당 vm_entry의 존재 유무 확인
	// 2. vm_entry의 가상 주소에 해당하는 물리페이지 할당
	// 3. vm_entry의 정보를 참조하여, 디스크에 저장되어 있는 실제 데이터 로드
	

	return vm_do_claim_page (page);
}

/* Free the page.
 * DO NOT MODIFY THIS FUNCTION. */
void
vm_dealloc_page (struct page *page) {
	destroy (page);
	free (page);
}

/* Claim the page that allocate on VA. */
// ▶ va에 페이지를 할당하고, 해당 페이지에 프레임을 할당한다.
// 06.15 : 구현
// 06.16 : 수정 1차
bool
vm_claim_page (void *va UNUSED) {
    struct page *page = NULL;
    /* TODO: Fill this function */
    // page = pml4_get_page (thread_current()->pml4, va);
    page = spt_find_page(&thread_current()->spt, va);
	// ★ 페이지가 없는 경우 -> page_fault()로 처리해야 하나, 우선적으로 직접 만들겠음!
	if(page == NULL){
		page = palloc_get_page(PAL_USER);
	}

    // 한 페이지를 얻는다.
    // 해당 페이지를 인자로 갖는 vm_do_claim_page를 호출한다.
    return vm_do_claim_page(page);
}

/* Claim the PAGE and set up the mmu. */
// ▶ 인자로 주어진 page에 물리 메모리 프레임을 할당한다.
// 06.15 : 구현
// 06.16 : 수정 1차
static bool
vm_do_claim_page (struct page *page) {
    struct frame *frame = vm_get_frame(); // 프레임 하나를 얻는다.
    /* Set links */
    frame->page = page;
    page->frame = frame;
    /* TODO: Insert page table entry to map page's VA to frame's PA. */
    // 가상 주소와 물리 주소를 매핑한 정보를 페이지 테이블에 추가한다. (MMU 세팅)
    // 성공 : true / 실패 : false
    return pml4_set_page(&thread_current()->pml4, page, frame, true);
    // return swap_in (page, frame->kva);
}

/* Initialize new supplemental page table */
// ▶ 보조 페이지 테이블을 초기화 한다.
// 06.15 : 구현
void
supplemental_page_table_init (struct supplemental_page_table *spt UNUSED) {
	// 구현할 자료구조를 선택한다.
	// initd로 새로운 프로세스가 시작하거나 __do_fork로 자식 프로세스가 생성될 때 호출된다.
	hash_init(&spt->table, &spt->table.hash, &spt->table.less, NULL);
}

/* Copy supplemental page table from src to dst */
bool
supplemental_page_table_copy (struct supplemental_page_table *dst UNUSED,
		struct supplemental_page_table *src UNUSED) {
}

/* Free the resource hold by the supplemental page table */
void
supplemental_page_table_kill (struct supplemental_page_table *spt UNUSED) {
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */
}
