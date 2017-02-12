// bindgen-flags: --use-core --raw-line "extern crate core;"

struct foo {
  int a, b;
  void* bar;
};

typedef void (*fooFunction)(int bar);
