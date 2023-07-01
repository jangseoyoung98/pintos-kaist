/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include "threads/mmu.h"
#include "lib/string.h"
#include "lib/kernel/hash.h"
#include "userprog/syscall.h"

struct list frame_table;
// ★
struct list_elem* start;

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
	list_init(&frame_table); // 6.30
	start = list_begin(&frame_table);
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
void hash_destroy_func(struct hash_elem *e, void* aux);

/* Create the pending page object with initializer. If you want to create a
 * page, do not create it directly and make it through this function or
 * `vm_alloc_page`. */
bool
vm_alloc_page_with_initializer (enum vm_type type, void *va, bool writable,
		vm_initializer *init, void *aux) {

	ASSERT (VM_TYPE(type) != VM_UNINIT)

	struct supplemental_page_table *spt = &thread_current ()->spt;

	/* Check wheter the va is already occupied or not. */
	if (spt_find_page (spt, va) == NULL) {
		/* TODO: Create the page, fetch the initialier according to the VM type,
		 * TODO: and then create "uninit" page struct by calling uninit_new. You
		 * TODO: should modify the field after calling the uninit_new. */
		struct page *p = (struct page *) malloc(sizeof(struct page)); // 06.18
		if (p== NULL){
			return false;
		}
		switch (VM_TYPE(type)){ // 06.20
			case VM_ANON:
				uninit_new(p, va, init, type, aux, anon_initializer);
				break;
			case VM_FILE:
				uninit_new(p, va, init, type, aux, file_backed_initializer);
				break;
			// case VM_PAGE_CACHE:
			default:
				return false;
		}
		p->writable = writable;
		/* TODO: Insert the page into the spt. */
		return spt_insert_page(spt, p);
	}
err:
	return false;
}

/* Find VA from spt and return page. On error, return NULL. */
struct page *
spt_find_page (struct supplemental_page_table *spt UNUSED, void *va UNUSED) {
    struct page *page = NULL;
    /* TODO: Fill this function. */
	
	page = (struct page *) malloc(sizeof(struct page));
	if (page == NULL) return NULL;

	page->va = pg_round_down(va);

    struct hash_elem *e = hash_find(&spt->table, &page->hash_elem);
    e = (e != NULL ? hash_entry(e, struct page, hash_elem) : NULL);
	
	free(page);
	return e;
}

/* Insert PAGE into spt with validation. */
bool
spt_insert_page (struct supplemental_page_table *spt UNUSED,
		struct page *page UNUSED) {
	int succ = false;
	/* TODO: Fill this function. */
	if(hash_insert(&spt->table, &page->hash_elem) == NULL) // 삽입 성공하면 NULL 리턴 (기존에 있으면 NULL 아님)
	{
        succ = true;
	}
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
	struct list_elem *e = start;
	struct thread *curr = thread_current();


	// if (!list_empty (&frame_table)) {
	// 	struct list_elem *e;
	// 	for (e = list_begin (&frame_table); e != list_end (&frame_table); e = e->prev){
	// 		struct frame *temp = list_entry(e, struct frame, frame_elem);
	// 		if ( pml4_is_accessed(thread_current()->pml4, temp->page->va) ){
	// 			pml4_set_accessed(thread_current()->pml4, temp->page->va, 0);
	// 		} else {
	// 			victim = temp;
	// 			break;
	// 		}
	// 	}
	// }
	// if (victim == NULL){
	// 	victim = list_entry(list_begin(&frame_table), struct frame, frame_elem);
	// }
	for (start = e; start != list_end(&frame_table); start = list_next(start))
	{
		victim = list_entry(start, struct frame, frame_elem);
		if (pml4_is_accessed(curr->pml4, victim->page->va))
			pml4_set_accessed(curr->pml4, victim->page->va, 0);
		else
			return victim;
	}

	for (start = list_begin(&frame_table); start != e; start = list_next(start))
	{
		victim = list_entry(start, struct frame, frame_elem);
		if (pml4_is_accessed(curr->pml4, victim->page->va))
			pml4_set_accessed(curr->pml4, victim->page->va, 0);
		else
			return victim;
	}


	return victim;
}

/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
static struct frame *
vm_evict_frame (void) {
	struct frame *victim = vm_get_victim ();
	/* TODO: swap out the victim and return the evicted frame. */
	// if ( victim == NULL ) return NULL;

	swap_out(victim->page);
	return victim;
}

/* palloc() and get frame. If there is no available page, evict the page
 * and return it. This always return valid address. That is, if the user pool
 * memory is full, this function evicts the frame to get the available memory
 * space.*/
