// https://clang-analyzer.llvm.org/available_checks.html

#include "structures.h"

int my_strlen(const char* s)
{
  return strlen(s); // warning
}

int bad_caller()
{
  const char* s = nullptr;
  return my_strlen(s);
}
