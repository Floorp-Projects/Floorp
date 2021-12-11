#include "structures.h"

int test_bcmp(void *a, void *b, size_t n) {
  return bcmp(a, b, n);
}
