/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
#ifndef nscore_h___
#define nscore_h___

/**
 * Make sure that we have the proper platform specific 
 * c++ definitions needed by nscore.h
 */
#ifndef _XPCOM_CONFIG_H_
#include "xpcom-config.h"
#endif

/**
 * Incorporate the core NSPR data types which XPCOM uses.
 */
#include "prtypes.h"

/* Core XPCOM declarations. */

/**
 * Macros defining the target platform...
 */
#ifdef _WIN32
#define NS_WIN32 1

#elif defined(__unix)
#define NS_UNIX 1

#elif defined(XP_OS2)
#define NS_OS2 1
#endif
/*----------------------------------------------------------------------*/
/* Import/export defines */

/**
 * Using the visibility("hidden") attribute allows the compiler to use
 * PC-relative addressing to call this function.  If a function does not
 * access any global data, and does not call any methods which are not either
 * file-local or hidden, then on ELF systems we avoid loading the address of
 * the PLT into a register at the start of the function, which reduces code
 * size and frees up a register for general use.
 *
 * As a general rule, this should be used for any non-exported symbol
 * (including virtual method implementations).  NS_IMETHOD uses this by
 * default; if you need to have your NS_IMETHOD functions exported, you can
 * wrap your class as follows:
 *
 * #undef  IMETHOD_VISIBILITY
 * #define IMETHOD_VISIBILITY NS_VISIBILITY_DEFAULT
 *
 * class Foo {
 * ...
 * };
 *
 * #undef  IMETHOD_VISIBILITY
 * #define IMETHOD_VISIBILITY NS_VISIBILITY_HIDDEN
 *
 * Don't forget to change the visibility back to hidden before the end
 * of a header!
 *
 * Other examples:
 *
 * NS_HIDDEN_(int) someMethod();
 * SomeCtor() NS_HIDDEN;
 */

#ifdef HAVE_VISIBILITY_ATTRIBUTE
#define NS_VISIBILITY_HIDDEN   __attribute__ ((visibility ("hidden")))
#define NS_VISIBILITY_DEFAULT

#define NS_HIDDEN_(type)   NS_VISIBILITY_HIDDEN type
#else
#define NS_VISIBILITY_HIDDEN
#define NS_VISIBILITY_DEFAULT

#define NS_HIDDEN_(type)   type
#endif

#define NS_HIDDEN           NS_VISIBILITY_HIDDEN

#undef  IMETHOD_VISIBILITY
#define IMETHOD_VISIBILITY  NS_VISIBILITY_HIDDEN

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
 *  - This has no effect for inline functions or functions which take a
 *    variable number of arguments.
 *
 * Examples: int NS_FASTCALL func1(char *foo);
 *           NS_HIDDEN_(int) NS_FASTCALL func2(char *foo);
 */

#if defined(__i386__) && defined(__GNUC__) && (__GNUC__ >= 3) && !defined(XP_OS2)
#define NS_FASTCALL __attribute__ ((regparm (3), stdcall))
#else
#define NS_FASTCALL
#endif

/*
 * NS_DEFCALL undoes the effect of a global regparm/stdcall setting
 * so that xptcall works correctly.
 */
#if defined(__i386__) && defined(__GNUC__) && (__GNUC__ >= 3) && !defined(XP_OS2)
#define NS_DEFCALL __attribute__ ((regparm (0), cdecl))
#else
#define NS_DEFCALL
#endif

#ifdef NS_WIN32

#define NS_IMPORT __declspec(dllimport)
#define NS_IMPORT_(type) type __declspec(dllimport) __stdcall
#define NS_EXPORT __declspec(dllexport)
#define NS_EXPORT_(type) type __declspec(dllexport) __stdcall
#define NS_IMETHOD_(type) virtual type __stdcall
#define NS_IMETHODIMP_(type) type __stdcall
#define NS_METHOD_(type) type __stdcall
#define NS_CALLBACK_(_type, _name) _type (__stdcall * _name)
#define NS_STDCALL __stdcall

#elif defined(XP_MAC)

#define NS_IMPORT
#define NS_IMPORT_(type) type
#define NS_EXPORT __declspec(export)
#define NS_EXPORT_(type) __declspec(export) type
#define NS_IMETHOD_(type) virtual type
#define NS_IMETHODIMP_(type) type
#define NS_METHOD_(type) type
#define NS_CALLBACK_(_type, _name) _type (* _name)
#define NS_STDCALL

#else

