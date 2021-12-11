/*
 *  Copyright 2006 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_CHECKS_H_
#define RTC_BASE_CHECKS_H_

#include "typedefs.h"  // NOLINT(build/include)

// If you for some reson need to know if DCHECKs are on, test the value of
// RTC_DCHECK_IS_ON. (Test its value, not if it's defined; it'll always be
// defined, to either a true or a false value.)
#if !defined(NDEBUG) || defined(DCHECK_ALWAYS_ON)
#define RTC_DCHECK_IS_ON 1
#else
#define RTC_DCHECK_IS_ON 0
#endif

#ifdef __cplusplus
extern "C" {
#endif
NO_RETURN void rtc_FatalMessage(const char* file, int line, const char* msg);
#ifdef __cplusplus
}  // extern "C"
#endif

#ifdef __cplusplus
// C++ version.

#include <sstream>
#include <string>

#include "rtc_base/numerics/safe_compare.h"

// The macros here print a message to stderr and abort under various
// conditions. All will accept additional stream messages. For example:
// RTC_DCHECK_EQ(foo, bar) << "I'm printed when foo != bar.";
//
// - RTC_CHECK(x) is an assertion that x is always true, and that if it isn't,
//   it's better to terminate the process than to continue. During development,
//   the reason that it's better to terminate might simply be that the error
//   handling code isn't in place yet; in production, the reason might be that
//   the author of the code truly believes that x will always be true, but that
//   she recognizes that if she is wrong, abrupt and unpleasant process
//   termination is still better than carrying on with the assumption violated.
//
//   RTC_CHECK always evaluates its argument, so it's OK for x to have side
//   effects.
//
// - RTC_DCHECK(x) is the same as RTC_CHECK(x)---an assertion that x is always
//   true---except that x will only be evaluated in debug builds; in production
//   builds, x is simply assumed to be true. This is useful if evaluating x is
//   expensive and the expected cost of failing to detect the violated
//   assumption is acceptable. You should not handle cases where a production
//   build fails to spot a violated condition, even those that would result in
//   crashes. If the code needs to cope with the error, make it cope, but don't
//   call RTC_DCHECK; if the condition really can't occur, but you'd sleep
//   better at night knowing that the process will suicide instead of carrying
//   on in case you were wrong, use RTC_CHECK instead of RTC_DCHECK.
//
//   RTC_DCHECK only evaluates its argument in debug builds, so if x has visible
//   side effects, you need to write e.g.
//     bool w = x; RTC_DCHECK(w);
//
// - RTC_CHECK_EQ, _NE, _GT, ..., and RTC_DCHECK_EQ, _NE, _GT, ... are
//   specialized variants of RTC_CHECK and RTC_DCHECK that print prettier
//   messages if the condition doesn't hold. Prefer them to raw RTC_CHECK and
//   RTC_DCHECK.
//
// - FATAL() aborts unconditionally.
//
// TODO(ajm): Ideally, checks.h would be combined with logging.h, but
// consolidation with system_wrappers/logging.h should happen first.

namespace rtc {

// Helper macro which avoids evaluating the arguments to a stream if
// the condition doesn't hold.
#define RTC_LAZY_STREAM(stream, condition)                                    \
  !(condition) ? static_cast<void>(0) : rtc::FatalMessageVoidify() & (stream)

// The actual stream used isn't important. We reference |ignored| in the code
// but don't evaluate it; this is to avoid "unused variable" warnings (we do so
// in a particularly convoluted way with an extra ?: because that appears to be
// the simplest construct that keeps Visual Studio from complaining about
// condition being unused).
#define RTC_EAT_STREAM_PARAMETERS(ignored) \
  (true ? true : ((void)(ignored), true))  \
      ? static_cast<void>(0)               \
      : rtc::FatalMessageVoidify() & rtc::FatalMessage("", 0).stream()

// Call RTC_EAT_STREAM_PARAMETERS with an argument that fails to compile if
// values of the same types as |a| and |b| can't be compared with the given
// operation, and that would evaluate |a| and |b| if evaluated.
#define RTC_EAT_STREAM_PARAMETERS_OP(op, a, b) \
  RTC_EAT_STREAM_PARAMETERS(((void)rtc::Safe##op(a, b)))

// RTC_CHECK dies with a fatal error if condition is not true. It is *not*
// controlled by NDEBUG or anything else, so the check will be executed
// regardless of compilation mode.
//
// We make sure RTC_CHECK et al. always evaluates their arguments, as
// doing RTC_CHECK(FunctionWithSideEffect()) is a common idiom.
#define RTC_CHECK(condition)                                      \
  RTC_LAZY_STREAM(rtc::FatalMessage(__FILE__, __LINE__).stream(), \
                  !(condition))                                   \
      << "Check failed: " #condition << std::endl << "# "

// Helper macro for binary operators.
// Don't use this macro directly in your code, use RTC_CHECK_EQ et al below.
//
// TODO(akalin): Rewrite this so that constructs like if (...)
// RTC_CHECK_EQ(...) else { ... } work properly.
#define RTC_CHECK_OP(name, op, val1, val2)                                 \
  if (std::string* _result =                                               \
          rtc::Check##name##Impl((val1), (val2), #val1 " " #op " " #val2)) \
    rtc::FatalMessage(__FILE__, __LINE__, _result).stream()

// Build the error message string.  This is separate from the "Impl"
// function template because it is not performance critical and so can
// be out of line, while the "Impl" code should be inline.  Caller
// takes ownership of the returned string.
template<class t1, class t2>
std::string* MakeCheckOpString(const t1& v1, const t2& v2, const char* names) {
  std::ostringstream ss;
  ss << names << " (" << v1 << " vs. " << v2 << ")";
  std::string* msg = new std::string(ss.str());
  return msg;
}

// MSVC doesn't like complex extern templates and DLLs.
#if !defined(COMPILER_MSVC)
// Commonly used instantiations of MakeCheckOpString<>. Explicitly instantiated
// in logging.cc.
extern template std::string* MakeCheckOpString<int, int>(
    const int&, const int&, const char* names);
extern template
std::string* MakeCheckOpString<unsigned long, unsigned long>(
    const unsigned long&, const unsigned long&, const char* names);
extern template
std::string* MakeCheckOpString<unsigned long, unsigned int>(
    const unsigned long&, const unsigned int&, const char* names);
extern template
std::string* MakeCheckOpString<unsigned int, unsigned long>(
    const unsigned int&, const unsigned long&, const char* names);
extern template
std::string* MakeCheckOpString<std::string, std::string>(
    const std::string&, const std::string&, const char* name);
#endif

// Helper functions for RTC_CHECK_OP macro.
// The (int, int) specialization works around the issue that the compiler
// will not instantiate the template version of the function on values of
// unnamed enum type - see comment below.
#define DEFINE_RTC_CHECK_OP_IMPL(name)                                       \
  template <class t1, class t2>                                              \
  inline std::string* Check##name##Impl(const t1& v1, const t2& v2,          \
                                        const char* names) {                 \
    if (rtc::Safe##name(v1, v2))                                             \
      return nullptr;                                                        \
    else                                                                     \
      return rtc::MakeCheckOpString(v1, v2, names);                          \
  }                                                                          \
  inline std::string* Check##name##Impl(int v1, int v2, const char* names) { \
    if (rtc::Safe##name(v1, v2))                                             \
      return nullptr;                                                        \
    else                                                                     \
      return rtc::MakeCheckOpString(v1, v2, names);                          \
  }
DEFINE_RTC_CHECK_OP_IMPL(Eq)
DEFINE_RTC_CHECK_OP_IMPL(Ne)
DEFINE_RTC_CHECK_OP_IMPL(Le)
DEFINE_RTC_CHECK_OP_IMPL(Lt)
DEFINE_RTC_CHECK_OP_IMPL(Ge)
DEFINE_RTC_CHECK_OP_IMPL(Gt)
#undef DEFINE_RTC_CHECK_OP_IMPL

#define RTC_CHECK_EQ(val1, val2) RTC_CHECK_OP(Eq, ==, val1, val2)
#define RTC_CHECK_NE(val1, val2) RTC_CHECK_OP(Ne, !=, val1, val2)
#define RTC_CHECK_LE(val1, val2) RTC_CHECK_OP(Le, <=, val1, val2)
#define RTC_CHECK_LT(val1, val2) RTC_CHECK_OP(Lt, <, val1, val2)
#define RTC_CHECK_GE(val1, val2) RTC_CHECK_OP(Ge, >=, val1, val2)
#define RTC_CHECK_GT(val1, val2) RTC_CHECK_OP(Gt, >, val1, val2)

// The RTC_DCHECK macro is equivalent to RTC_CHECK except that it only generates
// code in debug builds. It does reference the condition parameter in all cases,
// though, so callers won't risk getting warnings about unused variables.
#if RTC_DCHECK_IS_ON
#define RTC_DCHECK(condition) RTC_CHECK(condition)
#define RTC_DCHECK_EQ(v1, v2) RTC_CHECK_EQ(v1, v2)
#define RTC_DCHECK_NE(v1, v2) RTC_CHECK_NE(v1, v2)
#define RTC_DCHECK_LE(v1, v2) RTC_CHECK_LE(v1, v2)
#define RTC_DCHECK_LT(v1, v2) RTC_CHECK_LT(v1, v2)
#define RTC_DCHECK_GE(v1, v2) RTC_CHECK_GE(v1, v2)
#define RTC_DCHECK_GT(v1, v2) RTC_CHECK_GT(v1, v2)
#else
#define RTC_DCHECK(condition) RTC_EAT_STREAM_PARAMETERS(condition)
#define RTC_DCHECK_EQ(v1, v2) RTC_EAT_STREAM_PARAMETERS_OP(Eq, v1, v2)
#define RTC_DCHECK_NE(v1, v2) RTC_EAT_STREAM_PARAMETERS_OP(Ne, v1, v2)
#define RTC_DCHECK_LE(v1, v2) RTC_EAT_STREAM_PARAMETERS_OP(Le, v1, v2)
#define RTC_DCHECK_LT(v1, v2) RTC_EAT_STREAM_PARAMETERS_OP(Lt, v1, v2)
#define RTC_DCHECK_GE(v1, v2) RTC_EAT_STREAM_PARAMETERS_OP(Ge, v1, v2)
#define RTC_DCHECK_GT(v1, v2) RTC_EAT_STREAM_PARAMETERS_OP(Gt, v1, v2)
#endif

// This is identical to LogMessageVoidify but in name.
class FatalMessageVoidify {
 public:
  FatalMessageVoidify() { }
  // This has to be an operator with a precedence lower than << but
  // higher than ?:
  void operator&(std::ostream&) { }
};

#define RTC_UNREACHABLE_CODE_HIT false
#define RTC_NOTREACHED() RTC_DCHECK(RTC_UNREACHABLE_CODE_HIT)

// TODO(bugs.webrtc.org/8454): Add an RTC_ prefix or rename differently.
#define FATAL() rtc::FatalMessage(__FILE__, __LINE__).stream()
// TODO(ajm): Consider adding RTC_NOTIMPLEMENTED macro when
// base/logging.h and system_wrappers/logging.h are consolidated such that we
// can match the Chromium behavior.

// Like a stripped-down LogMessage from logging.h, except that it aborts.
class FatalMessage {
 public:
  FatalMessage(const char* file, int line);
  // Used for RTC_CHECK_EQ(), etc. Takes ownership of the given string.
  FatalMessage(const char* file, int line, std::string* result);
  NO_RETURN ~FatalMessage();

  std::ostream& stream() { return stream_; }

 private:
  void Init(const char* file, int line);

  std::ostringstream stream_;
};

// Performs the integer division a/b and returns the result. CHECKs that the
// remainder is zero.
template <typename T>
inline T CheckedDivExact(T a, T b) {
  RTC_CHECK_EQ(a % b, 0) << a << " is not evenly divisible by " << b;
  return a / b;
}

}  // namespace rtc

#else  // __cplusplus not defined
// C version. Lacks many features compared to the C++ version, but usage
// guidelines are the same.

#define RTC_CHECK(condition)                                             \
  do {                                                                   \
    if (!(condition)) {                                                  \
      rtc_FatalMessage(__FILE__, __LINE__, "CHECK failed: " #condition); \
    }                                                                    \
  } while (0)

#define RTC_CHECK_EQ(a, b) RTC_CHECK((a) == (b))
#define RTC_CHECK_NE(a, b) RTC_CHECK((a) != (b))
#define RTC_CHECK_LE(a, b) RTC_CHECK((a) <= (b))
#define RTC_CHECK_LT(a, b) RTC_CHECK((a) < (b))
#define RTC_CHECK_GE(a, b) RTC_CHECK((a) >= (b))
#define RTC_CHECK_GT(a, b) RTC_CHECK((a) > (b))

#define RTC_DCHECK(condition)                                             \
  do {                                                                    \
    if (RTC_DCHECK_IS_ON && !(condition)) {                               \
      rtc_FatalMessage(__FILE__, __LINE__, "DCHECK failed: " #condition); \
    }                                                                     \
  } while (0)

#define RTC_DCHECK_EQ(a, b) RTC_DCHECK((a) == (b))
#define RTC_DCHECK_NE(a, b) RTC_DCHECK((a) != (b))
#define RTC_DCHECK_LE(a, b) RTC_DCHECK((a) <= (b))
#define RTC_DCHECK_LT(a, b) RTC_DCHECK((a) < (b))
#define RTC_DCHECK_GE(a, b) RTC_DCHECK((a) >= (b))
#define RTC_DCHECK_GT(a, b) RTC_DCHECK((a) > (b))

#endif  // __cplusplus

#endif  // RTC_BASE_CHECKS_H_
