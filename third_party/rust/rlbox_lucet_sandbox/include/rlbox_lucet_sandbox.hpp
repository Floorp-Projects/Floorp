#pragma once

#include "lucet_sandbox.h"

#include <cstdint>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
// RLBox allows applications to provide a custom shared lock implementation
#ifndef RLBOX_USE_CUSTOM_SHARED_LOCK
#  include <shared_mutex>
#endif
#include <type_traits>
#include <utility>
#include <vector>

#define RLBOX_LUCET_UNUSED(...) (void)__VA_ARGS__

// Use the same convention as rlbox to allow applications to customize the
// shared lock
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

namespace rlbox {

namespace detail {
  // relying on the dynamic check settings (exception vs abort) in the rlbox lib
  inline void dynamic_check(bool check, const char* const msg);
}

namespace lucet_detail {

  template<typename T>
  constexpr bool false_v = false;

  // https://stackoverflow.com/questions/6512019/can-we-get-the-type-of-a-lambda-argument
  namespace return_argument_detail {
    template<typename Ret, typename... Rest>
    Ret helper(Ret (*)(Rest...));

    template<typename Ret, typename F, typename... Rest>
    Ret helper(Ret (F::*)(Rest...));

    template<typename Ret, typename F, typename... Rest>
    Ret helper(Ret (F::*)(Rest...) const);

    template<typename F>
    decltype(helper(&F::operator())) helper(F);
  } // namespace return_argument_detail

  template<typename T>
  using return_argument =
    decltype(return_argument_detail::helper(std::declval<T>()));

  ///////////////////////////////////////////////////////////////

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
  } // namespace compile_time_for_detail

  template<std::size_t N, typename F>
  inline void compile_time_for(F func)
  {
    compile_time_for_detail::compile_time_for_helper(
      func, std::make_index_sequence<N>());
  }

  ///////////////////////////////////////////////////////////////

  template<typename T, typename = void>
  struct convert_type_to_wasm_type
  {
    static_assert(std::is_void_v<T>, "Missing specialization");
    using type = void;
    static constexpr enum LucetValueType lucet_type = LucetValueType_Void;
  };

  template<typename T>
  struct convert_type_to_wasm_type<
    T,
    std::enable_if_t<(std::is_integral_v<T> || std::is_enum_v<T>)&&sizeof(T) <=
                     sizeof(uint32_t)>>
  {
    using type = uint32_t;
    static constexpr enum LucetValueType lucet_type = LucetValueType_I32;
  };

  template<typename T>
  struct convert_type_to_wasm_type<
    T,
    std::enable_if_t<(std::is_integral_v<T> ||
                      std::is_enum_v<T>)&&sizeof(uint32_t) < sizeof(T) &&
                     sizeof(T) <= sizeof(uint64_t)>>
  {
    using type = uint64_t;
    static constexpr enum LucetValueType lucet_type = LucetValueType_I64;
  };

  template<typename T>
  struct convert_type_to_wasm_type<T,
                                   std::enable_if_t<std::is_same_v<T, float>>>
  {
    using type = T;
    static constexpr enum LucetValueType lucet_type = LucetValueType_F32;
  };

  template<typename T>
  struct convert_type_to_wasm_type<T,
                                   std::enable_if_t<std::is_same_v<T, double>>>
  {
    using type = T;
    static constexpr enum LucetValueType lucet_type = LucetValueType_F64;
  };

  template<typename T>
  struct convert_type_to_wasm_type<
    T,
    std::enable_if_t<std::is_pointer_v<T> || std::is_class_v<T>>>
  {
    // pointers are 32 bit indexes in wasm
    // class paramters are passed as a pointer to an object in the stack or heap
    using type = uint32_t;
    static constexpr enum LucetValueType lucet_type = LucetValueType_I32;
  };

  ///////////////////////////////////////////////////////////////

  namespace prepend_arg_type_detail {
    template<typename T, typename T_ArgNew>
    struct helper;

    template<typename T_ArgNew, typename T_Ret, typename... T_Args>
    struct helper<T_Ret(T_Args...), T_ArgNew>
    {
      using type = T_Ret(T_ArgNew, T_Args...);
    };
  }

