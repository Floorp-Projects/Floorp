/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99 ft=cpp:
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at:
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Code.
 *
 * The Initial Developer of the Original Code is
 *   The Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jeff Walden <jwalden+code@mit.edu>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef mozilla_Assertions_h_
#define mozilla_Assertions_h_

#include "mozilla/Types.h"

/*
 * MOZ_STATIC_ASSERT may be used to assert a condition *at compile time*.  This
 * can be useful when you make certain assumptions about what must hold for
 * optimal, or even correct, behavior.  For example, you might assert that the
 * size of a struct is a multiple of the target architecture's word size:
 *
 *   struct S { ... };
 *   MOZ_STATIC_ASSERT(sizeof(S) % sizeof(size_t) == 0,
 *                     "S should be a multiple of word size for efficiency");
 *
 * This macro can be used in any location where both an extern declaration and a
 * typedef could be used.
 *
 * Be aware of the gcc 4.2 concerns noted further down when writing patches that
 * use this macro, particularly if a patch only bounces on OS X.
 */
#ifdef __cplusplus
#  if defined(__clang__)
#    ifndef __has_extension
#      define __has_extension __has_feature /* compatibility, for older versions of clang */
#    endif
#    if __has_extension(cxx_static_assert)
#      define MOZ_STATIC_ASSERT(cond, reason)    static_assert((cond), reason)
#    endif
#  elif defined(__GNUC__)
#    if (defined(__GXX_EXPERIMENTAL_CXX0X__) || __cplusplus >= 201103L) && \
        (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 3))
#      define MOZ_STATIC_ASSERT(cond, reason)    static_assert((cond), #cond)
#    endif
#  elif defined(_MSC_VER)
#    if _MSC_VER >= 1600 /* MSVC 10 */
#      define MOZ_STATIC_ASSERT(cond, reason)    static_assert((cond), #cond)
#    endif
#  elif defined(__HP_aCC)
#    if __HP_aCC >= 62500 && defined(_HP_CXX0x_SOURCE)
#      define MOZ_STATIC_ASSERT(cond, reason)    static_assert((cond), #cond)
#    endif
#  endif
#endif
#ifndef MOZ_STATIC_ASSERT
#  define MOZ_STATIC_ASSERT_GLUE1(x, y)          x##y
#  define MOZ_STATIC_ASSERT_GLUE(x, y)           MOZ_STATIC_ASSERT_GLUE1(x, y)
#  if defined(__SUNPRO_CC)
     /*
      * The Sun Studio C++ compiler is buggy when declaring, inside a function,
      * another extern'd function with an array argument whose length contains a
      * sizeof, triggering the error message "sizeof expression not accepted as
      * size of array parameter".  This bug (6688515, not public yet) would hit
      * defining moz_static_assert as a function, so we always define an extern
      * array for Sun Studio.
      *
      * We include the line number in the symbol name in a best-effort attempt
      * to avoid conflicts (see below).
      */
#    define MOZ_STATIC_ASSERT(cond, reason) \
       extern char MOZ_STATIC_ASSERT_GLUE(moz_static_assert, __LINE__)[(cond) ? 1 : -1]
#  elif defined(__COUNTER__)
     /*
      * If there was no preferred alternative, use a compiler-agnostic version.
      *
      * Note that the non-__COUNTER__ version has a bug in C++: it can't be used
      * in both |extern "C"| and normal C++ in the same translation unit.  (Alas
      * |extern "C"| isn't allowed in a function.)  The only affected compiler
      * we really care about is gcc 4.2.  For that compiler and others like it,
      * we include the line number in the function name to do the best we can to
      * avoid conflicts.  These should be rare: a conflict would require use of
      * MOZ_STATIC_ASSERT on the same line in separate files in the same
      * translation unit, *and* the uses would have to be in code with
      * different linkage, *and* the first observed use must be in C++-linkage
      * code.
      */
#    define MOZ_STATIC_ASSERT(cond, reason) \
       typedef int MOZ_STATIC_ASSERT_GLUE(moz_static_assert, __COUNTER__)[(cond) ? 1 : -1]
