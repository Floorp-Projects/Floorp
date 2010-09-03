#include <new>
#include "nscore.h"

struct NS_STACK_CLASS A
{
};

void foo() {
  A* a;
  void* v = ::operator new(16);
  a = (A*) v; // tricks analysis
}
