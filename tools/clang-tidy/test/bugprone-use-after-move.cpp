namespace std {
typedef unsigned size_t;

template <typename T>
struct unique_ptr {
  unique_ptr();
  T *get() const;
  explicit operator bool() const;
  void reset(T *ptr);
  T &operator*() const;
  T *operator->() const;
  T& operator[](size_t i) const;
};

template <typename>
struct remove_reference;

template <typename _Tp>
struct remove_reference {
  typedef _Tp type;
};

template <typename _Tp>
struct remove_reference<_Tp &> {
  typedef _Tp type;
};

template <typename _Tp>
struct remove_reference<_Tp &&> {
  typedef _Tp type;
};

template <typename _Tp>
constexpr typename std::remove_reference<_Tp>::type &&move(_Tp &&__t) noexcept {
  return static_cast<typename remove_reference<_Tp>::type &&>(__t);
}
}

class A {
public:
  A();
  A(const A &);
  A(A &&);

  A &operator=(const A &);
  A &operator=(A &&);

  void foo() const;
  int getInt() const;

  operator bool() const;

  int i;
};

void func() {
  std::unique_ptr<A> ptr;
  std::move(ptr);
  ptr.get();
  static_cast<bool>(ptr);
  *ptr;
}
