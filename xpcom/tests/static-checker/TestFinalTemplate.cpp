#include "nscore.h"

template<class T>
struct NS_FINAL_CLASS A
{
  T i;
};

struct Bint : A<int>
{
  int j;
};
