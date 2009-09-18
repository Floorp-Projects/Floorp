#include "nscore.h"

struct Base {
  NS_MUST_OVERRIDE virtual void f();  // normal case
  NS_MUST_OVERRIDE void g();          // virtual not required
  NS_MUST_OVERRIDE static void h();   // can even be static
};

void Base::f() {} // can be defined, or not, don't care

struct Derived1 : Base { // propagates override annotation
  NS_MUST_OVERRIDE virtual void f();
  NS_MUST_OVERRIDE void g();
  NS_MUST_OVERRIDE static void h();
};

struct Derived2 : Derived1 { // doesn't propagate override annotation
  virtual void f();
  void g();
  static void h();
};

struct Derived3 : Derived2 { // doesn't have to override anything
};
