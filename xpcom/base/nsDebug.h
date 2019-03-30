/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDebug_h___
#define nsDebug_h___

#include "nscore.h"
#include "nsError.h"

#include "nsXPCOM.h"
#include "mozilla/Assertions.h"
#include "mozilla/DbgMacro.h"
#include "mozilla/Likely.h"
#include <stdarg.h>

#ifdef DEBUG
#  include "mozilla/IntegerPrintfMacros.h"
#  include "mozilla/Printf.h"
#endif

/**
 * Warn if the given condition is true. The condition is evaluated in both
 * release and debug builds, and the result is an expression which can be
 * used in subsequent expressions, such as:
 *
 * if (NS_WARN_IF(NS_FAILED(rv)) {
 *   return rv;
 * }
 *
 * This explicit warning and return is preferred to the NS_ENSURE_* macros
 * which hide the warning and the return control flow.
 *
 * This macro can also be used outside of conditions just to issue a warning,
 * like so:
 *
 *   Unused << NS_WARN_IF(NS_FAILED(FnWithSideEffects());
 *
 * (The |Unused <<| is necessary because of the MOZ_MUST_USE annotation.)
 *
 * However, note that the argument to this macro is evaluated in all builds. If
 * you just want a warning assertion, it is better to use NS_WARNING_ASSERTION
 * (which evaluates the condition only in debug builds) like so:
 *
 *   NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "operation failed");
 *
 * @note This is C++-only
 */
