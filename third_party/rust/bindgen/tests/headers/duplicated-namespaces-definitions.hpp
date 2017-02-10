// bindgen-flags: --enable-cxx-namespaces

namespace foo {
  class Bar;
}

namespace bar {
  struct Foo {
    foo::Bar* ptr;
  };
};

namespace foo {
  class Bar {
    int foo;
    bool baz;
  };
}
