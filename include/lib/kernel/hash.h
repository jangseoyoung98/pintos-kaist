#ifndef __LIB_KERNEL_HASH_H
#define __LIB_KERNEL_HASH_H

/* 해시 테이블.
 *
 * 이 데이터 구조는 프로젝트 3의 핀토스 둘러보기에 자세히 설명되어 있습니다.
 *
 * 이것은 체인이 있는 표준 해시 테이블입니다.  
 * 테이블에서 요소를 찾으려면 요소의 데이터에 대한 해시 함수를 계산하고 
 * 이를 이중으로 연결된 목록 배열의 인덱스로 사용한 다음 목록을 선형적으로 검색합니다.
 *
 * 체인 목록은 동적 할당을 사용하지 않습니다.  
 * 대신, 해시에 포함될 수 있는 각 구조체는 구조체 해시_엘렘 멤버를 포함해야 합니다.  
 * 모든 해시 함수는 이러한 '구조체 해시_엘렘'에서 작동합니다.  
 * hash_entry 매크로를 사용하면 구조체 해시 엘리먼트에서 이를 포함하는 구조체 객체로 다시 변환할 수 있습니다.  
 * 이는 링크드 리스트 구현에서 사용된 것과 동일한 기법입니다.  
 * 자세한 설명은 lib/kernel/list.h를 참조하세요. */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "list.h"


/* Hash element. */
// 해시 테이블에 포함시키고 싶은 구조체의 멤버로 삽입되는 형태.
struct hash_elem {
	struct list_elem list_elem;
};

/* Converts pointer to hash element HASH_ELEM into a pointer to
 * the structure that HASH_ELEM is embedded inside.  Supply the
 * name of the outer structure STRUCT and the member name MEMBER
 * of the hash element.  See the big comment at the top of the
 * file for an example. */
// 기존에 주어진 STRUCT에서 특정 member를 찾아낸다.
#define hash_entry(HASH_ELEM, STRUCT, MEMBER)                   \
	((STRUCT *) ((uint8_t *) &(HASH_ELEM)->list_elem        \
		- offsetof (STRUCT, MEMBER.list_elem)))

/* Computes and returns the hash value for hash element E, given
 * auxiliary data AUX. */
// 해쉬값
typedef uint64_t hash_hash_func (const struct hash_elem *e, void *aux);

/* Compares the value of two hash elements A and B, given
 * auxiliary data AUX.  Returns true if A is less than B, or
 * false if A is greater than or equal to B. */
typedef bool hash_less_func (const struct hash_elem *a,
		const struct hash_elem *b,
		void *aux);

/* Performs some operation on hash element E, given auxiliary
 * data AUX. */
typedef void hash_action_func (struct hash_elem *e, void *aux);

/* Hash table. */
// hash 테이블은 hash_elem 타입의 원소로 연산한다!
struct hash {
	size_t elem_cnt;            /* 해시 테이블에 저장된 원소(아이템)의 수를 나타냅니다.  */
	size_t bucket_cnt;          /* 해시 테이블의 버킷 수를 나타냅니다. 일반적으로 이는 2의 거듭제곱입니다. */
	struct list *buckets;       /* 실제 해시 테이블의 버킷들을 가리키는 포인터를 담고 있습니다.  */
	hash_hash_func *hash;       /* 해시 함수를 나타냅니다. 이 함수는 키를 입력으로 받아 해시 값을 생성합니다. */
	hash_less_func *less;       /* 비교 함수를 나타냅니다. 이 함수는 두 개의 키를 비교하여 첫 번째 키가 두 번째 키보다 '작은지'를 판단합니다.
								   이는 해시 충돌 해결이나 키 기반 검색에서 사용됩니다. */
	void *aux;                  /* hash 함수와 less 함수에 추가로 전달되는 보조 데이터를 나타냅니다.  */
};

/* A hash table iterator. */
struct hash_iterator {
	struct hash *hash;          /* 순회할 해시 테이블을 가리키는 포인터입니다. */
	struct list *bucket;        /* 현재 반복자가 위치한 버킷을 가리키는 포인터입니다. */
	struct hash_elem *elem;     /*현재 반복자가 가리키고 있는 해시 테이블의 원소를 가리키는 포인터입니다. */
};

/* Basic life cycle. */
bool hash_init (struct hash *, hash_hash_func *, hash_less_func *, void *aux);
void hash_clear (struct hash *, hash_action_func *);
void hash_destroy (struct hash *, hash_action_func *);

/* Search, insertion, deletion. */
struct hash_elem *hash_insert (struct hash *, struct hash_elem *);
struct hash_elem *hash_replace (struct hash *, struct hash_elem *);
struct hash_elem *hash_find (struct hash *, struct hash_elem *);
struct hash_elem *hash_delete (struct hash *, struct hash_elem *);

/* Iteration. */
void hash_apply (struct hash *, hash_action_func *);
void hash_first (struct hash_iterator *, struct hash *);
struct hash_elem *hash_next (struct hash_iterator *);
struct hash_elem *hash_cur (struct hash_iterator *);

/* Information. */
size_t hash_size (struct hash *);
bool hash_empty (struct hash *);

/* Sample hash functions. */
uint64_t hash_bytes (const void *, size_t);
uint64_t hash_string (const char *);
uint64_t hash_int (int);

bool hash_less (const struct hash_elem *a_, const struct hash_elem *b_, void *aux );
uint64_t hash_hash (const struct hash_elem *p_, void *aux );

#endif /* lib/kernel/hash.h */
