/* Demonstrate that the stack can grow.
   This must succeed. */

#include <string.h>
#include "tests/arc4.h"
#include "tests/cksum.h"
#include "tests/lib.h"
#include "tests/main.h"

void
test_main (void)
{
  char stack_obj[4096]; // 스택 공간 -> 즉 이 안에서 스택이 grow 한다고 보면 됨..?
  struct arc4 arc4;

  arc4_init (&arc4, "foobar", 6);
  memset (stack_obj, 0, sizeof stack_obj);
  arc4_crypt (&arc4, stack_obj, sizeof stack_obj);
  msg ("cksum: %lu", cksum (stack_obj, sizeof stack_obj));
}
