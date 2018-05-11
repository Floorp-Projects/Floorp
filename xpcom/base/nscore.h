/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nscore_h___
#define nscore_h___

/**
 * Make sure that we have the proper platform specific
 * c++ definitions needed by nscore.h
 */
#ifndef _XPCOM_CONFIG_H_
#include "xpcom-config.h"
#endif

/* Definitions of functions and operators that allocate memory. */
#if !defined(NS_NO_XPCOM) && !defined(MOZ_NO_MOZALLOC)
#  include "mozilla/mozalloc.h"
#endif

/**
 * Incorporate the integer data types which XPCOM uses.
 */
#include <stddef.h>
#include <stdint.h>

#include "mozilla/RefCountType.h"

/* Core XPCOM declarations. */

/*----------------------------------------------------------------------*/
/* Import/export defines */

#ifdef HAVE_VISIBILITY_HIDDEN_ATTRIBUTE
#define NS_VISIBILITY_HIDDEN   __attribute__ ((visibility ("hidden")))
#else
#define NS_VISIBILITY_HIDDEN
#endif

#if defined(HAVE_VISIBILITY_ATTRIBUTE)
#define NS_VISIBILITY_DEFAULT __attribute__ ((visibility ("default")))
#elif defined(__SUNPRO_C) || defined(__SUNPRO_CC)
#define NS_VISIBILITY_DEFAULT __global
#else
#define NS_VISIBILITY_DEFAULT
#endif

#define NS_HIDDEN_(type)   NS_VISIBILITY_HIDDEN type
#define NS_EXTERNAL_VIS_(type) NS_VISIBILITY_DEFAULT type

#define NS_HIDDEN           NS_VISIBILITY_HIDDEN
#define NS_EXTERNAL_VIS     NS_VISIBILITY_DEFAULT

/**
 * Mark a function as using a potentially non-standard function calling
 * convention.  This can be used on functions that are called very
 * frequently, to reduce the overhead of the function call.  It is still worth
 * using the macro for C++ functions which take no parameters since it allows
 * passing |this| in a register.
 *
 *  - Do not use this on any scriptable interface method since xptcall won't be
 *    aware of the different calling convention.
 *  - This must appear on the declaration, not the definition.
 *  - Adding this to a public function _will_ break binary compatibility.
 *  - This may be used on virtual functions but you must ensure it is applied
 *    to all implementations - the compiler will _not_ warn but it will crash.
 *  - This has no effect for functions which take a variable number of
 *    arguments.
 *  - __fastcall on windows should not be applied to class
 *    constructors/destructors - use the NS_CONSTRUCTOR_FASTCALL macro for
 *    constructors/destructors.
 *
 * Examples: int NS_FASTCALL func1(char *foo);
 *           NS_HIDDEN_(int) NS_FASTCALL func2(char *foo);
 */

#if defined(__i386__) && defined(__GNUC__)
#define NS_FASTCALL __attribute__ ((regparm (3), stdcall))
#define NS_CONSTRUCTOR_FASTCALL __attribute__ ((regparm (3), stdcall))
#elif defined(XP_WIN) && !defined(_WIN64)
#define NS_FASTCALL __fastcall
#define NS_CONSTRUCTOR_FASTCALL
#else
#define NS_FASTCALL
#define NS_CONSTRUCTOR_FASTCALL
#endif

/**
 * Various API modifiers.
 *
 * - NS_IMETHOD/NS_IMETHOD_: use for in-class declarations and definitions.
 * - NS_IMETHODIMP/NS_IMETHODIMP_: use for out-of-class definitions.
 * - NS_METHOD_: usually used in conjunction with NS_CALLBACK_. Best avoided.
 * - NS_CALLBACK_: used in some legacy situations. Best avoided.
 */

#ifdef XP_WIN

#define NS_IMPORT __declspec(dllimport)
#define NS_IMPORT_(type) __declspec(dllimport) type __stdcall
#define NS_EXPORT __declspec(dllexport)
#define NS_EXPORT_(type) __declspec(dllexport) type __stdcall
#define NS_IMETHOD_(type) virtual type __stdcall
#define NS_IMETHODIMP_(type) type __stdcall
#define NS_METHOD_(type) type __stdcall
#define NS_CALLBACK_(_type, _name) _type (__stdcall * _name)
#ifndef _WIN64
// Win64 has only one calling convention.  __stdcall will be ignored by the compiler.
#define NS_STDCALL __stdcall
#define NS_HAVE_STDCALL
#else
#define NS_STDCALL
#endif
#define NS_FROZENCALL __cdecl

#else

#define NS_IMPORT NS_EXTERNAL_VIS
#define NS_IMPORT_(type) NS_EXTERNAL_VIS_(type)
#define NS_EXPORT NS_EXTERNAL_VIS
#define NS_EXPORT_(type) NS_EXTERNAL_VIS_(type)
#define NS_IMETHOD_(type) virtual type
#define NS_IMETHODIMP_(type) type
#define NS_METHOD_(type) type
#define NS_CALLBACK_(_type, _name) _type (* _name)
#define NS_STDCALL
#define NS_FROZENCALL

