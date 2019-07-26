#pragma once

#include <array>
#include <cstring>
#include <memory>
#include <type_traits>
#include <utility>

#include "rlbox_conversion.hpp"
#include "rlbox_helpers.hpp"
#include "rlbox_policy_types.hpp"
#include "rlbox_range.hpp"
#include "rlbox_sandbox.hpp"
#include "rlbox_stdlib.hpp"
#include "rlbox_struct_support.hpp"
#include "rlbox_type_traits.hpp"
#include "rlbox_types.hpp"
#include "rlbox_unwrap.hpp"
#include "rlbox_wrapper_traits.hpp"

namespace rlbox {

template<template<typename, typename> typename T_Wrap,
         typename T,
         typename T_Sbx>
class tainted_base_impl
{
  KEEP_CLASSES_FRIENDLY
  KEEP_CAST_FRIENDLY

private:
  inline auto& impl() { return *static_cast<T_Wrap<T, T_Sbx>*>(this); }
  inline auto& impl() const
  {
    return *static_cast<const T_Wrap<T, T_Sbx>*>(this);
  }

public:
  inline auto UNSAFE_unverified() { return impl().get_raw_value(); }
  inline auto UNSAFE_sandboxed() { return impl().get_raw_sandbox_value(); }
  inline auto UNSAFE_unverified() const { return impl().get_raw_value(); }
  inline auto UNSAFE_sandboxed() const
  {
    return impl().get_raw_sandbox_value();
  }

#define BinaryOpValAndPtr(opSymbol)                                            \
  template<typename T_Rhs>                                                     \
  inline auto operator opSymbol(T_Rhs&& rhs)                                   \
  {                                                                            \
    static_assert(detail::is_basic_type_v<T>,                                  \
                  "Operator " #opSymbol                                        \
                  " only supported for primitive and pointer types");          \
                                                                               \
    auto raw_rhs = detail::unwrap_value(rhs);                                  \
    static_assert(std::is_integral_v<decltype(raw_rhs)>,                       \
                  "Can only operate on numeric types");                        \
                                                                               \
    if constexpr (std::is_pointer_v<T>) {                                      \
      auto ptr = impl().get_raw_value();                                       \
      detail::dynamic_check(ptr != nullptr,                                    \
                            "Pointer arithmetic on a null pointer");           \
      /* increment the target by size of the data structure */                 \
      auto target =                                                            \
        reinterpret_cast<uintptr_t>(ptr) opSymbol raw_rhs * sizeof(*impl());   \
      auto no_overflow = rlbox_sandbox<T_Sbx>::is_in_same_sandbox(             \
        reinterpret_cast<const void*>(ptr),                                    \
        reinterpret_cast<const void*>(target));                                \
      detail::dynamic_check(                                                   \
        no_overflow,                                                           \
        "Pointer arithmetic overflowed a pointer beyond sandbox memory");      \
                                                                               \
      return tainted<T, T_Sbx>::internal_factory(reinterpret_cast<T>(target)); \
    } else {                                                                   \
      auto raw = impl().get_raw_value();                                       \
      auto ret = raw opSymbol raw_rhs;                                         \
      using T_Ret = decltype(ret);                                             \
      return tainted<T_Ret, T_Sbx>::internal_factory(ret);                     \
    }                                                                          \
  }                                                                            \
  RLBOX_REQUIRE_SEMI_COLON

  BinaryOpValAndPtr(+);
  BinaryOpValAndPtr(-);

#undef BinaryOpValAndPtr

#define BinaryOp(opSymbol)                                                     \
  template<typename T_Rhs>                                                     \
  inline auto operator opSymbol(T_Rhs&& rhs)                                   \
  {                                                                            \
    static_assert(detail::is_basic_type_v<T>,                                  \
                  "Operator " #opSymbol                                        \
                  " only supported for primitive and pointer types");          \
                                                                               \
    auto raw = impl().get_raw_value();                                         \
    auto raw_rhs = detail::unwrap_value(rhs);                                  \
    static_assert(std::is_integral_v<decltype(raw_rhs)>,                       \
                  "Can only operate on numeric types");                        \
                                                                               \
    auto ret = raw opSymbol raw_rhs;                                           \
    using T_Ret = decltype(ret);                                               \
    return tainted<T_Ret, T_Sbx>::internal_factory(ret);                       \
  }                                                                            \
  RLBOX_REQUIRE_SEMI_COLON

