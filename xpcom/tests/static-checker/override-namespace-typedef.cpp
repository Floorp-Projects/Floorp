#include "nscore.h"

namespace A {
namespace B {
namespace C {
  struct Param {};

  class Base {
    virtual void f(const Param& p) {}
  };
}}}

class Derived : public A::B::C::Base {
  typedef A::B::C::Param P;
  NS_OVERRIDE void f(const P& p) {}
};
