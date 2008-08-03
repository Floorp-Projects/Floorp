#include "nscore.h"

struct NS_STACK_CLASS A
{
  int i;
};

void* Foo()
{
  A *a = new A[8];
  return a;
}
