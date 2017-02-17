// bindgen-flags: -- --target=x86_64-pc-win32

template<typename Method, bool Cancelable>
struct nsRunnableMethodTraits;

template<class C, typename R, bool Cancelable, typename... As>
struct nsRunnableMethodTraits<R(C::*)(As...), Cancelable>
{
  static const bool can_cancel = Cancelable;
};
