#include "structures.h"

// bugprone-assert-side-effect
void misc_assert_side_effect() {
  int X = 0;
  assert(X == 1);
  assert(X = 1);
}
