#pragma once
// IWYU pragma: private, include "rlbox.hpp"
// IWYU pragma: friend "rlbox_.*\.hpp"

#include <algorithm>
#include <cstdlib>
#include <map>
#include <mutex>
#include <type_traits>
#include <utility>
#include <vector>

#include "rlbox_conversion.hpp"
#include "rlbox_helpers.hpp"
#include "rlbox_struct_support.hpp"
#include "rlbox_type_traits.hpp"
#include "rlbox_wrapper_traits.hpp"

namespace rlbox {

namespace convert_fn_ptr_to_sandbox_equivalent_detail {
  template<typename T, typename T_Sbx>
  using conv = ::rlbox::detail::convert_to_sandbox_equivalent_t<T, T_Sbx>;

  template<typename T_Ret, typename... T_Args>
  using T_Func = T_Ret (*)(T_Args...);

  template<typename T_Sbx, typename T_Ret, typename... T_Args>
  T_Func<conv<T_Ret, T_Sbx>, conv<T_Args, T_Sbx>...> helper(
    T_Ret (*)(T_Args...));
}

template<typename T_Sbx>
class rlbox_sandbox : protected T_Sbx
{
  KEEP_CLASSES_FRIENDLY

private:
  std::mutex func_ptr_cache_lock;
  std::map<std::string, void*> func_ptr_map;

  std::mutex creation_lock;
  // This variable tracks of the sandbox has already been created/destroyed.
  // APIs in this class should be called only when the sandbox is created.
  // However, it is expensive to check in APIs such as invoke or in the callback
  // interceptor. Instead we leave it up to the user to ensure these APIs are
  // never called prior to sandbox construction. We perform checks, where they
  // would not add too much overhead
  bool sandbox_created = false;

  std::mutex callback_lock;
  std::vector<void*> callback_keys;

  template<typename T>
  using convert_fn_ptr_to_sandbox_equivalent_t = decltype(
    ::rlbox::convert_fn_ptr_to_sandbox_equivalent_detail::helper<T_Sbx>(
      std::declval<T>()));

  template<typename T>
  inline constexpr void check_invoke_param_type_is_ok()
  {
    using T_NoRef = std::remove_reference_t<T>;
    if_constexpr_named(cond1,
                       detail::rlbox_is_wrapper_v<T_NoRef> ||
                         std::is_null_pointer_v<T_NoRef> ||
                         detail::is_fundamental_or_enum_v<T_NoRef>)
    {}
    else
    {
      constexpr auto unknownCase = !(cond1);
      rlbox_detail_static_fail_because(
        unknownCase,
        "Arguments to a sandbox function call should be primitives  or wrapped "
        "types like tainted, callbacks etc.");
    }
  }

  template<typename T>
  inline auto invoke_process_param(T&& param)
  {
    check_invoke_param_type_is_ok<T>();

    using T_NoRef = std::remove_reference_t<T>;

    if constexpr (detail::rlbox_is_tainted_opaque_v<T_NoRef>) {
      auto ret = from_opaque(param);
      return ret.UNSAFE_sandboxed();
    } else if constexpr (detail::rlbox_is_wrapper_v<T_NoRef>) {
      return param.UNSAFE_sandboxed();
    } else if constexpr (std::is_null_pointer_v<T_NoRef>) {
      tainted<void*, T_Sbx> ret = nullptr;
      return ret.UNSAFE_sandboxed();
    } else if constexpr (detail::is_fundamental_or_enum_v<T_NoRef>) {
      // For unwrapped primitives, assign to a tainted var and then unwrap so
      // that we adjust for machine model
      tainted<T_NoRef, T_Sbx> copy = param;
      return copy.UNSAFE_sandboxed();
    } else {
      rlbox_detail_static_fail_because(detail::true_v<T_NoRef>, "Unknown case");
    }
  }

  template<typename T, typename T_Arg>
  inline tainted<T, T_Sbx> sandbox_callback_intercept_convert_param(
    const T_Arg& arg,
    const void* example_unsandboxed_ptr)
  {
    tainted<T, T_Sbx> ret;
    detail::convert_type<T_Sbx, detail::adjust_type_direction::TO_APPLICATION>(
      ret.get_raw_value_ref(), arg, example_unsandboxed_ptr);
    return ret;
  }

