#include "nscore.h"

struct S {
  virtual NS_MUST_OVERRIDE void f();
  virtual void g();
};

struct B : S { virtual NS_MUST_OVERRIDE void f(); }; // also ok
struct F : B { }; // ERROR: B's definition of f() is still must-override

