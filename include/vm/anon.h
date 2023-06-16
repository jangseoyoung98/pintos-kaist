#ifndef VM_ANON_H
#define VM_ANON_H
#include "vm/vm.h"
struct page;
enum vm_type;

// 💬 : 익명 페이지를 위해 우리가 기억해야 하는 필수적인 정보들
struct anon_page {
};

void vm_anon_init (void);
bool anon_initializer (struct page *page, enum vm_type type, void *kva);

#endif
