#include "nscore.h"

template<class T>
struct NS_STACK_CLASS A
{
  A();

  T i;
};

void *Foo()
{
  return new A<int>();
}
