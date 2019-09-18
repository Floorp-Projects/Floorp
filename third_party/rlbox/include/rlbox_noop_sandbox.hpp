#pragma once

#include <cstdint>
#include <cstdlib>
#include <mutex>
#ifndef rlbox_use_custom_shared_lock
#  include <shared_mutex>
#endif
#include <utility>

#include "rlbox_helpers.hpp"

namespace rlbox {

/**
 * @brief Class that implements the null sandbox. This sandbox doesn't actually
 * provide any isolation and only serves as a stepping stone towards migrating
 * an application to use the RLBox API.
 */
class rlbox_noop_sandbox
{
public:
  // Stick with the system defaults
  using T_LongLongType = long long;
  using T_LongType = long;
  using T_IntType = int;
  using T_PointerType = uintptr_t;
  using T_ShortType = short;

private:
  rlbox_shared_lock(callback_mutex);
  static inline const uint32_t MAX_CALLBACKS = 64;
  void* callback_unique_keys[MAX_CALLBACKS]{ 0 };
  void* callbacks[MAX_CALLBACKS]{ 0 };

  struct rlbox_noop_sandbox_thread_local
  {
    rlbox_noop_sandbox* sandbox;
    uint32_t last_callback_invoked;
  };

  thread_local static inline rlbox_noop_sandbox_thread_local thread_data{ 0,
                                                                          0 };

  template<uint32_t N, typename T_Ret, typename... T_Args>
  static T_Ret callback_trampoline(T_Args... params)
  {
    thread_data.last_callback_invoked = N;
    using T_Func = T_Ret (*)(T_Args...);
    T_Func func;
    {
      rlbox_acquire_shared_guard(lock, thread_data.sandbox->callback_mutex);
      func = reinterpret_cast<T_Func>(thread_data.sandbox->callbacks[N]);
    }
    // Callbacks are invoked through function pointers, cannot use std::forward
    // as we don't have caller context for T_Args, which means they are all
    // effectively passed by value
    return func(params...);
  }

protected:
  inline void impl_create_sandbox() {}

  inline void impl_destroy_sandbox() {}

  template<typename T>
  inline void* impl_get_unsandboxed_pointer(T_PointerType p) const
  {
    return reinterpret_cast<void*>(static_cast<uintptr_t>(p));
  }

  template<typename T>
  inline T_PointerType impl_get_sandboxed_pointer(const void* p) const
  {
    return static_cast<T_PointerType>(reinterpret_cast<uintptr_t>(p));
  }

  template<typename T>
  static inline void* impl_get_unsandboxed_pointer_no_ctx(
    T_PointerType p,
    const void* /* example_unsandboxed_ptr */,
    rlbox_noop_sandbox* (*/* expensive_sandbox_finder */)(
      const void* example_unsandboxed_ptr))
  {
    return reinterpret_cast<void*>(static_cast<uintptr_t>(p));
  }

  template<typename T>
  static inline T_PointerType impl_get_sandboxed_pointer_no_ctx(
    const void* p,
    const void* /* example_unsandboxed_ptr */,
    rlbox_noop_sandbox* (*/* expensive_sandbox_finder */)(
      const void* example_unsandboxed_ptr))
  {
    return static_cast<T_PointerType>(reinterpret_cast<uintptr_t>(p));
  }

  inline T_PointerType impl_malloc_in_sandbox(size_t size)
  {
    void* p = std::malloc(size);
    return reinterpret_cast<uintptr_t>(p);
  }

  inline void impl_free_in_sandbox(T_PointerType p)
  {
    std::free(reinterpret_cast<void*>(p));
  }

  static inline bool impl_is_in_same_sandbox(const void*, const void*)
  {
    return true;
  }

  inline bool impl_is_pointer_in_sandbox_memory(const void*) { return true; }
  inline bool impl_is_pointer_in_app_memory(const void*) { return true; }

  inline size_t impl_get_total_memory()
  {
    return std::numeric_limits<size_t>::max();
  }

  inline void* impl_get_memory_location()
  {
    // There isn't any sandbox memory for the noop_sandbox as we just redirect
    // to the app. Also, this is mostly used for pointer swizzling or sandbox
    // bounds checks which is also not present/not required. So we can just
    // return null
    return nullptr;
  }

  // adding a template so that we can use static_assert to fire only if this
  // function is invoked
  template<typename T = void>
  void* impl_lookup_symbol(const char* /* func_name */)
  {
    // Will fire if this impl_lookup_symbol is ever called for the static
    // sandbox
    constexpr bool fail = std::is_same_v<T, void>;
    rlbox_detail_static_fail_because(
      fail,
      "The no_op_sandbox uses static calls and thus developers should add\n\n"
      "#define RLBOX_USE_STATIC_CALLS() rlbox_noop_sandbox_lookup_symbol\n\n"
      "to their code, to ensure that static calls are handled correctly.");

    return nullptr;
  }

#define rlbox_noop_sandbox_lookup_symbol(func_name)                            \
  reinterpret_cast<void*>(&func_name) /* NOLINT */

  template<typename T, typename T_Converted, typename... T_Args>
  auto impl_invoke_with_func_ptr(T_Converted* func_ptr, T_Args&&... params)
  {
    thread_data.sandbox = this;
    return (*func_ptr)(params...);
  }

  template<typename T_Ret, typename... T_Args>
  inline T_PointerType impl_register_callback(void* key, void* callback)
  {
    rlbox_acquire_unique_guard(lock, callback_mutex);

    void* chosen_trampoline = nullptr;

    // need a compile time for loop as we we need I to be a compile time value
    // this is because we are returning the I'th callback trampoline
    detail::compile_time_for<MAX_CALLBACKS>([&](auto I) {
      if (!chosen_trampoline && callback_unique_keys[I.value] == nullptr) {
        callback_unique_keys[I.value] = key;
        callbacks[I.value] = callback;
        chosen_trampoline = reinterpret_cast<void*>(
          callback_trampoline<I.value, T_Ret, T_Args...>);
      }
    });

    return reinterpret_cast<T_PointerType>(chosen_trampoline);
  }

  static inline std::pair<rlbox_noop_sandbox*, void*>
  impl_get_executed_callback_sandbox_and_key()
  {
    auto sandbox = thread_data.sandbox;
    auto callback_num = thread_data.last_callback_invoked;
    void* key = sandbox->callback_unique_keys[callback_num];
    return std::make_pair(sandbox, key);
  }

  template<typename T_Ret, typename... T_Args>
  inline void impl_unregister_callback(void* key)
  {
    rlbox_acquire_unique_guard(lock, callback_mutex);
    for (uint32_t i = 0; i < MAX_CALLBACKS; i++) {
      if (callback_unique_keys[i] == key) {
        callback_unique_keys[i] = nullptr;
        callbacks[i] = nullptr;
        break;
      }
    }
  }
};

}
