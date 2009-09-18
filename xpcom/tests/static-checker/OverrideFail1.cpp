#include "nscore.h"

struct S {
  virtual NS_MUST_OVERRIDE void f();
  virtual void g();
};
struct C : S { virtual void g(); }; // ERROR: must override f()
