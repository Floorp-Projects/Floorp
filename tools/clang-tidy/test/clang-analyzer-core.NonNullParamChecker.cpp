int f(int *p) __attribute__((nonnull));

void test(int *p) {
  if (!p)
    f(p); // warn
}
