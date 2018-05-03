#include <memory>

struct A {
  void f() {}
};

void foo() {
  std::unique_ptr<A> ptr = std::make_unique<A>();
  ptr.get()->f();
}