  template<typename T_Func, typename T_ArgNew>
  using prepend_arg_type =
    typename prepend_arg_type_detail::helper<T_Func, T_ArgNew>::type;

  ///////////////////////////////////////////////////////////////

  namespace change_return_type_detail {
    template<typename T, typename T_RetNew>
    struct helper;

    template<typename T_RetNew, typename T_Ret, typename... T_Args>
    struct helper<T_Ret(T_Args...), T_RetNew>
    {
      using type = T_RetNew(T_Args...);
    };
  }

  template<typename T_Func, typename T_RetNew>
  using change_return_type =
    typename change_return_type_detail::helper<T_Func, T_RetNew>::type;

  ///////////////////////////////////////////////////////////////

  namespace change_class_arg_types_detail {
    template<typename T, typename T_ArgNew>
    struct helper;

    template<typename T_ArgNew, typename T_Ret, typename... T_Args>
    struct helper<T_Ret(T_Args...), T_ArgNew>
    {
      using type =
        T_Ret(std::conditional_t<std::is_class_v<T_Args>, T_ArgNew, T_Args>...);
    };
  }

  template<typename T_Func, typename T_ArgNew>
  using change_class_arg_types =
    typename change_class_arg_types_detail::helper<T_Func, T_ArgNew>::type;

} // namespace lucet_detail

class rlbox_lucet_sandbox;

struct rlbox_lucet_sandbox_thread_data
{
  rlbox_lucet_sandbox* sandbox;
  uint32_t last_callback_invoked;
};

#ifdef RLBOX_EMBEDDER_PROVIDES_TLS_STATIC_VARIABLES

rlbox_lucet_sandbox_thread_data* get_rlbox_lucet_sandbox_thread_data();
#  define RLBOX_LUCET_SANDBOX_STATIC_VARIABLES()                               \
    thread_local rlbox::rlbox_lucet_sandbox_thread_data                        \
      rlbox_lucet_sandbox_thread_info{ 0, 0 };                                 \
    namespace rlbox {                                                          \
      rlbox_lucet_sandbox_thread_data* get_rlbox_lucet_sandbox_thread_data()   \
      {                                                                        \
        return &rlbox_lucet_sandbox_thread_info;                               \
      }                                                                        \
    }                                                                          \
    static_assert(true, "Enforce semi-colon")

#endif

class rlbox_lucet_sandbox
{
public:
  using T_LongLongType = int64_t;
  using T_LongType = int32_t;
  using T_IntType = int32_t;
  using T_PointerType = uint32_t;
  using T_ShortType = int16_t;

private:
  LucetSandboxInstance* sandbox = nullptr;
  uintptr_t heap_base;
  void* malloc_index = 0;
  void* free_index = 0;
  size_t return_slot_size = 0;
  T_PointerType return_slot = 0;

  static const size_t MAX_CALLBACKS = 128;
  RLBOX_SHARED_LOCK(callback_mutex);
  void* callback_unique_keys[MAX_CALLBACKS]{ 0 };
  void* callbacks[MAX_CALLBACKS]{ 0 };
  uint32_t callback_slot_assignment[MAX_CALLBACKS]{ 0 };

  using TableElementRef = LucetFunctionTableElement*;
  struct FunctionTable
  {
    TableElementRef elements[MAX_CALLBACKS];
    uint32_t slot_number[MAX_CALLBACKS];
  };
  inline static std::mutex callback_table_mutex;
  // We need to share the callback slot info across multiple sandbox instances
  // that may load the same sandboxed library. Thus if the sandboxed library is
  // already in the memory space, we should just use the previously saved info
  // as the load is destroys the callback info. Once all instances of the
  // library is unloaded, the sandboxed library is removed from the address
  // space and thus we can "reset" our state. The semantics of shared and weak
  // pointers ensure this and will automatically release the memory after all
  // instances are released.
  inline static std::map<void*, std::weak_ptr<FunctionTable>>
    shared_callback_slots;
  std::shared_ptr<FunctionTable> callback_slots = nullptr;
  // However, if the library is also loaded externally in the application, then
  // we don't know when we can ever "reset". In such scenarios, we are better of
  // never throwing away the callback info, rather than figuring out
  // what/why/when the application is loading or unloading the sandboxed
  // library. An extra reference to the shared_ptr will ensure this.
  inline static std::vector<std::shared_ptr<FunctionTable>>
    saved_callback_slot_info;

#ifndef RLBOX_EMBEDDER_PROVIDES_TLS_STATIC_VARIABLES
  thread_local static inline rlbox_lucet_sandbox_thread_data thread_data{ 0,
                                                                          0 };
#endif

