// https://clang.llvm.org/extra/clang-tidy/checks/bugprone-string-constructor.html

#include "structures.h"

void test()
{
  // A common mistake is to swap parameters to the ‘fill’ string-constructor.
  std::string str('x', 50); // should be str(50, 'x')

  // Calling the string-literal constructor with a length bigger than the
  // literal is suspicious and adds extra random characters to the string.
  std::string("test", 200);   // Will include random characters after "test".

  // Creating an empty string from constructors with parameters is considered
  // suspicious. The programmer should use the empty constructor instead.
  std::string("test", 0);   // Creation of an empty string.
}
