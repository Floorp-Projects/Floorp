#pragma once
// IWYU pragma: private, include "rlbox.hpp"
// IWYU pragma: friend "rlbox_.*\.hpp"

#include <array>
#include <type_traits>

namespace rlbox::detail {

#define RLBOX_ENABLE_IF(...) std::enable_if_t<__VA_ARGS__>* = nullptr

template<typename T>
constexpr bool true_v = true;

template<typename T>
constexpr bool is_fundamental_or_enum_v =
  std::is_fundamental_v<T> || std::is_enum_v<T>;

template<typename T>
constexpr bool is_basic_type_v =
  std::is_fundamental_v<T> || std::is_enum_v<T> || std::is_pointer_v<T>;

template<typename T>
using valid_return_t =
  std::conditional_t<std::is_function_v<T>, void*, std::decay_t<T>>;

template<typename T>
using valid_param_t = std::conditional_t<std::is_void_v<T>, void*, T>;

template<typename T>
using valid_array_el_t =
  std::conditional_t<std::is_void_v<T> || std::is_function_v<T>, int, T>;

template<typename T>
constexpr bool is_func_ptr_v = (std::is_pointer_v<T> &&
                                std::is_function_v<std::remove_pointer_t<T>>) ||
                               std::is_member_function_pointer_v<T>;

template<typename T>
constexpr bool is_func_or_func_ptr = std::is_function_v<T> || is_func_ptr_v<T>;

template<typename T>
constexpr bool is_one_level_ptr_v =
  std::is_pointer_v<T> && !std::is_pointer_v<std::remove_pointer_t<T>>;

template<typename T_To, typename T_From>
constexpr bool is_same_or_same_ref_v =
  std::is_same_v<T_To, T_From>&& std::is_reference_v<T_To&, T_From>;

template<typename T_This, typename T_Target>
using add_const_if_this_const_t =
  std::conditional_t<std::is_const_v<std::remove_pointer_t<T_This>>,
                     std::add_const_t<T_Target>,
                     T_Target>;

template<typename T>
using remove_const_from_pointer = std::conditional_t<
  std::is_pointer_v<T>,
  std::add_pointer_t<std::remove_const_t<std::remove_pointer_t<T>>>,
  T>;

template<typename T>
using add_const_from_pointer = std::conditional_t<
  std::is_pointer_v<T>,
  std::remove_pointer_t<std::add_const_t<std::remove_pointer_t<T>>>,
  T>;

template<typename T>
using remove_cv_ref_t = std::remove_cv_t<std::remove_reference_t<T>>;

template<typename T>
using c_to_std_array_t =
  std::conditional_t<std::is_array_v<T>,
                     std::array<std::remove_extent_t<T>, std::extent_v<T>>,
                     T>;

namespace std_array_to_c_arr_detail {
  template<typename T>
  struct W
  {
    using type = T;
  };

  template<typename T, size_t N>
  W<T[N]> std_array_to_c_arr_helper(std::array<T, N>);

  template<typename T>
  W<T> std_array_to_c_arr_helper(T&&);
}

template<typename T>
using std_array_to_c_arr_t =
  typename decltype(std_array_to_c_arr_detail::std_array_to_c_arr_helper(
    std::declval<T>()))::type;

template<typename T>
using dereference_result_t =
  std::conditional_t<std::is_pointer_v<T>,
                     std::remove_pointer_t<T>,
                     std::remove_extent_t<std_array_to_c_arr_t<T>> // is_array
                     >;

template<typename T>
using value_type_t =
  std::conditional_t<std::is_array_v<T>, c_to_std_array_t<T>, T>;

template<typename T>
using function_ptr_t =
  std::conditional_t<std::is_pointer_v<T> &&
                       std::is_function_v<std::remove_pointer_t<T>>,
                     T,
                     int (*)(int)>;

namespace is_c_or_std_array_detail {
  template<typename T, typename T_Enable = void>
  struct is_c_or_std_array_helper;

  template<typename T>
  struct is_c_or_std_array_helper<T, std::enable_if_t<std::is_array_v<T>>>
    : std::true_type
  {};

  template<typename T, size_t N>
  std::true_type is_std_array_helper(std::array<T, N>*);

  template<typename T>
  std::false_type is_std_array_helper(T*);

  template<typename T>
  constexpr bool is_std_array_v =
    decltype(is_std_array_helper(std::declval<std::add_pointer_t<T>>()))::value;

  template<typename T>
  struct is_c_or_std_array_helper<T, std::enable_if_t<is_std_array_v<T>>>
    : std::true_type
  {};

