static const char A[] = "abc";

int strcmp(const char *, const char *);

int test_warning_patterns() {
  if (strcmp(A, "a"))
    return 0;
}
