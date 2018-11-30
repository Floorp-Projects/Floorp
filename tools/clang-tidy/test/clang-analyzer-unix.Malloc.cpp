// https://clang-analyzer.llvm.org/available_checks.html

#include "structures.h"

void test_malloc()
{
  int *p = (int*) malloc(1);
  free(p);
  free(p); // warning: attempt to free released memory
}

void test_use_after_free()
{
  int *p = (int*) malloc(sizeof(int));
  free(p);
  *p = 1; // warning: use after free
}

void test_leak()
{
  int *p = (int*) malloc(1);
  if (p)
    return; // warning: memory is never released
}

void test_free_local()
{
  int a[] = { 1 };
  free(a); // warning: argument is not allocated by malloc
}

void test_free_offset()
{
  int *p = (int*) malloc(sizeof(char));
  p = p - 1;
  free(p); // warning: argument to free() is offset by -4 bytes
}
