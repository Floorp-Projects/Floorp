// bindgen-flags: -- -std=c++11

template<typename...> using __void_t = void;

template<typename _Iterator, typename = __void_t<>>
  struct __iterator_traits { };
