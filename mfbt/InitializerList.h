/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 *  * License, v. 2.0. If a copy of the MPL was not distributed with this
 *   * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* A polyfill for std::initializer_list if it doesn't exist */

#ifndef mozilla_InitializerList_h
#define mozilla_InitializerList_h

#include <mozilla/Compiler.h>
#include <mozilla/Attributes.h>
#if MOZ_USING_LIBCXX
#  define MOZ_HAVE_INITIALIZER_LIST
#elif MOZ_USING_LIBSTDCXX && __GLIBCXX__ >= 20090421 // GCC 4.4.0
#  define MOZ_HAVE_INITIALIZER_LIST
#elif _MSC_VER
#  define MOZ_HAVE_INITIALIZER_LIST
#elif defined(MOZ_USING_STLPORT)
#else
#  error "Unknown standard library situation"
#endif

#ifdef MOZ_HAVE_INITIALIZER_LIST
#  include <initializer_list>
#else
/* Normally we would put things in mozilla:: however, std::initializer_list is a
 * magic name used by the compiler and so we need to name it that way.
 */
namespace std
{

template<class T>
class initializer_list
{
    /* This matches the representation used by GCC and Clang which
     * are the only compilers we need to polyfill */
    const T* mBegin;
    size_t   mSize;

    /* This constructor is called directly by the compiler */
    initializer_list(const T* aBegin, size_t aSize)
      : mBegin(aBegin)
      , mSize(aSize) { }
public:

    MOZ_CONSTEXPR initializer_list() : mBegin(nullptr), mSize(0) {}

    typedef T value_type;
    typedef const T& reference;
    typedef const T& const_reference;
    typedef size_t size_type;
    typedef const T* iterator;
    typedef const T* const_iterator;

    size_t size() const { return mSize; }
    const T* begin() const { return mBegin; }
    const T* end() const { return mBegin + mSize; }
};

}
#endif /* MOZ_HAS_INITIALIZER_LIST */
#endif /* mozilla_InitializerList_h */
