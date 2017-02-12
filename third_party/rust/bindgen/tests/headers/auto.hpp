// bindgen-flags: -- -std=c++14
// bindgen-unstable

class Foo {
  static constexpr auto kFoo = 2 == 2;
};

template<typename T>
class Bar {
  static const constexpr auto kBar = T(1);
};

template<typename T> auto Test1() {
  return T(1);
}

auto Test2() {
  return Test1<unsigned int>();
}