  template<typename T_FormalRet, typename T_ActualRet>
  inline auto serialize_to_sandbox(T_ActualRet arg)
  {
    if constexpr (std::is_class_v<T_FormalRet>) {
      // structs returned as pointers into wasm memory/wasm stack
      auto ptr = reinterpret_cast<T_FormalRet*>(
        impl_get_unsandboxed_pointer<T_FormalRet*>(arg));
      T_FormalRet ret = *ptr;
      return ret;
    } else {
      return arg;
    }
  }

  inline std::shared_ptr<FunctionTable> get_callback_ref_data(
    LucetFunctionTable& functionPointerTable)
  {
    auto callback_slots = std::make_shared<FunctionTable>();

    for (size_t i = 0; i < MAX_CALLBACKS; i++) {
      uintptr_t reservedVal =
        lucet_get_reserved_callback_slot_val(sandbox, i + 1);

      bool found = false;
      for (size_t j = 0; j < functionPointerTable.length; j++) {
        if (functionPointerTable.data[j].rf == reservedVal) {
          functionPointerTable.data[j].rf = 0;
          callback_slots->elements[i] = &(functionPointerTable.data[j]);
          callback_slots->slot_number[i] = static_cast<uint32_t>(j);
          found = true;
          break;
        }
      }

      detail::dynamic_check(found, "Unable to intialize callback tables");
    }

    return callback_slots;
  }

  inline void reinit_callback_ref_data(
    LucetFunctionTable& functionPointerTable,
    std::shared_ptr<FunctionTable>& callback_slots)
  {
    for (size_t i = 0; i < MAX_CALLBACKS; i++) {
      uintptr_t reservedVal =
        lucet_get_reserved_callback_slot_val(sandbox, i + 1);

      for (size_t j = 0; j < functionPointerTable.length; j++) {
        if (functionPointerTable.data[j].rf == reservedVal) {
          functionPointerTable.data[j].rf = 0;

          detail::dynamic_check(
            callback_slots->elements[i] == &(functionPointerTable.data[j]) &&
              callback_slots->slot_number[i] == static_cast<uint32_t>(j),
            "Sandbox creation error: Error when checking the values of "
            "callback slot data");

          break;
        }
      }
    }
  }

  inline void set_callbacks_slots_ref(bool external_loads_exist)
  {
    LucetFunctionTable functionPointerTable =
      lucet_get_function_pointer_table(sandbox);
    void* key = functionPointerTable.data;

    std::lock_guard<std::mutex> lock(callback_table_mutex);
    std::weak_ptr<FunctionTable> slots = shared_callback_slots[key];

    if (auto shared_slots = slots.lock()) {
      // pointer exists
      callback_slots = shared_slots;
      // Sometimes, dlopen and process forking seem to act a little weird.
      // Writes to the writable page of the dynamic lib section seem to not
      // always be propagated (possibly when the dynamic library is opened
      // externally - "external_loads_exist")). This occurred in when RLBox was
      // used in ASAN builds of Firefox. In general, we take the precaution of
      // rechecking this on each sandbox creation.
      reinit_callback_ref_data(functionPointerTable, callback_slots);
      return;
    }

    callback_slots = get_callback_ref_data(functionPointerTable);
    shared_callback_slots[key] = callback_slots;
    if (external_loads_exist) {
      saved_callback_slot_info.push_back(callback_slots);
    }
  }

