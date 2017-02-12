// bindgen-flags: -- -std=c++11

namespace std {
  template<typename _Alloc> struct allocator_traits {
    typedef decltype ( _S_size_type_helper ( ( _Alloc * ) 0 ) ) __size_type;
  };
}
