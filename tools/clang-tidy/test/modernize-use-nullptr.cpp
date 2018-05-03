#define NULL 0
void f(void) {
  char *str = NULL; // ok
  (void)str;
}
