/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef nsDebug_h___
#define nsDebug_h___

#include "nsCom.h"
#include "prtypes.h"

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
  // XXX add in log controls here
  // XXX probably want printf type arguments

  /**
   * Abort the executing program. This works on all architectures.
   */
  static NS_COM void Abort(const char* aFile, PRIntn aLine);

  /**
   * Break the executing program into the debugger. 
   */
  static NS_COM void Break(const char* aFile, PRIntn aLine);

  /**
   * Log a pre-condition message to the debug log
   */
  static NS_COM void PreCondition(const char* aStr, const char* aExpr,
                                  const char* aFile, PRIntn aLine);

  /**
   * Log a post-condition message to the debug log
   */
  static NS_COM void PostCondition(const char* aStr, const char* aExpr,
                                   const char* aFile, PRIntn aLine);

  /**
   * Log an assertion message to the debug log
   */
  static NS_COM void Assertion(const char* aStr, const char* aExpr,
                               const char* aFile, PRIntn aLine);

  /**
   * Log a not-yet-implemented message to the debug log
   */
  static NS_COM void NotYetImplemented(const char* aMessage,
                                       const char* aFile, PRIntn aLine);

  /**
   * Log a not-reached message to the debug log
   */
  static NS_COM void NotReached(const char* aMessage,
                                const char* aFile, PRIntn aLine);

  /**
   * Log an error message to the debug log. This call returns.
   */
  static NS_COM void Error(const char* aMessage,
                           const char* aFile, PRIntn aLine);

  /**
   * Log a warning message to the debug log.
   */
  static NS_COM void Warning(const char* aMessage,
                             const char* aFile, PRIntn aLine);
};

#ifdef NS_DEBUG
/**
 * Test a precondition for truth. If the expression is not true then
 * trigger a program failure.
 */
#define NS_PRECONDITION(expr,str) \
if (!(expr))                      \
  nsDebug::PreCondition(str, #expr, __FILE__, __LINE__)

/**
 * Test an assertion for truth. If the expression is not true then
 * trigger a program failure.
 */
#define NS_ASSERTION(expr,str) \
if (!(expr))                   \
  nsDebug::Assertion(str, #expr, __FILE__, __LINE__)

/**
 * Test an assertion for truth. If the expression is not true then
 * trigger a program failure. The expression will still be
 * executed in release mode.
 */
#define NS_VERIFY(expr,str) \
if (!(expr))                \
  nsDebug::Assertion(str, #expr, __FILE__, __LINE__)

/**
 * Test a post-condition for truth. If the expression is not true then
 * trigger a program failure.
 */
#define NS_POSTCONDITION(expr,str) \
if (!(expr))                       \
  nsDebug::PostCondition(str, #expr, __FILE__, __LINE__)

/**
 * This macros triggers a program failure if executed. It indicates that
 * an attempt was made to execute some unimplimented functionality.
 */
#define NS_NOTYETIMPLEMENTED(str) \
  nsDebug::NotYetImplemented(str, __FILE__, __LINE__)

/**
 * This macros triggers a program failure if executed. It indicates that
 * an attempt was made to execute some unimplimented functionality.
 */
#define NS_NOTREACHED(str) \
  nsDebug::NotReached(str, __FILE__, __LINE__)

/**
 * Log an error message.
 */
#define NS_ERROR(str) \
  nsDebug::Error(str, __FILE__, __LINE__)

/**
 * Log a warning message.
 */
#define NS_WARNING(str) \
  nsDebug::Warning(str, __FILE__, __LINE__)

/**
 * Trigger an abort
 */
#define NS_ABORT() \
  nsDebug::Abort(__FILE__, __LINE__)

/**
 * Cause a break
 */
#define NS_BREAK() \
  nsDebug::Break(__FILE__, __LINE__)

#else /* NS_DEBUG */

#define NS_PRECONDITION(expr,str)  {}
#define NS_ASSERTION(expr,str)     {}
#define NS_VERIFY(expr,str)        expr
#define NS_POSTCONDITION(expr,str) {}
#define NS_NOTYETIMPLEMENTED(str)  {}
#define NS_NOTREACHED(str)         {}
#define NS_ERROR(str)              {}
#define NS_WARNING(str)            {}
#define NS_ABORT()                 {}
#define NS_BREAK()                 {}

#endif /* ! NS_DEBUG */
#endif /* __cplusplus */
#endif /* nsDebug_h___ */
