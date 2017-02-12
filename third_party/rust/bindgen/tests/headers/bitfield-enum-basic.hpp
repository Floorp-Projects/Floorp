// bindgen-flags: --bitfield-enum "Foo|Buz|NS_.*|DUMMY_.*" -- -std=c++11

enum Foo {
  Bar = 1 << 1,
  Baz = 1 << 2,
  Duplicated = 1 << 2,
  Negative = -3,
};

enum class Buz : signed char {
  Bar = 1 << 1,
  Baz = 1 << 2,
  Duplicated = 1 << 2,
  Negative = -3,
};

enum {
  NS_FOO = 1 << 0,
  NS_BAR = 1 << 1,
};

class Dummy {
  enum {
    DUMMY_FOO = 1 << 0,
    DUMMY_BAR = 1 << 1,
  };
};
