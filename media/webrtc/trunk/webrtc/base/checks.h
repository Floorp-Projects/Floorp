/*
 *  Copyright 2006 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_BASE_CHECKS_H_
#define WEBRTC_BASE_CHECKS_H_

#include <sstream>
#include <string>

#ifdef WEBRTC_CHROMIUM_BUILD
// Include logging.h in a Chromium build to enable the overrides mechanism for
// using Chromium's macros. Otherwise, don't depend on logging.h.
// TODO(ajm): Ideally, checks.h would be combined with logging.h, but
// consolidation with system_wrappers/logging.h should happen first.
#include "webrtc/base/logging.h"
#endif
#include "webrtc/typedefs.h"

// The macros here print a message to stderr and abort under various
// conditions. All will accept additional stream messages. For example:
// DCHECK_EQ(foo, bar) << "I'm printed when foo != bar.";
//
// - CHECK(x) is an assertion that x is always true, and that if it isn't, it's
//   better to terminate the process than to continue. During development, the
//   reason that it's better to terminate might simply be that the error
//   handling code isn't in place yet; in production, the reason might be that
//   the author of the code truly believes that x will always be true, but that
//   she recognizes that if she is wrong, abrupt and unpleasant process
//   termination is still better than carrying on with the assumption violated.
//
//   CHECK always evaluates its argument, so it's OK for x to have side
//   effects.
//
// - DCHECK(x) is the same as CHECK(x)---an assertion that x is always
//   true---except that x will only be evaluated in debug builds; in production
//   builds, x is simply assumed to be true. This is useful if evaluating x is
//   expensive and the expected cost of failing to detect the violated
//   assumption is acceptable. You should not handle cases where a production
//   build fails to spot a violated condition, even those that would result in
//   crashes. If the code needs to cope with the error, make it cope, but don't
//   call DCHECK; if the condition really can't occur, but you'd sleep better
//   at night knowing that the process will suicide instead of carrying on in
//   case you were wrong, use CHECK instead of DCHECK.
//
//   DCHECK only evaluates its argument in debug builds, so if x has visible
//   side effects, you need to write e.g.
//     bool w = x; DCHECK(w);
//
// - CHECK_EQ, _NE, _GT, ..., and DCHECK_EQ, _NE, _GT, ... are specialized
//   variants of CHECK and DCHECK that print prettier messages if the condition
//   doesn't hold. Prefer them to raw CHECK and DCHECK.
//
// - FATAL() aborts unconditionally.

namespace rtc {

// The use of overrides/webrtc/base/logging.h in a Chromium build results in
// redefined macro errors. Fortunately, Chromium's macros can be used as drop-in
// replacements for the standalone versions.
#ifndef WEBRTC_CHROMIUM_BUILD

// Helper macro which avoids evaluating the arguments to a stream if
// the condition doesn't hold.
#define LAZY_STREAM(stream, condition)                                        \
  !(condition) ? static_cast<void>(0) : rtc::FatalMessageVoidify() & (stream)

// The actual stream used isn't important.
#define EAT_STREAM_PARAMETERS                                           \
  true ? static_cast<void>(0)                                           \
       : rtc::FatalMessageVoidify() & rtc::FatalMessage("", 0).stream()

// CHECK dies with a fatal error if condition is not true.  It is *not*
// controlled by NDEBUG, so the check will be executed regardless of
// compilation mode.
//
// We make sure CHECK et al. always evaluates their arguments, as
// doing CHECK(FunctionWithSideEffect()) is a common idiom.
#define CHECK(condition)                                                    \
  LAZY_STREAM(rtc::FatalMessage(__FILE__, __LINE__).stream(), !(condition)) \
  << "Check failed: " #condition << std::endl << "# "

// Helper macro for binary operators.
// Don't use this macro directly in your code, use CHECK_EQ et al below.
//
// TODO(akalin): Rewrite this so that constructs like if (...)
// CHECK_EQ(...) else { ... } work properly.
#define CHECK_OP(name, op, val1, val2)                      \
  if (std::string* _result =                                \
      rtc::Check##name##Impl((val1), (val2),                \
                             #val1 " " #op " " #val2))      \
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

// Helper functions for CHECK_OP macro.
// The (int, int) specialization works around the issue that the compiler
// will not instantiate the template version of the function on values of
// unnamed enum type - see comment below.
#define DEFINE_CHECK_OP_IMPL(name, op) \
  template <class t1, class t2> \
  inline std::string* Check##name##Impl(const t1& v1, const t2& v2, \
                                        const char* names) { \
    if (v1 op v2) return NULL; \
    else return rtc::MakeCheckOpString(v1, v2, names); \
  } \
  inline std::string* Check##name##Impl(int v1, int v2, const char* names) { \
    if (v1 op v2) return NULL; \
    else return rtc::MakeCheckOpString(v1, v2, names); \
  }
DEFINE_CHECK_OP_IMPL(EQ, ==)
DEFINE_CHECK_OP_IMPL(NE, !=)
DEFINE_CHECK_OP_IMPL(LE, <=)
DEFINE_CHECK_OP_IMPL(LT, < )
DEFINE_CHECK_OP_IMPL(GE, >=)
DEFINE_CHECK_OP_IMPL(GT, > )
#undef DEFINE_CHECK_OP_IMPL

#define CHECK_EQ(val1, val2) CHECK_OP(EQ, ==, val1, val2)
#define CHECK_NE(val1, val2) CHECK_OP(NE, !=, val1, val2)
#define CHECK_LE(val1, val2) CHECK_OP(LE, <=, val1, val2)
#define CHECK_LT(val1, val2) CHECK_OP(LT, < , val1, val2)
#define CHECK_GE(val1, val2) CHECK_OP(GE, >=, val1, val2)
#define CHECK_GT(val1, val2) CHECK_OP(GT, > , val1, val2)

// The DCHECK macro is equivalent to CHECK except that it only generates code in
// debug builds.
#if (!defined(NDEBUG) || defined(DCHECK_ALWAYS_ON))
#define DCHECK(condition) CHECK(condition)
#define DCHECK_EQ(v1, v2) CHECK_EQ(v1, v2)
#define DCHECK_NE(v1, v2) CHECK_NE(v1, v2)
#define DCHECK_LE(v1, v2) CHECK_LE(v1, v2)
#define DCHECK_LT(v1, v2) CHECK_LT(v1, v2)
#define DCHECK_GE(v1, v2) CHECK_GE(v1, v2)
#define DCHECK_GT(v1, v2) CHECK_GT(v1, v2)
#else
#define DCHECK(condition) EAT_STREAM_PARAMETERS
#define DCHECK_EQ(v1, v2) EAT_STREAM_PARAMETERS
#define DCHECK_NE(v1, v2) EAT_STREAM_PARAMETERS
#define DCHECK_LE(v1, v2) EAT_STREAM_PARAMETERS
#define DCHECK_LT(v1, v2) EAT_STREAM_PARAMETERS
#define DCHECK_GE(v1, v2) EAT_STREAM_PARAMETERS
#define DCHECK_GT(v1, v2) EAT_STREAM_PARAMETERS
#endif

// This is identical to LogMessageVoidify but in name.
class FatalMessageVoidify {
 public:
  FatalMessageVoidify() { }
  // This has to be an operator with a precedence lower than << but
  // higher than ?:
  void operator&(std::ostream&) { }
};

#endif  // WEBRTC_CHROMIUM_BUILD

#define FATAL() rtc::FatalMessage(__FILE__, __LINE__).stream()
// TODO(ajm): Consider adding NOTIMPLEMENTED and NOTREACHED macros when
// base/logging.h and system_wrappers/logging.h are consolidated such that we
// can match the Chromium behavior.

// Like a stripped-down LogMessage from logging.h, except that it aborts.
class FatalMessage {
 public:
  FatalMessage(const char* file, int line);
  // Used for CHECK_EQ(), etc. Takes ownership of the given string.
  FatalMessage(const char* file, int line, std::string* result);
  NO_RETURN ~FatalMessage();

  std::ostream& stream() { return stream_; }

 private:
  void Init(const char* file, int line);

  std::ostringstream stream_;
};

}  // namespace rtc

#endif  // WEBRTC_BASE_CHECKS_H_
