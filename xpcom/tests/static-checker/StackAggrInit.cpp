#include "nscore.h"
#include "nsAutoPtr.h"

class NS_STACK_CLASS A
{
  int i;
};

void Foo()
{
  nsAutoPtr<A> a(new A);
}
