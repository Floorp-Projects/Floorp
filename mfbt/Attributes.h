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
 *   Jeff Walden <jwalden+code@mit.edu> (original author)
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

/* Implementations of various C++ class and method modifier attributes. */

#ifndef mozilla_Attributes_h_
#define mozilla_Attributes_h_

/*
 * This header does not include any other headers so that it can be included by
 * code that is (only currently) mfbt-incompatible.
 */

#ifndef __cplusplus
#error "mozilla/Attributes.h is only relevant to C++ code."
#endif

/*
 * g++ requires -std=c++0x or -std=gnu++0x to support C++11 functionality
 * without warnings (functionality used by the macros below).  These modes are
 * detectable by checking whether __GXX_EXPERIMENTAL_CXX0X__ is defined or, more
 * standardly, by checking whether __cplusplus has a C++11 or greater value.
 * Current versions of g++ do not correctly set __cplusplus, so we check both
 * for forward compatibility.
 */
#if defined(__clang__)
#  if __clang_major__ >= 3
#    define MOZ_HAVE_CXX11_DELETE
#    define MOZ_HAVE_CXX11_OVERRIDE
#    define MOZ_HAVE_CXX11_FINAL         final
#  elif __clang_major__ == 2
#    if __clang_minor__ >= 9
#      define MOZ_HAVE_CXX11_DELETE
#    endif
#  endif
#elif defined(__GNUC__)
#  if defined(__GXX_EXPERIMENTAL_CXX0X__) || __cplusplus >= 201103L
#    if __GNUC__ > 4
#      define MOZ_HAVE_CXX11_DELETE
#      define MOZ_HAVE_CXX11_OVERRIDE
#      define MOZ_HAVE CXX11_FINAL       final
#    elif __GNUC__ == 4
#      if __GNUC_MINOR__ >= 7
#        define MOZ_HAVE_CXX11_OVERRIDE
#      endif
#      if __GNUC_MINOR__ >= 4
#        define MOZ_HAVE_CXX11_DELETE
#      endif
#    endif
#  else
     /* __final is a non-C++11 GCC synonym for 'final', per GCC r176655. */
#    if __GNUC__ > 4
#      define MOZ_HAVE_CXX11_FINAL       __final
#    elif __GNUC__ == 4
#      if __GNUC_MINOR__ >= 7
#        define MOZ_HAVE_CXX11_FINAL     __final
#      endif
#    endif
#  endif
#elif defined(_MSC_VER)
#  if _MSC_VER >= 1400
#    define MOZ_HAVE_CXX11_OVERRIDE
     /* MSVC currently spells "final" as "sealed". */
#    define MOZ_HAVE_CXX11_FINAL         sealed
#  endif
#endif

/*
 * MOZ_DELETE, specified immediately prior to the ';' terminating an undefined-
 * method declaration, attempts to delete that method from the corresponding
 * class.  An attempt to use the method will always produce an error *at compile
 * time* (instead of sometimes as late as link time) when this macro can be
 * implemented.  For example, you can use MOZ_DELETE to produce classes with no
 * implicit copy constructor or assignment operator:
 *
 *   struct NonCopyable
 *   {
 *     private:
 *       NonCopyable(const NonCopyable& other) MOZ_DELETE;
 *       void operator=(const NonCopyable& other) MOZ_DELETE;
 *   };
 *
 * If MOZ_DELETE can't be implemented for the current compiler, use of the
 * annotated method will still cause an error, but the error might occur at link
 * time in some cases rather than at compile time.
 *
 * MOZ_DELETE relies on C++11 functionality not universally implemented.  As a
 * backstop, method declarations using MOZ_DELETE should be private.
 */
#if defined(MOZ_HAVE_CXX11_DELETE)
#  define MOZ_DELETE            = delete
#else
#  define MOZ_DELETE            /* no support */
#endif

