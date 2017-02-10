// bindgen-flags: --opaque-type=.* --whitelist-function=foo

class Container;

// The whitelist tracing should reach the Container type, even though it's
// marked as opaque.
void foo(Container* c);

template<typename T>
class Wat {
  T foo;
};

class OtherOpaque {
  int bar;
};

class Container {
  Wat<int> bar;
  OtherOpaque baz;
};
