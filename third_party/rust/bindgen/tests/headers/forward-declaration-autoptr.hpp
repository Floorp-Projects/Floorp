class Foo;

template <typename T>
struct RefPtr {
  T* m_inner;
};

struct Bar {
  RefPtr<Foo> m_member;
};
