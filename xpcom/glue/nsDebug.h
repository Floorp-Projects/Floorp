/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsDebug_h___
#define nsDebug_h___

#ifndef nscore_h___
#include "nscore.h"
#endif

#ifndef nsError_h__
#include "nsError.h"
#endif 

#ifdef DEBUG
#define NS_DEBUG
#endif

/**
 * Namespace for debugging methods. Note that your code must use the
 * macros defined later in this file so that the debug code can be
 * conditionally compiled out.
 */

/* in case this is included by a C file */
#ifdef __cplusplus

class nsDebug {
public:

  /**
   * Log a warning message to the debug log.
   */
  static NS_COM void Warning(const char* aMessage,
                             const char* aFile, PRIntn aLine);

  /**
   * Abort the executing program. This works on all architectures.
   */
  static NS_COM void Abort(const char* aFile, PRIntn aLine);

  /**
   * Break the executing program into the debugger. 
   */
  static NS_COM void Break(const char* aFile, PRIntn aLine);

  /**
   * Log an assertion message to the debug log
   */
  static NS_COM void Assertion(const char* aStr, const char* aExpr,
                               const char* aFile, PRIntn aLine);
};

#ifdef DEBUG

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
#define NS_ABORT_IF_FALSE(_expr, _msg)                        \
  PR_BEGIN_MACRO                                              \
    if (!(_expr)) {                                           \
      nsDebug::Assertion(_msg, #_expr, __FILE__, __LINE__);   \
    }                                                         \
  PR_END_MACRO

/**
 * Warn if a given condition is false.
 *
 * Program execution continues past the usage of this macro.
 *
 * Note also that the non-debug version of this macro does <b>not</b>
 * evaluate the message argument.
 */
#define NS_WARN_IF_FALSE(_expr,_msg)                          \
  PR_BEGIN_MACRO                                              \
    if (!(_expr)) {                                           \
      nsDebug::Assertion(_msg, #_expr, __FILE__, __LINE__);   \
    }                                                         \
  PR_END_MACRO

/**
 * Test a precondition for truth. If the expression is not true then
 * trigger a program failure.
 */
#define NS_PRECONDITION(expr, str)                            \
  PR_BEGIN_MACRO                                              \
    if (!(expr)) {                                            \
      nsDebug::Assertion(str, #expr, __FILE__, __LINE__);     \
    }                                                         \
  PR_END_MACRO

/**
 * Test an assertion for truth. If the expression is not true then
 * trigger a program failure.
 */
#define NS_ASSERTION(expr, str)                               \
  PR_BEGIN_MACRO                                              \
    if (!(expr)) {                                            \
      nsDebug::Assertion(str, #expr, __FILE__, __LINE__);     \
    }                                                         \
  PR_END_MACRO

/**
 * Test a post-condition for truth. If the expression is not true then
 * trigger a program failure.
 */
#define NS_POSTCONDITION(expr, str)                           \
  PR_BEGIN_MACRO                                              \
    if (!(expr)) {                                            \
      nsDebug::Assertion(str, #expr, __FILE__, __LINE__);     \
    }                                                         \
  PR_END_MACRO

/**
 * This macros triggers a program failure if executed. It indicates that
 * an attempt was made to execute some unimplimented functionality.
 */
#define NS_NOTYETIMPLEMENTED(str)                             \
  nsDebug::Assertion(str, "NotYetImplemented", __FILE__, __LINE__)

/**
 * This macros triggers a program failure if executed. It indicates that
 * an attempt was made to execute some unimplimented functionality.
 */
#define NS_NOTREACHED(str)                                    \
  nsDebug::Assertion(str, "Not Reached", __FILE__, __LINE__)

/**
 * Log an error message.
 */
#define NS_ERROR(str)                                         \
  nsDebug::Assertion(str, "Error", __FILE__, __LINE__)

/**
 * Log a warning message.
 */
#define NS_WARNING(str)                                       \
  nsDebug::Warning(str, __FILE__, __LINE__)

/**
 * Trigger an abort
 */
#define NS_ABORT()                                            \
  nsDebug::Abort(__FILE__, __LINE__)

/**
 * Cause a break
 */
#define NS_BREAK()                                            \
  nsDebug::Break(__FILE__, __LINE__)

#else /* NS_DEBUG */

/**
 * The non-debug version of these macros do not evaluate the
 * expression or the message arguments to the macro.
 */
#define NS_ABORT_IF_FALSE(_expr, _msg) /* nothing */
#define NS_WARN_IF_FALSE(_expr, _msg)  /* nothing */
#define NS_PRECONDITION(expr, str)     /* nothing */
#define NS_ASSERTION(expr, str)        /* nothing */
#define NS_POSTCONDITION(expr, str)    /* nothing */
#define NS_NOTYETIMPLEMENTED(str)      /* nothing */
#define NS_NOTREACHED(str)             /* nothing */
#define NS_ERROR(str)                  /* nothing */
#define NS_WARNING(str)                /* nothing */
#define NS_ABORT()                     /* nothing */
#define NS_BREAK()                     /* nothing */

#endif /* ! NS_DEBUG */
#endif /* __cplusplus */

// Macros for checking the trueness of an expression passed in within an 
// interface implementation.  These need to be compiled regardless of the 
// NS_DEBUG flag
///////////////////////////////////////////////////////////////////////////////

#define NS_ENSURE_TRUE(x, ret)                                \
  PR_BEGIN_MACRO                                              \
    if (NS_UNLIKELY(!(x))) {                                  \
       NS_WARNING("NS_ENSURE_TRUE(" #x ") failed");           \
       return ret;                                            \
    }                                                         \
  PR_END_MACRO

#define NS_ENSURE_FALSE(x, ret)                               \
  NS_ENSURE_TRUE(!(x), ret)

///////////////////////////////////////////////////////////////////////////////
// Macros for checking results
///////////////////////////////////////////////////////////////////////////////

#define NS_ENSURE_SUCCESS(res, ret) \
  NS_ENSURE_TRUE(NS_SUCCEEDED(res), ret)

///////////////////////////////////////////////////////////////////////////////
// Macros for checking state and arguments upon entering interface boundaries
///////////////////////////////////////////////////////////////////////////////

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

#define NS_ENSURE_PROPER_AGGREGATION(outer, iid)              \
  NS_ENSURE_FALSE(outer && !iid.Equals(NS_GET_IID(nsISupports)), NS_ERROR_INVALID_ARG)

///////////////////////////////////////////////////////////////////////////////

#define NS_CheckThreadSafe(owningThread, msg)                 \
  NS_ASSERTION(owningThread == PR_GetCurrentThread(), msg)

#endif /* nsDebug_h___ */