  template<typename T>
  struct is_c_or_std_array_helper<
    T,
    std::enable_if_t<!std::is_array_v<T> && !is_std_array_v<T>>>
    : std::false_type
  {};
}

template<typename T>
constexpr bool is_std_array_v = is_c_or_std_array_detail::is_std_array_v<T>;

template<typename T>
constexpr bool is_c_or_std_array_v =
  is_c_or_std_array_detail::is_c_or_std_array_helper<T>::value;

namespace std_array_el_detail {
  template<typename T>
  struct W
  {
    using type = T;
  };

  template<typename T, size_t N>
  W<T> is_std_array_helper(std::array<T, N>*);

  template<typename T>
  W<void> is_std_array_helper(T*);

  template<typename T>
  using std_array_el_t = decltype(std_array_el_detail::is_std_array_helper(
    std::declval<std::add_pointer_t<T>>));
}

template<typename T>
using std_array_el_t = typename std_array_el_detail::std_array_el_t<T>::type;

namespace all_extents_same_detail {

  template<typename T1, typename T2, typename T_Enable = void>
  struct all_extents_same_helper;

  template<typename T1, typename T2>
  struct all_extents_same_helper<
    T1,
    T2,
    std::enable_if_t<std::rank_v<T1> != std::rank_v<T2>>> : std::false_type
  {};

  template<typename T1, typename T2>
  struct all_extents_same_helper<
    T1,
    T2,
    std::enable_if_t<std::rank_v<T1> == std::rank_v<T2> &&
                     !std::is_array_v<T1> && !std::is_array_v<T2>>>
    : std::true_type
  {};

  template<typename T1, typename T2>
  struct all_extents_same_helper<
    T1,
    T2,
    std::enable_if_t<std::rank_v<T1> == std::rank_v<T2> &&
                     std::is_array_v<T1> && std::is_array_v<T2> &&
                     std::extent_v<T1> != std::extent_v<T2>>> : std::false_type
  {};

  template<typename T1, typename T2>
  struct all_extents_same_helper<
    T1,
    T2,
    std::enable_if_t<std::rank_v<T1> == std::rank_v<T2> &&
                     std::is_array_v<T1> && std::is_array_v<T2> &&
                     std::extent_v<T1> == std::extent_v<T2>>>
  {
    static constexpr bool value =
      all_extents_same_helper<std::remove_extent_t<T1>,
                              std::remove_extent_t<T2>>::value;
  };
}

template<typename T1, typename T2>
constexpr bool all_extents_same =
  all_extents_same_detail::all_extents_same_helper<T1, T2>::value;

// remove all pointers/extent types
namespace remove_all_pointers_detail {
  template<typename T>
  struct remove_all_pointers
  {
    typedef T type;
  };

  template<typename T>
  struct remove_all_pointers<T*>
  {
    typedef typename remove_all_pointers<T>::type type;
  };
}

template<typename T>
using remove_all_pointers_t =
  typename remove_all_pointers_detail::remove_all_pointers<T>::type;

// remove all pointers/extent types
namespace base_type_detail {
  template<typename T>
  struct base_type
  {
    typedef T type;
  };

  template<typename T>
  struct base_type<T*>
  {
    typedef typename base_type<T>::type type;
  };

  template<typename T>
  struct base_type<T[]>
  {
    typedef typename base_type<T>::type type;
  };

  template<typename T, std::size_t N>
  struct base_type<T[N]>
  {
    typedef typename base_type<T>::type type;
  };
}

template<typename T>
using base_type_t = typename base_type_detail::base_type<T>::type;

// convert types
namespace convert_detail {
  template<typename T,
           typename T_ShortType,
           typename T_IntType,
           typename T_LongType,
           typename T_LongLongType,
           typename T_PointerType,
           typename T_Enable = void>
  struct convert_base_types_t_helper;

  template<typename T,
           typename T_ShortType,
           typename T_IntType,
           typename T_LongType,
           typename T_LongLongType,
           typename T_PointerType>
  struct convert_base_types_t_helper<
    T,
    T_ShortType,
    T_IntType,
    T_LongType,
    T_LongLongType,
    T_PointerType,
    std::enable_if_t<std::is_same_v<short, T> && !std::is_const_v<T>>>
  {
    using type = T_ShortType;
  };

  template<typename T,
           typename T_ShortType,
           typename T_IntType,
           typename T_LongType,
           typename T_LongLongType,
           typename T_PointerType>
  struct convert_base_types_t_helper<
    T,
    T_ShortType,
    T_IntType,
    T_LongType,
    T_LongLongType,
    T_PointerType,
    std::enable_if_t<std::is_same_v<int, T> && !std::is_const_v<T>>>
  {
    using type = T_IntType;
  };

