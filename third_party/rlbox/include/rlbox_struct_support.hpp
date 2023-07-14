#pragma once
// IWYU pragma: private, include "rlbox.hpp"
// IWYU pragma: friend "rlbox_.*\.hpp"

#include <cstring>
#include <functional>
#include <type_traits>

#include "rlbox_conversion.hpp"
#include "rlbox_helpers.hpp"
#include "rlbox_types.hpp"
#include "rlbox_wrapper_traits.hpp"

namespace rlbox::detail {

template<typename T, typename T_Sbx, typename T_Enable = void>
struct convert_to_sandbox_equivalent_helper;

template<typename T, typename T_Sbx>
struct convert_to_sandbox_equivalent_helper<
  T,
  T_Sbx,
  std::enable_if_t<!std::is_class_v<T>>>
{
  using type = typename rlbox_sandbox<
    T_Sbx>::template convert_to_sandbox_equivalent_nonclass_t<T>;
};

template<typename T, typename T_Sbx>
using convert_to_sandbox_equivalent_t =
  typename convert_to_sandbox_equivalent_helper<T, T_Sbx>::type;

// This is used by rlbox_load_structs_from_library to test the current namespace
struct markerStruct
{};
}

#define helper_create_converted_field(fieldType, fieldName, isFrozen)          \
  typename detail::convert_to_sandbox_equivalent_t<fieldType, T_Sbx> fieldName;

#define helper_no_op()

#define sandbox_equivalent_specialization(T, libId)                            \
  template<typename T_Sbx>                                                     \
  struct Sbx_##libId##_##T                                                     \
  {                                                                            \
    sandbox_fields_reflection_##libId##_class_##T(                             \
      helper_create_converted_field,                                           \
      helper_no_op)                                                            \
  };                                                                           \
                                                                               \
  /* add convert_to_sandbox_equivalent_t specialization for new struct */      \
  namespace detail {                                                           \
    template<typename T_Template, typename T_Sbx>                              \
    struct convert_to_sandbox_equivalent_helper<                               \
      T_Template,                                                              \
      T_Sbx,                                                                   \
      std::enable_if_t<std::is_same_v<T_Template, T>>>                         \
    {                                                                          \
      using type = Sbx_##libId##_##T<T_Sbx>;                                   \
    };                                                                         \
  }

#define helper_create_tainted_field(                                           \
  fieldType, fieldName, isFrozen, MaybeConst)                                  \
  MaybeConst tainted<fieldType, T_Sbx> fieldName;

#define helper_create_tainted_vol_field(                                       \
  fieldType, fieldName, isFrozen, MaybeConst)                                  \
  MaybeConst tainted_volatile<fieldType, T_Sbx> fieldName;

#define helper_convert_type(fieldType, fieldName, isFrozen)                    \
  ::rlbox::detail::convert_type<T_Sbx, Direction, Context>(                    \
    lhs.fieldName, rhs.fieldName, example_unsandboxed_ptr, sandbox_ptr);

#define helper_find_example_pointer_or_null(fieldType, fieldName, isFrozen)    \
  {                                                                            \
    const void* ret = fieldName.find_example_pointer_or_null();                \
    if (ret != nullptr) {                                                      \
      return ret;                                                              \
    }                                                                          \
  }

