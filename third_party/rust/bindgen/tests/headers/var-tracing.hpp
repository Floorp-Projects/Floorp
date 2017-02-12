// bindgen-flags: --whitelist-type Baz

struct Bar {
  const int m_baz;
  Bar(int baz);
};

class Baz {
  static const Bar FOO[];
};
