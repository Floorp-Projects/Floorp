#pragma once
// IWYU pragma: private, include "rlbox.hpp"
// IWYU pragma: friend "rlbox_.*\.hpp"

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <type_traits>
#include <utility>
#ifndef RLBOX_USE_CUSTOM_SHARED_LOCK
#  include <mutex>
#endif

#include "rlbox_stdlib_polyfill.hpp"

namespace rlbox {
namespace detail {
  const int CompileErrorCode = 42;

  inline void dynamic_check(bool check, const char* const msg)
  {
    // clang-format off
  if (!check) {
    #if __cpp_exceptions && defined(RLBOX_USE_EXCEPTIONS)
      throw std::runtime_error(msg);
    #else
      #ifdef RLBOX_CUSTOM_ABORT
        RLBOX_CUSTOM_ABORT(msg);
      #else
        std::cerr << msg << std::endl;
        std::abort();
      #endif
    #endif
  }
    // clang-format on
  }

#ifdef RLBOX_NO_COMPILE_CHECKS
#  if __cpp_exceptions && defined(RLBOX_USE_EXCEPTIONS)
#    define rlbox_detail_static_fail_because(CondExpr, Message)                \
      ((void)(CondExpr)), throw std::runtime_error(Message)
#  else
#    define rlbox_detail_static_fail_because(CondExpr, Message) abort()
#  endif
#else
#  define rlbox_detail_static_fail_because(CondExpr, Message)                  \
    static_assert(!(CondExpr), Message)
#endif

#ifdef RLBOX_ENABLE_DEBUG_ASSERTIONS
#  define RLBOX_DEBUG_ASSERT(...)                                              \
    ::rlbox::detail::dynamic_check(__VA_ARGS__, "Debug assertion failed")
#else
#  define RLBOX_DEBUG_ASSERT(...) (void)0
#endif

#define RLBOX_UNUSED(...) (void)__VA_ARGS__

#define RLBOX_REQUIRE_SEMI_COLON static_assert(true)

#define if_constexpr_named(varName, ...)                                       \
  if constexpr (constexpr auto varName = __VA_ARGS__; varName)

  template<typename... TArgs>
  void printTypes()
  {
#if defined(__clang__) || defined(__GNUC__) || defined(__GNUG__)
    std::cout << __PRETTY_FUNCTION__ << std::endl; // NOLINT
#elif defined(_MSC_VER)
    std::cout << __FUNCSIG__ << std::endl; // NOLINT
#else
    std::cout << "Unsupported" << std::endl;
#endif
  }

// Create an extension point so applications can provide their own shared lock
// implementation
#ifndef RLBOX_USE_CUSTOM_SHARED_LOCK
#  define RLBOX_SHARED_LOCK(name) std::shared_timed_mutex name
#  define RLBOX_ACQUIRE_SHARED_GUARD(name, ...)                                \
    std::shared_lock<std::shared_timed_mutex> name(__VA_ARGS__)
#  define RLBOX_ACQUIRE_UNIQUE_GUARD(name, ...)                                \
    std::unique_lock<std::shared_timed_mutex> name(__VA_ARGS__)
#else
#  if !defined(RLBOX_SHARED_LOCK) || !defined(RLBOX_ACQUIRE_SHARED_GUARD) ||   \
    !defined(RLBOX_ACQUIRE_UNIQUE_GUARD)
#    error                                                                     \
      "RLBOX_USE_CUSTOM_SHARED_LOCK defined but missing definitions for RLBOX_SHARED_LOCK, RLBOX_ACQUIRE_SHARED_GUARD, RLBOX_ACQUIRE_UNIQUE_GUARD"
#  endif
#endif

#define rlbox_detail_forward_binop_to_base(opSymbol, ...)                      \
  template<typename T_Rhs>                                                     \
  inline auto operator opSymbol(T_Rhs rhs)                                     \
  {                                                                            \
    auto b = static_cast<__VA_ARGS__*>(this);                                  \
    return (*b)opSymbol rhs;                                                   \
  }                                                                            \
  RLBOX_REQUIRE_SEMI_COLON

#define rlbox_detail_forward_to_const(func_name, result_type)                  \
  using T_ConstClassPtr = std::add_pointer_t<                                  \
    std::add_const_t<std::remove_pointer_t<decltype(this)>>>;                  \
  if constexpr (detail::rlbox_is_tainted_v<result_type> &&                     \
                !std::is_reference_v<result_type>) {                           \
    return sandbox_const_cast<detail::rlbox_remove_wrapper_t<result_type>>(    \
      const_cast<T_ConstClassPtr>(this)->func_name());                         \
  } else if constexpr (detail::is_fundamental_or_enum_v<result_type> ||        \
                       detail::is_std_array_v<result_type> ||                  \
                       detail::is_func_ptr_v<result_type> ||                   \
                       std::is_class_v<result_type>) {                         \
    return const_cast<T_ConstClassPtr>(this)->func_name();                     \
  } else {                                                                     \
    return const_cast<result_type>(                                            \
      const_cast<T_ConstClassPtr>(this)->func_name());                         \
  }

#define rlbox_detail_forward_to_const_a(func_name, result_type, ...)           \
  using T_ConstClassPtr = std::add_pointer_t<                                  \
    std::add_const_t<std::remove_pointer_t<decltype(this)>>>;                  \
  if constexpr (detail::rlbox_is_tainted_v<result_type> &&                     \
                !std::is_reference_v<result_type>) {                           \
    static_assert(detail::rlbox_is_tainted_v<result_type>);                    \
    return sandbox_const_cast<detail::rlbox_remove_wrapper_t<result_type>>(    \
      const_cast<T_ConstClassPtr>(this)->func_name(__VA_ARGS__));              \
  } else if constexpr (detail::is_fundamental_or_enum_v<result_type> ||        \
                       detail::is_std_array_v<result_type> ||                  \
                       detail::is_func_ptr_v<result_type> ||                   \
                       std::is_class_v<result_type>) {                         \
    return const_cast<T_ConstClassPtr>(this)->func_name(__VA_ARGS__);          \
  } else {                                                                     \
    return const_cast<result_type>(                                            \
      const_cast<T_ConstClassPtr>(this)->func_name(__VA_ARGS__));              \
  }

#define rlbox_detail_member_and_const(sig, ...)                                \
  sig __VA_ARGS__                                                              \
                                                                               \
    sig const __VA_ARGS__                                                      \
                                                                               \
    static_assert(true)

