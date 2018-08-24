// https://clang.llvm.org/extra/clang-tidy/checks/bugprone-string-integer-assignment.html

#include "structures.h"

void test_int()
{
  // Numeric types can be implicitly casted to character types.
  std::string s;
  int x = 5965;
  s = 6; // warning
  s = x; // warning
}

void test_conversion()
{
  // Use the appropriate conversion functions or character literals.
  std::string s;
  int x = 5965;
  s = '6'; // OK
  s = std::to_string(x); // OK
}

void test_cast()
{
  // In order to suppress false positives, use an explicit cast.
  std::string s;
  s = static_cast<char>(6); // OK
}
