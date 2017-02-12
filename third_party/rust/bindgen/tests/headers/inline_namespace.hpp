// bindgen-flags: --enable-cxx-namespaces -- -std=c++11

namespace foo {
  inline namespace bar {
    using Ty = int;
  };
};

class Bar {
  foo::Ty baz;
};