  template<uint32_t N, typename T_Ret, typename... T_Args>
  static typename lucet_detail::convert_type_to_wasm_type<T_Ret>::type
  callback_interceptor(
    void* /* vmContext */,
    typename lucet_detail::convert_type_to_wasm_type<T_Args>::type... params)
  {
#ifdef RLBOX_EMBEDDER_PROVIDES_TLS_STATIC_VARIABLES
    auto& thread_data = *get_rlbox_lucet_sandbox_thread_data();
#endif
    thread_data.last_callback_invoked = N;
    using T_Func = T_Ret (*)(T_Args...);
    T_Func func;
    {
      RLBOX_ACQUIRE_SHARED_GUARD(lock, thread_data.sandbox->callback_mutex);
      func = reinterpret_cast<T_Func>(thread_data.sandbox->callbacks[N]);
    }
    // Callbacks are invoked through function pointers, cannot use std::forward
    // as we don't have caller context for T_Args, which means they are all
    // effectively passed by value
    return func(thread_data.sandbox->serialize_to_sandbox<T_Args>(params)...);
  }

  template<uint32_t N, typename T_Ret, typename... T_Args>
  static void callback_interceptor_promoted(
    void* /* vmContext */,
    typename lucet_detail::convert_type_to_wasm_type<T_Ret>::type ret,
    typename lucet_detail::convert_type_to_wasm_type<T_Args>::type... params)
  {
#ifdef RLBOX_EMBEDDER_PROVIDES_TLS_STATIC_VARIABLES
    auto& thread_data = *get_rlbox_lucet_sandbox_thread_data();
#endif
    thread_data.last_callback_invoked = N;
    using T_Func = T_Ret (*)(T_Args...);
    T_Func func;
    {
      RLBOX_ACQUIRE_SHARED_GUARD(lock, thread_data.sandbox->callback_mutex);
      func = reinterpret_cast<T_Func>(thread_data.sandbox->callbacks[N]);
    }
    // Callbacks are invoked through function pointers, cannot use std::forward
    // as we don't have caller context for T_Args, which means they are all
    // effectively passed by value
    auto ret_val =
      func(thread_data.sandbox->serialize_to_sandbox<T_Args>(params)...);
    // Copy the return value back
    auto ret_ptr = reinterpret_cast<T_Ret*>(
      thread_data.sandbox->template impl_get_unsandboxed_pointer<T_Ret*>(ret));
    *ret_ptr = ret_val;
  }

  template<typename T_Ret, typename... T_Args>
  inline T_PointerType get_lucet_type_index(
    T_Ret (*/* dummy for template inference */)(T_Args...) = nullptr) const
  {
    // Class return types as promoted to args
    constexpr bool promoted = std::is_class_v<T_Ret>;
    int32_t type_index;

    if constexpr (promoted) {
      LucetValueType ret_type = LucetValueType::LucetValueType_Void;
      LucetValueType param_types[] = {
        lucet_detail::convert_type_to_wasm_type<T_Ret>::lucet_type,
        lucet_detail::convert_type_to_wasm_type<T_Args>::lucet_type...
      };
      LucetFunctionSignature signature{ ret_type,
                                        sizeof(param_types) /
                                          sizeof(LucetValueType),
                                        &(param_types[0]) };
      type_index = lucet_get_function_type_index(sandbox, signature);
    } else {
      LucetValueType ret_type =
        lucet_detail::convert_type_to_wasm_type<T_Ret>::lucet_type;
      LucetValueType param_types[] = {
        lucet_detail::convert_type_to_wasm_type<T_Args>::lucet_type...
      };
      LucetFunctionSignature signature{ ret_type,
                                        sizeof(param_types) /
                                          sizeof(LucetValueType),
                                        &(param_types[0]) };
      type_index = lucet_get_function_type_index(sandbox, signature);
    }

    return type_index;
  }

