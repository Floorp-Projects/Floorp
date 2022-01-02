#pragma once
// IWYU pragma: private, include "rlbox.hpp"
// IWYU pragma: friend "rlbox_.*\.hpp"

#include <type_traits>
#include <utility>

#include "rlbox_helpers.hpp"
#include "rlbox_struct_support.hpp"
#include "rlbox_types.hpp"

namespace rlbox {

namespace callback_detail {

  // Compute the expected type of the callback
  template<typename T_Sbx, typename T_Ret, typename... T_Args>
  using T_Cb =
    std::conditional_t<std::is_void_v<T_Ret>, void, tainted<T_Ret, T_Sbx>> (*)(
      rlbox_sandbox<T_Sbx>&,
      tainted<T_Args, T_Sbx>...);

  template<typename T_Sbx, typename T_Ret, typename... T_Args>
  T_Cb<T_Sbx, T_Ret, T_Args...> callback_type_helper(T_Ret (*)(T_Args...));

  // Compute the expected type of the interceptor
  template<typename T_Sbx, typename T_Ret, typename... T_Args>
  using T_I = detail::convert_to_sandbox_equivalent_t<T_Ret, T_Sbx> (*)(
    detail::convert_to_sandbox_equivalent_t<T_Args, T_Sbx>...);

  template<typename T_Sbx, typename T_Ret, typename... T_Args>
  T_I<T_Sbx, T_Ret, T_Args...> interceptor_type_helper(T_Ret (*)(T_Args...));
}

template<typename T, typename T_Sbx>
class sandbox_callback
{
  KEEP_CLASSES_FRIENDLY

private:
  rlbox_sandbox<T_Sbx>* sandbox;

  using T_Callback =
    decltype(callback_detail::callback_type_helper<T_Sbx>(std::declval<T>()));
  T_Callback callback;

  // The interceptor is the function that runs between the sandbox invoking the
  // callback and the actual callback running The interceptor is responsible for
  // wrapping and converting callback arguments, returns etc. to their
  // appropriate representations
  using T_Interceptor =
    decltype(callback_detail::interceptor_type_helper<T_Sbx>(
      std::declval<T>()));
  T_Interceptor callback_interceptor;

  // The trampoline is the internal sandbox representation of the callback
  // Depending on the sandbox type, this could be the callback pointer directly
  // or a trampoline function that gates exits from the sandbox.
  using T_Trampoline = detail::convert_to_sandbox_equivalent_t<T, T_Sbx>;
  T_Trampoline callback_trampoline;

  // The unique key representing the callback to pass to unregister_callback on
  // destruction
  void* key;

  inline void move_obj(sandbox_callback&& other)
  {
    sandbox = other.sandbox;
    callback = other.callback;
    callback_interceptor = other.callback_interceptor;
    callback_trampoline = other.callback_trampoline;
    key = other.key;
    other.sandbox = nullptr;
    other.callback = nullptr;
    other.callback_interceptor = nullptr;
    other.callback_trampoline = 0;
    other.key = nullptr;
  }

  template<typename T_Ret, typename... T_Args>
  inline void unregister_helper(T_Ret (*)(T_Args...))
  {
    if (callback != nullptr) {
      // Don't need to worry about race between unregister and move as
      // 1) this will not happen in a correctly written program
      // 2) if this does happen, the worst that can happen is an invocation of a
      // null function pointer, which causes a crash that cannot be exploited
      // for RCE
      sandbox->template unregister_callback<T_Ret, T_Args...>(key);
      sandbox = nullptr;
      callback = nullptr;
      callback_interceptor = nullptr;
      callback_trampoline = 0;
      key = nullptr;
    }
  }

  inline T_Callback get_raw_value() const noexcept { return callback; }
  inline T_Trampoline get_raw_sandbox_value() const noexcept
  {
    return callback_trampoline;
  }
  inline T_Callback get_raw_value() noexcept { return callback; }
  inline T_Trampoline get_raw_sandbox_value() noexcept
  {
    return callback_trampoline;
  }

  // Keep constructor private as only rlbox_sandbox should be able to create
  // this object
  sandbox_callback(rlbox_sandbox<T_Sbx>* p_sandbox,
                   T_Callback p_callback,
                   T_Interceptor p_callback_interceptor,
                   T_Trampoline p_callback_trampoline,
                   void* p_key)
    : sandbox(p_sandbox)
    , callback(p_callback)
    , callback_interceptor(p_callback_interceptor)
    , callback_trampoline(p_callback_trampoline)
    , key(p_key)
  {
    detail::dynamic_check(sandbox != nullptr,
                          "Unexpected null sandbox when creating a callback");
  }

public:
  sandbox_callback()
    : sandbox(nullptr)
    , callback(nullptr)
    , callback_interceptor(nullptr)
    , callback_trampoline(0)
    , key(nullptr)
  {}

