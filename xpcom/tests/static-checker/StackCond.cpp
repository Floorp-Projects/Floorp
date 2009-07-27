#include "nscore.h"

struct A
{
};

A* tfunc(int len)
{
  A arr[5];
  A* a = len <= 5 ? arr : new A[len];
  return a;
}