  void ensure_return_slot_size(size_t size)
  {
    if (size > return_slot_size) {
      if (return_slot_size) {
        impl_free_in_sandbox(return_slot);
      }
      return_slot = impl_malloc_in_sandbox(size);
      detail::dynamic_check(
        return_slot != 0,
        "Error initializing return slot. Sandbox may be out of memory!");
      return_slot_size = size;
    }
  }

protected:
  // Set external_loads_exist to true, if the host application loads the
  // library lucet_module_path outside of rlbox_lucet_sandbox such as via dlopen
  // or the Windows equivalent
  inline void impl_create_sandbox(const char* lucet_module_path,
                                  bool external_loads_exist,
                                  bool allow_stdio)
  {
    detail::dynamic_check(sandbox == nullptr, "Sandbox already initialized");
    sandbox = lucet_load_module(lucet_module_path, allow_stdio);
    detail::dynamic_check(sandbox != nullptr, "Sandbox could not be created");

    heap_base = reinterpret_cast<uintptr_t>(impl_get_memory_location());
    // Check that the address space is larger than the sandbox heap i.e. 4GB
    // sandbox heap, host has to have more than 4GB
    static_assert(sizeof(uintptr_t) > sizeof(T_PointerType));
    // Check that the heap is aligned to the pointer size i.e. 32-bit pointer =>
    // aligned to 4GB. The implementations of
    // impl_get_unsandboxed_pointer_no_ctx and impl_get_sandboxed_pointer_no_ctx
    // below rely on this.
    uintptr_t heap_offset_mask = std::numeric_limits<T_PointerType>::max();
    detail::dynamic_check((heap_base & heap_offset_mask) == 0,
                          "Sandbox heap not aligned to 4GB");

    // cache these for performance
    malloc_index = impl_lookup_symbol("malloc");
    free_index = impl_lookup_symbol("free");

    set_callbacks_slots_ref(external_loads_exist);
  }

  inline void impl_create_sandbox(const char* lucet_module_path)
  {
    // Default is to assume that no external code will load the wasm library as
    // this is usually the case
    const bool external_loads_exist = false;
    const bool allow_stdio = true;
    impl_create_sandbox(lucet_module_path, external_loads_exist, allow_stdio);
  }

  inline void impl_destroy_sandbox() {
    if (return_slot_size) {
      impl_free_in_sandbox(return_slot);
    }
    lucet_drop_module(sandbox);
  }

  template<typename T>
  inline void* impl_get_unsandboxed_pointer(T_PointerType p) const
  {
    if constexpr (std::is_function_v<std::remove_pointer_t<T>>) {
      LucetFunctionTable functionPointerTable =
        lucet_get_function_pointer_table(sandbox);
      if (p >= functionPointerTable.length) {
        // Received out of range function pointer
        return nullptr;
      }
      auto ret = functionPointerTable.data[p].rf;
      return reinterpret_cast<void*>(static_cast<uintptr_t>(ret));
    } else {
      return reinterpret_cast<void*>(heap_base + p);
    }
  }

  template<typename T>
  inline T_PointerType impl_get_sandboxed_pointer(const void* p) const
  {
    if constexpr (std::is_function_v<std::remove_pointer_t<T>>) {
      // p is a pointer to a function internal to the lucet module
      // we need to either
      // 1) find the indirect function slot this is registered and return the
      // slot number. For this we need to scan the full indirect function table,
      // not just the portion we have reserved for callbacks.
      // 2) in the scenario this function has not ever been listed as an
      // indirect function, we need to register this like a normal callback.
      // However, unlike callbacks, we will not require the user to unregister
      // this. Instead, this permenantly takes up a callback slot.
      LucetFunctionTable functionPointerTable =
        lucet_get_function_pointer_table(sandbox);
      std::lock_guard<std::mutex> lock(callback_table_mutex);

      // Scenario 1 described above
      ssize_t empty_slot = -1;
      for (size_t i = 0; i < functionPointerTable.length; i++) {
        if (functionPointerTable.data[i].rf == reinterpret_cast<uintptr_t>(p)) {
          return static_cast<T_PointerType>(i);
        } else if (functionPointerTable.data[i].rf == 0 && empty_slot == -1) {
          // found an empty slot. Save it, as we may use it later.
          empty_slot = i;
        }
      }

      // Scenario 2 described above
      detail::dynamic_check(
        empty_slot != -1,
        "Could not find an empty slot in sandbox function table. This would "
        "happen if you have registered too many callbacks, or unsandboxed "
        "too many function pointers. You can file a bug if you want to "
        "increase the maximum allowed callbacks or unsadnboxed functions "
        "pointers");
      T dummy = nullptr;
      int32_t type_index = get_lucet_type_index(dummy);
      functionPointerTable.data[empty_slot].ty = type_index;
      functionPointerTable.data[empty_slot].rf = reinterpret_cast<uintptr_t>(p);
      return empty_slot;

    } else {
      return static_cast<T_PointerType>(reinterpret_cast<uintptr_t>(p));
    }
  }

