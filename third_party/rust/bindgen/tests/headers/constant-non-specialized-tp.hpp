// bindgen-flags: -- -std=c++11

// This test ensure we protect ourselves from an LLVM crash.

template <class... Args>
struct Test {
  static constexpr bool x[] = {Args::x...};
};

template<typename... T>
struct Outer {
  struct Inner {
    static constexpr int value[] = { T::value... };
  };
};
