template <typename T>
struct Iterator {
  void operator++() {}
  const T& operator*() {
    static T* TT = new T();
    return *TT;
  }
  bool operator!=(const Iterator &) { return false; }
  typedef const T& const_reference;
};
template <typename T>
struct View {
  T begin() { return T(); }
  T begin() const { return T(); }
  T end() { return T(); }
  T end() const { return T(); }
  typedef typename T::const_reference const_reference;
};

struct S {
  S();
  S(const S &);
  ~S();
  S &operator=(const S &);
};

void negativeConstReference() {
  for (const S S1 : View<Iterator<S>>()) {
  }
}