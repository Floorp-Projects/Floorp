#pragma once
// IWYU pragma: private, include "rlbox.hpp"
// IWYU pragma: friend "rlbox_.*\.hpp"

#include <type_traits>

#include "rlbox_types.hpp"

namespace rlbox::detail {

#define rlbox_generate_wrapper_check(name)                                     \
  namespace detail_rlbox_is_##name                                             \
  {                                                                            \
    template<typename T>                                                       \
    struct unwrapper : std::false_type                                         \
    {};                                                                        \
                                                                               \
    template<typename T, typename T_Sbx>                                       \
    struct unwrapper<name<T, T_Sbx>> : std::true_type                          \
    {};                                                                        \
  }                                                                            \
                                                                               \
  template<typename T>                                                         \
  constexpr bool rlbox_is_##name##_v =                                         \
    detail_rlbox_is_##name::unwrapper<T>::value;                               \
  RLBOX_REQUIRE_SEMI_COLON

rlbox_generate_wrapper_check(tainted);
rlbox_generate_wrapper_check(tainted_volatile);
rlbox_generate_wrapper_check(tainted_opaque);
rlbox_generate_wrapper_check(sandbox_callback);

#undef rlbox_generate_wrapper_check

namespace detail_rlbox_is_tainted_boolean_hint {
  template<typename T>
  struct unwrapper : std::false_type
  {};

  template<>
  struct unwrapper<tainted_boolean_hint> : std::true_type
  {};
}

template<typename T>
constexpr bool rlbox_is_tainted_boolean_hint_v =
  detail_rlbox_is_tainted_boolean_hint::unwrapper<T>::value;

template<typename T>
constexpr bool rlbox_is_tainted_or_vol_v =
  rlbox_is_tainted_v<T> || rlbox_is_tainted_volatile_v<T>;

template<typename T>
constexpr bool rlbox_is_tainted_or_opaque_v =
  rlbox_is_tainted_v<T> || rlbox_is_tainted_opaque_v<T>;

// tainted_hint is NOT considered a wrapper type... This carries no particular
// significant and is just a convention choice
template<typename T>
constexpr bool rlbox_is_wrapper_v =
  rlbox_is_tainted_v<T> || rlbox_is_tainted_volatile_v<T> ||
  rlbox_is_tainted_opaque_v<T> || rlbox_is_sandbox_callback_v<T>;

namespace detail_rlbox_remove_wrapper {
  template<typename T>
  struct unwrapper
  {
    using type = T;
    using type_sbx = void;
  };

  template<typename T, typename T_Sbx>
  struct unwrapper<tainted<T, T_Sbx>>
  {
    using type = T;
    using type_sbx = T_Sbx;
  };

  template<typename T, typename T_Sbx>
  struct unwrapper<tainted_volatile<T, T_Sbx>>
  {
    using type = T;
    using type_sbx = T_Sbx;
  };

  template<typename T, typename T_Sbx>
  struct unwrapper<tainted_opaque<T, T_Sbx>>
  {
    using type = T;
    using type_sbx = T_Sbx;
  };

  template<typename T, typename T_Sbx>
  struct unwrapper<sandbox_callback<T, T_Sbx>>
  {
    using type = T;
    using type_sbx = T_Sbx;
  };
}

template<typename T>
using rlbox_remove_wrapper_t =
  typename detail_rlbox_remove_wrapper::unwrapper<T>::type;

template<typename T>
using rlbox_get_wrapper_sandbox_t =
  typename detail_rlbox_remove_wrapper::unwrapper<T>::type_sbx;

template<typename T, typename T_Sbx>
using rlbox_tainted_opaque_to_tainted_t =
  std::conditional_t<rlbox_is_tainted_opaque_v<T>,
                     tainted<rlbox_remove_wrapper_t<T>, T_Sbx>,
                     T>;

// https://stackoverflow.com/questions/34974844/check-if-a-type-is-from-a-particular-namespace
namespace detail_is_member_of_rlbox_detail {
  template<typename T, typename = void>
  struct is_member_of_rlbox_detail_helper : std::false_type
  {};

  template<typename T>
  struct is_member_of_rlbox_detail_helper<
    T,
    decltype(struct_is_member_of_rlbox_detail(std::declval<T>()))>
    : std::true_type
  {};
}

template<typename T>
void struct_is_member_of_rlbox_detail(T&&);

template<typename T>
constexpr auto is_member_of_rlbox_detail =
  detail_is_member_of_rlbox_detail::is_member_of_rlbox_detail_helper<T>::value;

}