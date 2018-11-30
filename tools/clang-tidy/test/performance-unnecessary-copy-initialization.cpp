#include "structures.h"

extern const std::string& constReference();

void foo() {
  const std::string UnnecessaryCopy = constReference();
}