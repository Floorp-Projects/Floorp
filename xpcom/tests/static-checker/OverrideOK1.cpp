#include "nscore.h"

struct S {
  virtual NS_MUST_OVERRIDE void f();
  virtual void g();
};

struct A : S { virtual void f(); }; // ok
struct B : S { virtual NS_MUST_OVERRIDE void f(); }; // also ok
struct E : A { }; // ok: A's definition of f() is good for subclasses
