
namespace detail {
template<typename T>
struct PointerType {
  typedef T* Type;
};
}
template<typename T>
class UniquePtr {
  typedef typename detail::PointerType<T> Pointer;
};
