#pragma once
// IWYU pragma: private, include "rlbox.hpp"
// IWYU pragma: friend "rlbox_.*\.hpp"

namespace rlbox {

template<typename T, typename T_Sbx>
class tainted;

template<typename T, typename T_Sbx>
class tainted_opaque
{
private:
  T data {0};
};

template<typename T, typename T_Sbx>
class tainted_volatile;

template<typename T_Sbx>
class rlbox_sandbox;

template<typename T, typename T_Sbx>
class sandbox_callback;

template<typename T, typename T_Sbx>
class sandbox_function;

class rlbox_noop_sandbox;
}