#  else
#    define MOZ_STATIC_ASSERT(cond, reason) \
       extern void MOZ_STATIC_ASSERT_GLUE(moz_static_assert, __LINE__)(int arg[(cond) ? 1 : -1])
#  endif
#endif

#define MOZ_STATIC_ASSERT_IF(cond, expr, reason)  MOZ_STATIC_ASSERT(!(cond) || (expr), reason)

/*
 * XXX: we're cheating here in order to avoid creating object files
 * for mfbt /just/ to provide a function like FatalError() to be used
 * by MOZ_ASSERT().  (It'll happen eventually, but for just ASSERT()
 * it isn't worth the pain.)  JS_Assert(), although unfortunately
 * named, is part of SpiderMonkey's stable, external API, so this
 * isn't quite as bad as it seems.
 *
 * Once mfbt needs object files, this unholy union with JS_Assert()
 * will be broken.
 *
 * JS_Assert is present even in release builds, for the benefit of applications
 * that build DEBUG and link against a non-DEBUG SpiderMonkey library.
 */
#ifdef __cplusplus
extern "C" {
#endif

extern MFBT_API(void)
JS_Assert(const char* s, const char* file, int ln);

#ifdef __cplusplus
} /* extern "C" */
#endif

/*
 * MOZ_ASSERT() is a "strong" assertion of state, like libc's
 * assert().
 *
 *   MOZ_ASSERT(twoToThePowerOf(5) == 32);
 *
 * If a MOZ_ASSERT() fails in a debug build, the process in which it fails will
 * stop running in a loud and dramatic way.  It has no effect in an optimized
 * build.  This macro is designed to catch bugs during debugging, not "in the
 * field".
 */
#ifdef DEBUG
#  define MOZ_ASSERT(expr_)                                      \
     ((expr_) ? ((void)0) : JS_Assert(#expr_, __FILE__, __LINE__))
#else
#  define MOZ_ASSERT(expr_) ((void)0)
#endif /* DEBUG */

/*
 * MOZ_ASSERT_IF(cond1, cond2) is equivalent to MOZ_ASSERT(cond2) if cond1 is
 * true.
 *
 *   MOZ_ASSERT_IF(isPrime(num), num == 2 || isOdd(num));
 *
 * As with MOZ_ASSERT, MOZ_ASSERT_IF has effect only in debug builds.  It is
 * designed to catch bugs during debugging, not "in the field".
 */
#ifdef DEBUG
#  define MOZ_ASSERT_IF(cond, expr)  ((cond) ? MOZ_ASSERT(expr) : ((void)0))
#else
#  define MOZ_ASSERT_IF(cond, expr)  ((void)0)
#endif

/*
 * MOZ_NOT_REACHED(reason) indicates that the given point can't be reached
 * during execution: simply reaching that point in execution is a bug.  It takes
 * as an argument an error message indicating the reason why that point should
 * not have been reachable.
 *
 *   // ...in a language parser...
 *   void handle(BooleanLiteralNode node)
 *   {
 *     if (node.isTrue())
 *       handleTrueLiteral();
 *     else if (node.isFalse())
 *       handleFalseLiteral();
 *     else
 *       MOZ_NOT_REACHED("boolean literal that's not true or false?");
 *   }
 */
#ifdef DEBUG
#  define MOZ_NOT_REACHED(reason)    JS_Assert(reason, __FILE__, __LINE__)
#else
#  define MOZ_NOT_REACHED(reason)    ((void)0)
#endif

/*
 * MOZ_ALWAYS_TRUE(expr) and MOZ_ALWAYS_FALSE(expr) always evaluate the provided
 * expression, in debug builds and in release builds both.  Then, in debug
 * builds only, the value of the expression is asserted either true or false
 * using MOZ_ASSERT.
 */
#ifdef DEBUG
#  define MOZ_ALWAYS_TRUE(expr)      MOZ_ASSERT((expr))
#  define MOZ_ALWAYS_FALSE(expr)     MOZ_ASSERT(!(expr))
#else
#  define MOZ_ALWAYS_TRUE(expr)      ((void)(expr))
#  define MOZ_ALWAYS_FALSE(expr)     ((void)(expr))
#endif

#endif /* mozilla_Assertions_h_ */
