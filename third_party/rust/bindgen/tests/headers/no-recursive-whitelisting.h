// bindgen-flags: --no-recursive-whitelist --whitelist-type "Foo" --raw-line "pub enum Bar {}"

struct Bar;

struct Foo {
  struct Bar* baz;
};