  template<typename T_Ret, typename... T_Args>
  static detail::convert_to_sandbox_equivalent_t<T_Ret, T_Sbx>
  sandbox_callback_interceptor(
    detail::convert_to_sandbox_equivalent_t<T_Args, T_Sbx>... args)
  {
    std::pair<T_Sbx*, void*> context =
      T_Sbx::impl_get_executed_callback_sandbox_and_key();
    auto& sandbox = *(reinterpret_cast<rlbox_sandbox<T_Sbx>*>(context.first));
    auto key = context.second;

    using T_Func_Ret =
      std::conditional_t<std::is_void_v<T_Ret>, void, tainted<T_Ret, T_Sbx>>;
    using T_Func =
      T_Func_Ret (*)(rlbox_sandbox<T_Sbx>&, tainted<T_Args, T_Sbx>...);
    auto target_fn_ptr = reinterpret_cast<T_Func>(key);
    const void* example_unsandboxed_ptr = sandbox.get_memory_location();

    // Some branches (after inlining function calls) don't use the param
    RLBOX_UNUSED(example_unsandboxed_ptr);

    if constexpr (std::is_void_v<T_Func_Ret>) {
      (*target_fn_ptr)(
        sandbox,
        sandbox.template sandbox_callback_intercept_convert_param<T_Args>(
          args, example_unsandboxed_ptr)...);
      return;
    } else {
      auto tainted_ret = (*target_fn_ptr)(
        sandbox,
        sandbox.template sandbox_callback_intercept_convert_param<T_Args>(
          args, example_unsandboxed_ptr)...);

      detail::convert_to_sandbox_equivalent_t<T_Ret, T_Sbx> ret;
      detail::convert_type<T_Sbx, detail::adjust_type_direction::TO_SANDBOX>(
        ret, tainted_ret.get_raw_value_ref());
      return ret;
    }
  }

  template<typename T_Ret, typename... T_Args>
  inline void unregister_callback(void* key)
  {
    {
      std::lock_guard<std::mutex> lock(creation_lock);
      // Silent failure is better here as RAII types may try to invoke this
      // after destruction
      if (!sandbox_created) {
        return;
      }
    }

    this->template impl_unregister_callback<T_Ret, T_Args...>(key);

    std::lock_guard<std::mutex> lock(callback_lock);
    auto el_ref = std::find(callback_keys.begin(), callback_keys.end(), key);
    detail::dynamic_check(
      el_ref != callback_keys.end(),
      "Unexpected state. Unregistering a callback that was never registered.");
    callback_keys.erase(el_ref);
  }

public:
  /***** Function to adjust for custom machine models *****/

  template<typename T>
  using convert_to_sandbox_equivalent_nonclass_t =
    detail::convert_base_types_t<T,
                                 typename T_Sbx::T_ShortType,
                                 typename T_Sbx::T_IntType,
                                 typename T_Sbx::T_LongType,
                                 typename T_Sbx::T_LongLongType,
                                 typename T_Sbx::T_PointerType>;

  T_Sbx* get_sandbox_impl() { return this; }

  template<typename... T_Args>
  inline auto create_sandbox(T_Args... args)
  {
    detail::return_first_result(
      [&]() {
        return this->impl_create_sandbox(std::forward<T_Args>(args)...);
      },
      [&]() {
        std::lock_guard<std::mutex> lock(creation_lock);
        sandbox_created = true;
      });
  }

  inline auto destroy_sandbox()
  {
    {
      std::lock_guard<std::mutex> lock(creation_lock);
      detail::dynamic_check(sandbox_created,
                            "destroy_sandbox called without sandbox creation");
      sandbox_created = false;
    }

    return this->impl_destroy_sandbox();
  }

  template<typename T>
  inline T* get_unsandboxed_pointer(
    convert_to_sandbox_equivalent_nonclass_t<T*> p) const
  {
    if (p == 0) {
      return nullptr;
    }
    auto ret = this->template impl_get_unsandboxed_pointer<T>(p);
    return reinterpret_cast<T*>(ret);
  }

  template<typename T>
  inline convert_to_sandbox_equivalent_nonclass_t<T*> get_sandboxed_pointer(
    const void* p) const
  {
    if (p == nullptr) {
      return 0;
    }
    return this->template impl_get_sandboxed_pointer<T>(p);
  }

