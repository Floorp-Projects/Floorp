#include <stdio.h>
#include "mozilla/Unused.h"

int main()
{
  char tmp;
  mozilla::Unused << fread(&tmp, sizeof(tmp), 1, stdin);
  return 0;
}
