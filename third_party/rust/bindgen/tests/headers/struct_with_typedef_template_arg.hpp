template<typename T, typename ...Args>
struct Proxy {
  typedef void (*foo)(T* bar);
};
