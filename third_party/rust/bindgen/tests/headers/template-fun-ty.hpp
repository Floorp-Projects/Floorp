template <typename T>
class Foo
{
  typedef T (FunctionPtr)();
};

template<typename T>
class RefPtr {
  template<typename R, typename... Args>
  class Proxy {
    typedef R (T::*member_function)(Args...);
  };
};

template<typename T>
using Returner = T(*)();