/*
 * MOZ_OVERRIDE explicitly indicates that a virtual member function in a class
 * overrides a member function of a base class, rather than potentially being a
 * new member function.  MOZ_OVERRIDE should be placed immediately before the
 * ';' terminating the member function's declaration, or before '= 0;' if the
 * member function is pure.  If the member function is defined in the class
 * definition, it should appear before the opening brace of the function body.
 *
 *   class Base
 *   {
 *     public:
 *       virtual void f() = 0;
 *   };
 *   class Derived1 : public Base
 *   {
 *     public:
 *       virtual void f() MOZ_OVERRIDE;
 *   };
 *   class Derived2 : public Base
 *   {
 *     public:
 *       virtual void f() MOZ_OVERRIDE = 0;
 *   };
 *   class Derived3 : public Base
 *   {
 *     public:
 *       virtual void f() MOZ_OVERRIDE { }
 *   };
 *
 * In compilers supporting C++11 override controls, MOZ_OVERRIDE *requires* that
 * the function marked with it override a member function of a base class: it
 * is a compile error if it does not.  Otherwise MOZ_OVERRIDE does not affect
 * semantics and merely documents the override relationship to the reader (but
 * of course must still be used correctly to not break C++11 compilers).
 */
#if defined(MOZ_HAVE_CXX11_OVERRIDE)
#  define MOZ_OVERRIDE          override
#else
#  define MOZ_OVERRIDE          /* no support */
#endif

/*
 * MOZ_FINAL indicates that some functionality cannot be overridden through
 * inheritance.  It can be used to annotate either classes/structs or virtual
 * member functions.
 *
 * To annotate a class/struct with MOZ_FINAL, place MOZ_FINAL immediately after
 * the name of the class, before the list of classes from which it derives (if
 * any) and before its opening brace.  MOZ_FINAL must not be used to annotate
 * unnamed classes or structs.  (With some compilers, and with C++11 proper, the
 * underlying expansion is ambiguous with specifying a class name.)
 *
 *   class Base MOZ_FINAL
 *   {
 *     public:
 *       Base();
 *       ~Base();
 *       virtual void f() { }
 *   };
 *   // This will be an error in some compilers:
 *   class Derived : public Base
 *   {
 *     public:
 *       ~Derived() { }
 *   };
 *
 * One particularly common reason to specify MOZ_FINAL upon a class is to tell
 * the compiler that it's not dangerous for it to have a non-virtual destructor
 * yet have one or more virtual functions, silencing the warning it might emit
 * in this case.  Suppose Base above weren't annotated with MOZ_FINAL.  Because
 * ~Base() is non-virtual, an attempt to delete a Derived* through a Base*
 * wouldn't call ~Derived(), so any cleanup ~Derived() might do wouldn't happen.
 * (Formally C++ says behavior is undefined, but compilers will likely just call
 * ~Base() and not ~Derived().)  Specifying MOZ_FINAL tells the compiler that
 * it's safe for the destructor to be non-virtual.
 *
 * In compilers implementing final controls, it is an error to inherit from a
 * class annotated with MOZ_FINAL.  In other compilers it serves only as
 * documentation.
 *
 * To annotate a virtual member function with MOZ_FINAL, place MOZ_FINAL
 * immediately before the ';' terminating the member function's declaration, or
 * before '= 0;' if the member function is pure.  If the member function is
 * defined in the class definition, it should appear before the opening brace of
 * the function body.  (This placement is identical to that for MOZ_OVERRIDE.
 * If both are used, they should appear in the order 'MOZ_FINAL MOZ_OVERRIDE'
 * for consistency.)
 *
 *   class Base
 *   {
 *     public:
 *       virtual void f() MOZ_FINAL;
 *   };
 *   class Derived
 *   {
 *     public:
 *       // This will be an error in some compilers:
 *       virtual void f();
 *   };
 *
 * In compilers implementing final controls, it is an error for a derived class
 * to override a method annotated with MOZ_FINAL.  In other compilers it serves
 * only as documentation.
 */
#if defined(MOZ_HAVE_CXX11_FINAL)
#  define MOZ_FINAL             MOZ_HAVE_CXX11_FINAL
#else
#  define MOZ_FINAL             /* no support */
#endif

#endif  /* mozilla_Attributes_h_ */
