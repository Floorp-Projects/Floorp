#include "nscore.h"

struct NS_STACK_CLASS A
{
  // BUG: currently classes which are marked NS_STACK_CLASS must have a
  // constructor
  A();

  int i;
};

void* Foo()
{
  return new A();
}