  template<typename T>
  static inline T* get_unsandboxed_pointer_no_ctx(
    convert_to_sandbox_equivalent_nonclass_t<T*> p,
    const void* example_unsandboxed_ptr)
  {
    if (p == 0) {
      return nullptr;
    }
    auto ret = T_Sbx::template impl_get_unsandboxed_pointer_no_ctx<T>(
      p, example_unsandboxed_ptr);
    return reinterpret_cast<T*>(ret);
  }

  template<typename T>
  static inline convert_to_sandbox_equivalent_nonclass_t<T*>
  get_sandboxed_pointer_no_ctx(const void* p)
  {
    if (p == nullptr) {
      return 0;
    }
    return T_Sbx::template impl_get_sandboxed_pointer_no_ctx<T>(p);
  }

  /**
   * @brief Create a pointer accessible to the sandbox. The pointer is allocated
   * in sandbox memory.
   *
   * @tparam T - the type of the pointer you want to create. If T=int, this
   * would return a pointer to an int, accessible to the sandbox which is
   * tainted.
   * @return tainted<T*, T_Sbx> - Tainted pointer accessible to the sandbox.
   */
  template<typename T>
  inline tainted<T*, T_Sbx> malloc_in_sandbox()
  {
    const uint32_t defaultCount = 1;
    return malloc_in_sandbox<T>(defaultCount);
  }
  template<typename T>
  inline tainted<T*, T_Sbx> malloc_in_sandbox(uint32_t count)
  {
    {
      std::lock_guard<std::mutex> lock(creation_lock);
      // Silent failure is better here as RAII types may try to invoke this
      // after destruction
      if (!sandbox_created) {
        return tainted<T*, T_Sbx>::internal_factory(nullptr);
      }
    }

    detail::dynamic_check(count != 0, "Malloc tried to allocate 0 bytes");
    auto ptr_in_sandbox = this->impl_malloc_in_sandbox(sizeof(T) * count);
    auto ptr = get_unsandboxed_pointer<T>(ptr_in_sandbox);
    detail::dynamic_check(is_pointer_in_sandbox_memory(ptr),
                          "Malloc returned pointer outside the sandbox memory");
    auto ptr_end = reinterpret_cast<uintptr_t>(ptr + (count - 1));
    detail::dynamic_check(
      is_in_same_sandbox(ptr, reinterpret_cast<void*>(ptr_end)),
      "Malloc returned a pointer whose range goes beyond sandbox memory");
    auto cast_ptr = reinterpret_cast<T*>(ptr);
    return tainted<T*, T_Sbx>::internal_factory(cast_ptr);
  }

  template<typename T>
  inline void free_in_sandbox(tainted<T*, T_Sbx> ptr)
  {
    {
      std::lock_guard<std::mutex> lock(creation_lock);
      // Silent failure is better here as RAII types may try to invoke this
      // after destruction
      if (!sandbox_created) {
        return;
      }
    }

    this->impl_free_in_sandbox(ptr.get_raw_sandbox_value());
  }

  static inline bool is_in_same_sandbox(const void* p1, const void* p2)
  {
    return T_Sbx::impl_is_in_same_sandbox(p1, p2);
  }

  inline bool is_pointer_in_sandbox_memory(const void* p)
  {
    return this->impl_is_pointer_in_sandbox_memory(p);
  }

  inline bool is_pointer_in_app_memory(const void* p)
  {
    return this->impl_is_pointer_in_app_memory(p);
  }

  inline size_t get_total_memory() { return this->impl_get_total_memory(); }

  inline void* get_memory_location()
  {
    return this->impl_get_memory_location();
  }

  void* lookup_symbol(const char* func_name)
  {
    std::lock_guard<std::mutex> lock(func_ptr_cache_lock);

    auto func_ptr_ref = func_ptr_map.find(func_name);

    void* func_ptr;
    if (func_ptr_ref == func_ptr_map.end()) {
      func_ptr = this->impl_lookup_symbol(func_name);
      func_ptr_map[func_name] = func_ptr;
    } else {
      func_ptr = func_ptr_ref->second;
    }

    return func_ptr;
  }

