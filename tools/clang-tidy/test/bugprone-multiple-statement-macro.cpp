void F();

#define BAD_MACRO(x) \
  F();               \
  F()

void positives() {
  if (1)
    BAD_MACRO(1);
}
