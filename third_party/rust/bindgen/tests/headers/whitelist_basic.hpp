// bindgen-flags: --whitelist-type WhitelistMe

template<typename T>
class WhitelistMe {
  class Inner {
    T bar;
  };

  int foo;
  Inner bar;
};

struct DontWhitelistMe {
  void* foo;
  double _Complex noComplexGenerated;
};
