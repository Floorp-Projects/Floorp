// bindgen-flags: --enable-cxx-namespaces --conservative-inline-namespaces -- -std=c++11

namespace foo {
  inline namespace bar {
    using Ty = int;
  };
  using Ty = long long;
};

class Bar {
  foo::bar::Ty baz;
};