  template<typename T,
           typename T_ShortType,
           typename T_IntType,
           typename T_LongType,
           typename T_LongLongType,
           typename T_PointerType>
  struct convert_base_types_t_helper<
    T,
    T_ShortType,
    T_IntType,
    T_LongType,
    T_LongLongType,
    T_PointerType,
    std::enable_if_t<std::is_same_v<long, T> && !std::is_const_v<T>>>
  {
    using type = T_LongType;
  };

  template<typename T,
           typename T_ShortType,
           typename T_IntType,
           typename T_LongType,
           typename T_LongLongType,
           typename T_PointerType>
  struct convert_base_types_t_helper<
    T,
    T_ShortType,
    T_IntType,
    T_LongType,
    T_LongLongType,
    T_PointerType,
    std::enable_if_t<std::is_same_v<long long, T> && !std::is_const_v<T>>>
  {
    using type = T_LongLongType;
  };

  template<typename T,
           typename T_ShortType,
           typename T_IntType,
           typename T_LongType,
           typename T_LongLongType,
           typename T_PointerType>
  struct convert_base_types_t_helper<
    T,
    T_ShortType,
    T_IntType,
    T_LongType,
    T_LongLongType,
    T_PointerType,
    std::enable_if_t<std::is_pointer_v<T> && !std::is_const_v<T>>>
  {
    using type = T_PointerType;
  };

  template<typename T,
           typename T_ShortType,
           typename T_IntType,
           typename T_LongType,
           typename T_LongLongType,
           typename T_PointerType>
  struct convert_base_types_t_helper<
    T,
    T_ShortType,
    T_IntType,
    T_LongType,
    T_LongLongType,
    T_PointerType,
    std::enable_if_t<std::is_unsigned_v<T> && !std::is_same_v<T, bool> &&
                     !std::is_const_v<T> && !std::is_enum_v<T>>>
  {
    using type = std::make_unsigned_t<
      typename convert_base_types_t_helper<std::make_signed_t<T>,
                                           T_ShortType,
                                           T_IntType,
                                           T_LongType,
                                           T_LongLongType,
                                           T_PointerType>::type>;
  };

  template<typename T,
           typename T_ShortType,
           typename T_IntType,
           typename T_LongType,
           typename T_LongLongType,
           typename T_PointerType>
  struct convert_base_types_t_helper<
    T,
    T_ShortType,
    T_IntType,
    T_LongType,
    T_LongLongType,
    T_PointerType,
    std::enable_if_t<(
      std::is_same_v<bool, T> || std::is_same_v<void, T> ||
      std::is_same_v<char, T> || std::is_same_v<signed char, T> ||
      std::is_floating_point_v<T> || std::is_enum_v<T>)&&!std::is_const_v<T>>>
  {
    using type = T;
  };

  template<typename T,
           typename T_ShortType,
           typename T_IntType,
           typename T_LongType,
           typename T_LongLongType,
           typename T_PointerType>
  struct convert_base_types_t_helper<
    T,
    T_ShortType,
    T_IntType,
    T_LongType,
    T_LongLongType,
    T_PointerType,
    std::enable_if_t<std::is_array_v<T> && !std::is_const_v<T>>>
  {
    using type = typename convert_base_types_t_helper<
      std::remove_extent_t<T>,
      T_ShortType,
      T_IntType,
      T_LongType,
      T_LongLongType,
      T_PointerType>::type[std::extent_v<T>];
  };

  template<typename T,
           typename T_ShortType,
           typename T_IntType,
           typename T_LongType,
           typename T_LongLongType,
           typename T_PointerType>
  struct convert_base_types_t_helper<T,
                                     T_ShortType,
                                     T_IntType,
                                     T_LongType,
                                     T_LongLongType,
                                     T_PointerType,
                                     std::enable_if_t<std::is_const_v<T>>>
  {
    using type = std::add_const_t<
      typename convert_base_types_t_helper<std::remove_const_t<T>,
                                           T_ShortType,
                                           T_IntType,
                                           T_LongType,
                                           T_LongLongType,
                                           T_PointerType>::type>;
  };
}

template<typename T,
         typename T_ShortType,
         typename T_IntType,
         typename T_LongType,
         typename T_LongLongType,
         typename T_PointerType>
using convert_base_types_t =
  typename convert_detail::convert_base_types_t_helper<T,
                                                       T_ShortType,
                                                       T_IntType,
                                                       T_LongType,
                                                       T_LongLongType,
                                                       T_PointerType>::type;

}