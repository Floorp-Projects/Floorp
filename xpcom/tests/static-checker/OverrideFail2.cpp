#include "nscore.h"

struct S {
  virtual NS_MUST_OVERRIDE void f();
  virtual void g();
};

struct D : S { virtual void f(int); }; // ERROR: different overload
