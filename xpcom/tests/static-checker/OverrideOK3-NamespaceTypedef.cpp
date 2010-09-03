#include "nscore.h"

namespace A {
namespace B {
namespace C {
  struct Param {};

  class Base {
    NS_MUST_OVERRIDE virtual void f(Param p) {}
  };
}}}

class Derived : public A::B::C::Base {
  typedef A::B::C::Param P;
  void f(P p) {}
};
