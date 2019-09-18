#pragma once
// IWYU pragma: private, include "rlbox.hpp"
// IWYU pragma: friend "rlbox_.*\.hpp"

#include <cstring>
#include <type_traits>

#include "rlbox_helpers.hpp"
#include "rlbox_types.hpp"
#include "rlbox_unwrap.hpp"
#include "rlbox_wrapper_traits.hpp"

namespace rlbox {
#define KEEP_CAST_FRIENDLY                                                     \
  template<typename T_C_Lhs,                                                   \
           typename T_C_Rhs,                                                   \
           typename T_C_Sbx,                                                   \
           template<typename, typename>                                        \
           typename T_C_Wrap>                                                  \
  friend inline tainted<T_C_Lhs, T_C_Sbx> sandbox_reinterpret_cast(            \
    const T_C_Wrap<T_C_Rhs, T_C_Sbx>& rhs) noexcept;                           \
                                                                               \
  template<typename T_C_Lhs,                                                   \
           typename T_C_Rhs,                                                   \
           typename T_C_Sbx,                                                   \
           template<typename, typename>                                        \
           typename T_C_Wrap>                                                  \
  friend inline tainted<T_C_Lhs, T_C_Sbx> sandbox_const_cast(                  \
    const T_C_Wrap<T_C_Rhs, T_C_Sbx>& rhs) noexcept;                           \
                                                                               \
  template<typename T_C_Lhs,                                                   \
           typename T_C_Rhs,                                                   \
           typename T_C_Sbx,                                                   \
           template<typename, typename>                                        \
           typename T_C_Wrap>                                                  \
  friend inline tainted<T_C_Lhs, T_C_Sbx> sandbox_static_cast(                 \
    const T_C_Wrap<T_C_Rhs, T_C_Sbx>& rhs) noexcept;

/**
 * @brief The equivalent of a reinterpret_cast but operates on sandboxed values.
 */
template<typename T_Lhs,
         typename T_Rhs,
         typename T_Sbx,
         template<typename, typename>
         typename T_Wrap>
inline tainted<T_Lhs, T_Sbx> sandbox_reinterpret_cast(
  const T_Wrap<T_Rhs, T_Sbx>& rhs) noexcept
{
  static_assert(detail::rlbox_is_wrapper_v<T_Wrap<T_Rhs, T_Sbx>> &&
                  std::is_pointer_v<T_Lhs> && std::is_pointer_v<T_Rhs>,
                "sandbox_reinterpret_cast on incompatible types");

  tainted<T_Rhs, T_Sbx> taintedVal = rhs;
  auto raw =
    reinterpret_cast<T_Lhs>(taintedVal.unverified_safe_because("internal use"));
  auto ret = tainted<T_Lhs, T_Sbx>::internal_factory(raw);
  return ret;
}

/**
 * @brief The equivalent of a const_cast but operates on sandboxed values.
 */
template<typename T_Lhs,
         typename T_Rhs,
         typename T_Sbx,
         template<typename, typename>
         typename T_Wrap>
inline tainted<T_Lhs, T_Sbx> sandbox_const_cast(
  const T_Wrap<T_Rhs, T_Sbx>& rhs) noexcept
{
  static_assert(detail::rlbox_is_wrapper_v<T_Wrap<T_Rhs, T_Sbx>>,
                "sandbox_const_cast on incompatible types");

  tainted<T_Rhs, T_Sbx> taintedVal = rhs;
  auto raw =
    const_cast<T_Lhs>(taintedVal.unverified_safe_because("internal use"));
  auto ret = tainted<T_Lhs, T_Sbx>::internal_factory(raw);
  return ret;
}

/**
 * @brief The equivalent of a static_cast but operates on sandboxed values.
 */
template<typename T_Lhs,
         typename T_Rhs,
         typename T_Sbx,
         template<typename, typename>
         typename T_Wrap>
inline tainted<T_Lhs, T_Sbx> sandbox_static_cast(
  const T_Wrap<T_Rhs, T_Sbx>& rhs) noexcept
{
  static_assert(detail::rlbox_is_wrapper_v<T_Wrap<T_Rhs, T_Sbx>>,
                "sandbox_static_cast on incompatible types");

  tainted<T_Rhs, T_Sbx> taintedVal = rhs;
  auto raw =
    static_cast<T_Lhs>(taintedVal.unverified_safe_because("internal use"));
  auto ret = tainted<T_Lhs, T_Sbx>::internal_factory(raw);
  return ret;
}

/**
 * @brief Fill sandbox memory with a constant byte.
 */
template<typename T_Sbx,
         typename T_Rhs,
         typename T_Val,
         typename T_Num,
         template<typename, typename>
         typename T_Wrap>
inline T_Wrap<T_Rhs*, T_Sbx> memset(rlbox_sandbox<T_Sbx>& sandbox,
                                    T_Wrap<T_Rhs*, T_Sbx> ptr,
                                    T_Val value,
                                    T_Num num)
{

  static_assert(detail::rlbox_is_tainted_or_vol_v<T_Wrap<T_Rhs, T_Sbx>>,
                "memset called on non wrapped type");

  static_assert(!std::is_const_v<T_Rhs>, "Destination is const");

  auto num_val = detail::unwrap_value(num);
  detail::dynamic_check(num_val <= sandbox.get_total_memory(),
                        "Called memset for memory larger than the sandbox");

  tainted<T_Rhs*, T_Sbx> ptr_tainted = ptr;
  void* dest_start = ptr_tainted.unverified_safe_because("internal use");
  detail::check_range_doesnt_cross_app_sbx_boundary<T_Sbx>(dest_start, num_val);

  std::memset(dest_start, detail::unwrap_value(value), num_val);
  return ptr;
}

/**
 * @brief Copy to sandbox memory area.
 */
template<typename T_Sbx,
         typename T_Rhs,
         typename T_Lhs,
         typename T_Num,
         template<typename, typename>
         typename T_Wrap>
inline T_Wrap<T_Rhs*, T_Sbx> memcpy(rlbox_sandbox<T_Sbx>& sandbox,
                                    T_Wrap<T_Rhs*, T_Sbx> dest,
                                    T_Lhs src,
                                    T_Num num)
{

  static_assert(detail::rlbox_is_tainted_or_vol_v<T_Wrap<T_Rhs, T_Sbx>>,
                "memcpy called on non wrapped type");

  static_assert(!std::is_const_v<T_Rhs>, "Destination is const");

  auto num_val = detail::unwrap_value(num);
  detail::dynamic_check(num_val <= sandbox.get_total_memory(),
                        "Called memcpy for memory larger than the sandbox");

  tainted<T_Rhs*, T_Sbx> dest_tainted = dest;
  void* dest_start = dest_tainted.unverified_safe_because("internal use");
  detail::check_range_doesnt_cross_app_sbx_boundary<T_Sbx>(dest_start, num_val);

  // src also needs to be checked, as we don't want to allow a src rand to start
  // inside the sandbox and end outside, and vice versa
  // src may or may not be a wrapper, so use unwrap_value
  const void* src_start = detail::unwrap_value(src);
  detail::check_range_doesnt_cross_app_sbx_boundary<T_Sbx>(src_start, num_val);

  std::memcpy(dest_start, src_start, num_val);

  return dest;
}

}
