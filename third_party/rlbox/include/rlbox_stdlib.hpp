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
  auto raw = reinterpret_cast<T_Lhs>(taintedVal.INTERNAL_unverified_safe());
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
  auto raw = const_cast<T_Lhs>(taintedVal.INTERNAL_unverified_safe());
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
  auto raw = static_cast<T_Lhs>(taintedVal.INTERNAL_unverified_safe());
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
  void* dest_start = ptr_tainted.INTERNAL_unverified_safe();
  detail::check_range_doesnt_cross_app_sbx_boundary<T_Sbx>(dest_start, num_val);

  std::memset(dest_start, detail::unwrap_value(value), num_val);
  return ptr;
}

/**
 * @brief types that do not need to be adjusted to fix ABI differences.
 * Currently these are only char, wchar, float, and double
 */

template<typename T>
static constexpr bool can_type_be_memcopied =
  std::is_same_v<char, std::remove_cv_t<T>> || std::is_same_v<wchar_t, std::remove_cv_t<T>> ||
  std::is_same_v<float, std::remove_cv_t<T>> || std::is_same_v<double, std::remove_cv_t<T>> ||
  std::is_same_v<char16_t, std::remove_cv_t<T>> || std::is_same_v<short, std::remove_cv_t<T>>;

/**
 * @brief Copy to sandbox memory area. Note that memcpy is meant to be called on
 * byte arrays does not adjust data according to ABI differences. If the
 * programmer does accidentally call memcpy on buffers that needs ABI
 * adjustment, this may cause compatibility issues, but will not cause a
 * security issue as the destination is always a tainted or tainted_volatile
 * pointer
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
  void* dest_start = dest_tainted.INTERNAL_unverified_safe();
  detail::check_range_doesnt_cross_app_sbx_boundary<T_Sbx>(dest_start, num_val);

  // src also needs to be checked, as we don't want to allow a src rand to start
  // inside the sandbox and end outside, and vice versa
  // src may or may not be a wrapper, so use unwrap_value
  const void* src_start = detail::unwrap_value(src);
  detail::check_range_doesnt_cross_app_sbx_boundary<T_Sbx>(src_start, num_val);

  std::memcpy(dest_start, src_start, num_val);

  return dest;
}

/**
 * @brief Compare data in sandbox memory area.
 */
template<typename T_Sbx, typename T_Rhs, typename T_Lhs, typename T_Num>
inline tainted_int_hint memcmp(rlbox_sandbox<T_Sbx>& sandbox,
                               T_Rhs&& dest,
                               T_Lhs&& src,
                               T_Num&& num)
{
  static_assert(
    detail::rlbox_is_tainted_or_vol_v<detail::remove_cv_ref_t<T_Rhs>> ||
      detail::rlbox_is_tainted_or_vol_v<detail::remove_cv_ref_t<T_Lhs>>,
    "memcmp called on non wrapped type");

  auto num_val = detail::unwrap_value(num);
  detail::dynamic_check(num_val <= sandbox.get_total_memory(),
                        "Called memcmp for memory larger than the sandbox");

  void* dest_start = dest.INTERNAL_unverified_safe();
  detail::check_range_doesnt_cross_app_sbx_boundary<T_Sbx>(dest_start, num_val);

  // src also needs to be checked, as we don't want to allow a src rand to start
  // inside the sandbox and end outside, and vice versa
  // src may or may not be a wrapper, so use unwrap_value
  const void* src_start = detail::unwrap_value(src);
  detail::check_range_doesnt_cross_app_sbx_boundary<T_Sbx>(src_start, num_val);

  int ret = std::memcmp(dest_start, src_start, num_val);
  tainted_int_hint converted_ret(ret);
  return converted_ret;
}

/**
 * @brief This function either
 * - copies the given buffer into the sandbox calling delete on the src
 * OR
 * - if the sandbox allows, adds the buffer to the existing sandbox memory
 * @param sandbox Target sandbox
 * @param src Raw pointer to the buffer
 * @param num Number of T-sized elements in the buffer
 * @param free_source_on_copy If the source buffer was copied, this variable
 * controls whether copy_memory_or_grant_access should call delete on the src.
 * This calls delete[] if num > 1.
 * @param copied out parameter indicating if the source was copied or transfered
 */
