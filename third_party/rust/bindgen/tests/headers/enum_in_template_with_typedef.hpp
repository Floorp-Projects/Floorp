// bindgen-flags: -- -std=c++11

namespace std {
  template <typename Char> class fbstring_core;
}

typedef unsigned char uint8_t;
namespace std {
  template <typename Char> class fbstring_core {
    typedef uint8_t category_type;
    enum Category : category_type {
      Foo = 1,
      Bar = 4,
    };
  };
}