  BinaryOp(*);
  BinaryOp(/);
  BinaryOp(%);
  BinaryOp(^);
  BinaryOp(&);
  BinaryOp(|);
  BinaryOp(<<);
  BinaryOp(>>);

#undef BinaryOp

#define UnaryOp(opSymbol)                                                      \
  inline auto operator opSymbol()                                              \
  {                                                                            \
    static_assert(detail::is_fundamental_or_enum_v<T>,                         \
                  "Operator " #opSymbol " only supported for primitive");      \
                                                                               \
    auto raw = impl().get_raw_value();                                         \
    auto ret = opSymbol raw;                                                   \
    using T_Ret = decltype(ret);                                               \
    return tainted<T_Ret, T_Sbx>::internal_factory(ret);                       \
  }                                                                            \
  RLBOX_REQUIRE_SEMI_COLON

  UnaryOp(-);
  UnaryOp(~);

#undef UnaryOp

private:
  using T_OpSubscriptArrRet = std::conditional_t<
    std::is_pointer_v<T>,
    tainted_volatile<detail::dereference_result_t<T>, T_Sbx>, // is_pointer
    T_Wrap<detail::dereference_result_t<T>, T_Sbx>            // is_array
    >;

public:
  template<typename T_Rhs>
  inline const T_OpSubscriptArrRet& operator[](T_Rhs&& rhs) const
  {
    static_assert(std::is_pointer_v<T> || detail::is_c_or_std_array_v<T>,
                  "Operator [] supports pointers and arrays only");

    auto raw_rhs = detail::unwrap_value(rhs);
    static_assert(std::is_integral_v<decltype(raw_rhs)>,
                  "Can only index with numeric types");

    if constexpr (std::is_pointer_v<T>) {
      auto ptr = this->impl().get_raw_value();

      // increment the target by size of the data structure
      auto target =
        reinterpret_cast<uintptr_t>(ptr) + raw_rhs * sizeof(*this->impl());
      auto no_overflow = rlbox_sandbox<T_Sbx>::is_in_same_sandbox(
        ptr, reinterpret_cast<const void*>(target));
      detail::dynamic_check(
        no_overflow,
        "Pointer arithmetic overflowed a pointer beyond sandbox memory");

      auto target_wrap = tainted<const T, T_Sbx>::internal_factory(
        reinterpret_cast<const T>(target));
      return *target_wrap;
    } else {
      using T_Rhs_Unsigned = std::make_unsigned_t<decltype(raw_rhs)>;
      detail::dynamic_check(
        raw_rhs >= 0 && static_cast<T_Rhs_Unsigned>(raw_rhs) <
                          std::extent_v<detail::std_array_to_c_arr_t<T>, 0>,
        "Static array indexing overflow");

      const void* target_ptr;
      if constexpr (detail::rlbox_is_tainted_v<T_Wrap<T, T_Sbx>>) {
        auto& data_ref = impl().get_raw_value_ref();
        target_ptr = &(data_ref[raw_rhs]);
      } else {
        auto& data_ref = impl().get_sandbox_value_ref();
        auto target_ptr_vol = &(data_ref[raw_rhs]);
        // target_ptr is a volatile... remove this.
        // Safe as we will return a tainted_volatile if this is the case
        target_ptr = detail::remove_volatile_from_ptr_cast(target_ptr_vol);
      }

      using T_Target = const T_Wrap<detail::dereference_result_t<T>, T_Sbx>;
      auto wrapped_target_ptr = reinterpret_cast<T_Target*>(target_ptr);
      return *wrapped_target_ptr;
    }
  }

  template<typename T_Rhs>
  inline T_OpSubscriptArrRet& operator[](T_Rhs&& rhs)
  {
    rlbox_detail_forward_to_const_a(operator[], T_OpSubscriptArrRet&, rhs);
  }

private:
  using T_OpDerefRet = tainted_volatile<std::remove_pointer_t<T>, T_Sbx>;

public:
  inline T_OpDerefRet& operator*() const
  {
    static_assert(std::is_pointer_v<T>, "Operator * only allowed on pointers");
    auto ret_ptr_const =
      reinterpret_cast<const T_OpDerefRet*>(impl().get_raw_value());
    // Safe - If T_OpDerefRet is not a const ptr, this is trivially safe
    //        If T_OpDerefRet is a const ptr, then the const is captured
    //        inside the wrapper
    auto ret_ptr = const_cast<T_OpDerefRet*>(ret_ptr_const);
    return *ret_ptr;
  }

  inline T_OpDerefRet& operator*()
  {
    rlbox_detail_forward_to_const(operator*, T_OpDerefRet&);
  }

  // We need to implement the -> operator even though T is not a struct
  // So that we can support code patterns such as the below
  // tainted<T*> a;
  // a->UNSAFE_unverified();
  inline auto operator-> () const
  {
    static_assert(std::is_pointer_v<T>,
                  "Operator -> only supported for pointer types");
    auto ret = impl().get_raw_value();
    using T_Ret = std::remove_pointer_t<T>;
    using T_RetWrap = const tainted_volatile<T_Ret, T_Sbx>;
    return reinterpret_cast<T_RetWrap*>(ret);
  }

  inline auto operator-> ()
  {
    using T_Ret = tainted_volatile<std::remove_pointer_t<T>, T_Sbx>*;
    rlbox_detail_forward_to_const(operator->, T_Ret);
  }

  // The verifier should have the following signature for the given types
  // If tainted type is simple such as int
  //      using T_Func = T_Ret(*)(int)
  // If tainted type is a pointer to a simple type such as int*
  //      using T_Func = T_Ret(*)(unique_ptr<int>)
  // If tainted type is a pointer to class such as Foo*
  //      using T_Func = T_Ret(*)(unique_ptr<Foo>)
  // If tainted type is an array such as int[4]
  //      using T_Func = T_Ret(*)(std::array<int, 4>)
  // For completeness, if tainted_type is a class such as Foo, the
  //  copy_and_verify is implemented in rlbox_struct_support.hpp. The verifier
  //  should be
  //      using T_Func = T_Ret(*)(tainted<Foo>)
  //
  // In the above signatures T_Ret is not constrained, and can be anything the
  // caller chooses.
  template<typename T_Func>
  inline auto copy_and_verify(T_Func verifier) const
  {
    using T_Deref = std::remove_cv_t<std::remove_pointer_t<T>>;

    if_constexpr_named(cond1, detail::is_fundamental_or_enum_v<T>)
    {
      auto val = impl().get_raw_value();
      return verifier(val);
    }
    else if_constexpr_named(
      cond2, detail::is_one_level_ptr_v<T> && !std::is_class_v<T_Deref>)
    {
      static_assert(!std::is_void_v<T_Deref>,
                    "copy_and_verify not recommended for void* as it could "
                    "lead to some anti-patterns in verifiers. Cast it to a "
                    "different tainted pointer with sandbox_reinterpret_cast "
                    "and then call copy_and_verify. Alternately, you can use "
                    "the UNSAFE_unverified API to do this without casting.");

      auto val = impl().get_raw_value();
      if (val == nullptr) {
        return verifier(nullptr);
      } else {
        // Important to assign to a local variable (i.e. make a copy)
        // Else, for tainted_volatile, this will allow a
        // time-of-check-time-of-use attack
        auto val_copy = std::make_unique<T_Deref>();
        *val_copy = *val;
        return verifier(std::move(val_copy));
      }
    }
    else if_constexpr_named(
      cond3, detail::is_one_level_ptr_v<T> && std::is_class_v<T_Deref>)
    {
      auto val_copy = std::make_unique<tainted<T_Deref, T_Sbx>>(*impl());
      return verifier(std::move(val_copy));
    }
    else if_constexpr_named(cond4, std::is_array_v<T>)
    {
      static_assert(
        detail::is_fundamental_or_enum_v<std::remove_all_extents_t<T>>,
        "copy_and_verify on arrays is only safe for fundamental or enum types. "
        "For arrays of other types, apply copy_and_verify on each element "
        "individually --- a[i].copy_and_verify(...)");

      auto copy = impl().get_raw_value();
      return verifier(copy);
    }
    else
    {
      auto unknownCase = !(cond1 || cond2 || cond3 || cond4);
      rlbox_detail_static_fail_because(
        unknownCase,
        "copy_and_verify not supported for this type as it may be unsafe");
    }
  }

private:
  using T_CopyAndVerifyRangeEl =
    detail::valid_array_el_t<std::remove_cv_t<std::remove_pointer_t<T>>>;

  // Template needed to ensure that function isn't instantiated for unsupported
  // types like function pointers which causes compile errors...
  template<typename T2 = T>
  inline std::unique_ptr<T_CopyAndVerifyRangeEl[]> copy_and_verify_range_helper(
    std::size_t count) const
  {
    static_assert(std::is_pointer_v<T>);
    static_assert(detail::is_fundamental_or_enum_v<T_CopyAndVerifyRangeEl>);

    detail::dynamic_check(
      count != 0,
      "Called copy_and_verify_range/copy_and_verify_string with count 0");

    auto start = reinterpret_cast<const void*>(impl().get_raw_value());
    if (start == nullptr) {
      return nullptr;
    }

    detail::check_range_doesnt_cross_app_sbx_boundary<T_Sbx>(
      start, count * sizeof(T_CopyAndVerifyRangeEl));

    auto target = std::make_unique<T_CopyAndVerifyRangeEl[]>(count);

    for (size_t i = 0; i < count; i++) {
      auto p_src_i_tainted = &(impl()[i]);
      auto p_src_i = p_src_i_tainted.get_raw_value();
      detail::convert_type_fundamental_or_array(target[i], *p_src_i);
    }

    return std::move(target);
  }

public:
  // The verifier should have the following signature.
  // If the tainted type is int*
  //      using T_Func = T_Ret(*)(unique_ptr<int[]>)
  // T_Ret is not constrained, and can be anything the caller chooses.
  template<typename T_Func>
  inline auto copy_and_verify_range(T_Func verifier, std::size_t count) const
  {
    static_assert(std::is_pointer_v<T>,
                  "Can only call copy_and_verify_range on pointers");

    static_assert(
      detail::is_fundamental_or_enum_v<T_CopyAndVerifyRangeEl>,
      "copy_and_verify_range is only safe for ranges of "
      "fundamental or enum types. For other types, call "
      "copy_and_verify on each element --- a[i].copy_and_verify(...)");

    std::unique_ptr<T_CopyAndVerifyRangeEl[]> target =
      copy_and_verify_range_helper(count);
    return verifier(std::move(target));
  }

  // The verifier should have the following signature.
  //      using T_Func = T_Ret(*)(unique_ptr<char[]>)
  // T_Ret is not constrained, and can be anything the caller chooses.
  template<typename T_Func>
  inline auto copy_and_verify_string(T_Func verifier) const
  {
    static_assert(std::is_pointer_v<T>,
                  "Can only call copy_and_verify_string on pointers");

    static_assert(std::is_same_v<char, T_CopyAndVerifyRangeEl>,
                  "copy_and_verify_string only allows char*");

    auto start = impl().get_raw_value();
    if (start == nullptr) {
      return verifier(nullptr);
    }

    // it is safe to run strlen on a tainted<string> as worst case, the string
    // does not have a null and we try to copy all the memory out of the sandbox
    // however, copy_and_verify_range ensures that we never copy memory outsider
    // the range
    auto str_len = std::strlen(start) + 1;
    std::unique_ptr<T_CopyAndVerifyRangeEl[]> target =
      copy_and_verify_range_helper(str_len);

    // ensure the string has a trailing null
    target[str_len - 1] = '\0';

    return verifier(std::move(target));
  }
};

namespace tainted_detail {
  template<typename T, typename T_Sbx>
  using tainted_repr_t = detail::c_to_std_array_t<T>;

