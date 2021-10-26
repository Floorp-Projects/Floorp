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