  template<typename T>
  static inline void* impl_get_unsandboxed_pointer_no_ctx(
    T_PointerType p,
    const void* example_unsandboxed_ptr,
    rlbox_lucet_sandbox* (*expensive_sandbox_finder)(
      const void* example_unsandboxed_ptr))
  {
    if constexpr (std::is_function_v<std::remove_pointer_t<T>>) {
      // swizzling function pointers needs access to the function pointer tables
      // and thus cannot be done without context
      auto sandbox = expensive_sandbox_finder(example_unsandboxed_ptr);
      return sandbox->impl_get_unsandboxed_pointer<T>(p);
    } else {
      // grab the memory base from the example_unsandboxed_ptr
      uintptr_t heap_base_mask =
        std::numeric_limits<uintptr_t>::max() &
        ~(static_cast<uintptr_t>(std::numeric_limits<T_PointerType>::max()));
      uintptr_t computed_heap_base =
        reinterpret_cast<uintptr_t>(example_unsandboxed_ptr) & heap_base_mask;
      uintptr_t ret = computed_heap_base | p;
      return reinterpret_cast<void*>(ret);
    }
  }

  template<typename T>
  static inline T_PointerType impl_get_sandboxed_pointer_no_ctx(
    const void* p,
    const void* example_unsandboxed_ptr,
    rlbox_lucet_sandbox* (*expensive_sandbox_finder)(
      const void* example_unsandboxed_ptr))
  {
    if constexpr (std::is_function_v<std::remove_pointer_t<T>>) {
      // swizzling function pointers needs access to the function pointer tables
      // and thus cannot be done without context
      auto sandbox = expensive_sandbox_finder(example_unsandboxed_ptr);
      return sandbox->impl_get_sandboxed_pointer<T>(p);
    } else {
      // Just clear the memory base to leave the offset
      RLBOX_LUCET_UNUSED(example_unsandboxed_ptr);
      uintptr_t ret = reinterpret_cast<uintptr_t>(p) &
                      std::numeric_limits<T_PointerType>::max();
      return static_cast<T_PointerType>(ret);
    }
  }

  static inline bool impl_is_in_same_sandbox(const void* p1, const void* p2)
  {
    uintptr_t heap_base_mask = std::numeric_limits<uintptr_t>::max() &
                               ~(std::numeric_limits<T_PointerType>::max());
    return (reinterpret_cast<uintptr_t>(p1) & heap_base_mask) ==
           (reinterpret_cast<uintptr_t>(p2) & heap_base_mask);
  }

  inline bool impl_is_pointer_in_sandbox_memory(const void* p)
  {
    size_t length = impl_get_total_memory();
    uintptr_t p_val = reinterpret_cast<uintptr_t>(p);
    return p_val >= heap_base && p_val < (heap_base + length);
  }

  inline bool impl_is_pointer_in_app_memory(const void* p)
  {
    return !(impl_is_pointer_in_sandbox_memory(p));
  }

  inline size_t impl_get_total_memory() { return lucet_get_heap_size(sandbox); }

  inline void* impl_get_memory_location()
  {
    return lucet_get_heap_base(sandbox);
  }

  void* impl_lookup_symbol(const char* func_name)
  {
    return lucet_lookup_function(sandbox, func_name);
  }