template<typename T_Sbx, typename T>
tainted<T*, T_Sbx> copy_memory_or_grant_access(rlbox_sandbox<T_Sbx>& sandbox,
                                               T* src,
                                               size_t num,
                                               bool free_source_on_copy,
                                               bool& copied)
{
  copied = false;

  // This function is meant for byte buffers only
  static_assert(can_type_be_memcopied<std::remove_pointer_t<T>>,
                "copy_memory_or_grant_access not supported on this type as "
                "there may be ABI differences");

  // overflow ok
  size_t source_size = num * sizeof(T);

  // sandbox can grant access if it includes the following line
  // using can_grant_deny_access = void;
  if constexpr (detail::has_member_using_can_grant_deny_access_v<T_Sbx>) {
    detail::check_range_doesnt_cross_app_sbx_boundary<T_Sbx>(src, source_size);

    bool success;
    auto ret = sandbox.INTERNAL_grant_access(src, num, success);
    if (success) {
      return ret;
    }
  }

  // Malloc in sandbox takes a uint32_t as the parameter, need a bounds check
  detail::dynamic_check(num <= std::numeric_limits<uint32_t>::max(),
                        "Granting access too large a region");
  using T_nocv = std::remove_cv_t<T>;
  tainted<T_nocv*, T_Sbx> copy =
    sandbox.template malloc_in_sandbox<T_nocv>(static_cast<uint32_t>(num));
  if (!copy) {
    return nullptr;
  }

  rlbox::memcpy(sandbox, copy, src, source_size);
  if (free_source_on_copy) {
    free(const_cast<void*>(reinterpret_cast<const void*>(src)));
  }

  copied = true;
  return sandbox_const_cast<T*>(copy);
}

/**
 * @brief This function either
 * - copies the given buffer out of the sandbox calling free_in_sandbox on the
 * src
 * OR
 * - if the sandbox allows, moves the buffer out of existing sandbox memory
 * @param sandbox Target sandbox
 * @param src Raw pointer to the buffer
 * @param num Number of T-sized elements in the buffer
 * @param free_source_on_copy If the source buffer was copied, this variable
 * controls whether copy_memory_or_deny_access should call delete on the src.
 * This calls delete[] if num > 1.
 * @param copied out parameter indicating if the source was copied or transfered
 */
template<typename T_Sbx,
         typename T,
         template<typename, typename>
         typename T_Wrap>
T* copy_memory_or_deny_access(rlbox_sandbox<T_Sbx>& sandbox,
                              T_Wrap<T*, T_Sbx> src,
                              size_t num,
                              bool free_source_on_copy,
                              bool& copied)
{
  copied = false;

  // This function is meant for byte buffers only - so char and char16
  static_assert(can_type_be_memcopied<std::remove_pointer_t<T>>,
                "copy_memory_or_deny_access not supported on this type as "
                "there may be ABI differences");

  // overflow ok
  size_t source_size = num * sizeof(T);

  // sandbox can grant access if it includes the following line
  // using can_grant_deny_access = void;
  if constexpr (detail::has_member_using_can_grant_deny_access_v<T_Sbx>) {
    detail::check_range_doesnt_cross_app_sbx_boundary<T_Sbx>(
      src.INTERNAL_unverified_safe(), source_size);

    bool success;
    auto ret = sandbox.INTERNAL_deny_access(src, num, success);
    if (success) {
      return ret;
    }
  }

  auto copy = static_cast<T*>(malloc(source_size));
  if (!copy) {
    return nullptr;
  }

  tainted<T*, T_Sbx> src_tainted = src;
  char* src_raw = src_tainted.copy_and_verify_buffer_address(
    [](uintptr_t val) { return reinterpret_cast<char*>(val); }, num);
  std::memcpy(copy, src_raw, source_size);
  if (free_source_on_copy) {
    sandbox.free_in_sandbox(src);
  }

  copied = true;
  return copy;
}

}