  template<typename T, typename... T_Args>
  auto invoke_with_func_ptr(void* func_ptr, T_Args&&... params)
  {
    (check_invoke_param_type_is_ok<T_Args>(), ...);

    static_assert(
      std::is_invocable_v<
        T,
        detail::rlbox_remove_wrapper_t<std::remove_reference_t<T_Args>>...>,
      "Mismatched arguments types for function");

    using T_Result = std::invoke_result_t<
      T,
      detail::rlbox_remove_wrapper_t<std::remove_reference_t<T_Args>>...>;

    using T_Converted =
      std::remove_pointer_t<convert_fn_ptr_to_sandbox_equivalent_t<T*>>;

    if constexpr (std::is_void_v<T_Result>) {
      this->template impl_invoke_with_func_ptr<T>(
        reinterpret_cast<T_Converted*>(func_ptr),
        invoke_process_param(params)...);
      return;
    } else {
      auto raw_result = this->template impl_invoke_with_func_ptr<T>(
        reinterpret_cast<T_Converted*>(func_ptr),
        invoke_process_param(params)...);
      tainted<T_Result, T_Sbx> wrapped_result;
      const void* example_unsandboxed_ptr = get_memory_location();
      detail::convert_type<T_Sbx,
                           detail::adjust_type_direction::TO_APPLICATION>(
        wrapped_result.get_raw_value_ref(),
        raw_result,
        example_unsandboxed_ptr);
      return wrapped_result;
    }
  }

  // Useful in the porting stage to temporarily allow non tainted pointers to go
  // through. This will only ever work in the rlbox_noop_sandbox. Any sandbox
  // that actually enforces isolation will crash here.
  template<typename T2>
  tainted<T2, T_Sbx> UNSAFE_accept_pointer(T2 ptr)
  {
    static_assert(std::is_pointer_v<T2>,
                  "UNSAFE_accept_pointer expects a pointer param");
    tainted<T2, T_Sbx> ret;
    ret.assign_raw_pointer(*this, ptr);
    return ret;
  }

  template<typename T_Ret, typename... T_Args>
  using T_Cb_no_wrap = detail::rlbox_remove_wrapper_t<T_Ret>(
    detail::rlbox_remove_wrapper_t<T_Args>...);

  template<typename T_Ret>
  sandbox_callback<T_Cb_no_wrap<T_Ret>*, T_Sbx> register_callback(T_Ret (*)())
  {
    rlbox_detail_static_fail_because(
      detail::true_v<T_Ret>,
      "Modify the callback to change the first parameter to a sandbox."
      "For instance if a callback has type\n\n"
      "int foo() {...}\n\n"
      "Change this to \n\n"
      "tainted<int, T_Sbx> foo(rlbox_sandbox<T_Sbx>& sandbox) {...}\n");

    // this is never executed, but we need it for the function to type-check
    std::abort();
  }

