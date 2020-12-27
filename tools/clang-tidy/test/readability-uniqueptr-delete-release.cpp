#include "structures.h"

int foo() {
  std::unique_ptr<int> P;
  delete P.release();
}