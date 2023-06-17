/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include "threads/thread.h"
#include "threads/mmu.h"
#include "lib/string.h"
// 06.17 : 추가
#include "vm/anon.h"
#include "vm/uninit.h"
#include "vm/file.h"

// 06.17 : 추가
// struct list frame_table; // 아래 함수들..frame 관련된 것들 전부..frame 생성할 때마다 frame table에 넣어야 할 듯..?

// 06.17 : 위치 변경
typedef bool (*page_initializer) (struct page *, enum vm_type, void *kva);


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
// 06.16 : 구현
// 06.17 : 1차 수정
// ▶ (초기화 된) 새로운 페이지 구조체를 할당한다.
bool
vm_alloc_page_with_initializer (enum vm_type type, void *upage, bool writable,
		vm_initializer *init, void *aux) {// upage -> 새로 만들 페이지의 va

	// ※ 이 함수는 커널이 새로운 페이지 달라는 요청 받으면 호출된다.
	// ※ 페이지 폴트를 처리하는 과정에서, uninit_initialize() 함수가 호출되는데 이때 여기서 셋팅한 초기화 함수를 사용된다.

	// 1) 페이지 구조체를 할당하고 
	// 2) 페이지 타입에 맞는 적절한 초기화 함수를 세팅함으로써 새로운 페이지를 초기화한다.
	//    -> 초기화 함수 종류 : anon_initializer / file_backed_initializer
	// 	  -> VM_TYPE에 맞는 적절한 초기화 함수를 가져오고
	// 3) 이 함수를 인자로 갖는 uninit_new를 호출한다.
	// 4) uninit_new 호출 후에 spt에 페이지를 삽입한다.
	
	ASSERT (VM_TYPE(type) != VM_UNINIT)

	struct supplemental_page_table *spt = &thread_current()->spt;

	/* Check wheter the upage is already occupied or not. */
	if (spt_find_page(spt, upage) == NULL) {
		/* TODO: Create the page, fetch the initialier according to the VM type,
		 * TODO: and then create "uninit" page struct by calling uninit_new. You
		 * TODO: should modify the field after calling the uninit_new. */
		/* TODO: Insert the page into the spt. */
		
		struct page p;
		page_initializer new_initializer = NULL;
		switch (type){
			case VM_UNINIT:
				new_initializer = &uninit_initializer;
			case VM_ANON:
				new_initializer = &anon_initializer;	
			case VM_FILE:
				new_initializer = &file_backed_initializer;
			// case VM_PAGE_CACHE:
		}
		uninit_new(&p, upage, init, type, aux, new_initializer);
		
		spt_insert_page(spt, &p);
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
// 06.17 : 수정 2차
static struct frame *
vm_get_frame (void) {
    struct frame *frame = NULL;
    /* TODO: Fill this function. */
    // palloc_get_page()를 호출해서 새로운 물리 메모리 페이지를 가져온다.
    // 성공 : 프레임 할당 -> 프레임 구조체의 멤버들을 초기화 한 후 -> 해당 프레임을 반환한다.
    // 이후 모든 유저 공간 페이지들은 이 함수를 통해 할당한다.
    // 실패 : PANIC("todo")
	// 06.17 : 추가 + 수정
	frame = malloc(sizeof(struct frame));
    frame->kva = palloc_get_page(PAL_USER); // 물리 프레임의 주소(kva)를 반환하는 것
	if (frame->kva == NULL){
        PANIC("todo");
		// 추후 vm_evict_frame() 호출
    }
    memset(frame, 0, sizeof(frame)); 
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
// ANON 2
bool
vm_try_handle_fault (struct intr_frame *f UNUSED, void *addr UNUSED,
	bool user UNUSED, bool write UNUSED, bool not_present UNUSED) 
{
	// vm_try_handle_fault (f, fault_addr, user, write, not_present)
	struct supplemental_page_table *spt UNUSED = &thread_current ()->spt;
	struct page *page = NULL;
	/* TODO: Validate the fault */
	/* TODO: Your code goes here */
	// 1. 페이지 폴트 발생 시, 해당 vm_entry의 존재 유무 확인 (물리 메모리 매핑 되어 있는지)
	// page = pml4_get_page(thread_current()->pml4, addr);
	// if(!page){
	// 	page = addr;
	// 	return vm_do_claim_page(page); // 페이지와 프레임을 연결 -> 찐 페이지 폴트일 때 호출!
	// }
	page = spt_find_page(spt, addr);
	if(!pml4_get_page(thread_current()->pml4, addr))
		return vm_do_claim_page(page);


	// ★ intr_frame이 어디서 사용되는지 모르겠다.
	// ★ bogus fault일 때, 나머지 인자를 어떻게 사용할지 생각해 보기!

	// 2. vm_entry의 가상 주소에 해당하는 물리페이지 할당
	
	// 3. vm_entry의 정보를 참조하여, 디스크에 저장되어 있는 실제 데이터 로드
	


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
// 06.17 : 수정 2차
bool
vm_claim_page (void *va UNUSED) {
    struct page *page = NULL;
    /* TODO: Fill this function */
    // page = pml4_get_page (thread_current()->pml4, va);
    page = spt_find_page(&thread_current()->spt, va);
	// ★ 페이지가 없는 경우 -> page_fault()로 처리해야 하나, 우선적으로 직접 만들겠음!
	// 06.17 : page 대신 page->va로 바꿈
	if(page == NULL){
		page->va = palloc_get_page(PAL_USER); // va 리턴
	}

    // 한 페이지를 얻는다.
    // 해당 페이지를 인자로 갖는 vm_do_claim_page를 호출한다.
    return vm_do_claim_page(page);
}

/* Claim the PAGE and set up the mmu. */
// ▶ 인자로 주어진 page에 물리 메모리 프레임을 할당한다.
// 06.15 : 구현
// 06.16 : 수정 1차
// 06.16 : 수정 2차 - writable
// 06.17 : 수정 3차
static bool
vm_do_claim_page (struct page *page) {
	// 06.17 : 추가
	// 해당 페이지가 이미 물리 주소와 연결되어 있는지 확인 -> 있으면 ptov로 전환된 kva 반환 / 없으면 NULL
	if(pml4_get_page(&thread_current()->pml4, page->va)){
		// page->frame->kva = 
		// 매핑되어 있다는 건..return false?(bool)'
		return false;
	}
	
	// 매핑되어 있지 않은 경우에 한해서 진행하는데..!
	// (미리 연결된 kva가 없는 경우) 해당 va를 kva에 set 한다.
	// 최종적으로 페이지와 프레임 간의 연결이 완료되었을 경우, swap_in()을 통해 해당 페이지를 물리 메모리에 올린다.
    
	/* 기존 코드 */
	struct frame *frame = vm_get_frame(); // 프레임 하나를 얻는다.
    /* Set links */
    frame->page = page;
    page->frame = frame;
	/*          */

    /* TODO: Insert page table entry to map page's VA to frame's PA. */
    // 가상 주소와 물리 주소를 매핑한 정보를 페이지 테이블에 추가한다. (MMU 세팅)
    // 성공 : true / 실패 : false
    
	// 06.17 : 수정
	// is_writable(pte) 대신 일단 r/w 페이지는 나중에 다룰 것이므로, 1로 설정하겠음
	// pml4_set_page(&thread_current()->pml4, page, frame, 1);
	if(pml4_set_page(&thread_current()->pml4, page->va, frame->kva, 1)){ // 아닌가..?

	/* 기존 코드 */
	return swap_in (page, frame->kva); // operations 셋팅에 성공하면 bool을 리턴함 (true..?)
	/*          */
	}

	return false; // 06.17 : 추가

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