#ifdef __cplusplus
#  ifdef DEBUG
inline MOZ_MUST_USE bool NS_warn_if_impl(bool aCondition, const char* aExpr,
                                         const char* aFile, int32_t aLine) {
  if (MOZ_UNLIKELY(aCondition)) {
    NS_DebugBreak(NS_DEBUG_WARNING, nullptr, aExpr, aFile, aLine);
  }
  return aCondition;
}
#    define NS_WARN_IF(condition) \
      NS_warn_if_impl(condition, #condition, __FILE__, __LINE__)
#  else
#    define NS_WARN_IF(condition) (bool)(condition)
#  endif
#endif

/**
 * Test an assertion for truth. If the expression is not true then
 * emit a warning.
 *
 * Program execution continues past the usage of this macro.
 *
 * Note also that the non-debug version of this macro does <b>not</b>
 * evaluate the message argument.
 */
#ifdef DEBUG
#  define NS_WARNING_ASSERTION(_expr, _msg)                                \
    do {                                                                   \
      if (!(_expr)) {                                                      \
        NS_DebugBreak(NS_DEBUG_WARNING, _msg, #_expr, __FILE__, __LINE__); \
      }                                                                    \
    } while (false)
#else
#  define NS_WARNING_ASSERTION(_expr, _msg) \
    do { /* nothing */                      \
    } while (false)
#endif

/**
 * Test an assertion for truth. If the expression is not true then
 * trigger a program failure.
 *
 * Note that the non-debug version of this macro does <b>not</b>
 * evaluate the message argument.
 */
#ifdef DEBUG
inline void MOZ_PretendNoReturn() MOZ_PRETEND_NORETURN_FOR_STATIC_ANALYSIS {}
#  define NS_ASSERTION(expr, str)                                          \
    do {                                                                   \
      if (!(expr)) {                                                       \
        NS_DebugBreak(NS_DEBUG_ASSERTION, str, #expr, __FILE__, __LINE__); \
        MOZ_PretendNoReturn();                                             \
      }                                                                    \
    } while (0)
#else
#  define NS_ASSERTION(expr, str) \
    do { /* nothing */            \
    } while (0)
#endif

/**
 * Log an error message.
 */
#ifdef DEBUG
#  define NS_ERROR(str)                                                    \
    do {                                                                   \
      NS_DebugBreak(NS_DEBUG_ASSERTION, str, "Error", __FILE__, __LINE__); \
      MOZ_PretendNoReturn();                                               \
    } while (0)
#else
#  define NS_ERROR(str) \
    do { /* nothing */  \
    } while (0)
#endif

/**
 * Log a warning message.
 */
#ifdef DEBUG
#  define NS_WARNING(str) \
    NS_DebugBreak(NS_DEBUG_WARNING, str, nullptr, __FILE__, __LINE__)
#else
#  define NS_WARNING(str) \
    do { /* nothing */    \
    } while (0)
#endif

/**
 * Trigger a debugger breakpoint, only in debug builds.
 */
#ifdef DEBUG
#  define NS_BREAK()                                                       \
    do {                                                                   \
      NS_DebugBreak(NS_DEBUG_BREAK, nullptr, nullptr, __FILE__, __LINE__); \
      MOZ_PretendNoReturn();                                               \
    } while (0)
#else
#  define NS_BREAK()   \
    do { /* nothing */ \
    } while (0)
#endif

/******************************************************************************
** Macros for static assertions.  These are used by the sixgill tool.
** When the tool is not running these macros are no-ops.
******************************************************************************/

/* Avoid name collision if included with other headers defining annotations. */
#ifndef HAVE_STATIC_ANNOTATIONS
#  define HAVE_STATIC_ANNOTATIONS

#  ifdef XGILL_PLUGIN

#    define STATIC_PRECONDITION(COND) __attribute__((precondition(#    COND)))
#    define STATIC_PRECONDITION_ASSUME(COND) \
      __attribute__((precondition_assume(#COND)))
#    define STATIC_POSTCONDITION(COND) __attribute__((postcondition(#    COND)))
#    define STATIC_POSTCONDITION_ASSUME(COND) \
      __attribute__((postcondition_assume(#COND)))
#    define STATIC_INVARIANT(COND) __attribute__((invariant(#    COND)))
#    define STATIC_INVARIANT_ASSUME(COND) \
      __attribute__((invariant_assume(#COND)))

/* Used to make identifiers for assert/assume annotations in a function. */
#    define STATIC_PASTE2(X, Y) X##Y
#    define STATIC_PASTE1(X, Y) STATIC_PASTE2(X, Y)

#    define STATIC_ASSUME(COND)                                          \
      do {                                                               \
        __attribute__((assume_static(#COND), unused)) int STATIC_PASTE1( \
            assume_static_, __COUNTER__);                                \
      } while (false)

#    define STATIC_ASSERT_RUNTIME(COND)                                   \
      do {                                                                \
        __attribute__((assert_static_runtime(#COND),                      \
                       unused)) int STATIC_PASTE1(assert_static_runtime_, \
                                                  __COUNTER__);           \
      } while (false)

#  else /* XGILL_PLUGIN */

#    define STATIC_PRECONDITION(COND)         /* nothing */
#    define STATIC_PRECONDITION_ASSUME(COND)  /* nothing */
#    define STATIC_POSTCONDITION(COND)        /* nothing */
#    define STATIC_POSTCONDITION_ASSUME(COND) /* nothing */
#    define STATIC_INVARIANT(COND)            /* nothing */
#    define STATIC_INVARIANT_ASSUME(COND)     /* nothing */

#    define STATIC_ASSUME(COND) \
      do { /* nothing */        \
      } while (false)
#    define STATIC_ASSERT_RUNTIME(COND) \
      do { /* nothing */                \
      } while (false)

#  endif /* XGILL_PLUGIN */

#  define STATIC_SKIP_INFERENCE STATIC_INVARIANT(skip_inference())

#endif /* HAVE_STATIC_ANNOTATIONS */

/******************************************************************************
** Macros for terminating execution when an unrecoverable condition is
** reached.  These need to be compiled regardless of the DEBUG flag.
******************************************************************************/

/* Macros for checking the trueness of an expression passed in within an
 * interface implementation.  These need to be compiled regardless of the
 * DEBUG flag. New code should use NS_WARN_IF(condition) instead!
 * @status deprecated
 */

#define NS_ENSURE_TRUE(x, ret)                     \
  do {                                             \
    if (MOZ_UNLIKELY(!(x))) {                      \
      NS_WARNING("NS_ENSURE_TRUE(" #x ") failed"); \
      return ret;                                  \
    }                                              \
  } while (false)

#define NS_ENSURE_FALSE(x, ret) NS_ENSURE_TRUE(!(x), ret)

#define NS_ENSURE_TRUE_VOID(x)                     \
  do {                                             \
    if (MOZ_UNLIKELY(!(x))) {                      \
      NS_WARNING("NS_ENSURE_TRUE(" #x ") failed"); \
      return;                                      \
    }                                              \
  } while (false)

#define NS_ENSURE_FALSE_VOID(x) NS_ENSURE_TRUE_VOID(!(x))

/******************************************************************************
** Macros for checking results
******************************************************************************/

#if defined(DEBUG) && !defined(XPCOM_GLUE_AVOID_NSPR)

#  define NS_ENSURE_SUCCESS_BODY(res, ret)            \
    mozilla::SmprintfPointer msg = mozilla::Smprintf( \
        "NS_ENSURE_SUCCESS(%s, %s) failed with "      \
        "result 0x%" PRIX32,                          \
        #res, #ret, static_cast<uint32_t>(__rv));     \
    NS_WARNING(msg.get());

#  define NS_ENSURE_SUCCESS_BODY_VOID(res)            \
    mozilla::SmprintfPointer msg = mozilla::Smprintf( \
        "NS_ENSURE_SUCCESS_VOID(%s) failed with "     \
        "result 0x%" PRIX32,                          \
        #res, static_cast<uint32_t>(__rv));           \
    NS_WARNING(msg.get());

#else

#  define NS_ENSURE_SUCCESS_BODY(res, ret) \
    NS_WARNING("NS_ENSURE_SUCCESS(" #res ", " #ret ") failed");

#  define NS_ENSURE_SUCCESS_BODY_VOID(res) \
    NS_WARNING("NS_ENSURE_SUCCESS_VOID(" #res ") failed");

#endif

#define NS_ENSURE_SUCCESS(res, ret)                                \
  do {                                                             \
    nsresult __rv = res; /* Don't evaluate |res| more than once */ \
    if (NS_FAILED(__rv)) {                                         \
      NS_ENSURE_SUCCESS_BODY(res, ret)                             \
      return ret;                                                  \
    }                                                              \
  } while (false)

#define NS_ENSURE_SUCCESS_VOID(res)    \
  do {                                 \
    nsresult __rv = res;               \
    if (NS_FAILED(__rv)) {             \
      NS_ENSURE_SUCCESS_BODY_VOID(res) \
      return;                          \
    }                                  \
  } while (false)

/******************************************************************************
** Macros for checking state and arguments upon entering interface boundaries
******************************************************************************/

#define NS_ENSURE_ARG(arg) NS_ENSURE_TRUE(arg, NS_ERROR_INVALID_ARG)

#define NS_ENSURE_ARG_POINTER(arg) NS_ENSURE_TRUE(arg, NS_ERROR_INVALID_POINTER)

#define NS_ENSURE_ARG_MIN(arg, min) \
  NS_ENSURE_TRUE((arg) >= min, NS_ERROR_INVALID_ARG)

#define NS_ENSURE_ARG_MAX(arg, max) \
  NS_ENSURE_TRUE((arg) <= max, NS_ERROR_INVALID_ARG)

#define NS_ENSURE_ARG_RANGE(arg, min, max) \
  NS_ENSURE_TRUE(((arg) >= min) && ((arg) <= max), NS_ERROR_INVALID_ARG)

#define NS_ENSURE_STATE(state) NS_ENSURE_TRUE(state, NS_ERROR_UNEXPECTED)

#define NS_ENSURE_NO_AGGREGATION(outer) \
  NS_ENSURE_FALSE(outer, NS_ERROR_NO_AGGREGATION)

/*****************************************************************************/

#if (defined(DEBUG) || (defined(NIGHTLY_BUILD) && !defined(MOZ_PROFILING))) && \
    !defined(XPCOM_GLUE_AVOID_NSPR)
#  define MOZ_THREAD_SAFETY_OWNERSHIP_CHECKS_SUPPORTED 1
#endif

#ifdef MOZILLA_INTERNAL_API
void NS_ABORT_OOM(size_t aSize);
#else
inline void NS_ABORT_OOM(size_t) { MOZ_CRASH(); }
#endif

/* When compiling the XPCOM Glue on Windows, we pretend that it's going to
 * be linked with a static CRT (-MT) even when it's not. This means that we
 * cannot link to data exports from the CRT, only function exports. So,
 * instead of referencing "stderr" directly, use fdopen.
 */
#ifdef __cplusplus
extern "C" {
#endif

/**
 * printf_stderr(...) is much like fprintf(stderr, ...), except that:
 *  - on Android and Firefox OS, *instead* of printing to stderr, it
 *    prints to logcat.  (Newlines in the string lead to multiple lines
 *    of logcat, but each function call implicitly completes a line even
 *    if the string does not end with a newline.)
 *  - on Windows, if a debugger is present, it calls OutputDebugString
 *    in *addition* to writing to stderr
 */
void printf_stderr(const char* aFmt, ...) MOZ_FORMAT_PRINTF(1, 2);

/**
 * Same as printf_stderr, but taking va_list instead of varargs
 */
void vprintf_stderr(const char* aFmt, va_list aArgs) MOZ_FORMAT_PRINTF(1, 0);

/**
 * fprintf_stderr is like fprintf, except that if its file argument
 * is stderr, it invokes printf_stderr instead.
 *
 * This is useful for general debugging code that logs information to a
 * file, but that you would like to be useful on Android and Firefox OS.
 * If you use fprintf_stderr instead of fprintf in such debugging code,
 * then callers can pass stderr to get logging that works on Android and
 * Firefox OS (and also the other side-effects of using printf_stderr).
 *
 * Code that is structured this way needs to be careful not to split a
 * line of output across multiple calls to fprintf_stderr, since doing
 * so will cause it to appear in multiple lines in logcat output.
 * (Producing multiple lines at once is fine.)
 */
void fprintf_stderr(FILE* aFile, const char* aFmt, ...) MOZ_FORMAT_PRINTF(2, 3);

#ifdef __cplusplus
}
#endif

#endif /* nsDebug_h___ */