  template<typename T>
  inline auto remove_volatile_from_ptr_cast(T* ptr)
  {
    using T_Result = std::add_pointer_t<std::remove_volatile_t<T>>;
    return const_cast<T_Result>(ptr);
  }

  // https://stackoverflow.com/questions/37602057/why-isnt-a-for-loop-a-compile-time-expression
  namespace compile_time_for_detail {
    template<std::size_t N>
    struct num
    {
      static const constexpr auto value = N;
    };

    template<class F, std::size_t... Is>
    inline void compile_time_for_helper(F func, std::index_sequence<Is...>)
    {
      (func(num<Is>{}), ...);
    }
  }

  template<std::size_t N, typename F>
  inline void compile_time_for(F func)
  {
    compile_time_for_detail::compile_time_for_helper(
      func, std::make_index_sequence<N>());
  }

  template<typename T, typename T2>
  [[nodiscard]] inline auto return_first_result(T first_task, T2 second_task)
  {
    using T_Result = rlbox::detail::polyfill::invoke_result_t<T>;

    if constexpr (std::is_void_v<T_Result>) {
      first_task();
      second_task();
    } else {
      auto val = first_task();
      second_task();
      return val;
    }
  }

  // Scope Exit guards
  template<typename T_ExitFunc>
  class scope_exit
  {
    T_ExitFunc exit_func;
    bool released;

  public:
    explicit scope_exit(T_ExitFunc&& cleanup)
      : exit_func(cleanup)
      , released(true)
    {}

    scope_exit(scope_exit&& rhs)
      : exit_func(std::move(rhs.exit_func))
      , released(rhs.released)
    {
      rhs.release();
    }

    ~scope_exit()
    {
      if (released) {
        exit_func();
      }
    }

    void release() { released = false; }

  private:
    explicit scope_exit(const scope_exit&) = delete;
    scope_exit& operator=(const scope_exit&) = delete;
    scope_exit& operator=(scope_exit&&) = delete;
  };

  template<typename T_ExitFunc>
  [[nodiscard]] scope_exit<T_ExitFunc> make_scope_exit(
    T_ExitFunc&& exitFunction)
  {
    return scope_exit<T_ExitFunc>(std::move(exitFunction));
  }

/*
Make sure classes can access the private memmbers of tainted<T1> and
tainted_volatile. Ideally, this should be

template <typename U1>
friend class tainted<U1, T_Sandbox>;

But C++ doesn't seem to allow the above
*/
#define KEEP_CLASSES_FRIENDLY                                                  \
  template<template<typename, typename> typename U1, typename U2, typename U3> \
  friend class tainted_base_impl;                                              \
                                                                               \
  template<typename U1, typename U2>                                           \
  friend class tainted;                                                        \
                                                                               \
  template<typename U1, typename U2>                                           \
  friend class tainted_volatile;                                               \
                                                                               \
  template<typename U1>                                                        \
  friend class rlbox_sandbox;                                                  \
                                                                               \
  template<typename U1, typename U2>                                           \
  friend class sandbox_callback;                                               \
                                                                               \
  template<typename U1, typename U2>                                           \
  friend class app_pointer;
}

}
