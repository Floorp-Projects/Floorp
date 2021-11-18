#pragma once
// IWYU pragma: private, include "rlbox.hpp"
// IWYU pragma: friend "rlbox_.*\.hpp"

namespace rlbox {

template<typename T, typename T_Sbx>
class tainted_opaque
{
private:
  T data{ 0 };

public:
  template<typename T2 = T>
  void set_zero()
  {
    data = 0;
  }
};

template<typename T, typename T_Sbx>
class tainted;

template<typename T, typename T_Sbx>
class tainted_volatile;

class tainted_boolean_hint;

class tainted_int_hint;

template<typename T_Sbx>
class rlbox_sandbox;

template<typename T, typename T_Sbx>
class sandbox_callback;

template<typename T, typename T_Sbx>
class app_pointer;

class rlbox_noop_sandbox;

class rlbox_dylib_sandbox;
}

#define RLBOX_DEFINE_BASE_TYPES_FOR(SBXNAME, SBXTYPE)                          \
  namespace rlbox {                                                            \
    class rlbox_##SBXTYPE##_sandbox;                                           \
  }                                                                            \
  using rlbox_##SBXNAME##_sandbox_type = rlbox::rlbox_##SBXTYPE##_sandbox;     \
  using rlbox_sandbox_##SBXNAME =                                              \
    rlbox::rlbox_sandbox<rlbox_##SBXNAME##_sandbox_type>;                      \
  template<typename T>                                                         \
  using sandbox_callback_##SBXNAME =                                           \
    rlbox::sandbox_callback<T, rlbox_##SBXNAME##_sandbox_type>;                \
  template<typename T>                                                         \
  using tainted_##SBXNAME = rlbox::tainted<T, rlbox_##SBXNAME##_sandbox_type>; \
  template<typename T>                                                         \
  using tainted_opaque_##SBXNAME =                                             \
    rlbox::tainted_opaque<T, rlbox_##SBXNAME##_sandbox_type>;                  \
  template<typename T>                                                         \
  using tainted_volatile_##SBXNAME =                                           \
    rlbox::tainted_volatile<T, rlbox_##SBXNAME##_sandbox_type>;                \
  using rlbox::tainted_boolean_hint;                                           \
  template<typename T>                                                         \
  using app_pointer_##SBXNAME =                                                \
    rlbox::app_pointer<T, rlbox_##SBXNAME##_sandbox_type>;
