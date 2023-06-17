#ifndef VM_ANON_H
#define VM_ANON_H
#include "vm/vm.h"
struct page;
enum vm_type;

// 06.16 : 1차 수정
struct anon_page { 
    // 익명 페이지의 상태 + 그 밖의 필요한 정보
	vm_initializer *init;   
	enum vm_type type;    
	void *aux;
	/* Initiate the struct page and maps the pa to the va */
	bool (*page_initializer) (struct page *, enum vm_type, void *kva);
};

void vm_anon_init (void);
bool anon_initializer (struct page *page, enum vm_type type, void *kva);

#endif
