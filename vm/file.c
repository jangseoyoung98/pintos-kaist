/* file.c: Implementation of memory backed file object (mmaped object). */

#include "vm/vm.h"
// 06.27
#include "filesys/inode.h"
#include "userprog/process.h"
#include "threads/vaddr.h"
#include "lib/round.h"
#include "threads/mmu.h"
// 06.28
#include "lib/string.h"


static bool file_backed_swap_in (struct page *page, void *kva);
static bool file_backed_swap_out (struct page *page);
static void file_backed_destroy (struct page *page);

/* DO NOT MODIFY this struct */
static const struct page_operations file_ops = {
	.swap_in = file_backed_swap_in,
	.swap_out = file_backed_swap_out,
	.destroy = file_backed_destroy,
	.type = VM_FILE,
};

/* The initializer of file vm */
void
vm_file_init (void) {
}

/* Initialize the file backed page */
bool
file_backed_initializer (struct page *page, enum vm_type type, void *kva) {
	/* Set up the handler */
	page->operations = &file_ops;

	struct file_page *file_page = &page->file;
}

/* Swap in the page by read contents from the file. */
static bool
file_backed_swap_in (struct page *page, void *kva) {
	struct file_page *file_page = &page->file;
}

/* Swap out the page by writeback contents to the file. */
static bool
file_backed_swap_out (struct page *page) {
	struct file_page *file_page = &page->file;
}

// 06.27
/* Destory the file backed page. PAGE will be freed by the caller. */
static void
file_backed_destroy (struct page *page) {
	struct file_page *file_page = &page->file; // union 구조체에 이미 file이 됨
	// struct file* file_re = file_reopen();

	struct thread* cur_thread = thread_current();

	bool is_dirty = pml4_is_dirty(cur_thread->pml4, file_page);
	if(is_dirty){ // 변경된 사항이 있으면
		// ★★★ 변경 사항을 파일에 다시 기록
		
		pml4_set_dirty(cur_thread->pml4, file_page, 0);	
	}
}

// 06.27
/* Do the mmap */
void *
do_mmap (void *addr, size_t length, int writable, struct file *file, off_t offset) 
{	
	struct file* temp_re = file_reopen(file); // 도중에 file이 close 되는 불상사 예방
	void* temp_addr = addr; 
	int num = 1;

	uint32_t read_bytes = length;
	while(read_bytes > 0)
	{
		size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
		size_t page_zero_bytes = PGSIZE - page_read_bytes;
		// 예외 처리 : 최종 매핑된 페이지의 일부 바이트가 파일 끝을 넘어 stick out 됨 (페이지 폴트 시, 이 바이트를 0으로 설정하고, 페이지를 디스크에 다시 쓸 때 버림)
		// -> lazy_load_segment로 가면 됨

		struct lazy_load_file* aux = (struct lazy_load_file*)malloc(sizeof(struct lazy_load_file));

		aux->file = temp_re; // file로..?
		aux->page_read_bytes = page_read_bytes;
		aux->page_zero_bytes = page_zero_bytes;
		aux->ofs = offset;

		if(!vm_alloc_page_with_initializer(VM_FILE, addr, writable, lazy_load_segment, aux))
			return NULL;
		
		read_bytes -= page_read_bytes;
		offset += page_read_bytes;
		
		// 06.28
		// ★ 위에 while문이 얼마나 돌았는지 기록해야 함!! munmap으로 돌릴 페이지 갯수를 여기 시퀀스에서 저장해야 함 (while문 조건)
		// page 구조체의 seq_num 멤버 이용해서 기록!
		struct page* page = spt_find_page(&thread_current()->spt, addr);
		page->seq_num = num++;

		addr += PGSIZE;
	}

	return temp_addr;
}

/* Do the munmap */
void
do_munmap (void *addr) {

	// //06.28 
	// struct thread* cur_thread = thread_current();
	// struct page* cur_page = spt_find_page(&cur_thread->spt, addr);
	// struct page* next_page = spt_find_page(&cur_thread->spt, addr +PGZI);
	// void* temp_addr = addr;

	// while(temp_page->seq_num ){ // 모든 
	// 	temp_page = spt_find_page(&cur_thread->spt, addr);
	// 	file_backed_destroy(temp_page);
	// }
	// struct file_page *file_page = &temp_page->file; 
	// bool is_dirty = pml4_is_dirty(cur_thread->pml4, file_page);

	// if(is_dirty){
	// 	pml4_clear_page(cur_thread->pml4, temp_page);
	// 	addr += PGSIZE;
	// }

}