#endif

#define NS_IMETHOD          NS_IMETHOD_(nsresult)
#define NS_IMETHODIMP       NS_IMETHODIMP_(nsresult)

/**
 * Import/Export macros for XPCOM APIs
 */

#define EXPORT_XPCOM_API(type) type
#define IMPORT_XPCOM_API(type) type
#define GLUE_XPCOM_API(type) type

#ifdef __cplusplus
#define NS_EXTERN_C extern "C"
#else
#define NS_EXTERN_C
#endif

#define XPCOM_API(type) NS_EXTERN_C type

#if (defined(DEBUG) || defined(FORCE_BUILD_REFCNT_LOGGING))
/* Make refcnt logging part of the build. This doesn't mean that
 * actual logging will occur (that requires a separate enable; see
 * nsTraceRefcnt and nsISupportsImpl.h for more information).  */
#define NS_BUILD_REFCNT_LOGGING
#endif

/* If NO_BUILD_REFCNT_LOGGING is defined then disable refcnt logging
 * in the build. This overrides FORCE_BUILD_REFCNT_LOGGING. */
#if defined(NO_BUILD_REFCNT_LOGGING)
#undef NS_BUILD_REFCNT_LOGGING
#endif

/* If a program allocates memory for the lifetime of the app, it doesn't make
 * sense to touch memory pages and free that memory at shutdown,
 * unless we are running leak stats.
 */
#if defined(NS_BUILD_REFCNT_LOGGING) || defined(MOZ_VALGRIND) || defined(MOZ_ASAN) || defined(MOZ_CODE_COVERAGE)
#define NS_FREE_PERMANENT_DATA
#endif

/**
 * NS_NO_VTABLE is emitted by xpidl in interface declarations whenever
 * xpidl can determine that the interface can't contain a constructor.
 * This results in some space savings and possible runtime savings -
 * see bug 49416.  We undefine it first, as xpidl-generated headers
 * define it for IDL uses that don't include this file.
 */
#ifdef NS_NO_VTABLE
#undef NS_NO_VTABLE
#endif
#if defined(_MSC_VER)
#define NS_NO_VTABLE __declspec(novtable)
#else
#define NS_NO_VTABLE
#endif


/**
 * Generic XPCOM result data type
 */
#include "nsError.h"

typedef MozRefCountType nsrefcnt;

namespace mozilla {
// Extensions to the mozilla::Result type for handling of nsresult values.
//
// Note that these specializations need to be defined before Result.h is
// included, or we run into explicit specialization after instantiation errors,
// especially if Result.h is used in multiple sources in a unified compile.

namespace detail {
// When used as an error value, nsresult should never be NS_OK.
// This specialization allows us to pack Result<Ok, nsresult> into a
// nsresult-sized value.
template<typename T> struct UnusedZero;
template<>
struct UnusedZero<nsresult>
{
  static const bool value = true;
};
} // namespace detail

template <typename T> class MOZ_MUST_USE_TYPE GenericErrorResult;
template <> class MOZ_MUST_USE_TYPE GenericErrorResult<nsresult>;

struct Ok;
template <typename V, typename E> class Result;

// Allow MOZ_TRY to handle `nsresult` values.
inline Result<Ok, nsresult> ToResult(nsresult aValue);
} // namespace mozilla

/*
 * Use these macros to do 64bit safe pointer conversions.
 */

#define NS_PTR_TO_INT32(x) ((int32_t)(intptr_t)(x))
#define NS_PTR_TO_UINT32(x) ((uint32_t)(intptr_t)(x))
#define NS_INT32_TO_PTR(x) ((void*)(intptr_t)(x))

/*
 * Use NS_STRINGIFY to form a string literal from the value of a macro.
 */
#define NS_STRINGIFY_HELPER(x_) #x_
#define NS_STRINGIFY(x_) NS_STRINGIFY_HELPER(x_)

/*
 * If we're being linked as standalone glue, we don't want a dynamic
 * dependency on NSPR libs, so we skip the debug thread-safety
 * checks, and we cannot use the THREADSAFE_ISUPPORTS macros.
 */
#if defined(XPCOM_GLUE) && !defined(XPCOM_GLUE_USE_NSPR)
#define XPCOM_GLUE_AVOID_NSPR
#endif

/*
 * SEH exception macros.
 */
#ifdef HAVE_SEH_EXCEPTIONS
#define MOZ_SEH_TRY           __try
#define MOZ_SEH_EXCEPT(expr)  __except(expr)
#else
#define MOZ_SEH_TRY           if(true)
#define MOZ_SEH_EXCEPT(expr)  else
#endif

#endif /* nscore_h___ */