#define NS_IMPORT
#define NS_IMPORT_(type) type
#define NS_EXPORT
#define NS_EXPORT_(type) type
#define NS_IMETHOD_(type) virtual IMETHOD_VISIBILITY type NS_DEFCALL
#define NS_IMETHODIMP_(type) type
#define NS_METHOD_(type) type
#define NS_CALLBACK_(_type, _name) _type (* _name)
#define NS_STDCALL
#endif

/**
 * Macro for creating typedefs for pointer-to-member types which are
 * declared with stdcall.  It is important to use this for any type which is
 * declared as stdcall (i.e. NS_IMETHOD).  For example, instead of writing:
 *
 *  typedef nsresult (nsIFoo::*someType)(nsISupports* arg);
 *
 *  you should write:
 *
 *  typedef
 *  NS_STDCALL_FUNCPROTO(nsresult, someType, nsIFoo, typeFunc, (nsISupports*));
 *
 *  where nsIFoo::typeFunc is any method declared as
 *  NS_IMETHOD typeFunc(nsISupports*);
 *
 *  XXX this can be simplified to always use the non-typeof implementation
 *  when http://gcc.gnu.org/bugzilla/show_bug.cgi?id=11893 is fixed.
 */

#ifdef __GNUC__
#define NS_STDCALL_FUNCPROTO(ret, name, class, func, args) \
  typeof(&class::func) name
#else
#define NS_STDCALL_FUNCPROTO(ret, name, class, func, args) \
  ret (NS_STDCALL class::*name) args
#endif

/**
 * Generic API modifiers which return the standard XPCOM nsresult type
 */
#define NS_IMETHOD          NS_IMETHOD_(nsresult)
#define NS_IMETHODIMP       NS_IMETHODIMP_(nsresult)
#define NS_METHOD           NS_METHOD_(nsresult)
#define NS_CALLBACK(_name)  NS_CALLBACK_(nsresult, _name)

/**
 * Import/Export macros for XPCOM APIs
 */

#ifdef _IMPL_NS_COM
#define NS_COM NS_EXPORT
#elif  _IMPL_NS_COM_OFF
#define NS_COM
#elif  XPCOM_GLUE
#define NS_COM
#else
#define NS_COM NS_IMPORT
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
#if defined(_MSC_VER) && _MSC_VER >= 1100
#define NS_NO_VTABLE __declspec(novtable)
#else
#define NS_NO_VTABLE
#endif


/**
 * Generic XPCOM result data type
 */
typedef PRUint32 nsresult;

/**
 * The preferred symbol for null.
 */
#define nsnull 0

#include "nsError.h"

/* ------------------------------------------------------------------------ */
/* Casting macros for hiding C++ features from older compilers */

  /*
    All our compiler support template specialization, but not all support the
    |template <>| notation.  The compiler that don't understand this notation
    just omit it for specialization.

    Need to add an autoconf test for this.
  */

  /* under Metrowerks (Mac), we don't have autoconf yet */
#ifdef __MWERKS__
  #define HAVE_CPP_PARTIAL_SPECIALIZATION
  #define HAVE_CPP_MODERN_SPECIALIZE_TEMPLATE_SYNTAX

  #define HAVE_CPP_ACCESS_CHANGING_USING
  #define HAVE_CPP_AMBIGUITY_RESOLVING_USING
  #define HAVE_CPP_EXPLICIT
  #define HAVE_CPP_TYPENAME
  #define HAVE_CPP_BOOL
  #define HAVE_CPP_NAMESPACE_STD
  #define HAVE_CPP_UNAMBIGUOUS_STD_NOTEQUAL
  #define HAVE_CPP_2BYTE_WCHAR_T
#endif

  /* under VC++ (Windows), we don't have autoconf yet */
#if defined(_MSC_VER) && (_MSC_VER>=1100)
  /* VC++ 5.0 and greater implement template specialization, 4.2 is unknown */
  #define HAVE_CPP_MODERN_SPECIALIZE_TEMPLATE_SYNTAX

  #define HAVE_CPP_EXPLICIT
  #define HAVE_CPP_TYPENAME
  #define HAVE_CPP_ACCESS_CHANGING_USING

  #if (_MSC_VER==1100)
      /* VC++5.0 has an internal compiler error (sometimes) without this */
    #undef HAVE_CPP_ACCESS_CHANGING_USING
  #endif

  #define HAVE_CPP_NAMESPACE_STD
  #define HAVE_CPP_UNAMBIGUOUS_STD_NOTEQUAL
  #define HAVE_CPP_2BYTE_WCHAR_T
#endif

