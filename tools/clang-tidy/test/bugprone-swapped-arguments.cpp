// https://clang.llvm.org/extra/clang-tidy/checks/bugprone-swapped-arguments.html

void test_d_i(double d, int i);
void test_d_i_i(double d, int i, int ii);
void test_i_d(int i, double d);
void test_i_i_d(int i, int ii, double d);

void test()
{
  double d = 1;
  int i = 1;

  test_d_i(d, i); // OK
  test_d_i(i, d); // WARNING

  test_i_d(i, d); // OK
  test_i_d(d, i); // WARNING

  test_i_i_d(i, i, d); // OK
  test_i_i_d(i, d, i); // WARNING
  test_i_i_d(d, i, i); // NO WARNING after second parameter

  test_d_i_i(d, i, i); // OK
  test_d_i_i(i, d, i); // WARNING
  test_d_i_i(i, i, d); // NO WARNING after second parameter
}
