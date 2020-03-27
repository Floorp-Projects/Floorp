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
  inline bool UNSAFE_unverified() const { return val; }
  inline bool UNSAFE_unverified() { return val; }
  inline auto INTERNAL_unverified_safe() { return UNSAFE_unverified(); }
  inline auto INTERNAL_unverified_safe() const { return UNSAFE_unverified(); }
};

/**
 * @brief Tainted integer value that serves as a "hint" and not a definite
 * answer.  Comparisons with a tainted_volatile return such hints.  They are
 * not `tainted<int>` values because a compromised sandbox can modify
 * tainted_volatile data at any time.
 */
class tainted_int_hint
{
private:
  int val;

public:
  tainted_int_hint(int init)
    : val(init)
  {}
  tainted_int_hint(const tainted_int_hint&) = default;
  inline tainted_int_hint& operator=(int rhs)
  {
    val = rhs;
    return *this;
  }
  inline tainted_boolean_hint operator!() { return tainted_boolean_hint(!val); }
  template<size_t N>
  inline int unverified_safe_because(const char (&reason)[N]) const
  {
    (void)reason; /* unused */
    return val;
  }
  inline int UNSAFE_unverified() const { return val; }
  inline int UNSAFE_unverified() { return val; }
  inline auto INTERNAL_unverified_safe() { return UNSAFE_unverified(); }
  inline auto INTERNAL_unverified_safe() const { return UNSAFE_unverified(); }
};

template<typename T_Sbx>
class rlbox_sandbox;

template<typename T, typename T_Sbx>
class sandbox_callback;

class rlbox_noop_sandbox;
}
