// bindgen-flags: -- -std=c++11

template <typename... T>
struct B {
    // Can't generate anything meaningful in Rust for this, but we shouldn't
    // trigger an assertion inside Clang.
    static const long c = sizeof...(T);
};
