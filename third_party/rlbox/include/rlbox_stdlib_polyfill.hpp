#pragma once

// This file is a polyfill for parts of the C++ standard library available only
// in newer compilers. Since these are only compile time requirements, we can
// just include these as part of the rlbox library in case the target compiler
// doesn't support these features. For instance clang-5 which rlbox supports
// does not support std::invocable and related functionality in <type_traits>
// and is polyfilled here.
//
// This code was borrowed from clang's standard library - libc++
//
// Link:
// https://github.com/llvm-mirror/libcxx/blob/master/include/type_traits
//
// libc++ is dual licensed under the MIT license and the UIUC License (a
// BSD-like license) and is therefore compatible with our code base

// std::invocable and friends

namespace rlbox::detail::polyfill {

struct __nat
{
  __nat() = delete;
  __nat(const __nat&) = delete;
  __nat& operator=(const __nat&) = delete;
  ~__nat() = delete;
};

template<bool _Val>
using _BoolConstant = std::integral_constant<bool, _Val>;

template<class _Tp, class _Up>
using _IsNotSame = _BoolConstant<!std::is_same<_Tp, _Up>::value>;

#define INVOKE_RETURN(...)                                                     \
  noexcept(noexcept(__VA_ARGS__))->decltype(__VA_ARGS__) { return __VA_ARGS__; }

template<class _Fp, class... _Args>
inline auto helper__invoke(_Fp&& __f, _Args&&... __args)
  INVOKE_RETURN(std::forward<_Fp>(__f)(std::forward<_Args>(__args)...))

    template<class _Fp, class... _Args>
    inline constexpr auto helper__invoke_constexpr(_Fp&& __f, _Args&&... __args)
      INVOKE_RETURN(std::forward<_Fp>(__f)(std::forward<_Args>(__args)...))

#undef INVOKE_RETURN

  // __invokable
  template<class _Ret, class _Fp, class... _Args>
  struct __invokable_r
{
  template<class _XFp, class... _XArgs>
  static auto __try_call(int)
    -> decltype(helper__invoke(std::declval<_XFp>(),
                               std::declval<_XArgs>()...));
  template<class _XFp, class... _XArgs>
  static __nat __try_call(...);

  // FIXME: Check that _Ret, _Fp, and _Args... are all complete types, cv void,
  // or incomplete array types as required by the standard.
  using _Result = decltype(__try_call<_Fp, _Args...>(0));

  using type = typename std::conditional<
    _IsNotSame<_Result, __nat>::value,
    typename std::conditional<std::is_void<_Ret>::value,
                              std::true_type,
                              std::is_convertible<_Result, _Ret>>::type,
    std::false_type>::type;
  static const bool value = type::value;
};
template<class _Fp, class... _Args>
using __invokable = __invokable_r<void, _Fp, _Args...>;

template<bool _IsInvokable,
         bool _IsCVVoid,
         class _Ret,
         class _Fp,
         class... _Args>
struct __nothrow_invokable_r_imp
{
  static const bool value = false;
};

template<class _Ret, class _Fp, class... _Args>
struct __nothrow_invokable_r_imp<true, false, _Ret, _Fp, _Args...>
{
  typedef __nothrow_invokable_r_imp _ThisT;

  template<class _Tp>
  static void __test_noexcept(_Tp) noexcept;

  static const bool value = noexcept(_ThisT::__test_noexcept<_Ret>(
    helper__invoke(std::declval<_Fp>(), std::declval<_Args>()...)));
};

template<class _Ret, class _Fp, class... _Args>
struct __nothrow_invokable_r_imp<true, true, _Ret, _Fp, _Args...>
{
  static const bool value =
    noexcept(helper__invoke(std::declval<_Fp>(), std::declval<_Args>()...));
};

template<class _Ret, class _Fp, class... _Args>
using __nothrow_invokable_r =
  __nothrow_invokable_r_imp<__invokable_r<_Ret, _Fp, _Args...>::value,
                            std::is_void<_Ret>::value,
                            _Ret,
                            _Fp,
                            _Args...>;

template<class _Fp, class... _Args>
using __nothrow_invokable =
  __nothrow_invokable_r_imp<__invokable<_Fp, _Args...>::value,
                            true,
                            void,
                            _Fp,
                            _Args...>;

template<class _Fp, class... _Args>
struct helper__invoke_of
  : public std::enable_if<__invokable<_Fp, _Args...>::value,
                          typename __invokable_r<void, _Fp, _Args...>::_Result>
{};

// invoke_result

template<class _Fn, class... _Args>
struct invoke_result : helper__invoke_of<_Fn, _Args...>
{};

template<class _Fn, class... _Args>
using invoke_result_t = typename invoke_result<_Fn, _Args...>::type;

// is_invocable

template<class _Fn, class... _Args>
struct is_invocable
  : std::integral_constant<bool, __invokable<_Fn, _Args...>::value>
{};

template<class _Ret, class _Fn, class... _Args>
struct is_invocable_r
  : std::integral_constant<bool, __invokable_r<_Ret, _Fn, _Args...>::value>
{};

template<class _Fn, class... _Args>
inline constexpr bool is_invocable_v = is_invocable<_Fn, _Args...>::value;

template<class _Ret, class _Fn, class... _Args>
inline constexpr bool is_invocable_r_v =
  is_invocable_r<_Ret, _Fn, _Args...>::value;

// is_nothrow_invocable

template<class _Fn, class... _Args>
struct is_nothrow_invocable
  : std::integral_constant<bool, __nothrow_invokable<_Fn, _Args...>::value>
{};

template<class _Ret, class _Fn, class... _Args>
struct is_nothrow_invocable_r
  : std::integral_constant<bool,
                           __nothrow_invokable_r<_Ret, _Fn, _Args...>::value>
{};

template<class _Fn, class... _Args>
inline constexpr bool is_nothrow_invocable_v =
  is_nothrow_invocable<_Fn, _Args...>::value;

template<class _Ret, class _Fn, class... _Args>
inline constexpr bool is_nothrow_invocable_r_v =
  is_nothrow_invocable_r<_Ret, _Fn, _Args...>::value;

}
