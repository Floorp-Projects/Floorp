
struct S {
  S() = default;
  S(int i) : i(i) {}
  int i = 1;
};

struct F1 {
  F1() : f() {}
  S f;
};
