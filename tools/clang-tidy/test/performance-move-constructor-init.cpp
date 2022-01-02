struct B {
  B() {}
  B(const B&) {}
  B(B &&) {}
};

struct D : B {
  D() : B() {}
  D(const D &RHS) : B(RHS) {}
  D(D &&RHS) : B(RHS) {}
};
