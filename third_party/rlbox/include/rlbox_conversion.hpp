#pragma once
// IWYU pragma: private, include "rlbox.hpp"
// IWYU pragma: friend "rlbox_.*\.hpp"

#include <array>
#include <cstring>
#include <limits>
#include <type_traits>

#include "rlbox_helpers.hpp"
#include "rlbox_type_traits.hpp"
#include "rlbox_types.hpp"

namespace rlbox::detail {

template<typename T_To, typename T_From>
inline constexpr void convert_type_fundamental(T_To& to,
                                               const volatile T_From& from)
{
  using namespace std;

  if_constexpr_named(cond1, !is_fundamental_or_enum_v<T_To>)
  {
    rlbox_detail_static_fail_because(
      cond1, "Conversion target should be fundamental or enum type");
  }
  else if_constexpr_named(cond2, !is_fundamental_or_enum_v<T_From>)
  {
    rlbox_detail_static_fail_because(
      cond2, "Conversion source should be fundamental or enum type");
  }
  else if_constexpr_named(cond3, is_enum_v<T_To> || is_enum_v<T_From>)
  {
    static_assert(std::is_same_v<T_To, T_From>);
    to = from;
  }
  else if_constexpr_named(
    cond4, is_floating_point_v<T_To> || is_floating_point_v<T_From>)
  {
    static_assert(is_floating_point_v<T_To> && is_floating_point_v<T_From>);
    // language coerces different float types
    to = from;
  }
  else if_constexpr_named(cond5, is_integral_v<T_To> || is_integral_v<T_From>)
  {
    static_assert(is_integral_v<T_To> && is_integral_v<T_From>);

    const char* err_msg =
      "Over/Underflow when converting between integer types";

    // Some branches don't use the param
    RLBOX_UNUSED(err_msg);

    if constexpr (is_signed_v<T_To> == is_signed_v<T_From> &&
                  sizeof(T_To) >= sizeof(T_From)) {
      // Eg: int64_t from int32_t, uint64_t from uint32_t
    } else if constexpr (is_unsigned_v<T_To> && is_unsigned_v<T_From>) {
      // Eg: uint32_t from uint64_t
      dynamic_check(from <= numeric_limits<T_To>::max(), err_msg);
    } else if constexpr (is_signed_v<T_To> && is_signed_v<T_From>) {
      // Eg: int32_t from int64_t
      dynamic_check(from >= numeric_limits<T_To>::min(), err_msg);
      dynamic_check(from <= numeric_limits<T_To>::max(), err_msg);
    } else if constexpr (is_unsigned_v<T_To> && is_signed_v<T_From>) {
      if constexpr (sizeof(T_To) < sizeof(T_From)) {
        // Eg: uint32_t from int64_t
        dynamic_check(from >= 0, err_msg);
        auto to_max = numeric_limits<T_To>::max();
        dynamic_check(from <= static_cast<T_From>(to_max), err_msg);
      } else {
        // Eg: uint32_t from int32_t, uint64_t from int32_t
        dynamic_check(from >= 0, err_msg);
      }
    } else if constexpr (is_signed_v<T_To> && is_unsigned_v<T_From>) {
      if constexpr (sizeof(T_To) <= sizeof(T_From)) {
        // Eg: int32_t from uint32_t, int32_t from uint64_t
        auto to_max = numeric_limits<T_To>::max();
        dynamic_check(from <= static_cast<T_From>(to_max), err_msg);
      } else {
        // Eg: int64_t from uint32_t
      }
    }
    to = static_cast<T_To>(from);
  }
  else
  {
    constexpr auto unknownCase = !(cond1 || cond2 || cond3 || cond4 || cond5);
    rlbox_detail_static_fail_because(
      unknownCase, "Unexpected case for convert_type_fundamental");
  }
}

template<typename T_To, typename T_From>
inline constexpr void convert_type_fundamental_or_array(T_To& to,
                                                        const T_From& from)
{
  using namespace std;

  using T_To_C = std_array_to_c_arr_t<T_To>;
  using T_From_C = std_array_to_c_arr_t<T_From>;
  using T_To_El = remove_all_extents_t<T_To_C>;
  using T_From_El = remove_all_extents_t<T_From_C>;

  if_constexpr_named(cond1, is_array_v<T_To_C> != is_array_v<T_From_C>)
  {
    rlbox_detail_static_fail_because(
      cond1, "Conversion should not go between array and non array types");
  }
  else if constexpr (!is_array_v<T_To_C>)
  {
    return convert_type_fundamental(to, from);
  }
  else if_constexpr_named(cond2, !all_extents_same<T_To_C, T_From_C>)
  {
    rlbox_detail_static_fail_because(
      cond2, "Conversion between arrays should have same dimensions");
  }
  else if_constexpr_named(cond3,
                          is_pointer_v<T_To_El> || is_pointer_v<T_From_El>)
  {
    rlbox_detail_static_fail_because(cond3,
                                     "convert_type_fundamental_or_array "
                                     "does not allow arrays of pointers");
  }
  else
  {
    // Explicitly using size to check for element type as we may be going across
    // different types of the same width such as void* and uintptr_t
    if constexpr (sizeof(T_To_El) == sizeof(T_From_El) &&
                  is_signed_v<T_To_El> == is_signed_v<T_From_El>) {
      // Sanity check - this should definitely be true
      static_assert(sizeof(T_From_C) == sizeof(T_To_C));
      memcpy(&to, &from, sizeof(T_To_C));
    } else {
      for (size_t i = 0; i < std::extent_v<T_To_C>; i++) {
        convert_type_fundamental_or_array(to[i], from[i]);
      }
    }
  }
}

enum class adjust_type_direction
{
  TO_SANDBOX,
  TO_APPLICATION,
  NO_CHANGE
};

enum class adjust_type_context
{
  EXAMPLE,
  SANDBOX
};

template<typename T_Sbx,
         adjust_type_direction Direction,
         adjust_type_context Context,
         typename T_To,
         typename T_From>
inline constexpr void convert_type_non_class(
  T_To& to,
  const T_From& from,
  const void* example_unsandboxed_ptr,
  rlbox_sandbox<T_Sbx>* sandbox_ptr)
{
  using namespace std;

  // Some branches don't use the param
  RLBOX_UNUSED(example_unsandboxed_ptr);
  RLBOX_UNUSED(sandbox_ptr);

  using T_To_C = std_array_to_c_arr_t<T_To>;
  using T_From_C = std_array_to_c_arr_t<T_From>;
  using T_To_El = remove_all_extents_t<T_To_C>;
  using T_From_El = remove_all_extents_t<T_From_C>;

  if constexpr (is_pointer_v<T_To_C> || is_pointer_v<T_From_C>) {

    if constexpr (Direction == adjust_type_direction::NO_CHANGE) {

      static_assert(is_pointer_v<T_To_C> && is_pointer_v<T_From_C> &&
                    sizeof(T_To_C) == sizeof(T_From_C));
      to = from;

    } else if constexpr (Direction == adjust_type_direction::TO_SANDBOX) {

      static_assert(is_pointer_v<T_From_C>);
      // Maybe a function pointer, so convert
      auto from_c = reinterpret_cast<const void*>(from);
      if constexpr (Context == adjust_type_context::SANDBOX) {
        RLBOX_DEBUG_ASSERT(sandbox_ptr != nullptr);
        to = sandbox_ptr->template get_sandboxed_pointer<T_From_C>(from_c);
      } else {
        RLBOX_DEBUG_ASSERT(from_c == nullptr ||
                           example_unsandboxed_ptr != nullptr);
        to =
          rlbox_sandbox<T_Sbx>::template get_sandboxed_pointer_no_ctx<T_From_C>(
            from_c, example_unsandboxed_ptr);
      }

    } else if constexpr (Direction == adjust_type_direction::TO_APPLICATION) {

      static_assert(is_pointer_v<T_To_C>);
      if constexpr (Context == adjust_type_context::SANDBOX) {
        RLBOX_DEBUG_ASSERT(sandbox_ptr != nullptr);
        to = sandbox_ptr->template get_unsandboxed_pointer<T_To_C>(from);
      } else {
        RLBOX_DEBUG_ASSERT(from == 0 || example_unsandboxed_ptr != nullptr);
        to =
          rlbox_sandbox<T_Sbx>::template get_unsandboxed_pointer_no_ctx<T_To_C>(
            from, example_unsandboxed_ptr);
      }
    }

  } else if constexpr (is_pointer_v<T_To_El> || is_pointer_v<T_From_El>) {

    if constexpr (Direction == adjust_type_direction::NO_CHANGE) {
      // Sanity check - this should definitely be true
      static_assert(sizeof(T_To_El) == sizeof(T_From_El) &&
                    sizeof(T_From_C) == sizeof(T_To_C));
      memcpy(&to, &from, sizeof(T_To_C));
    } else {
      for (size_t i = 0; i < std::extent_v<T_To_C>; i++) {
        convert_type_non_class<T_Sbx, Direction, Context>(
          to[i], from[i], example_unsandboxed_ptr, sandbox_ptr);
      }
    }

  } else {
    convert_type_fundamental_or_array(to, from);
  }
}

// Structs implement their own convert_type by specializing this class
// Have to do this via a class, as functions can't be partially specialized
template<typename T_Sbx,
         adjust_type_direction Direction,
         adjust_type_context Context,
         typename T_To,
         typename T_From>
class convert_type_class;
// The specialization implements the following
// {
//   static inline void run(T_To& to,
//                          const T_From& from,
//                          const void* example_unsandboxed_ptr);
// }

template<typename T_Sbx,
         adjust_type_direction Direction,
         adjust_type_context Context,
         typename T_To,
         typename T_From>
inline void convert_type(T_To& to,
                         const T_From& from,
                         const void* example_unsandboxed_ptr,
                         rlbox_sandbox<T_Sbx>* sandbox_ptr)
{
  if constexpr ((std::is_class_v<T_To> ||
                 std::is_class_v<T_From>)&&!detail::is_std_array_v<T_To> &&
                !detail::is_std_array_v<T_From>) {
    // Sanity check
    static_assert(std::is_class_v<T_From> && std::is_class_v<T_To>);
    convert_type_class<T_Sbx, Direction, Context, T_To, T_From>::run(
      to, from, example_unsandboxed_ptr, sandbox_ptr);
  } else {
    convert_type_non_class<T_Sbx, Direction, Context>(
      to, from, example_unsandboxed_ptr, sandbox_ptr);
  }
}

}