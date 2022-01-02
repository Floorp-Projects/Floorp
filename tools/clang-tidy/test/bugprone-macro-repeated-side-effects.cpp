// https://clang.llvm.org/extra/clang-tidy/checks/bugprone-macro-repeated-side-effects.html

#define MACRO_WITHOUT_REPEATED_ARG(x) (x)
#define MACRO_WITH_REPEATED_ARG(x) ((x) + (x))

static int g;

int function_with_side_effects(int i)
{
  g += i;
  return g;
}

void test()
{
  int i;
  i = MACRO_WITHOUT_REPEATED_ARG(1); // OK
  i = MACRO_WITH_REPEATED_ARG(1); // OK

  i = MACRO_WITHOUT_REPEATED_ARG(i); // OK
  i = MACRO_WITH_REPEATED_ARG(i); // OK

  i = MACRO_WITHOUT_REPEATED_ARG(function_with_side_effects(i)); // OK
  i = MACRO_WITH_REPEATED_ARG(function_with_side_effects(i)); // NO WARNING

  i = MACRO_WITHOUT_REPEATED_ARG(i++); // OK
  i = MACRO_WITH_REPEATED_ARG(i++); // WARNING
}
