#include "nscore.h"
#include NEW_H

struct NS_STACK_CLASS A
{
  int i;
};

int Foo()
{
  char buf[sizeof(A)];

  A *a = new(&buf) A;
  return a->i;
}
