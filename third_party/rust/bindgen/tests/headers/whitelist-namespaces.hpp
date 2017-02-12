// bindgen-flags: --enable-cxx-namespaces --whitelist-type '.*'

namespace outer {
  namespace inner {
    struct Helper {};
  }
  struct Test {
    inner::Helper helper;
  };
}