#define tainted_data_specialization_helper(MaybeConst, T, libId)               \
                                                                               \
  template<typename T_Sbx>                                                     \
  class tainted_volatile<MaybeConst T, T_Sbx>                                  \
  {                                                                            \
    KEEP_CLASSES_FRIENDLY                                                      \
    KEEP_CAST_FRIENDLY                                                         \
                                                                               \
  private:                                                                     \
    inline MaybeConst Sbx_##libId##_##T<T_Sbx>&                                \
    get_sandbox_value_ref() noexcept                                           \
    {                                                                          \
      return *reinterpret_cast<MaybeConst Sbx_##libId##_##T<T_Sbx>*>(this);    \
    }                                                                          \
                                                                               \
    inline const Sbx_##libId##_##T<T_Sbx>& get_sandbox_value_ref()             \
      const noexcept                                                           \
    {                                                                          \
      return *reinterpret_cast<const Sbx_##libId##_##T<T_Sbx>*>(this);         \
    }                                                                          \
                                                                               \
    inline T get_raw_value() const noexcept                                    \
    {                                                                          \
      T lhs;                                                                   \
      const auto& rhs = get_sandbox_value_ref();                               \
      constexpr auto Direction =                                               \
        detail::adjust_type_direction::TO_APPLICATION;                         \
      constexpr auto Context = detail::adjust_type_context::EXAMPLE;           \
      /* This is a tainted_volatile, so its address is a valid example for use \
       * as example_unsandboxed_ptr */                                         \
      const void* example_unsandboxed_ptr = &rhs;                              \
      rlbox_sandbox<T_Sbx>* sandbox_ptr = nullptr;                             \
      sandbox_fields_reflection_##libId##_class_##T(helper_convert_type,       \
                                                    helper_no_op)              \
                                                                               \
        return lhs;                                                            \
    }                                                                          \
                                                                               \
    /* get_raw_sandbox_value has to return a custom struct to deal with the    \
     * adjusted machine model, to ensure */                                    \
    inline Sbx_##libId##_##T<T_Sbx> get_raw_sandbox_value() const noexcept     \
    {                                                                          \
      auto ret_ptr = reinterpret_cast<const Sbx_##libId##_##T<T_Sbx>*>(this);  \
      return *ret_ptr;                                                         \
    }                                                                          \
                                                                               \
    tainted_volatile() = default;                                              \
    tainted_volatile(const tainted_volatile<MaybeConst T, T_Sbx>& p) =         \
      default;                                                                 \
                                                                               \
  public:                                                                      \
    sandbox_fields_reflection_##libId##_class_##T(                             \
      helper_create_tainted_vol_field,                                         \
      helper_no_op,                                                            \
      MaybeConst)                                                              \
                                                                               \
      inline tainted<MaybeConst T*, T_Sbx>                                     \
      operator&() const noexcept                                               \
    {                                                                          \
      auto ref_cast =                                                          \
        reinterpret_cast<MaybeConst T*>(&get_sandbox_value_ref());             \
      auto ret = tainted<MaybeConst T*, T_Sbx>::internal_factory(ref_cast);    \
      return ret;                                                              \
    }                                                                          \
                                                                               \
    inline auto UNSAFE_unverified() const { return get_raw_value(); }          \
    inline auto UNSAFE_sandboxed(rlbox_sandbox<T_Sbx>& sandbox) const          \
    {                                                                          \
      return get_raw_sandbox_value(sandbox);                                   \
    }                                                                          \
                                                                               \
    template<size_t N>                                                         \
    inline auto unverified_safe_because(const char (&reason)[N]) const         \
    {                                                                          \
      RLBOX_UNUSED(reason);                                                    \
      return UNSAFE_unverified();                                              \
    }                                                                          \
                                                                               \
    T copy_and_verify(std::function<T(tainted<T, T_Sbx>)> verifier)            \
    {                                                                          \
      tainted<T, T_Sbx> val(*this);                                            \
      return verifier(val);                                                    \
    }                                                                          \
                                                                               \
    /* Can't define this yet due, to mutually dependent definition between     \
    tainted and tainted_volatile for structs */                                \
    inline tainted_volatile<MaybeConst T, T_Sbx>& operator=(                   \
      const tainted<T, T_Sbx>& rhs);                                           \
  };                                                                           \
                                                                               \
  template<typename T_Sbx>                                                     \
  class tainted<MaybeConst T, T_Sbx>                                           \
  {                                                                            \
    KEEP_CLASSES_FRIENDLY                                                      \
    KEEP_CAST_FRIENDLY                                                         \
                                                                               \
  private:                                                                     \
    inline MaybeConst T& get_raw_value_ref() noexcept                          \
    {                                                                          \
      return *reinterpret_cast<MaybeConst T*>(this);                           \
    }                                                                          \
                                                                               \
    inline const T& get_raw_value_ref() const noexcept                         \
    {                                                                          \
      return *reinterpret_cast<const T*>(this);                                \
    }                                                                          \
                                                                               \
    inline T get_raw_value() const noexcept                                    \
    {                                                                          \
      auto ret_ptr = reinterpret_cast<const T*>(this);                         \
      return *ret_ptr;                                                         \
    }                                                                          \
                                                                               \
    /* get_raw_sandbox_value has to return a custom struct to deal with the    \
     * adjusted machine model, to ensure */                                    \
    inline Sbx_##libId##_##T<T_Sbx> get_raw_sandbox_value(                     \
      rlbox_sandbox<T_Sbx>& sandbox) const noexcept                            \
    {                                                                          \
      Sbx_##libId##_##T<T_Sbx> lhs;                                            \
      const auto& rhs = get_raw_value_ref();                                   \
      constexpr auto Direction = detail::adjust_type_direction::TO_SANDBOX;    \
      constexpr auto Context = detail::adjust_type_context::SANDBOX;           \
      const void* example_unsandboxed_ptr = nullptr;                           \
      rlbox_sandbox<T_Sbx>* sandbox_ptr = &sandbox;                            \
      sandbox_fields_reflection_##libId##_class_##T(helper_convert_type,       \
                                                    helper_no_op)              \
                                                                               \
        return lhs;                                                            \
    }                                                                          \
                                                                               \
    inline const void* find_example_pointer_or_null() const noexcept           \
    {                                                                          \
      sandbox_fields_reflection_##libId##_class_##T(                           \
        helper_find_example_pointer_or_null, helper_no_op)                     \
                                                                               \
        return nullptr;                                                        \
    }                                                                          \
                                                                               \
  public:                                                                      \
    sandbox_fields_reflection_##libId##_class_##T(helper_create_tainted_field, \
                                                  helper_no_op,                \
                                                  MaybeConst)                  \
                                                                               \
      tainted() = default;                                                     \
    tainted(const tainted<MaybeConst T, T_Sbx>& p) = default;                  \
                                                                               \
    tainted(const tainted_volatile<T, T_Sbx>& p)                               \
    {                                                                          \
      auto& lhs = get_raw_value_ref();                                         \
      auto& rhs = p.get_sandbox_value_ref();                                   \
      constexpr auto Direction =                                               \
        detail::adjust_type_direction::TO_APPLICATION;                         \
      constexpr auto Context = detail::adjust_type_context::EXAMPLE;           \
      /* This is a tainted_volatile, so its address is a valid for use as */   \
      /* example_unsandboxed_ptr */                                            \
      const void* example_unsandboxed_ptr = &rhs;                              \
      rlbox_sandbox<T_Sbx>* sandbox_ptr = nullptr;                             \
      sandbox_fields_reflection_##libId##_class_##T(helper_convert_type,       \
                                                    helper_no_op)              \
    }                                                                          \
                                                                               \
    inline tainted_opaque<MaybeConst T, T_Sbx> to_opaque()                     \
    {                                                                          \
      return *reinterpret_cast<tainted_opaque<MaybeConst T, T_Sbx>*>(this);    \
    }                                                                          \
                                                                               \
    inline auto UNSAFE_unverified() const { return get_raw_value(); }          \
    inline auto UNSAFE_sandboxed(rlbox_sandbox<T_Sbx>& sandbox) const          \
    {                                                                          \
      return get_raw_sandbox_value(sandbox);                                   \
    }                                                                          \
                                                                               \
    template<size_t N>                                                         \
    inline auto unverified_safe_because(const char (&reason)[N]) const         \
    {                                                                          \
      RLBOX_UNUSED(reason);                                                    \
      return UNSAFE_unverified();                                              \
    }                                                                          \
                                                                               \
    T copy_and_verify(std::function<T(tainted<T, T_Sbx>)> verifier)            \
    {                                                                          \
      return verifier(*this);                                                  \
    }                                                                          \
  };                                                                           \
                                                                               \
  /* Had to delay the definition due, to mutually dependence between           \
    tainted and tainted_volatile for structs */                                \
  template<typename T_Sbx>                                                     \
  inline tainted_volatile<MaybeConst T, T_Sbx>&                                \
  tainted_volatile<MaybeConst T, T_Sbx>::operator=(                            \
    const tainted<T, T_Sbx>& rhs_wrap)                                         \
  {                                                                            \
    auto& lhs = get_sandbox_value_ref();                                       \
    auto& rhs = rhs_wrap.get_raw_value_ref();                                  \
    constexpr auto Direction = detail::adjust_type_direction::TO_SANDBOX;      \
    constexpr auto Context = detail::adjust_type_context::EXAMPLE;             \
    /* This is a tainted_volatile, so its address is a valid example for */    \
    /*  use as example_unsandboxed_ptr */                                      \
    const void* example_unsandboxed_ptr = &lhs;                                \
    rlbox_sandbox<T_Sbx>* sandbox_ptr = nullptr;                               \
    sandbox_fields_reflection_##libId##_class_##T(helper_convert_type,         \
                                                  helper_no_op)                \
                                                                               \
      return *this;                                                            \
  }

