
namespace std {
template <typename> struct remove_reference;

template <typename _Tp> struct remove_reference { typedef _Tp type; };

template <typename _Tp> struct remove_reference<_Tp &> { typedef _Tp type; };

template <typename _Tp> struct remove_reference<_Tp &&> { typedef _Tp type; };

template <typename _Tp>
constexpr typename std::remove_reference<_Tp>::type &&move(_Tp &&__t);

} // namespace std

// Standard case.
template <typename T, typename U> void f1(U &&SomeU) {
  T SomeT(std::move(SomeU));
  // CHECK-MESSAGES: :[[@LINE-1]]:11: warning: forwarding reference passed to
  // CHECK-FIXES: T SomeT(std::forward<U>(SomeU));
}

void foo() {
  f1<int, int>(2);
}

