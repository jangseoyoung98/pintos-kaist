/* anon.c: Implementation of page for non-disk image (a.k.a. anonymous page). */

#include "vm/vm.h"
#include "devices/disk.h"
#include "threads/mmu.h"

/* DO NOT MODIFY BELOW LINE */
static struct disk *swap_disk;	
struct bitmap *swap_bitmap;		// 사용 가능&불가능한 swap slot(disk)을 관리하는 자료구조

static bool anon_swap_in (struct page *page, void *kva);
static bool anon_swap_out (struct page *page);
static void anon_destroy (struct page *page);

/* DO NOT MODIFY this struct */
static const struct page_operations anon_ops = {
	.swap_in = anon_swap_in,
	.swap_out = anon_swap_out,
	.destroy = anon_destroy,
	.type = VM_ANON,
};

/* Initialize the data for anonymous pages */
void
vm_anon_init (void) { 
	/* TODO: Set up the swap_disk. */
	swap_disk = disk_get(1, 1);
	swap_bitmap = bitmap_create(disk_size(swap_disk)/8);
	// swap_bitmap = bitmap_create(disk_size(swap_disk));

}

/* Initialize the file mapping */
bool
anon_initializer (struct page *page, enum vm_type type, void *kva) {
	// ★★★
	struct uninit_page* uninit = &page->uninit;
	memset(uninit, 0, sizeof(struct uninit_page));

	/* Set up the handler */
	page->operations = &anon_ops;
	struct anon_page *anon_page = &page->anon;

	// ★★★
	anon_page->swap_index = -1;

	return true;
}

/* Swap in the page by read contents from the swap disk. */
static bool
anon_swap_in (struct page *page, void *kva) {
	struct anon_page *anon_page = &page->anon;

	int idx = anon_page->swap_index;
	
	if(bitmap_test(swap_bitmap, idx) == false){
		return false;
	}

	for ( int i = 0 ; i < 8 ; i ++){
		disk_read(swap_disk, idx * 8 + i, kva+(512*i)); // ptr..?
	}
	bitmap_set(swap_bitmap, idx, 0);
	
	return true;
}

/* Swap out the page by writing contents to the swap disk. */
static bool
anon_swap_out (struct page *page) {
	struct anon_page *anon_page = &page->anon;

	// bitmap_scan으로 idx구하기
	size_t idx = bitmap_scan(swap_bitmap, 0, 1, 0); // ★★★ (기존) disk_size(swap_disk)/8
	if ( idx == BITMAP_ERROR ) 
		return false;


	// 디스크에 write하기
	for ( int i = 0 ; i < 8 ; i ++){
		disk_write(swap_disk, (idx*8)+i, page->va+(512*i)); // ★★★ page->frame->kva+(512*i)
	}

	// bitmap 업데이트
	bitmap_set(swap_bitmap, idx, 1);
	// swap_slot 업데이트
	// page->swap_slot = idx; -> ★★★
	
	pml4_clear_page(thread_current()->pml4, page->va);
	anon_page->swap_index = idx;
	
	return true;
}

/* Destroy the anonymous page. PAGE will be freed by the caller. */
static void
anon_destroy (struct page *page) {
	struct anon_page *anon_page = &page->anon;
}
