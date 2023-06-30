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
//🔥 MMF -> 이거 구지 static으로 선언한 이유는..?
static struct disk* file_disk;
void
vm_file_init (void) {
	file_disk = NULL;
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
	struct file_page *file_page UNUSED = &page->file; // union 구조체에 이미 file이 됨
	struct thread* cur_thread = thread_current();

	//🔥 MMF
	bool is_dirty = pml4_is_dirty(cur_thread->pml4, page->va);
	if(is_dirty){ // 변경된 사항이 있으면
	// ★★★ 변경 사항을 파일에 다시 기록
	file_write_at(file_page->file, page->va, file_page->page_read_bytes, file_page->offset);
	pml4_set_dirty(cur_thread->pml4, page->va, 0); 
	}
	pml4_clear_page(cur_thread->pml4, page->va);
	// file_close(file_page->file);
}

// 06.27
/* Do the mmap */
void *
do_mmap (void *addr, size_t length, int writable, struct file *file, off_t offset) 
{	
	struct file* temp_re = file_reopen(file); // 도중에 file이 close 되는 불상사 예방	
	void* temp_addr = addr; 
	int num = 0;

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
		num++;
		addr += PGSIZE;

	}
	struct page* cur_page = spt_find_page(&thread_current()->spt, temp_addr);
	cur_page->seq_num = num;
	// cur_page->file.page_read_bytes = length; // 위에 while 내에서 계속 할당해야 하나..? 

	return temp_addr;
}

/* Do the munmap */
//🔥 MMF 주석 부분 (내가 원래 한 것)
void
do_munmap (void *addr) {

	// //06.28 
	struct thread* cur_thread = thread_current();
	struct page* cur_page = spt_find_page(&cur_thread->spt, addr); 
	int iter_num = cur_page->seq_num;
	void* temp_addr = addr;	

	int i = 1;
	while(i <= iter_num){ // file_backed_page의 시작 

		// bool is_dirty = pml4_is_dirty(cur_thread->pml4, cur_page->va);
		// if(is_dirty){ // 변경된 사항이 있으면
		// // ★★★ 변경 사항을 파일에 다시 기록
		// struct file* cur_file = cur_page->file.file;
		// file_write_at(cur_file, cur_page->va, cur_page->file.page_read_bytes, cur_page->file.offset);
		// pml4_set_dirty(cur_thread->pml4, cur_page->va, 0);

		// file_backed_destroy(cur_page);

		// temp_addr += PGSIZE;
		// cur_page = spt_find_page(&cur_thread->spt, temp_addr); 
		// i++;
		
		cur_page = spt_find_page(&cur_thread->spt, temp_addr); 
		if(!cur_page || cur_page->operations->type != VM_FILE) break; // 🔥예외 추가

		file_backed_destroy(cur_page);

		temp_addr += PGSIZE;
		i++;
	}
}