  template<typename T, typename T_Converted, typename... T_Args>
  auto impl_invoke_with_func_ptr(T_Converted* func_ptr, T_Args&&... params)
  {
#ifdef RLBOX_EMBEDDER_PROVIDES_TLS_STATIC_VARIABLES
    auto& thread_data = *get_rlbox_lucet_sandbox_thread_data();
#endif
    thread_data.sandbox = this;
    lucet_set_curr_instance(sandbox);

    // WASM functions are mangled in the following manner
    // 1. All primitive types are left as is and follow an LP32 machine model
    // (as opposed to the possibly 64-bit application)
    // 2. All pointers are changed to u32 types
    // 3. Returned class are returned as an out parameter before the actual
    // function parameters
    // 4. All class parameters are passed as pointers (u32 types)
    // 5. The heap address is passed in as the first argument to the function
    //
    // RLBox accounts for the first 2 differences in T_Converted type, but we
    // need to handle the rest

    // Handle point 3
    using T_Ret = lucet_detail::return_argument<T_Converted>;
    if constexpr (std::is_class_v<T_Ret>) {
      using T_Conv1 = lucet_detail::change_return_type<T_Converted, void>;
      using T_Conv2 = lucet_detail::prepend_arg_type<T_Conv1, T_PointerType>;
      auto func_ptr_conv =
        reinterpret_cast<T_Conv2*>(reinterpret_cast<uintptr_t>(func_ptr));
      ensure_return_slot_size(sizeof(T_Ret));
      impl_invoke_with_func_ptr<T>(func_ptr_conv, return_slot, params...);

      auto ptr = reinterpret_cast<T_Ret*>(
        impl_get_unsandboxed_pointer<T_Ret*>(return_slot));
      T_Ret ret = *ptr;
      return ret;
    }

    // Handle point 4
    constexpr size_t alloc_length = [&] {
      if constexpr (sizeof...(params) > 0) {
        return ((std::is_class_v<T_Args> ? 1 : 0) + ...);
      } else {
        return 0;
      }
    }();

    // 0 arg functions create 0 length arrays which is not allowed
    T_PointerType allocations_buff[alloc_length == 0 ? 1 : alloc_length];
    T_PointerType* allocations = allocations_buff;

    auto serialize_class_arg =
      [&](auto arg) -> std::conditional_t<std::is_class_v<decltype(arg)>,
                                          T_PointerType,
                                          decltype(arg)> {
      using T_Arg = decltype(arg);
      if constexpr (std::is_class_v<T_Arg>) {
        auto slot = impl_malloc_in_sandbox(sizeof(T_Arg));
        auto ptr =
          reinterpret_cast<T_Arg*>(impl_get_unsandboxed_pointer<T_Arg*>(slot));
        *ptr = arg;
        allocations[0] = slot;
        allocations++;
        return slot;
      } else {
        return arg;
      }
    };

    // 0 arg functions don't use serialize
    RLBOX_LUCET_UNUSED(serialize_class_arg);

    using T_ConvNoClass =
      lucet_detail::change_class_arg_types<T_Converted, T_PointerType>;

    // Handle Point 5
    using T_ConvHeap = lucet_detail::prepend_arg_type<T_ConvNoClass, uint64_t>;

    // Function invocation
    auto func_ptr_conv =
      reinterpret_cast<T_ConvHeap*>(reinterpret_cast<uintptr_t>(func_ptr));

    using T_NoVoidRet =
      std::conditional_t<std::is_void_v<T_Ret>, uint32_t, T_Ret>;
    T_NoVoidRet ret;

    if constexpr (std::is_void_v<T_Ret>) {
      RLBOX_LUCET_UNUSED(ret);
      func_ptr_conv(heap_base, serialize_class_arg(params)...);
    } else {
      ret = func_ptr_conv(heap_base, serialize_class_arg(params)...);
    }

    for (size_t i = 0; i < alloc_length; i++) {
      impl_free_in_sandbox(allocations_buff[i]);
    }

    if constexpr (!std::is_void_v<T_Ret>) {
      return ret;
    }
  }