#ifndef __PRUNICHAR__
#define __PRUNICHAR__
  /* For now, don't use wchar_t on Unix because it breaks the Netscape
   * commercial build.  When this is fixed there will be no need for the
   * |NS_REINTERPRET_CAST| in nsLiteralString.h either.
   */
  #if defined(HAVE_CPP_2BYTE_WCHAR_T) && (defined(NS_WIN32) || defined(XP_MAC))
    typedef wchar_t PRUnichar;
  #else
    typedef PRUint16 PRUnichar;
  #endif
#endif

  /*
    If the compiler doesn't support |explicit|, we'll just make it go away, trusting
    that the builds under compilers that do have it will keep us on the straight and narrow.
  */
#ifndef HAVE_CPP_EXPLICIT
  #define explicit
#endif

#ifndef HAVE_CPP_TYPENAME
  #define typename
#endif

#ifdef HAVE_CPP_MODERN_SPECIALIZE_TEMPLATE_SYNTAX
  #define NS_SPECIALIZE_TEMPLATE  template <>
#else
  #define NS_SPECIALIZE_TEMPLATE
#endif

/* unix and beos now determine this automatically */
#if ! defined XP_UNIX && ! defined XP_BEOS && !defined(XP_OS2)
#ifndef HAVE_CPP_NEW_CASTS
#define HAVE_CPP_NEW_CASTS 1 /* we'll be optimistic. */
#endif
#endif

#if defined(HAVE_CPP_NEW_CASTS)
#define NS_STATIC_CAST(__type, __ptr)      static_cast< __type >(__ptr)
#define NS_CONST_CAST(__type, __ptr)       const_cast< __type >(__ptr)

#define NS_REINTERPRET_POINTER_CAST(__type, __ptr)    reinterpret_cast< __type >(__ptr)
#define NS_REINTERPRET_NONPOINTER_CAST(__type, __obj) reinterpret_cast< __type >(__obj)
#define NS_REINTERPRET_CAST(__type, __expr)           reinterpret_cast< __type >(__expr)

#else
#define NS_STATIC_CAST(__type, __ptr)      ((__type)(__ptr))
#define NS_CONST_CAST(__type, __ptr)       ((__type)(__ptr))

#define NS_REINTERPRET_POINTER_CAST(__type, __ptr)     ((__type)((void*)(__ptr)))
#define NS_REINTERPRET_NONPOINTER_CAST(__type, __obj)  ((__type)(__obj))

  /* Note: the following is only appropriate for pointers. */
#define NS_REINTERPRET_CAST(__type, __expr)            NS_REINTERPRET_POINTER_CAST(__type, __expr)
  /*
    Why cast to a |void*| first?  Well, when old-style casting from
    a pointer to a base to a pointer to a derived class, the cast will be
    ambiguous if the source pointer type appears multiple times in the
    destination, e.g.,
    
      class Base {};
      class Derived : public Base, public Base {};
      
      void foo( Base* b )
        {
          ((Derived*)b)->some_derived_member ... // Error: Ambiguous, expand from which |Base|?
        }

    an old-style cast (like |static_cast|) will change the pointer, but
    here, doesn't know how.  The cast to |void*| prevents it from thinking
    it needs to expand the original pointer.

    The cost is, |NS_REINTERPRET_CAST| is no longer appropriate for non-pointer
    conversions.  Also, mis-applying |NS_REINTERPRET_CAST| to cast |this| to something
    will still expand the pointer to the outer object in standards complying compilers.
  */

  /*
    No sense in making an NS_DYNAMIC_CAST() macro: you can't duplicate
    the semantics. So if you want to dynamic_cast, then just use it
    "straight", no macro.
  */
#endif
 
/* 
 * Use these macros to do 64bit safe pointer conversions.
 */

#define NS_PTR_TO_INT32(x) ((char *)(x) - (char *)0)
#define NS_INT32_TO_PTR(x) ((void *)((char *)0 + (x)))

/*
 * These macros allow you to give a hint to the compiler about branch
 * probability so that it can better optimize.  Use them like this:
 *
 *  if (NS_LIKELY(v == 1)) {
 *    ... expected code path ...
 *  }
 *
 *  if (NS_UNLIKELY(v == 0)) {
 *    ... non-expected code path ...
 *  }
 *
 */

#if defined(__GNUC__) && (__GNUC__ > 2)
#define NS_LIKELY(x)    (__builtin_expect((x), 1))
#define NS_UNLIKELY(x)  (__builtin_expect((x), 0))
#else
#define NS_LIKELY(x)    (x)
#define NS_UNLIKELY(x)  (x)
#endif

#endif /* nscore_h___ */