static struct frame *
vm_get_frame (void) {
	struct frame *frame = (struct frame *) malloc(sizeof(struct frame));

    /* TODO: Fill this function. */
    // palloc_get_page()를 호출해서 새로운 물리 메모리 페이지를 가져온다.
    // 성공 : 프레임 할당 -> 프레임 구조체의 멤버들을 초기화 한 후 -> 해당 프레임을 반환한다.
    // 이후 모든 유저 공간 페이지들은 이 함수를 통해 할당한다.
    // 실패 : PANIC("todo")

	// malloc() -> kva = palloc_get_page
	bool is_evic = false;

    frame->kva = palloc_get_page(PAL_USER); // 물리 프레임의 주소(kva)를 반환하는 것
	if (frame->kva == NULL){
        // PANIC("todo");
		frame = vm_evict_frame(); // 6.30

		is_evic = true;
    }
	
	if(!is_evic){ // 07.01 추가
	list_push_back(&frame_table, &frame->frame_elem);
	}
	frame->page = NULL;
    ASSERT (frame != NULL);
    ASSERT (frame->page == NULL);
	
    return frame;
}

/* Growing the stack. */
static void
vm_stack_growth (void *new_stack_bottom UNUSED) {

	if(vm_alloc_page(VM_ANON, new_stack_bottom, true))	   // 페이지 할당


	thread_current()->stack_bottom = new_stack_bottom;	   // 현제 쓰레드의 stack_bottom 값 설정.
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
	if (is_kernel_vaddr(addr) || !addr || !not_present)	{
		return false;
	}

	page = spt_find_page(spt, addr);
	if ( page == NULL && USER_STACK >= addr && addr >= USER_STACK-(1 << 20)){
		void *stack_bottom = thread_current()->stack_bottom;
		void *new_stack_bottom = stack_bottom - PGSIZE;

		if (f->rsp-8 <= addr  && addr <= stack_bottom) {   // addr이 grow limit안에 들어오는지 체크
			vm_stack_growth(new_stack_bottom);
		}
		return vm_claim_page(new_stack_bottom);
	}
	if (page == NULL) return false;

	return vm_do_claim_page(page);
}

/* Free the page.
 * DO NOT MODIFY THIS FUNCTION. */
void
vm_dealloc_page (struct page *page) {
	destroy (page);
	free (page);
}

/* Claim the page that allocate on VA. */
bool
vm_claim_page (void *va UNUSED) {
	struct page *page = NULL;
	/* TODO: Fill this function */
	page = spt_find_page(&thread_current()->spt, va);

	if(page == NULL){
		return false;
		// vm_alloc_page(VM_ANON, va, 1); // 06.19 나중에 변경!
		// page = spt_find_page(&thread_current()->spt, va);
	}

	return vm_do_claim_page (page);
}

/* Claim the PAGE and set up the mmu. */
static bool
vm_do_claim_page (struct page *page) {
	struct frame *frame = vm_get_frame ();

	if (frame == NULL){
		return false;
	}

	/* Set links */
	frame->page = page;
	page->frame = frame;

	/* TODO: Insert page table entry to map page's VA to frame's PA. */
	if (pml4_get_page(thread_current()->pml4, page->va) == NULL && pml4_set_page(thread_current()->pml4, page->va, frame->kva, page->writable)){
		return swap_in (page, frame->kva);
	}
	return false;
}

/* Initialize new supplemental page table */
void
supplemental_page_table_init (struct supplemental_page_table *spt UNUSED) {
	hash_init(&spt->table, hash_hash, hash_less, NULL);
}

/* Copy supplemental page table from src to dst */
bool
supplemental_page_table_copy (struct supplemental_page_table *dst ,
		struct supplemental_page_table *src ) {
	struct hash_iterator i;

	hash_first (&i, &src->table);
	while (hash_next (&i))
	{
		struct page *par_page = hash_entry (hash_cur (&i), struct page, hash_elem);
		vm_alloc_page_with_initializer(page_get_type(par_page), par_page->va, par_page->writable, par_page->anon.init, par_page->anon.aux);
		
		if (par_page->operations->type == VM_ANON){
			vm_claim_page(par_page->va);
			memcpy(spt_find_page(dst, par_page->va)->frame->kva, par_page->frame->kva, PGSIZE);
		}

	};
	return true;

}

// 06.21 : kill 구현 (1)
void
supplemental_page_table_kill (struct supplemental_page_table *spt) {
	// /* TODO: Destroy all the supplemental_page_table hold by thread and
	//  * TODO: writeback all the modified contents to the storage. */
	
	// hash_destroy()로 해시 테이블의 버킷 리스트와, vm_entry(page-hash_elem) 제거
	hash_destroy(&spt->table, hash_destroy_func);	

	// 추가 : spt_remove_page()도 있다..! -> 깃북을 보면 spt는 함수 호출자가 알아서 정리한다고 한다.

}

// 06.21 : kill 구현 (2)
// ▶ 해시 테이블 제거 함수
void
hash_destroy_func(struct hash_elem *e, void* aux){

	/* Get hash element (hash_entry() 사용) */
	/* load가 되어 있는 page의 vm_entry인 경우
	page의 할당 해제 및 page mapping 해제 (palloc_free_page()와
	pagedir_clear_page() 사용) */
	/* vm_entry 객체 할당 해제 */
	struct page* kill_page = hash_entry(e, struct page, hash_elem);
	vm_dealloc_page(kill_page);

	// 페이지 로드 여부 확인 및 page 할당 해제 + page mapping 해제 -> 이거 지금 해야 하나?
}