  sandbox_callback(sandbox_callback&& other)
  {
    move_obj(std::forward<sandbox_callback>(other));
  }

  inline sandbox_callback& operator=(sandbox_callback&& other)
  {
    if (this != &other) {
      move_obj(std::forward<sandbox_callback>(other));
    }
    return *this;
  }

  void unregister()
  {
    T dummy = nullptr;
    unregister_helper(dummy);
  }

  ~sandbox_callback() { unregister(); }

  /**
   * @brief Check if callback is _not_ registered.
   */
  inline bool is_unregistered() const noexcept
  {
    return get_raw_value() == nullptr;
  }

  /**
   * @brief Unwrap a callback without verification. This is an unsafe operation
   * and should be used with care.
   */
  inline auto UNSAFE_unverified() const noexcept { return get_raw_value(); }
  /**
   * @brief Like UNSAFE_unverified, but get the underlying sandbox
   * representation.
   *
   * @param sandbox Reference to sandbox.
   */
  inline auto UNSAFE_sandboxed(rlbox_sandbox<T_Sbx>& sandbox) const noexcept
  {
    RLBOX_UNUSED(sandbox);
    return get_raw_sandbox_value();
  }
  inline auto UNSAFE_unverified() noexcept { return get_raw_value(); }
  inline auto UNSAFE_sandboxed(rlbox_sandbox<T_Sbx>& sandbox) noexcept
  {
    RLBOX_UNUSED(sandbox);
    return get_raw_sandbox_value();
  }
};

template<typename T, typename T_Sbx>
class app_pointer
{
  KEEP_CLASSES_FRIENDLY

private:
  app_pointer_map<typename T_Sbx::T_PointerType>* map;
  typename T_Sbx::T_PointerType idx;
  T idx_unsandboxed;

  inline void move_obj(app_pointer&& other)
  {
    map = other.map;
    idx = other.idx;
    idx_unsandboxed = other.idx_unsandboxed;
    other.map = nullptr;
    other.idx = 0;
    other.idx_unsandboxed = nullptr;
  }

  inline T get_raw_value() const noexcept
  {
    return to_tainted().get_raw_value();
  }
  inline typename T_Sbx::T_PointerType get_raw_sandbox_value() const noexcept
  {
    return idx;
  }
  inline T get_raw_value() noexcept { return to_tainted().get_raw_value(); }
  inline typename T_Sbx::T_PointerType get_raw_sandbox_value() noexcept
  {
    return idx;
  }

  app_pointer(app_pointer_map<typename T_Sbx::T_PointerType>* a_map,
              typename T_Sbx::T_PointerType a_idx,
              T a_idx_unsandboxed)
    : map(a_map)
    , idx(a_idx)
    , idx_unsandboxed(a_idx_unsandboxed)
  {}

public:
  app_pointer()
    : map(nullptr)
    , idx(0)
    , idx_unsandboxed(0)
  {}

  ~app_pointer() { unregister(); }

  app_pointer(app_pointer&& other)
  {
    move_obj(std::forward<app_pointer>(other));
  }

  inline app_pointer& operator=(app_pointer&& other)
  {
    if (this != &other) {
      move_obj(std::forward<app_pointer>(other));
    }
    return *this;
  }

  void unregister()
  {
    if (idx != 0) {
      map->remove_app_ptr(idx);
      map = nullptr;
      idx = 0;
      idx_unsandboxed = nullptr;
    }
  }

  tainted<T, T_Sbx> to_tainted()
  {
    return tainted<T, T_Sbx>::internal_factory(
      reinterpret_cast<T>(idx_unsandboxed));
  }

  /**
   * @brief Check if app pointer is _not_ registered.
   */
  inline bool is_unregistered() const noexcept { return idx == 0; }

