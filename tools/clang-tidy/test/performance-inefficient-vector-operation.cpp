#include "structures.h"

void foo()
{
  std::vector<int> v;
  int n = 100;
  for (int i = 0; i < n; ++i) {
    v.push_back(n);
  }
}