#include "structures.h"

extern void fstring(std::string);

void foo() {
  std::string mystr1, mystr2;
  auto myautostr1 = mystr1;
  auto myautostr2 = mystr2;

  for (int i = 0; i < 10; ++i) {
    fstring(mystr1 + mystr2 + mystr1);
  }
}