  template<typename T_RL, typename T_Ret, typename... T_Args>
  sandbox_callback<T_Cb_no_wrap<T_Ret, T_Args...>*, T_Sbx> register_callback(
    T_Ret (*func_ptr)(T_RL, T_Args...))
  {
    // Some branches don't use the param
    RLBOX_UNUSED(func_ptr);

    if_constexpr_named(cond1, !std::is_same_v<T_RL, rlbox_sandbox<T_Sbx>&>)
    {
      rlbox_detail_static_fail_because(
        cond1,
        "Modify the callback to change the first parameter to a sandbox."
        "For instance if a callback has type\n\n"
        "int foo(int a, int b) {...}\n\n"
        "Change this to \n\n"
        "tainted<int, T_Sbx> foo(rlbox_sandbox<T_Sbx>& sandbox,"
        "tainted<int, T_Sbx> a, tainted<int, T_Sbx> b) {...}\n");
    }
    else if_constexpr_named(
      cond2, !(detail::rlbox_is_tainted_or_opaque_v<T_Args> && ...))
    {
      rlbox_detail_static_fail_because(
        cond2,
        "Change all arguments to the callback have to be tainted or "
        "tainted_opaque."
        "For instance if a callback has type\n\n"
        "int foo(int a, int b) {...}\n\n"
        "Change this to \n\n"
        "tainted<int, T_Sbx> foo(rlbox_sandbox<T_Sbx>& sandbox,"
        "tainted<int, T_Sbx> a, tainted<int, T_Sbx> b) {...}\n");
    }
    else if_constexpr_named(
      cond3, (std::is_array_v<detail::rlbox_remove_wrapper_t<T_Args>> || ...))
    {
      rlbox_detail_static_fail_because(
        cond3,
        "Change all static array arguments to the callback to be pointers."
        "For instance if a callback has type\n\n"
        "int foo(int a[4]) {...}\n\n"
        "Change this to \n\n"
        "tainted<int, T_Sbx> foo(rlbox_sandbox<T_Sbx>& sandbox,"
        "tainted<int*, T_Sbx> a) {...}\n");
    }
    else if_constexpr_named(
      cond4,
      !(std::is_void_v<T_Ret> || detail::rlbox_is_tainted_or_opaque_v<T_Ret>))
    {
      rlbox_detail_static_fail_because(
        cond4,
        "Change the callback return type to be tainted or tainted_opaque if it "
        "is not void."
        "For instance if a callback has type\n\n"
        "int foo(int a, int b) {...}\n\n"
        "Change this to \n\n"
        "tainted<int, T_Sbx> foo(rlbox_sandbox<T_Sbx>& sandbox,"
        "tainted<int, T_Sbx> a, tainted<int, T_Sbx> b) {...}\n");
    }
    else
    {
      {
        std::lock_guard<std::mutex> lock(creation_lock);
        detail::dynamic_check(
          sandbox_created, "register_callback called without sandbox creation");
      }
      // Need unique key for each callback we register - just use the func addr
      void* unique_key = reinterpret_cast<void*>(func_ptr);

      // Make sure that the user hasn't previously registered this function...
      // If they have, we would returning 2 owning types (sandbox_callback) to
      // the same callback which would be bad
      {
        std::lock_guard<std::mutex> lock(callback_lock);
        bool exists =
          std::find(callback_keys.begin(), callback_keys.end(), unique_key) !=
          callback_keys.end();
        detail::dynamic_check(
          !exists, "You have previously already registered this callback.");
        callback_keys.push_back(unique_key);
      }

      auto callback_interceptor =
        sandbox_callback_interceptor<detail::rlbox_remove_wrapper_t<T_Ret>,
                                     detail::rlbox_remove_wrapper_t<T_Args>...>;

      auto callback_trampoline = this->template impl_register_callback<
        detail::rlbox_remove_wrapper_t<T_Ret>,
        detail::rlbox_remove_wrapper_t<T_Args>...>(
        unique_key, reinterpret_cast<void*>(callback_interceptor));

      auto tainted_func_ptr = reinterpret_cast<
        detail::rlbox_tainted_opaque_to_tainted_t<T_Ret, T_Sbx> (*)(
          T_RL, detail::rlbox_tainted_opaque_to_tainted_t<T_Args, T_Sbx>...)>(
        func_ptr);

      auto ret = sandbox_callback<T_Cb_no_wrap<T_Ret, T_Args...>*, T_Sbx>(
        this,
        tainted_func_ptr,
        callback_interceptor,
        callback_trampoline,
        unique_key);
      return ret;
    }
  }

  // this is an internal function, but as it is invoked from macros it needs to
  // be public
  template<typename T>
  inline sandbox_function<T*, T_Sbx> INTERNAL_get_sandbox_function(
    void* func_ptr)
  {
    auto internal_func_ptr = get_sandboxed_pointer<T*>(func_ptr);
    return sandbox_function<T*, T_Sbx>(internal_func_ptr);
  }
};

#if defined(__clang__)
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wgnu-zero-variadic-macro-arguments"
#elif defined(__GNUC__) || defined(__GNUG__)
// Can't turn off the variadic macro warning emitted from -pedantic so use a
// hack to stop GCC emitting warnings for the reminder of this file
#  pragma GCC system_header
#elif defined(_MSC_VER)
// Doesn't seem to emit the warning
#else
// Don't know the compiler... just let it go through
#endif

#if defined(RLBOX_USE_STATIC_CALLS)
#  define sandbox_lookup_symbol_helper(prefix, sandbox, func_name)             \
    prefix(sandbox, func_name)

#  define sandbox_lookup_symbol(sandbox, func_name)                            \
    sandbox_lookup_symbol_helper(RLBOX_USE_STATIC_CALLS(), sandbox, func_name)
#else
#  define sandbox_lookup_symbol(sandbox, func_name)                            \
    (sandbox).lookup_symbol(#func_name)
#endif

#define sandbox_invoke(sandbox, func_name, ...)                                \
  (sandbox).template invoke_with_func_ptr<decltype(func_name)>(                \
    sandbox_lookup_symbol(sandbox, func_name), ##__VA_ARGS__)

#define sandbox_function_address(sandbox, func_name)                           \
  (sandbox).template INTERNAL_get_sandbox_function<decltype(func_name)>(       \
    sandbox_lookup_symbol(sandbox, func_name))

#if defined(__clang__)
#  pragma clang diagnostic pop
#else
#endif

}