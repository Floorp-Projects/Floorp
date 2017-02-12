
template<typename T>
class Foo {
public:
  Foo();

  void doBaz();
};

template<typename T>
inline void
Foo<T>::doBaz() {
}

class Bar {
public:
  Bar();
};

template<typename T>
Foo<T>::Foo() {
}

inline
Bar::Bar() {
}