  inline T_PointerType impl_malloc_in_sandbox(size_t size)
  {
    detail::dynamic_check(size <= std::numeric_limits<uint32_t>::max(),
                          "Attempting to malloc more than the heap size");
    using T_Func = void*(size_t);
    using T_Converted = T_PointerType(uint32_t);
    T_PointerType ret = impl_invoke_with_func_ptr<T_Func, T_Converted>(
      reinterpret_cast<T_Converted*>(malloc_index),
      static_cast<uint32_t>(size));
    return ret;
  }

  inline void impl_free_in_sandbox(T_PointerType p)
  {
    using T_Func = void(void*);
    using T_Converted = void(T_PointerType);
    impl_invoke_with_func_ptr<T_Func, T_Converted>(
      reinterpret_cast<T_Converted*>(free_index), p);
  }

  template<typename T_Ret, typename... T_Args>
  inline T_PointerType impl_register_callback(void* key, void* callback)
  {
    int32_t type_index = get_lucet_type_index<T_Ret, T_Args...>();

    detail::dynamic_check(
      type_index != -1,
      "Could not find lucet type for callback signature. This can "
      "happen if you tried to register a callback whose signature "
      "does not correspond to any callbacks used in the library.");

    bool found = false;
    uint32_t found_loc = 0;
    uint32_t slot_number = 0;

    {
      std::lock_guard<std::mutex> lock(callback_table_mutex);

      // need a compile time for loop as we we need I to be a compile time value
      // this is because we are setting the I'th callback ineterceptor
      lucet_detail::compile_time_for<MAX_CALLBACKS>([&](auto I) {
        constexpr auto i = I.value;
        if (!found && callback_slots->elements[i]->rf == 0) {
          found = true;
          found_loc = i;
          slot_number = callback_slots->slot_number[i];

          void* chosen_interceptor;
          if constexpr (std::is_class_v<T_Ret>) {
            chosen_interceptor = reinterpret_cast<void*>(
              callback_interceptor_promoted<i, T_Ret, T_Args...>);
          } else {
            chosen_interceptor = reinterpret_cast<void*>(
              callback_interceptor<i, T_Ret, T_Args...>);
          }
          callback_slots->elements[i]->ty = type_index;
          callback_slots->elements[i]->rf =
            reinterpret_cast<uintptr_t>(chosen_interceptor);
        }
      });
    }

    detail::dynamic_check(
      found,
      "Could not find an empty slot in sandbox function table. This would "
      "happen if you have registered too many callbacks, or unsandboxed "
      "too many function pointers. You can file a bug if you want to "
      "increase the maximum allowed callbacks or unsadnboxed functions "
      "pointers");

    {
      RLBOX_ACQUIRE_UNIQUE_GUARD(lock, callback_mutex);
      callback_unique_keys[found_loc] = key;
      callbacks[found_loc] = callback;
      callback_slot_assignment[found_loc] = slot_number;
    }

    return static_cast<T_PointerType>(slot_number);
  }

  static inline std::pair<rlbox_lucet_sandbox*, void*>
  impl_get_executed_callback_sandbox_and_key()
  {
#ifdef RLBOX_EMBEDDER_PROVIDES_TLS_STATIC_VARIABLES
    auto& thread_data = *get_rlbox_lucet_sandbox_thread_data();
#endif
    auto sandbox = thread_data.sandbox;
    auto callback_num = thread_data.last_callback_invoked;
    void* key = sandbox->callback_unique_keys[callback_num];
    return std::make_pair(sandbox, key);
  }

  template<typename T_Ret, typename... T_Args>
  inline void impl_unregister_callback(void* key)
  {
    bool found = false;
    uint32_t i = 0;
    {
      RLBOX_ACQUIRE_UNIQUE_GUARD(lock, callback_mutex);
      for (; i < MAX_CALLBACKS; i++) {
        if (callback_unique_keys[i] == key) {
          callback_unique_keys[i] = nullptr;
          callbacks[i] = nullptr;
          callback_slot_assignment[i] = 0;
          found = true;
          break;
        }
      }
    }

    detail::dynamic_check(
      found, "Internal error: Could not find callback to unregister");

    std::lock_guard<std::mutex> shared_lock(callback_table_mutex);
    callback_slots->elements[i]->rf = 0;
    return;
  }
};

} // namespace rlbox