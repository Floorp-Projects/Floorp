/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Miscellaneous uncategorized functionality.  Please add new functionality to
 * new headers, or to other appropriate existing headers, not here.
 */

#ifndef mozilla_Util_h
#define mozilla_Util_h

#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "mozilla/Types.h"

#ifdef __cplusplus

#include "mozilla/Alignment.h"

namespace mozilla {

/*
 * Small utility for lazily constructing objects without using dynamic storage.
 * When a Maybe<T> is constructed, it is |empty()|, i.e., no value of T has
 * been constructed and no T destructor will be called when the Maybe<T> is
 * destroyed. Upon calling |construct|, a T object will be constructed with the
 * given arguments and that object will be destroyed when the owning Maybe<T>
 * is destroyed.
 *
 * N.B. GCC seems to miss some optimizations with Maybe and may generate extra
 * branches/loads/stores. Use with caution on hot paths.
 */
template<class T>
class Maybe
{
    AlignedStorage2<T> storage;
    bool constructed;

    T& asT() { return *storage.addr(); }

  public:
    Maybe() { constructed = false; }
    ~Maybe() { if (constructed) asT().~T(); }

    bool empty() const { return !constructed; }

    void construct() {
      MOZ_ASSERT(!constructed);
      ::new (storage.addr()) T();
      constructed = true;
    }

    template<class T1>
    void construct(const T1& t1) {
      MOZ_ASSERT(!constructed);
      ::new (storage.addr()) T(t1);
      constructed = true;
    }

    template<class T1, class T2>
    void construct(const T1& t1, const T2& t2) {
      MOZ_ASSERT(!constructed);
      ::new (storage.addr()) T(t1, t2);
      constructed = true;
    }

    template<class T1, class T2, class T3>
    void construct(const T1& t1, const T2& t2, const T3& t3) {
      MOZ_ASSERT(!constructed);
      ::new (storage.addr()) T(t1, t2, t3);
      constructed = true;
    }

    template<class T1, class T2, class T3, class T4>
    void construct(const T1& t1, const T2& t2, const T3& t3, const T4& t4) {
      MOZ_ASSERT(!constructed);
      ::new (storage.addr()) T(t1, t2, t3, t4);
      constructed = true;
    }

    template<class T1, class T2, class T3, class T4, class T5>
    void construct(const T1& t1, const T2& t2, const T3& t3, const T4& t4, const T5& t5) {
      MOZ_ASSERT(!constructed);
      ::new (storage.addr()) T(t1, t2, t3, t4, t5);
      constructed = true;
    }

    template<class T1, class T2, class T3, class T4, class T5,
             class T6>
    void construct(const T1& t1, const T2& t2, const T3& t3, const T4& t4, const T5& t5,
                   const T6& t6) {
      MOZ_ASSERT(!constructed);
      ::new (storage.addr()) T(t1, t2, t3, t4, t5, t6);
      constructed = true;
    }

    template<class T1, class T2, class T3, class T4, class T5,
             class T6, class T7>
    void construct(const T1& t1, const T2& t2, const T3& t3, const T4& t4, const T5& t5,
                   const T6& t6, const T7& t7) {
      MOZ_ASSERT(!constructed);
      ::new (storage.addr()) T(t1, t2, t3, t4, t5, t6, t7);
      constructed = true;
    }

    template<class T1, class T2, class T3, class T4, class T5,
             class T6, class T7, class T8>
    void construct(const T1& t1, const T2& t2, const T3& t3, const T4& t4, const T5& t5,
                   const T6& t6, const T7& t7, const T8& t8) {
      MOZ_ASSERT(!constructed);
      ::new (storage.addr()) T(t1, t2, t3, t4, t5, t6, t7, t8);
      constructed = true;
    }

    template<class T1, class T2, class T3, class T4, class T5,
             class T6, class T7, class T8, class T9>
    void construct(const T1& t1, const T2& t2, const T3& t3, const T4& t4, const T5& t5,
                   const T6& t6, const T7& t7, const T8& t8, const T9& t9) {
      MOZ_ASSERT(!constructed);
      ::new (storage.addr()) T(t1, t2, t3, t4, t5, t6, t7, t8, t9);
      constructed = true;
    }

    template<class T1, class T2, class T3, class T4, class T5,
             class T6, class T7, class T8, class T9, class T10>
    void construct(const T1& t1, const T2& t2, const T3& t3, const T4& t4, const T5& t5,
                   const T6& t6, const T7& t7, const T8& t8, const T9& t9, const T10& t10) {
      MOZ_ASSERT(!constructed);
      ::new (storage.addr()) T(t1, t2, t3, t4, t5, t6, t7, t8, t9, t10);
      constructed = true;
    }

    T* addr() {
      MOZ_ASSERT(constructed);
      return &asT();
    }

    T& ref() {
      MOZ_ASSERT(constructed);
      return asT();
    }

    const T& ref() const {
      MOZ_ASSERT(constructed);
      return const_cast<Maybe*>(this)->asT();
    }

    void destroy() {
      ref().~T();
      constructed = false;
    }

    void destroyIfConstructed() {
      if (!empty())
        destroy();
    }

  private:
    Maybe(const Maybe& other) MOZ_DELETE;
    const Maybe& operator=(const Maybe& other) MOZ_DELETE;
};

/*
 * Safely subtract two pointers when it is known that end >= begin.  This avoids
 * the common compiler bug that if (size_t(end) - size_t(begin)) has the MSB
 * set, the unsigned subtraction followed by right shift will produce -1, or
 * size_t(-1), instead of the real difference.
 */
template<class T>
MOZ_ALWAYS_INLINE size_t
PointerRangeSize(T* begin, T* end)
{
  MOZ_ASSERT(end >= begin);
  return (size_t(end) - size_t(begin)) / sizeof(T);
}

/*
 * Compute the length of an array with constant length.  (Use of this method
 * with a non-array pointer will not compile.)
 *
 * Beware of the implicit trailing '\0' when using this with string constants.
 */
template<typename T, size_t N>
MOZ_CONSTEXPR size_t
ArrayLength(T (&arr)[N])
{
  return N;
}

/*
 * Compute the address one past the last element of a constant-length array.
 *
 * Beware of the implicit trailing '\0' when using this with string constants.
 */
template<typename T, size_t N>
MOZ_CONSTEXPR T*
ArrayEnd(T (&arr)[N])
{
  return arr + ArrayLength(arr);
}

} /* namespace mozilla */

#endif /* __cplusplus */

/*
 * MOZ_ARRAY_LENGTH() is an alternative to mozilla::ArrayLength() for C files
 * that can't use C++ template functions and for static_assert() calls that
 * can't call ArrayLength() when it is not a C++11 constexpr function.
 */
#ifdef MOZ_HAVE_CXX11_CONSTEXPR
#  define MOZ_ARRAY_LENGTH(array)   mozilla::ArrayLength(array)
#else
#  define MOZ_ARRAY_LENGTH(array)   (sizeof(array)/sizeof((array)[0]))
#endif

#endif /* mozilla_Util_h */
