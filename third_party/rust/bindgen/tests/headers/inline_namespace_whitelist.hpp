// bindgen-flags: --enable-cxx-namespaces --whitelist-type=std::string -- -std=c++11

namespace std {
  inline namespace bar {
    using string = const char*;
  };
};