  template<typename T, typename T_Sbx>
  using tainted_vol_repr_t =
    detail::c_to_std_array_t<std::add_volatile_t<typename rlbox_sandbox<
      T_Sbx>::template convert_to_sandbox_equivalent_nonclass_t<T>>>;
}

template<typename T, typename T_Sbx>
class tainted : public tainted_base_impl<tainted, T, T_Sbx>
{
  KEEP_CLASSES_FRIENDLY
  KEEP_CAST_FRIENDLY

  // Classes recieve their own specialization
  static_assert(
    !std::is_class_v<T>,
    "Missing specialization for class T. This error occurs for one "
    "of 2 reasons.\n"
    "  1) Make sure you have include a call rlbox_load_structs_from_library "
    "for this library.\n"
    "  2) Make sure you run (re-run) the struct-dump tool to list "
    "all structs in use by your program.\n");

  static_assert(
    detail::is_basic_type_v<T> || std::is_array_v<T>,
    "Tainted types only support fundamental, enum, pointer, array and struct "
    "types. Please file a bug if more support is needed.");

private:
  using T_ClassBase = tainted_base_impl<tainted, T, T_Sbx>;
  using T_AppType = tainted_detail::tainted_repr_t<T, T_Sbx>;
  using T_SandboxedType = tainted_detail::tainted_vol_repr_t<T, T_Sbx>;
  T_AppType data;

