#pragma once
// IWYU pragma: private, include "rlbox.hpp"
// IWYU pragma: friend "rlbox_.*\.hpp"

namespace rlbox {

template<typename T, typename T_Sbx>
class tainted_opaque
{
private:
  T data{ 0 };
};

template<typename T, typename T_Sbx>
class tainted;

template<typename T, typename T_Sbx>
class tainted_volatile;

/**
 * @brief Tainted boolean value that serves as a "hint" and not a definite
 * answer.  Comparisons with a tainted_volatile return such hints.  They are
 * not `tainted<bool>` values because a compromised sandbox can modify
 * tainted_volatile data at any time.
 */
class tainted_boolean_hint
{
private:
  bool val;

public:
  tainted_boolean_hint(bool init)
    : val(init)
  {}
  tainted_boolean_hint(const tainted_boolean_hint&) = default;
  inline tainted_boolean_hint& operator=(bool rhs)
  {
    val = rhs;
    return *this;
  }
  inline tainted_boolean_hint operator!() { return tainted_boolean_hint(!val); }
  template<size_t N>
  inline bool unverified_safe_because(const char (&reason)[N]) const
  {
    (void)reason; /* unused */
    return val;
  }
};

template<typename T_Sbx>
class rlbox_sandbox;

template<typename T, typename T_Sbx>
class sandbox_callback;

class rlbox_noop_sandbox;
}