  /**
   * @brief Unwrap app_pointer without verification. This is an unsafe operation
   * and should be used with care.
   */
  inline auto UNSAFE_unverified() const noexcept { return get_raw_value(); }
  /**
   * @brief Like UNSAFE_unverified, but get the underlying sandbox
   * representation.
   *
   * @param sandbox Reference to sandbox.
   */
  inline auto UNSAFE_sandboxed(rlbox_sandbox<T_Sbx>& sandbox) const noexcept
  {
    RLBOX_UNUSED(sandbox);
    return get_raw_sandbox_value();
  }
  inline auto UNSAFE_unverified() noexcept { return get_raw_value(); }
  inline auto UNSAFE_sandboxed(rlbox_sandbox<T_Sbx>& sandbox) noexcept
  {
    RLBOX_UNUSED(sandbox);
    return get_raw_sandbox_value();
  }
};

/**
 * @brief Tainted boolean value that serves as a "hint" and not a definite
 * answer.  Comparisons with a tainted_volatile return such hints.  They are
 * not `tainted<bool>` values because a compromised sandbox can modify
 * tainted_volatile data at any time.
 */
class tainted_boolean_hint
{
private:
  bool val;

public:
  tainted_boolean_hint(bool init)
    : val(init)
  {}
  tainted_boolean_hint(const tainted_boolean_hint&) = default;
  inline tainted_boolean_hint& operator=(bool rhs)
  {
    val = rhs;
    return *this;
  }
  inline tainted_boolean_hint operator!() { return tainted_boolean_hint(!val); }
  template<size_t N>
  inline bool unverified_safe_because(const char (&reason)[N]) const
  {
    (void)reason; /* unused */
    return val;
  }
  inline bool UNSAFE_unverified() const { return val; }
  inline bool UNSAFE_unverified() { return val; }
  inline auto INTERNAL_unverified_safe() { return UNSAFE_unverified(); }
  inline auto INTERNAL_unverified_safe() const { return UNSAFE_unverified(); }

  // Add a template parameter to make sure the assert only fires when called
  template<typename T = void>
  inline bool copy_and_verify(...) const
  {
    rlbox_detail_static_fail_because(
      detail::true_v<T>,
      "You can't call copy_and_verify on this value, as this is a result of a "
      "comparison with memory accessible by the sandbox. \n"
      "The sandbox could unexpectedly change the value leading to "
      "time-of-check-time-of-use attacks. \n"
      "You can avoid this by making a local copy of the data."
      "For example, if your original code, looked like \n"
      "if ((tainted_ptr->member == 5).copy_and_verify(...)) { ... } \n\n"
      "Change this to \n\n"
      "tainted<int> val = tainted_ptr->member\n"
      "if ((val == 5).copy_and_verify(...)) { ... } \n\n"
      "tainted<int, T_Sbx> foo(rlbox_sandbox<T_Sbx>& sandbox) {...} \n\n"
      "Alternately, if you are sure your code is safe you can use the "
      "unverified_safe_because API to remove tainting\n");

    // this is never executed, but we need it for the function to type-check
    return false;
  }
};

/**
 * @brief Tainted integer value that serves as a "hint" and not a definite
 * answer.  Comparisons with a tainted_volatile return such hints.  They are
 * not `tainted<int>` values because a compromised sandbox can modify
 * tainted_volatile data at any time.
 */
class tainted_int_hint
{
private:
  int val;

public:
  tainted_int_hint(int init)
    : val(init)
  {}
  tainted_int_hint(const tainted_int_hint&) = default;
  inline tainted_int_hint& operator=(int rhs)
  {
    val = rhs;
    return *this;
  }
  inline tainted_boolean_hint operator!() { return tainted_boolean_hint(!val); }
  template<size_t N>
  inline int unverified_safe_because(const char (&reason)[N]) const
  {
    (void)reason; /* unused */
    return val;
  }
  inline int UNSAFE_unverified() const { return val; }
  inline int UNSAFE_unverified() { return val; }
  inline auto INTERNAL_unverified_safe() { return UNSAFE_unverified(); }
  inline auto INTERNAL_unverified_safe() const { return UNSAFE_unverified(); }

  // Add a template parameter to make sure the assert only fires when called
  template<typename T = void>
  inline int copy_and_verify(...) const
  {
    rlbox_detail_static_fail_because(
      detail::true_v<T>,
      "You can't call copy_and_verify on this value, as this is a result of a "
      "comparison with memory accessible by the sandbox. \n"
      "The sandbox could unexpectedly change the value leading to "
      "time-of-check-time-of-use attacks. \n"
      "You can avoid this by making a local copy of the data."
      "For example, if your original code, looked like \n"
      "if ((tainted_ptr->member == 5).copy_and_verify(...)) { ... } \n\n"
      "Change this to \n\n"
      "tainted<int> val = tainted_ptr->member\n"
      "if ((val == 5).copy_and_verify(...)) { ... } \n\n"
      "tainted<int, T_Sbx> foo(rlbox_sandbox<T_Sbx>& sandbox) {...} \n\n"
      "Alternately, if you are sure your code is safe you can use the "
      "unverified_safe_because API to remove tainting\n");

    // this is never executed, but we need it for the function to type-check
    return 0;
  }
};

}
