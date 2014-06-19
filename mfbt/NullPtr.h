/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Implements a workaround for compilers which do not support the C++11 nullptr
 * constant.
 */

#ifndef mozilla_NullPtr_h
#define mozilla_NullPtr_h

#if defined(__clang__)
#  if !__has_extension(cxx_nullptr)
#    error "clang version natively supporting nullptr is required."
#  endif
#  define MOZ_HAVE_CXX11_NULLPTR
#elif defined(__GNUC__)
#  if defined(__GXX_EXPERIMENTAL_CXX0X__) || __cplusplus >= 201103L
#    include "mozilla/Compiler.h"
#    if MOZ_GCC_VERSION_AT_LEAST(4, 6, 0)
#      define MOZ_HAVE_CXX11_NULLPTR
#    endif
#  endif
#elif defined(_MSC_VER)
   // The minimum supported MSVC (10, _MSC_VER 1600) supports nullptr.
#  define MOZ_HAVE_CXX11_NULLPTR
#endif

namespace mozilla {

/**
 * IsNullPointer<T>::value is true iff T is the type of C++11's nullptr.  If
 * nullptr is emulated, IsNullPointer<T>::value is false for all T.
 *
 * IsNullPointer is useful to give an argument the true decltype(nullptr) type.
 * decltype(nullptr) doesn't work when nullptr is emulated.  The simplest
 * workaround is to use template overloading and SFINAE to expose an overload
 * only if an argument's type is decltype(nullptr).  Some examples (that assume
 * namespace mozilla has been opened, for simplicity):
 *
 *   // foo(T*), foo(stuff that converts to T*), and foo(decltype(nullptr))
 *   // (only invoked if nullptr is true nullptr, otherwise foo(T*) is invoked)
 *   // but nothing else
 *   void foo(T*) { }
 *   template<typename N>
 *   void foo(N,
 *            typename EnableIf<IsNullPointer<N>::value, int>::Type dummy = 0)
 *   { }
 *
 *   // foo(T*) *exactly* and foo(decltype(nullptr)), nothing else
 *   void foo(T*) { }
 *   template<typename U>
 *   void foo(U,
 *            typename EnableIf<!IsNullPointer<U>::value, int>::Type dummy = 0)
 *   MOZ_DELETE;
 *
 * The exact details of how set up the SFINAE bits vary on a case-by-case basis.
 * If you need help with this (and unless you've internalized way more sadmaking
 * nullptr-emulation knowledge than you should have, you do), feel free to poke
 * the person with blame on this comment with questions.   :-)
 *
 * Ideally this would be in TypeTraits.h, but C++11 omitted std::is_null_pointer
 * (fixed in C++1y), so in the interests of easing a switch to <type_traits>,
 * this trait lives elsewhere.
 */
template<typename T>
struct IsNullPointer { static const bool value = false; };

} // namespace mozilla

/**
 * mozilla::NullptrT is a type that's sort of like decltype(nullptr).  But it
 * can't be identical, because emulated nullptr doesn't have a distinct type.
 * Only with gcc 4.4/4.5, emulated nullptr is __null, and decltype(__null) is
 * int or long.  But passing __null to an int/long parameter triggers
 * -Werror=conversion-null errors with gcc 4.5, or (depending on subsequent use
 * inside the overloaded function) can trigger pointer-to-integer comparison
 * compiler errors.  So fairly often, actually, NullptrT is *not* what you want.
 *
 * Instead, often you should use template-based overloading in concert with
 * SFINAE to add a nullptr overload -- see the comments by IsNullPointer.
 *
 * So when *should* you use NullptrT?  Thus far, the only truly good use seems
 * to be as an argument type for operator overloads (because C++ doesn't allow
 * operator= to have more than one argument, operator== to have more than two,
 * &c.).  But even in such cases, it really only works if there are no other
 * overloads of the operator that accept a pointer type.  If you want both T*
 * and nullptr_t overloads, you'll have to wait til we drop gcc 4.4/4.5 support.
 * (Currently b2g is the only impediment to this.)
 */
#ifdef MOZ_HAVE_CXX11_NULLPTR
// decltype does the right thing for actual nullptr.
namespace mozilla {
typedef decltype(nullptr) NullptrT;
template<>
struct IsNullPointer<decltype(nullptr)> { static const bool value = true; };
}
#  undef MOZ_HAVE_CXX11_NULLPTR
#elif MOZ_IS_GCC
#  define nullptr __null
// void* sweeps up more than just nullptr, but compilers supporting true
// nullptr are the majority now, so they should detect mistakes.  If you're
// feeling paranoid, check/assert that your NullptrT equals nullptr.
namespace mozilla { typedef void* NullptrT; }
#else
#  error "No compiler support for nullptr or its emulation."
#endif

#endif /* mozilla_NullPtr_h */