  inline auto& get_raw_value_ref() noexcept { return data; }
  inline auto& get_raw_value_ref() const noexcept { return data; }

  inline std::remove_cv_t<T_AppType> get_raw_value() const noexcept
  {
    return data;
  }

  inline std::remove_cv_t<T_SandboxedType> get_raw_sandbox_value() const
  {
    std::remove_cv_t<T_SandboxedType> ret;
    detail::convert_type_non_class<T_Sbx,
                                   detail::adjust_type_direction::TO_SANDBOX>(
      ret, data);
    return ret;
  };

  inline std::remove_cv_t<T_AppType> get_raw_value() noexcept
  {
    rlbox_detail_forward_to_const(get_raw_value, std::remove_cv_t<T_AppType>);
  }

  inline std::remove_cv_t<T_SandboxedType> get_raw_sandbox_value()
  {
    rlbox_detail_forward_to_const(get_raw_sandbox_value,
                                  std::remove_cv_t<T_SandboxedType>);
  };

  // Initializing with a pointer is dangerous and permitted only internally
  template<typename T2 = T, RLBOX_ENABLE_IF(std::is_pointer_v<T2>)>
  tainted(T2 val, const void* /* internal_tag */)
    : data(val)
  {
    // Sanity check
    static_assert(std::is_pointer_v<T>);
  }

  template<typename T_Rhs>
  static inline tainted<T, T_Sbx> internal_factory(T_Rhs&& rhs)
  {
    if constexpr (std::is_pointer_v<std::remove_reference_t<T_Rhs>>) {
      const void* internal_tag = nullptr;
      return tainted(std::forward<T_Rhs>(rhs), internal_tag);
    } else {
      return tainted(std::forward<T_Rhs>(rhs));
    }
  }

public:
  tainted() = default;
  tainted(const tainted<T, T_Sbx>& p) = default;

  tainted(const tainted_volatile<T, T_Sbx>& p)
  {
    // Need to construct an example_unsandboxed_ptr for pointers or arrays of
    // pointers. Since tainted_volatile is the type of data in sandbox memory,
    // the address of data (&data) refers to a location in sandbox memory and
    // can thus be the example_unsandboxed_ptr
    const volatile void* p_data_ref = &p.get_sandbox_value_ref();
    const void* example_unsandboxed_ptr = const_cast<const void*>(p_data_ref);
    detail::convert_type_non_class<
      T_Sbx,
      detail::adjust_type_direction::TO_APPLICATION>(
      get_raw_value_ref(), p.get_sandbox_value_ref(), example_unsandboxed_ptr);
  }

