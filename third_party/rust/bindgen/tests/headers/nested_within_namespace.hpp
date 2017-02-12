// bindgen-flags: --enable-cxx-namespaces

namespace foo {
  class Bar {
    int foo;

    class Baz {
      int foo;
    };
  };

  class Baz {
    int baz;
  };
}
