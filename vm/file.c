/* file.c: Implementation of memory backed file object (mmaped object). */

#include "vm/vm.h"
// 06.27
#include "userprog/process.h"
#include "threads/vaddr.h"
#include "threads/mmu.h"

static bool file_backed_swap_in (struct page *page, void *kva);
static bool file_backed_swap_out (struct page *page);
static void file_backed_destroy (struct page *page);

/* DO NOT MODIFY this struct */
//💡 file_backed 페이지에 대한 함수 포인터 테이블
static const struct page_operations file_ops = {
	.swap_in = file_backed_swap_in,
	.swap_out = file_backed_swap_out,
	.destroy = file_backed_destroy, // 페이지를 삭제하는 함수
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
	struct file_page *file_page UNUSED = &page->file;
}

/* Swap out the page by writeback contents to the file. */
static bool
file_backed_swap_out (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;
}

/* Destory the file backed page. PAGE will be freed by the caller. */
static void
file_backed_destroy (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;

	if (pml4_is_dirty(thread_current()->pml4, page->va)){ // 접근 했으면 다시 써야됨!	
		file_write_at(file_page->file, page->va, file_page->page_read_bytes, file_page->offset);
		pml4_set_dirty(thread_current()->pml4, page->va, 0);
	}
	pml4_clear_page(thread_current()->pml4, page->va);
}
/* Do the mmap */
// 06.27
void *
do_mmap (void *addr, size_t length, int writable,
		struct file *file, off_t offset) {
	
	struct file *n_file = file_reopen(file);
	size_t temp_length = file_length(file);
	length = temp_length > length ? length : temp_length;

	void *result = addr;
	while (length > 0)
   {
		size_t page_read_bytes = length < PGSIZE ? length : PGSIZE;
		size_t page_zero_bytes = PGSIZE - page_read_bytes;

		struct lazy_load_file *aux = (struct lazy_load_file *) malloc(sizeof(struct lazy_load_file));
		aux->file = n_file;
		aux->page_read_bytes = page_read_bytes;
		aux->page_zero_bytes = page_zero_bytes;
		aux->ofs = offset;

		if (!vm_alloc_page_with_initializer(VM_FILE, addr, writable, lazy_load_segment, aux)){
			return NULL;
		}
		length -= page_read_bytes;
		offset += page_read_bytes;
		addr += PGSIZE;
	}
	int seq_num = (addr - result) / PGSIZE;
	struct page *page = spt_find_page(&thread_current()->spt, result);
	page->seq_num = seq_num;
	return result;
}

/* Do the munmap */
void
do_munmap (void *addr) {
	int temp = 0;
	struct page *page = spt_find_page(&thread_current()->spt, addr);
	while (page->seq_num-temp != 0){

	struct page *page = spt_find_page(&thread_current()->spt, addr);

	if (page == NULL && page->operations->type != VM_FILE) break;
	file_backed_destroy(page);
	addr += PGSIZE;
	temp++;
	}
}