  // Initializing with a pointer is dangerous and permitted only internally
  template<typename T2 = T, RLBOX_ENABLE_IF(std::is_pointer_v<T2>)>
  tainted(T2 val)
    : data(val)
  {
    rlbox_detail_static_fail_because(
      std::is_pointer_v<T2>,
      "Assignment of pointers is not safe as it could\n "
      "1) Leak pointers from the appliction to the sandbox which may break "
      "ASLR\n "
      "2) Pass inaccessible pointers to the sandbox leading to crash\n "
      "3) Break sandboxes that require pointers to be swizzled first\n "
      "\n "
      "Instead, if you want to pass in a pointer, do one of the following\n "
      "1) Allocate with malloc_in_sandbox, and pass in a tainted pointer\n "
      "2) For pointers that point to functions in the application, register "
      "with sandbox.register_callback(\"foo\"), and pass in the registered "
      "value\n "
      "3) For pointers that point to functions in the sandbox, get the "
      "address with sandbox_function_address(sandbox, foo), and pass in the "
      "address\n "
      "4) For raw pointers, use assign_raw_pointer which performs required "
      "safety checks\n ");
  }

  tainted(
    const sandbox_callback<
      detail::function_ptr_t<T> // Need to ensure we never generate code that
                                // creates a sandbox_callback of a non function
      ,
      T_Sbx>&)
  {
    rlbox_detail_static_fail_because(
      detail::true_v<T>,
      "RLBox does not support assigning sandbox_callback values to tainted "
      "types (i.e. types that live in application memory).\n"
      "If you still want to do this, consider changing your code to store the "
      "value in sandbox memory as follows. Convert\n\n"
      "sandbox_callback<T_Func, Sbx> cb = ...;\n"
      "tainted<T_Func, Sbx> foo = cb;\n\n"
      "to\n\n"
      "tainted<T_Func*, Sbx> foo_ptr = sandbox.malloc_in_sandbox<T_Func*>();\n"
      "*foo_ptr = cb;\n\n"
      "This would keep the assignment in sandbox memory");
  }

  tainted(
    const sandbox_function<
      detail::function_ptr_t<T> // Need to ensure we never generate code that
                                // creates a sandbox_function of a non function
      ,
      T_Sbx>&)
  {
    rlbox_detail_static_fail_because(
      detail::true_v<T>,
      "RLBox does not support assigning sandbox_function values to tainted "
      "types (i.e. types that live in application memory).\n"
      "If you still want to do this, consider changing your code to store the "
      "value in sandbox memory as follows. Convert\n\n"
      "sandbox_function<T_Func, Sbx> cb = ...;\n"
      "tainted<T_Func, Sbx> foo = cb;\n\n"
      "to\n\n"
      "tainted<T_Func*, Sbx> foo_ptr = sandbox.malloc_in_sandbox<T_Func*>();\n"
      "*foo_ptr = cb;\n\n"
      "This would keep the assignment in sandbox memory");
  }

  tainted(const std::nullptr_t& arg)
    : data(arg)
  {
    static_assert(std::is_pointer_v<T>);
  }

  // We explicitly disable this constructor if it has one of the signatures
  // above, so that we give the above constructors a higher priority. We only
  // allow this for fundamental types as this is potentially unsafe for pointers
  // and structs
  template<typename T_Arg,
           RLBOX_ENABLE_IF(
             !detail::rlbox_is_wrapper_v<std::remove_reference_t<T_Arg>> &&
             detail::is_fundamental_or_enum_v<T> &&
             detail::is_fundamental_or_enum_v<std::remove_reference_t<T_Arg>>)>
  tainted(T_Arg&& arg)
    : data(std::forward<T_Arg>(arg))
  {}

  template<typename T_Rhs>
  void assign_raw_pointer(rlbox_sandbox<T_Sbx>& sandbox, T_Rhs val)
  {
    static_assert(std::is_pointer_v<T_Rhs>, "Must be a pointer");
    static_assert(std::is_assignable_v<T&, T_Rhs>,
                  "Should assign pointers of compatible types.");
    // Maybe a function pointer, so we need to cast
    const void* cast_val = reinterpret_cast<const void*>(val);
    bool safe = sandbox.is_pointer_in_sandbox_memory(cast_val);
    detail::dynamic_check(
      safe,
      "Tried to assign a pointer that is not in the sandbox.\n "
      "This is not safe as it could\n "
      "1) Leak pointers from the appliction to the sandbox which may break "
      "ASLR\n "
      "2) Pass inaccessible pointers to the sandbox leading to crash\n "
      "3) Break sandboxes that require pointers to be swizzled first\n "
      "\n "
      "Instead, if you want to pass in a pointer, do one of the following\n "
      "1) Allocate with malloc_in_sandbox, and pass in a tainted pointer\n "
      "2) For pointers that point to functions in the application, register "
      "with sandbox.register_callback(\"foo\"), and pass in the registered "
      "value\n "
      "3) For pointers that point to functions in the sandbox, get the "
      "address with sandbox_function_address(sandbox, foo), and pass in the "
      "address\n ");
    data = val;
  }

