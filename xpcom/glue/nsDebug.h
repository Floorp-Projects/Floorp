/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDebug_h___
#define nsDebug_h___

#ifndef nscore_h___
#include "nscore.h"
#endif

#ifndef nsError_h__
#include "nsError.h"
#endif

#include "nsXPCOM.h"
#include "mozilla/Assertions.h"
#include "mozilla/Likely.h"

#ifdef DEBUG
#include "prprf.h"
#endif

/**
 * Warn if the given condition is true. The condition is evaluated in both
 * release and debug builds, and the result is an expression which can be
 * used in subsequent expressions, such as:
 *
 * if (NS_WARN_IF(NS_FAILED(rv))
 *   return rv;
 *
 * This explicit warning and return is preferred to the NS_ENSURE_* macros
 * which hide the warning and the return control flow.
 *
 * @note This is C++-only
 */
#ifdef __cplusplus
#ifdef DEBUG
inline bool NS_warn_if_impl(bool condition, const char* expr, const char* file,
                            int32_t line)
{
  if (MOZ_UNLIKELY(condition)) {
    NS_DebugBreak(NS_DEBUG_WARNING, nullptr, expr, file, line);
  }
  return condition;
}
#define NS_WARN_IF(condition) \
  NS_warn_if_impl(condition, #condition, __FILE__, __LINE__)
#else
#define NS_WARN_IF(condition) (bool)(condition)
#endif
#endif

/**
 * Abort the execution of the program if the expression evaluates to
 * false.
 *
 * There is no status value returned from the macro.
 *
 * Note that the non-debug version of this macro does <b>not</b>
 * evaluate the expression argument. Hence side effect statements
 * as arguments to the macro will yield improper execution in a
 * non-debug build. For example:
 *
 *      NS_ABORT_IF_FALSE(0 == foo++, "yikes foo should be zero");
 *
 * Note also that the non-debug version of this macro does <b>not</b>
 * evaluate the message argument.
 */
#ifdef DEBUG
#define NS_ABORT_IF_FALSE(_expr, _msg)                        \
  do {                                                        \
    if (!(_expr)) {                                           \
      NS_DebugBreak(NS_DEBUG_ABORT, _msg, #_expr, __FILE__, __LINE__); \
    }                                                         \
  } while(0)
#else
#define NS_ABORT_IF_FALSE(_expr, _msg) do { /* nothing */ } while(0)
#endif

/**
 * Warn if a given condition is false.
 *
 * Program execution continues past the usage of this macro.
 *
 * Note also that the non-debug version of this macro does <b>not</b>
 * evaluate the message argument.
 */
#ifdef DEBUG
#define NS_WARN_IF_FALSE(_expr,_msg)                          \
  do {                                                        \
    if (!(_expr)) {                                           \
      NS_DebugBreak(NS_DEBUG_WARNING, _msg, #_expr, __FILE__, __LINE__); \
    }                                                         \
  } while(0)
#else
#define NS_WARN_IF_FALSE(_expr, _msg)  do { /* nothing */ } while(0)
#endif


/**
 * Test an assertion for truth. If the expression is not true then
 * trigger a program failure.
 *
 * Note that the non-debug version of this macro does <b>not</b>
 * evaluate the message argument.
 */
#ifdef DEBUG
#define NS_ASSERTION(expr, str)                               \
  do {                                                        \
    if (!(expr)) {                                            \
      NS_DebugBreak(NS_DEBUG_ASSERTION, str, #expr, __FILE__, __LINE__); \
    }                                                         \
  } while(0)
#else
#define NS_ASSERTION(expr, str)        do { /* nothing */ } while(0)
#endif

/**
 * NS_PRECONDITION/POSTCONDITION are synonyms for NS_ASSERTION.
 */
#define NS_PRECONDITION(expr, str) NS_ASSERTION(expr, str)
#define NS_POSTCONDITION(expr, str) NS_ASSERTION(expr, str)

/**
 * This macros triggers a program failure if executed. It indicates that
 * an attempt was made to execute some unimplemented functionality.
 */
#ifdef DEBUG
#define NS_NOTYETIMPLEMENTED(str)                             \
  NS_DebugBreak(NS_DEBUG_ASSERTION, str, "NotYetImplemented", __FILE__, __LINE__)
#else
#define NS_NOTYETIMPLEMENTED(str)      do { /* nothing */ } while(0)
#endif

/**
 * This macros triggers a program failure if executed. It indicates that
 * an attempt was made to execute a codepath which should not be reachable.
 */
#ifdef DEBUG
#define NS_NOTREACHED(str)                                    \
  NS_DebugBreak(NS_DEBUG_ASSERTION, str, "Not Reached", __FILE__, __LINE__)
#else
#define NS_NOTREACHED(str)             do { /* nothing */ } while(0)
#endif

/**
 * Log an error message.
 */
#ifdef DEBUG
#define NS_ERROR(str)                                         \
  NS_DebugBreak(NS_DEBUG_ASSERTION, str, "Error", __FILE__, __LINE__)
#else
#define NS_ERROR(str)                  do { /* nothing */ } while(0)
#endif

/**
 * Log a warning message.
 */
#ifdef DEBUG
#define NS_WARNING(str)                                       \
  NS_DebugBreak(NS_DEBUG_WARNING, str, nullptr, __FILE__, __LINE__)
#else
#define NS_WARNING(str)                do { /* nothing */ } while(0)
#endif

/**
 * Trigger an debug-only abort.
 *
 * @see NS_RUNTIMEABORT for release-mode asserts.
 */
#ifdef DEBUG
#define NS_ABORT()                                            \
  NS_DebugBreak(NS_DEBUG_ABORT, nullptr, nullptr, __FILE__, __LINE__)
#else
#define NS_ABORT()                     do { /* nothing */ } while(0)
#endif

/**
 * Trigger a debugger breakpoint, only in debug builds.
 */
#ifdef DEBUG
#define NS_BREAK()                                            \
  NS_DebugBreak(NS_DEBUG_BREAK, nullptr, nullptr, __FILE__, __LINE__)
#else
#define NS_BREAK()                     do { /* nothing */ } while(0)
#endif

/******************************************************************************
** Macros for static assertions.  These are used by the sixgill tool.
** When the tool is not running these macros are no-ops.
******************************************************************************/

/* Avoid name collision if included with other headers defining annotations. */
#ifndef HAVE_STATIC_ANNOTATIONS
#define HAVE_STATIC_ANNOTATIONS

#ifdef XGILL_PLUGIN

#define STATIC_PRECONDITION(COND)         __attribute__((precondition(#COND)))
#define STATIC_PRECONDITION_ASSUME(COND)  __attribute__((precondition_assume(#COND)))
#define STATIC_POSTCONDITION(COND)        __attribute__((postcondition(#COND)))
#define STATIC_POSTCONDITION_ASSUME(COND) __attribute__((postcondition_assume(#COND)))
#define STATIC_INVARIANT(COND)            __attribute__((invariant(#COND)))
#define STATIC_INVARIANT_ASSUME(COND)     __attribute__((invariant_assume(#COND)))

/* Used to make identifiers for assert/assume annotations in a function. */
#define STATIC_PASTE2(X,Y) X ## Y
#define STATIC_PASTE1(X,Y) STATIC_PASTE2(X,Y)

#define STATIC_ASSERT(COND)                          \
  do {                                               \
    __attribute__((assert_static(#COND), unused))    \
    int STATIC_PASTE1(assert_static_, __COUNTER__);  \
  } while(0)

#define STATIC_ASSUME(COND)                          \
  do {                                               \
    __attribute__((assume_static(#COND), unused))    \
    int STATIC_PASTE1(assume_static_, __COUNTER__);  \
  } while(0)

#define STATIC_ASSERT_RUNTIME(COND)                         \
  do {                                                      \
    __attribute__((assert_static_runtime(#COND), unused))   \
    int STATIC_PASTE1(assert_static_runtime_, __COUNTER__); \
  } while(0)

#else /* XGILL_PLUGIN */

#define STATIC_PRECONDITION(COND)          /* nothing */
#define STATIC_PRECONDITION_ASSUME(COND)   /* nothing */
#define STATIC_POSTCONDITION(COND)         /* nothing */
#define STATIC_POSTCONDITION_ASSUME(COND)  /* nothing */
#define STATIC_INVARIANT(COND)             /* nothing */
#define STATIC_INVARIANT_ASSUME(COND)      /* nothing */

#define STATIC_ASSERT(COND)          do { /* nothing */ } while(0)
#define STATIC_ASSUME(COND)          do { /* nothing */ } while(0)
#define STATIC_ASSERT_RUNTIME(COND)  do { /* nothing */ } while(0)

#endif /* XGILL_PLUGIN */

#define STATIC_SKIP_INFERENCE STATIC_INVARIANT(skip_inference())

#endif /* HAVE_STATIC_ANNOTATIONS */

#ifdef XGILL_PLUGIN

/* Redefine runtime assertion macros to perform static assertions, for both
 * debug and release builds. Don't include the original runtime assertions;
 * this ensures the tool will consider cases where the assertion fails. */

#undef NS_PRECONDITION
#undef NS_ASSERTION
#undef NS_POSTCONDITION

#define NS_PRECONDITION(expr, str)   STATIC_ASSERT_RUNTIME(expr)
#define NS_ASSERTION(expr, str)      STATIC_ASSERT_RUNTIME(expr)
#define NS_POSTCONDITION(expr, str)  STATIC_ASSERT_RUNTIME(expr)

#endif /* XGILL_PLUGIN */

/******************************************************************************
** Macros for terminating execution when an unrecoverable condition is
** reached.  These need to be compiled regardless of the DEBUG flag.
******************************************************************************/

/**
 * Terminate execution <i>immediately</i>, and if possible on the current
 * platform, in such a way that execution can't be continued by other
 * code (e.g., by intercepting a signal).
 */
#define NS_RUNTIMEABORT(msg)                                    \
  NS_DebugBreak(NS_DEBUG_ABORT, msg, nullptr, __FILE__, __LINE__)


/* Macros for checking the trueness of an expression passed in within an
 * interface implementation.  These need to be compiled regardless of the
 * DEBUG flag. New code should use NS_WARN_IF(condition) instead!
 * @status deprecated
 */

#define NS_ENSURE_TRUE(x, ret)                                \
  do {                                                        \
    if (MOZ_UNLIKELY(!(x))) {                                 \
       NS_WARNING("NS_ENSURE_TRUE(" #x ") failed");           \
       return ret;                                            \
    }                                                         \
  } while(0)

#define NS_ENSURE_FALSE(x, ret)                               \
  NS_ENSURE_TRUE(!(x), ret)

#define NS_ENSURE_TRUE_VOID(x)                                \
  do {                                                        \
    if (MOZ_UNLIKELY(!(x))) {                                 \
       NS_WARNING("NS_ENSURE_TRUE(" #x ") failed");           \
       return;                                                \
    }                                                         \
  } while(0)

#define NS_ENSURE_FALSE_VOID(x)                               \
  NS_ENSURE_TRUE_VOID(!(x))

/******************************************************************************
** Macros for checking results
******************************************************************************/

#if defined(DEBUG) && !defined(XPCOM_GLUE_AVOID_NSPR)

#define NS_ENSURE_SUCCESS_BODY(res, ret)                                  \
    char *msg = PR_smprintf("NS_ENSURE_SUCCESS(%s, %s) failed with "      \
                            "result 0x%X", #res, #ret, __rv);             \
    NS_WARNING(msg);                                                      \
    PR_smprintf_free(msg);

#define NS_ENSURE_SUCCESS_BODY_VOID(res)                                  \
    char *msg = PR_smprintf("NS_ENSURE_SUCCESS_VOID(%s) failed with "     \
                            "result 0x%X", #res, __rv);                   \
    NS_WARNING(msg);                                                      \
    PR_smprintf_free(msg);

#else

#define NS_ENSURE_SUCCESS_BODY(res, ret)                                  \
    NS_WARNING("NS_ENSURE_SUCCESS(" #res ", " #ret ") failed");

#define NS_ENSURE_SUCCESS_BODY_VOID(res)                                  \
    NS_WARNING("NS_ENSURE_SUCCESS_VOID(" #res ") failed");

#endif

#define NS_ENSURE_SUCCESS(res, ret)                                       \
  do {                                                                    \
    nsresult __rv = res; /* Don't evaluate |res| more than once */        \
    if (NS_FAILED(__rv)) {                                                \
      NS_ENSURE_SUCCESS_BODY(res, ret)                                    \
      return ret;                                                         \
    }                                                                     \
  } while(0)

#define NS_ENSURE_SUCCESS_VOID(res)                                       \
  do {                                                                    \
    nsresult __rv = res;                                                  \
    if (NS_FAILED(__rv)) {                                                \
      NS_ENSURE_SUCCESS_BODY_VOID(res)                                    \
      return;                                                             \
    }                                                                     \
  } while(0)

/******************************************************************************
** Macros for checking state and arguments upon entering interface boundaries
******************************************************************************/

#define NS_ENSURE_ARG(arg)                                    \
  NS_ENSURE_TRUE(arg, NS_ERROR_INVALID_ARG)

#define NS_ENSURE_ARG_POINTER(arg)                            \
  NS_ENSURE_TRUE(arg, NS_ERROR_INVALID_POINTER)

#define NS_ENSURE_ARG_MIN(arg, min)                           \
  NS_ENSURE_TRUE((arg) >= min, NS_ERROR_INVALID_ARG)

#define NS_ENSURE_ARG_MAX(arg, max)                           \
  NS_ENSURE_TRUE((arg) <= max, NS_ERROR_INVALID_ARG)

#define NS_ENSURE_ARG_RANGE(arg, min, max)                    \
  NS_ENSURE_TRUE(((arg) >= min) && ((arg) <= max), NS_ERROR_INVALID_ARG)

#define NS_ENSURE_STATE(state)                                \
  NS_ENSURE_TRUE(state, NS_ERROR_UNEXPECTED)

#define NS_ENSURE_NO_AGGREGATION(outer)                       \
  NS_ENSURE_FALSE(outer, NS_ERROR_NO_AGGREGATION)

/*****************************************************************************/

#ifdef XPCOM_GLUE
  #define NS_CheckThreadSafe(owningThread, msg)
#else
  #define NS_CheckThreadSafe(owningThread, msg)                 \
    if (MOZ_UNLIKELY(owningThread != PR_GetCurrentThread())) {  \
      MOZ_CRASH(msg);                                           \
    }
#endif

/* When compiling the XPCOM Glue on Windows, we pretend that it's going to
 * be linked with a static CRT (-MT) even when it's not. This means that we
 * cannot link to data exports from the CRT, only function exports. So,
 * instead of referencing "stderr" directly, use fdopen.
 */
#ifdef __cplusplus
extern "C" {
#endif

NS_COM_GLUE void
printf_stderr(const char *fmt, ...);

NS_COM_GLUE void
vprintf_stderr(const char *fmt, va_list args);

// fprintf with special handling for stderr to print to the console
NS_COM_GLUE void
fprintf_stderr(FILE* aFile, const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif /* nsDebug_h___ */
