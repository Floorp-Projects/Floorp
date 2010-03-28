#include "nscore.h"

class Base {
  virtual void NS_FINAL final() {}
};

class Derived : public Base {
};

class VeryDerived : public Derived {
  void final() {}
};
