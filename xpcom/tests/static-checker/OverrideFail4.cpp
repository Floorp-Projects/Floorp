#include "nscore.h"

struct Base {
  NS_MUST_OVERRIDE void f();
};

struct Intermediate : Base {
  NS_MUST_OVERRIDE void f();
};

struct Derived : Intermediate {
  // error: must override Intermediate's f()
};
