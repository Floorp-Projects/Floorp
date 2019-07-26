
#pragma once
// IWYU pragma: private, include "rlbox.hpp"
// IWYU pragma: friend "rlbox_.*\.hpp"

#include <cstdint>

#include "rlbox_types.hpp"

namespace rlbox::detail {

// Checks that a given range is either entirely in a sandbox or entirely
// outside
template<typename T_Sbx>
inline void check_range_doesnt_cross_app_sbx_boundary(const void* ptr,
                                                      size_t size)
{
  auto ptr_start_val = reinterpret_cast<uintptr_t>(ptr);
  detail::dynamic_check(
    ptr_start_val,
    "Performing memory operation memset/memcpy on a null pointer");
  auto ptr_end_val = ptr_start_val + size - 1;

  auto ptr_start = reinterpret_cast<void*>(ptr_start_val);
  auto ptr_end = reinterpret_cast<void*>(ptr_end_val);

  detail::dynamic_check(
    rlbox_sandbox<T_Sbx>::is_in_same_sandbox(ptr_start, ptr_end),
    "range has overflowed sandbox bounds");
}

}