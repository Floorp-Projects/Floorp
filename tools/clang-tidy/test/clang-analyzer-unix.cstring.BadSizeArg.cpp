// https://clang-analyzer.llvm.org/available_checks.html

#include "structures.h"

void test()
{
  char dest[3];
  strncat(dest, "***", sizeof(dest)); // warning : potential buffer overflow
}
