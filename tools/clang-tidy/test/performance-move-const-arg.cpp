namespace std {
template <typename _Tp>
struct remove_reference {
  typedef _Tp type;
};

template <typename _Tp>
constexpr typename std::remove_reference<_Tp>::type &&move(_Tp &&__t) {
  return static_cast<typename std::remove_reference<_Tp>::type &&>(__t);
}
} // namespace std

struct TriviallyCopyable {
  int i;
};

class A {
public:
  A() {}
  A(const A &rhs) {}
  A(A &&rhs) {}
};

void f(TriviallyCopyable) {}

void g() {
  TriviallyCopyable obj;
  f(std::move(obj));
}

A f5(const A x5) {
  return std::move(x5);
}