  inline tainted_opaque<T, T_Sbx> to_opaque()
  {
    return *reinterpret_cast<tainted_opaque<T, T_Sbx>*>(this);
  }

  // In general comparison operators are unsafe.
  // However comparing tainted with nullptr is fine because
  // 1) tainted values are in application memory and thus cannot change the
  // value after comparision
  // 2) Checking that a pointer is null doesn't "really" taint the result as
  // the result is always safe
  template<typename T_Rhs>
  inline bool operator==(T_Rhs&& arg) const
  {
    if_constexpr_named(
      cond1,
      !std::is_same_v<std::remove_const_t<std::remove_reference_t<T_Rhs>>,
                      std::nullptr_t>)
    {
      rlbox_detail_static_fail_because(
        cond1,
        "Only comparisons to nullptr are allowed. All other comparisons to "
        "tainted types create many antipatterns. Rather than comparing tainted "
        "values directly, unwrap the values with the copy_and_verify API and "
        "then perform the comparisons.");
    }
    else if_constexpr_named(cond2, std::is_pointer_v<T>)
    {
      return get_raw_value() == arg;
    }
    else
    {
      rlbox_detail_static_fail_because(
        !cond2, "Comparisons to nullptr only permitted for pointer types");
    }
  }

  template<typename T_Rhs>
  inline bool operator!=(T_Rhs&& arg) const
  {
    if_constexpr_named(
      cond1,
      !std::is_same_v<std::remove_const_t<std::remove_reference_t<T_Rhs>>,
                      std::nullptr_t>)
    {
      rlbox_detail_static_fail_because(
        cond1,
        "Only comparisons to nullptr are allowed. All other comparisons to "
        "tainted types create many antipatterns. Rather than comparing tainted "
        "values directly, unwrap the values with the copy_and_verify API and "
        "then perform the comparisons.");
    }
    else if_constexpr_named(cond2, std::is_pointer_v<T>)
    {
      return get_raw_value() != arg;
    }
    else
    {
      rlbox_detail_static_fail_because(
        !cond2, "Comparisons to nullptr only permitted for pointer types");
    }
  }

  inline bool operator!()
  {
    if_constexpr_named(cond1, std::is_pointer_v<T>)
    {
      // Checking for null pointer
      return get_raw_value() == nullptr;
    }
    else
    {
      auto unknownCase = !(cond1);
      rlbox_detail_static_fail_because(
        unknownCase,
        "Operator ! only permitted for pointer types. For other types, unwrap "
        "the tainted value with the copy_and_verify API and then use operator "
        "!");
    }
  }

  template<typename T_Dummy = void>
  operator bool() const
  {
    if_constexpr_named(cond1, std::is_pointer_v<T>)
    {
      // Checking for null pointer
      return get_raw_value() != nullptr;
    }
    else
    {
      auto unknownCase = !(cond1);
      rlbox_detail_static_fail_because(
        unknownCase,
        "Implicit conversion to bool is only permitted for pointer types. For "
        "other types, unwrap the tainted value with the copy_and_verify API "
        "and then perform the required checks");
    }
  }
};

template<typename T, typename T_Sbx>
inline tainted<T, T_Sbx> from_opaque(tainted_opaque<T, T_Sbx> val)
{
  return *reinterpret_cast<tainted<T, T_Sbx>*>(&val);
}

template<typename T, typename T_Sbx>
class tainted_volatile : public tainted_base_impl<tainted_volatile, T, T_Sbx>
{
  KEEP_CLASSES_FRIENDLY
  KEEP_CAST_FRIENDLY

  // Classes recieve their own specialization
  static_assert(
    !std::is_class_v<T>,
    "Missing specialization for class T. This error occurs for one "
    "of 2 reasons.\n"
    "  1) Make sure you have include a call rlbox_load_structs_from_library "
    "for this library.\n"
    "  2) Make sure you run (re-run) the struct-dump tool to list "
    "all structs in use by your program.\n");

  static_assert(
    detail::is_basic_type_v<T> || std::is_array_v<T>,
    "Tainted types only support fundamental, enum, pointer, array and struct "
    "types. Please file a bug if more support is needed.");

private:
  using T_ClassBase = tainted_base_impl<tainted_volatile, T, T_Sbx>;
  using T_AppType = tainted_detail::tainted_repr_t<T, T_Sbx>;
  using T_SandboxedType = tainted_detail::tainted_vol_repr_t<T, T_Sbx>;
  T_SandboxedType data;

  inline auto& get_sandbox_value_ref() noexcept { return data; }
  inline auto& get_sandbox_value_ref() const noexcept { return data; }

