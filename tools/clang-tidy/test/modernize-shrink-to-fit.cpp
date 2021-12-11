#include "structures.h"

void f() {
  std::vector<int> v;

  std::vector<int>(v).swap(v);

  std::vector<int> &vref = v;
  std::vector<int>(vref).swap(vref);
}