#define tainted_data_specialization(T, libId)                                  \
  tainted_data_specialization_helper(, T, libId)                               \
    tainted_data_specialization_helper(const, T, libId)

#define convert_type_specialization(T, libId)                                  \
  namespace detail {                                                           \
    template<typename T_Sbx,                                                   \
             detail::adjust_type_direction Direction,                          \
             adjust_type_context Context,                                      \
             typename T_From>                                                  \
    class convert_type_class<T_Sbx, Direction, Context, T, T_From>             \
    {                                                                          \
    public:                                                                    \
      static inline void run(T& lhs,                                           \
                             const T_From& rhs,                                \
                             const void* example_unsandboxed_ptr,              \
                             rlbox_sandbox<T_Sbx>* sandbox_ptr)                \
      {                                                                        \
        sandbox_fields_reflection_##libId##_class_##T(helper_convert_type,     \
                                                      helper_no_op)            \
      }                                                                        \
    };                                                                         \
                                                                               \
    template<typename T_Sbx,                                                   \
             detail::adjust_type_direction Direction,                          \
             adjust_type_context Context,                                      \
             typename T_From>                                                  \
    class convert_type_class<T_Sbx,                                            \
                             Direction,                                        \
                             Context,                                          \
                             Sbx_##libId##_##T<T_Sbx>,                         \
                             T_From>                                           \
    {                                                                          \
    public:                                                                    \
      static inline void run(Sbx_##libId##_##T<T_Sbx>& lhs,                    \
                             const T_From& rhs,                                \
                             const void* example_unsandboxed_ptr,              \
                             rlbox_sandbox<T_Sbx>* sandbox_ptr)                \
      {                                                                        \
        sandbox_fields_reflection_##libId##_class_##T(helper_convert_type,     \
                                                      helper_no_op)            \
      }                                                                        \
    };                                                                         \
  }

// clang-format off
#define rlbox_load_structs_from_library(libId)                                 \
  namespace rlbox {                                                            \
    /* check that this macro is called in a global namespace */                \
    static_assert(                                                             \
      ::rlbox::detail::is_member_of_rlbox_detail<detail::markerStruct>,        \
      "Invoke rlbox_load_structs_from_library in the global namespace");       \
                                                                               \
    sandbox_fields_reflection_##libId##_allClasses(                            \
      sandbox_equivalent_specialization)                                       \
                                                                               \
    sandbox_fields_reflection_##libId##_allClasses(                            \
      tainted_data_specialization)                                             \
                                                                               \
    sandbox_fields_reflection_##libId##_allClasses(                            \
      convert_type_specialization)                                             \
  }                                                                            \
  RLBOX_REQUIRE_SEMI_COLON

// clang-format on