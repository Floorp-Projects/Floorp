// https://clang.llvm.org/extra/clang-tidy/checks/bugprone-suspicious-memset-usage.html

#include "structures.h"

void test(int* ip, char* cp)
{
  // Case 1: Fill value is a character '0' instead of NUL '\0'.
  memset(ip, '0', 1); // WARNING: suspicious for non-char pointers
  memset(cp, '0', 1); // OK for char pointers

  // Case 2: Fill value is truncated.
  memset(ip, 0xabcd, 1); // WARNING: fill value gets truncated
  memset(ip, 0x00cd, 1); // OK because value 0xcd is not truncated.
  memset(ip, 0x00, 1);   // OK because value is not truncated.

  // Case 3: Byte count is zero.
  memset(ip, sizeof(int), 0); // WARNING: zero length, potentially swapped
  memset(ip, sizeof(int), 1); // OK with non-zero length

  // See clang bug https://bugs.llvm.org/show_bug.cgi?id=38098
  memset(ip, 8, 0); // OK with zero length without sizeof
}
