#define NULL __null

namespace std {

template <typename T>
struct unique_ptr {
  T& operator*() const;
  T* operator->() const;
  T* get() const;
  explicit operator bool() const noexcept;
};
}  // namespace std

struct A {};

void foo() { A& b2 = *std::unique_ptr<A>().get(); }
