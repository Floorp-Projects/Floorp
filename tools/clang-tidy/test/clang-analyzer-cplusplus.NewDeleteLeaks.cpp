// https://clang-analyzer.llvm.org/available_checks.html

void test()
{
  int *p = new int;
} // warning
