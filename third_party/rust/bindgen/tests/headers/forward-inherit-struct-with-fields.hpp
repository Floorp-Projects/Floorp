template <typename> class Rooted;
namespace js {
    template <typename T> class RootedBase {
      T* foo;
      Rooted<T>* next;
    };
}
template <typename T> class Rooted : js::RootedBase<T> {};
