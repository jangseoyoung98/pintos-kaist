/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"
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
// void destructor_func (struct hash_elem, void *aux);

/* Create the pending page object with initializer. If you want to create a
 * page, do not create it directly and make it through this function or
 * `vm_alloc_page`. */
bool
vm_alloc_page_with_initializer (enum vm_type type, void *par_va, bool writable,
		vm_initializer *init, void *aux) {

	ASSERT (VM_TYPE(type) != VM_UNINIT)

	struct supplemental_page_table *spt = &thread_current ()->spt;

	/* Check wheter the par_va is already occupied or not. */
	if (spt_find_page (spt, par_va) == NULL) {
		/* TODO: Create the page, fetch the initialier according to the VM type,
		 * TODO: and then create "uninit" page struct by calling uninit_new. You
		 * TODO: should modify the field after calling the uninit_new. */
		struct page *p = (struct page *) malloc(sizeof(struct page)); // 06.18
		if (p== NULL){
			return false;
		}
		switch (VM_TYPE(type)){ // 06.20
			case VM_ANON:
				uninit_new(p, par_va, init, type, aux, anon_initializer);
				break;
			case VM_FILE:
				uninit_new(p, par_va, init, type, aux, file_backed_initializer);
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
spt_find_page (struct supplemental_page_table *spt, void *va) {
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
spt_insert_page (struct supplemental_page_table *spt,
		struct page *page) {
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
static struct frame *
vm_get_frame (void) {
	struct frame *frame = (struct frame *) malloc(sizeof(struct frame));

    /* TODO: Fill this function. */
    // palloc_get_page()를 호출해서 새로운 물리 메모리 페이지를 가져온다.
    // 성공 : 프레임 할당 -> 프레임 구조체의 멤버들을 초기화 한 후 -> 해당 프레임을 반환한다.
    // 이후 모든 유저 공간 페이지들은 이 함수를 통해 할당한다.
    // 실패 : PANIC("todo")

	// malloc() -> kva = palloc_get_page

    frame->kva = palloc_get_page(PAL_USER); // 물리 프레임의 주소(kva)를 반환하는 것
	if (frame->kva == NULL){
        PANIC("todo");
    }
    frame->page = NULL;
    ASSERT (frame != NULL);
    ASSERT (frame->page == NULL);
    return frame;
}

/* Growing the stack. */
// 06.24
// ▶ addr을 통해 UNINIT 페이지를 만들고 앞서 생성한 stack_bottom 값을 설정
bool
vm_stack_growth (void *addr) {

	struct thread* cur_thread = thread_current();
	struct page* page = NULL;
	bool succ = true;

	void* page_up = cur_thread->stack_bottom - PGSIZE;
	while(page_up > addr){
		vm_alloc_page(VM_ANON, page_up, 1);
		if(!vm_claim_page(page_up))
			return false;
		// page = spt_find_page(&cur_thread->spt, page_up); 
		// vm_do_claim_page(page);

		cur_thread->stack_bottom = page_up;
		page_up = cur_thread->stack_bottom - PGSIZE;
	}
	if(!vm_alloc_page(VM_ANON, page_up, 1)){
		return false;
	}
	cur_thread->stack_bottom = page_up;
	return true;
}

/* Handle the fault on write_protected page */
static bool
vm_handle_wp (struct page *page UNUSED) {
}

/* Return true on success */
// 06.23 : 구현 1차
// 스택 확장이 가능한지 여부를 먼저 검사한다.
bool
vm_try_handle_fault (struct intr_frame *f, void *addr,
		bool user, bool write, bool not_present) {
	
	struct thread* cur_thread = thread_current();
	
	struct supplemental_page_table *spt = &cur_thread->spt;
	struct page *page = NULL;

	// 1. 주소 유효성 검사
	// if(!user){ // 유저 모드에서 엑세스 한 경우 -> 일단 false 리턴
	// 	return false;
	// }
	if (is_kernel_vaddr(addr) || !addr || !not_present)	{ // 주소가 커널 영역에 존재, 주소에서 데이터 얻을 수 없음, 읽기 전용 페이지에서 쓰기 시도 -> 프로세스 종료 + 프로세스 자원 해제
		return false;
	}	
	
	page = spt_find_page(spt, addr);
	if(page != NULL)
		goto done;

	if(addr > USER_STACK || addr < USER_STACK - (1<<20)) // 🔥 추가
		goto done;
	
	if(addr <= cur_thread->stack_bottom && addr >= (f->rsp - 8)){
		// if(addr < USER_STACK - (1 << 20)) // 스택 확장 최대 1MB 
		// 	return false;

		cur_thread->rsp_stack = f->rsp; // 현재 스레드(가 위치할 곳)의 주솟값
		if(!vm_stack_growth(addr)) // 스택을 확장하고 + 해당 주소에 페이지를 할당함
			return false;
	}
	goto done;

	// addr 이전의 페이지까지는 앞서 할당했기 때문에, 이젠 addr 페이지에 프레임을 할당할 차례!
	// page = spt_find_page(spt, addr);
	// if(!vm_do_claim_page(page)) 
	// 	return false;
done:
	// 06.28
	// 페이지 폴트가 발생하면 물리적 프레임이 즉시 할당되고, 내용이 파일에서 메모리로 복사된다.
	if(!vm_claim_page(pg_round_down(addr)))
		return false;
	
	return true;
	// if ( page == NULL && USER_STACK >= addr && addr >= USER_STACK-(1 << 20)){
	// 	// printf("여긴가?\n");
	// 	void *stack_bottom = thread_current()->stack_bottom;
	// 	void *new_stack_bottom = stack_bottom - PGSIZE;

	// 	if (f->rsp-8 <= addr  && addr <= stack_bottom) {   // addr이 grow limit안에 들어오는지 체크
	// 		vm_stack_growth(new_stack_bottom);
	// 	}
	// 	return vm_claim_page(new_stack_bottom);
	// }
	// if (page == NULL) return false;
	// return vm_do_claim_page(page);

}

/* Free the page.
 * DO NOT MODIFY THIS FUNCTION. */
void
vm_dealloc_page (struct page *page) {
	destroy (page);
	free (page);

// 06.23 -> 아래 거는 나중에!!!
/* palloc_get_page()를 이용해서 물리메모리 할당 */
/* switch문으로 vm_entry의 타입별 처리 (VM_BIN외의 나머지 타입은 mmf
와 swapping에서 다룸*/
/* VM_BIN일 경우 load_file()함수를 이용해서 물리메모리에 로드 */
/* install_page를 이용해서 물리페이지와 가상페이지 맵핑 */
/* 로드 성공 여부 반환 */
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

	if (frame == NULL)
		return false;

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
// 06.23 : Revisit 완료
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

/* Free the resource hold by the supplemental page table */
void
supplemental_page_table_kill (struct supplemental_page_table *spt UNUSED) {
	
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */
	
}

// void destructor_func (struct hash_elem hash_elem, void *aux) {
// 	hash_entry(hash_elem, struct page, hash_elem);

// } 