  inline std::remove_cv_t<T_AppType> get_raw_value() const
  {
    std::remove_cv_t<T_AppType> ret;
    // Need to construct an example_unsandboxed_ptr for pointers or arrays of
    // pointers. Since tainted_volatile is the type of data in sandbox memory,
    // the address of data (&data) refers to a location in sandbox memory and
    // can thus be the example_unsandboxed_ptr
    const volatile void* data_ref = &data;
    const void* example_unsandboxed_ptr = const_cast<const void*>(data_ref);
    detail::convert_type_non_class<
      T_Sbx,
      detail::adjust_type_direction::TO_APPLICATION>(
      ret, data, example_unsandboxed_ptr);
    return ret;
  }

  inline std::remove_cv_t<T_SandboxedType> get_raw_sandbox_value() const
    noexcept
  {
    return data;
  };

  inline std::remove_cv_t<T_AppType> get_raw_value()
  {
    rlbox_detail_forward_to_const(get_raw_value, std::remove_cv_t<T_AppType>);
  }

  inline std::remove_cv_t<T_SandboxedType> get_raw_sandbox_value() noexcept
  {
    rlbox_detail_forward_to_const(get_raw_sandbox_value,
                                  std::remove_cv_t<T_SandboxedType>);
  };

  tainted_volatile() = default;
  tainted_volatile(const tainted_volatile<T, T_Sbx>& p) = default;

public:
  inline tainted<const T*, T_Sbx> operator&() const noexcept
  {
    auto ref =
      detail::remove_volatile_from_ptr_cast(&this->get_sandbox_value_ref());
    auto ref_cast = reinterpret_cast<const T*>(ref);
    auto ret = tainted<const T*, T_Sbx>::internal_factory(ref_cast);
    return ret;
  }

  inline tainted<T*, T_Sbx> operator&() noexcept
  {
    using T_Ret = tainted<T*, T_Sbx>;
    rlbox_detail_forward_to_const(operator&, T_Ret);
  }

  // Needed as the definition of unary & above shadows the base's binary &
  rlbox_detail_forward_binop_to_base(&, T_ClassBase);

  template<typename T_RhsRef>
  inline tainted_volatile<T, T_Sbx>& operator=(T_RhsRef&& val)
  {
    using T_Rhs = std::remove_reference_t<T_RhsRef>;
    using T_Rhs_El = std::remove_all_extents_t<T_Rhs>;

    if_constexpr_named(
      cond1, std::is_same_v<std::remove_const_t<T_Rhs>, std::nullptr_t>)
    {
      static_assert(std::is_pointer_v<T>,
                    "Null pointer can only be assigned to pointers");
      // assign using an integer instead of nullptr, as the pointer field may be
      // represented as integer
      data = 0;
    }
    else if_constexpr_named(cond2, detail::rlbox_is_tainted_v<T_Rhs>)
    {
      // Need to construct an example_unsandboxed_ptr for pointers or arrays of
      // pointers. Since tainted_volatile is the type of data in sandbox memory,
      // the address of data (&data) refers to a location in sandbox memory and
      // can thus be the example_unsandboxed_ptr
      const volatile void* data_ref = &get_sandbox_value_ref();
      const void* example_unsandboxed_ptr = const_cast<const void*>(data_ref);
      detail::convert_type_non_class<T_Sbx,
                                     detail::adjust_type_direction::TO_SANDBOX>(
        get_sandbox_value_ref(),
        val.get_raw_value_ref(),
        example_unsandboxed_ptr);
    }
    else if_constexpr_named(cond3, detail::rlbox_is_tainted_volatile_v<T_Rhs>)
    {
      detail::convert_type_non_class<T_Sbx,
                                     detail::adjust_type_direction::NO_CHANGE>(
        get_sandbox_value_ref(), val.get_sandbox_value_ref());
    }
    else if_constexpr_named(cond4,
                            detail::rlbox_is_sandbox_callback_v<T_Rhs> ||
                              detail::rlbox_is_sandbox_function_v<T_Rhs>)
    {
      using T_RhsFunc = detail::rlbox_remove_wrapper_t<T_Rhs>;

      // need to perform some typechecking to ensure we are assigning compatible
      // function pointer types only
      if_constexpr_named(subcond1, !std::is_assignable_v<T&, T_RhsFunc>)
      {
        rlbox_detail_static_fail_because(
          subcond1,
          "Trying to assign function pointer to field of incompatible types");
      }
      else
      {
        // Need to reinterpret_cast as the representation of the signature of a
        // callback uses the machine model of the sandbox, while the field uses
        // that of the application. But we have already checked above that this
        // is safe.
        auto func = val.get_raw_sandbox_value();
        using T_Cast = std::remove_volatile_t<T_SandboxedType>;
        get_sandbox_value_ref() = reinterpret_cast<T_Cast>(func);
      }
    }
    else if_constexpr_named(
      cond5,
      detail::is_fundamental_or_enum_v<T> ||
        (std::is_array_v<T> && !std::is_pointer_v<T_Rhs_El>))
    {
      detail::convert_type_fundamental_or_array(get_sandbox_value_ref(), val);
    }
    else if_constexpr_named(
      cond6, std::is_pointer_v<T_Rhs> || std::is_pointer_v<T_Rhs_El>)
    {
      rlbox_detail_static_fail_because(
        cond6,
        "Assignment of pointers is not safe as it could\n "
        "1) Leak pointers from the appliction to the sandbox which may break "
        "ASLR\n "
        "2) Pass inaccessible pointers to the sandbox leading to crash\n "
        "3) Break sandboxes that require pointers to be swizzled first\n "
        "\n "
        "Instead, if you want to pass in a pointer, do one of the following\n "
        "1) Allocate with malloc_in_sandbox, and pass in a tainted pointer\n "
        "2) For pointers that point to functions in the application, register "
        "with sandbox.register_callback(\"foo\"), and pass in the registered "
        "value\n "
        "3) For pointers that point to functions in the sandbox, get the "
        "address with sandbox_function_address(sandbox, foo), and pass in the "
        "address\n "
        "4) For raw pointers, use assign_raw_pointer which performs required "
        "safety checks\n ");
    }
    else
    {
      auto unknownCase =
        !(cond1 || cond2 || cond3 || cond4 || cond5 /* || cond6 */);
      rlbox_detail_static_fail_because(
        unknownCase, "Assignment of the given type of value is not supported");
    }

    return *this;
  }

