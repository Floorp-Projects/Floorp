// bindgen-flags: -- -std=c++14
template <typename A> using MaybeWrapped = A;

template<class T>
class Rooted {
  MaybeWrapped<T> ptr;
};
