#pragma once
// IWYU pragma: private, include "rlbox.hpp"
// IWYU pragma: friend "rlbox_.*\.hpp"

#include <type_traits>

#include "rlbox_type_traits.hpp"
#include "rlbox_types.hpp"

namespace rlbox::detail {

template<typename T_Rhs>
inline auto unwrap_value(const T_Rhs& rhs) noexcept
{
  if constexpr (detail::rlbox_is_wrapper_v<T_Rhs>) {
    return rhs.UNSAFE_unverified();
  } else {
    return rhs;
  }
}

}