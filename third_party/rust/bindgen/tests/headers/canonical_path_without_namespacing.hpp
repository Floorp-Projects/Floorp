// bindgen-flags: --disable-name-namespacing

namespace foo {
  struct Bar {};
}

void baz(foo::Bar*);