  template<typename T_Rhs>
  void assign_raw_pointer(rlbox_sandbox<T_Sbx>& sandbox, T_Rhs val)
  {
    static_assert(std::is_pointer_v<T_Rhs>, "Must be a pointer");
    static_assert(std::is_assignable_v<T&, T_Rhs>,
                  "Should assign pointers of compatible types.");
    // Maybe a function pointer, so we need to cast
    const void* cast_val = reinterpret_cast<const void*>(val);
    bool safe = sandbox.is_pointer_in_sandbox_memory(cast_val);
    detail::dynamic_check(
      safe,
      "Tried to assign a pointer that is not in the sandbox.\n "
      "This is not safe as it could\n "
      "1) Leak pointers from the appliction to the sandbox which may break "
      "ASLR\n "
      "2) Pass inaccessible pointers to the sandbox leading to crash\n "
      "3) Break sandboxes that require pointers to be swizzled first\n "
      "\n "
      "Instead, if you want to pass in a pointer, do one of the following\n "
      "1) Allocate with malloc_in_sandbox, and pass in a tainted pointer\n "
      "2) For pointers that point to functions in the application, register "
      "with sandbox.register_callback(\"foo\"), and pass in the registered "
      "value\n "
      "3) For pointers that point to functions in the sandbox, get the "
      "address with sandbox_function_address(sandbox, foo), and pass in the "
      "address\n ");
    get_sandbox_value_ref() =
      sandbox.template get_sandboxed_pointer<T_Rhs>(cast_val);
  }

  // ==, != and ! are not supported for tainted_volatile, however, we implement
  // this to ensure the user doesn't see a confusing error message
  template<typename T_Rhs>
  inline bool operator==(T_Rhs&&) const
  {
    rlbox_detail_static_fail_because(
      detail::true_v<T_Rhs>,
      "Cannot compare values that are located in sandbox memory. This error "
      "occurs if you compare a dereferenced value such as the code shown "
      "below\n\n"
      "tainted<int**> a = ...;\n"
      "assert(*a == nullptr);\n\n"
      "Instead you can write this code as \n"
      "tainted<int*> temp = *a;\n"
      "assert(temp == nullptr);\n");
    return false;
  }

  template<typename T_Rhs>
  inline bool operator!=(const std::nullptr_t&) const
  {
    rlbox_detail_static_fail_because(
      detail::true_v<T_Rhs>,
      "Cannot compare values that are located in sandbox memory. This error "
      "occurs if you compare a dereferenced value such as the code shown "
      "below\n\n"
      "tainted<int**> a = ...;\n"
      "assert(*a != nullptr);\n\n"
      "Instead you can write this code as \n"
      "tainted<int*> temp = *a;\n"
      "assert(temp != nullptr);\n");
    return false;
  }

  template<typename T_Dummy = void>
  inline bool operator!()
  {
    rlbox_detail_static_fail_because(
      detail::true_v<T_Dummy>,
      "Cannot apply 'operator not' on values that are located in sandbox "
      "memory. This error occurs if you compare a dereferenced value such as "
      "the code shown below\n\n"
      "tainted<int**> a = ...;\n"
      "assert(!(*a));\n\n"
      "Instead you can write this code as \n"
      "tainted<int*> temp = *a;\n"
      "assert(!temp);\n");
    return false;
  }

  template<typename T_Dummy = void>
  operator bool() const
  {
    rlbox_detail_static_fail_because(
      detail::true_v<T_Dummy>,
      "Cannot apply implicit conversion to bool on values that are located in "
      "sandbox memory. This error occurs if you compare a dereferenced value "
      "such as the code shown below\n\n"
      "tainted<int**> a = ...;\n"
      "assert(*a);\n\n"
      "Instead you can write this code as \n"
      "tainted<int*> temp = *a;\n"
      "assert(temp);\n");
    return false;
  }
};

}
