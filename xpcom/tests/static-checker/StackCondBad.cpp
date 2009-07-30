#include "nscore.h"

struct NS_STACK_CLASS A
{
};

A* tfunc(int len)
{
  A arr[5];
  A* a = len <= 5 ? arr : new A[len];
  return a;
}
