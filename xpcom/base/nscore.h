/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#ifndef nscore_h___
#define nscore_h___

#ifdef _WIN32
#define NS_WIN32 1
#endif

#if defined(__unix)
#define NS_UNIX 1
#endif

#if defined(XP_OS2)
#define NS_OS2 1
#endif

#include "prtypes.h"

#ifdef __cplusplus
#include "nsDebug.h"
#endif

/* The preferred symbol for null. */
#define nsnull 0

/* Define brackets for protecting C code from C++ */
#ifdef __cplusplus
#define NS_BEGIN_EXTERN_C extern "C" {
#define NS_END_EXTERN_C }
#else
#define NS_BEGIN_EXTERN_C
#define NS_END_EXTERN_C
#endif

/*----------------------------------------------------------------------*/
/* Import/export defines */

#ifdef NS_WIN32
#define NS_IMPORT _declspec(dllimport)
#define NS_IMPORT_(type) type _declspec(dllimport) __stdcall
#define NS_EXPORT _declspec(dllexport)
/* XXX NS_EXPORT_ defined in nsCOm.h (xpcom) differs in where the __declspec
   is placed. It needs to be done this way to make the 4.x compiler happy... */
#undef NS_EXPORT_
#define NS_EXPORT_(type) type _declspec(dllexport) __stdcall
#elif defined(XP_MAC)

#define NS_IMPORT
#define NS_IMPORT_(type) type

/* XXX NS_EXPORT_ defined in nsCom.h actually does an export. Here it's just sugar. */
#undef NS_EXPORT
#undef NS_EXPORT_

#define NS_EXPORT __declspec(export)
#define NS_EXPORT_(type) __declspec(export) type

#else
/* XXX do something useful? */
#define NS_IMPORT
#define NS_IMPORT_(type) type
#define NS_EXPORT
#define NS_EXPORT_(type) type
#endif

#ifdef _IMPL_NS_BASE
#define NS_BASE NS_EXPORT
#else
#define NS_BASE NS_IMPORT
#endif

#ifdef _IMPL_NS_NET
#define NS_NET NS_EXPORT
#else
#define NS_NET NS_IMPORT
#endif

#ifdef _IMPL_NS_DOM
#define NS_DOM NS_EXPORT
#else
#define NS_DOM NS_IMPORT
#endif

#ifdef _IMPL_NS_WIDGET
#define NS_WIDGET NS_EXPORT
#else
#define NS_WIDGET NS_IMPORT
#endif

#ifdef _IMPL_NS_VIEW
#define NS_VIEW NS_EXPORT
#else
#define NS_VIEW NS_IMPORT
#endif

#ifdef _IMPL_NS_GFXNONXP
#define NS_GFXNONXP NS_EXPORT
#define NS_GFXNONXP_(type) NS_EXPORT_(type)
#else
#define NS_GFXNONXP NS_IMPORT
#define NS_GFXNONXP_(type) NS_IMPORT_(type)
#endif

#ifdef _IMPL_NS_GFX
#define NS_GFX NS_EXPORT
#define NS_GFX_(type) NS_EXPORT_(type)
#else
#define NS_GFX NS_IMPORT
#define NS_GFX_(type) NS_IMPORT_(type)
#endif

#ifdef _IMPL_NS_PLUGIN
#define NS_PLUGIN NS_EXPORT
#else
#define NS_PLUGIN NS_IMPORT
#endif

#ifdef _IMPL_NS_APPSHELL
#define NS_APPSHELL NS_EXPORT
#else
#define NS_APPSHELL NS_IMPORT
#endif


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
  #define HAVE_CPP_SPECIALIZATION
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
  #define HAVE_CPP_SPECIALIZATION
  #define HAVE_CPP_MODERN_SPECIALIZE_TEMPLATE_SYNTAX

  #define HAVE_CPP_EXPLICIT
  #define HAVE_CPP_TYPENAME
  #define HAVE_CPP_ACCESS_CHANGING_USING

  #if (_MSC_VER<1100)
      // before 5.0, VC++ couldn't handle explicit
    #undef HAVE_CPP_EXPLICIT
  #elif (_MSC_VER==1100)
      // VC++5.0 has an internal compiler error (sometimes) without this
    #undef HAVE_CPP_ACCESS_CHANGING_USING
  #endif

  #define HAVE_CPP_NAMESPACE_STD
  #define HAVE_CPP_UNAMBIGUOUS_STD_NOTEQUAL
//#define HAVE_CPP_2BYTE_WCHAR_T
#endif

  /* until we get an autoconf test for this, we'll assume it's on (since we're using it already) */
#define HAVE_CPP_TYPENAME

  /* waiting to find out if OS/2 VisualAge participates in autoconf */
#if defined(XP_OS2_VACPP) || defined(AIX) || (defined(IRIX) && !defined(__GNUC__))
  #undef HAVE_CPP_TYPENAME
#endif


#ifndef __PRUNICHAR__
#define __PRUNICHAR__
  #ifdef HAVE_CPP_2BYTE_WCHAR_T
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
#define HAVE_CPP_NEW_CASTS /* we'll be optimistic. */
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
          ((Derived*)b)->some_deried_member ... // Error: Ambiguous, expand from which |Base|?
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

#endif /* nscore_h___ */
