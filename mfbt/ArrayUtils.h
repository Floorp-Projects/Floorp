/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Implements various helper functions related to arrays.
 */

#ifndef mozilla_ArrayUtils_h
#define mozilla_ArrayUtils_h

#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"

#include <stddef.h>

#ifdef __cplusplus

#include "mozilla/Alignment.h"
#include "mozilla/Array.h"
#include "mozilla/TypeTraits.h"

namespace mozilla {

/*
 * Safely subtract two pointers when it is known that aEnd >= aBegin.  This
 * avoids the common compiler bug that if (size_t(aEnd) - size_t(aBegin)) has
 * the MSB set, the unsigned subtraction followed by right shift will produce
 * -1, or size_t(-1), instead of the real difference.
 */
template<class T>
MOZ_ALWAYS_INLINE size_t
PointerRangeSize(T* aBegin, T* aEnd)
{
  MOZ_ASSERT(aEnd >= aBegin);
  return (size_t(aEnd) - size_t(aBegin)) / sizeof(T);
}

/*
 * Compute the length of an array with constant length.  (Use of this method
 * with a non-array pointer will not compile.)
 *
 * Beware of the implicit trailing '\0' when using this with string constants.
 */
template<typename T, size_t N>
MOZ_CONSTEXPR size_t
ArrayLength(T (&aArr)[N])
{
  return N;
}

template<typename T, size_t N>
MOZ_CONSTEXPR size_t
ArrayLength(const Array<T, N>& aArr)
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
ArrayEnd(T (&aArr)[N])
{
  return aArr + ArrayLength(aArr);
}

template<typename T, size_t N>
MOZ_CONSTEXPR T*
ArrayEnd(Array<T, N>& aArr)
{
  return &aArr[0] + ArrayLength(aArr);
}

template<typename T, size_t N>
MOZ_CONSTEXPR const T*
ArrayEnd(const Array<T, N>& aArr)
{
  return &aArr[0] + ArrayLength(aArr);
}

namespace detail {

template<typename AlignType, typename Pointee>
struct AlignedChecker
{
  static void
  test(Pointee* aPtr)
  {
    MOZ_ASSERT((uintptr_t(aPtr) % MOZ_ALIGNOF(AlignType)) == 0,
               "performing a range-check with a misaligned pointer");
  }
};

template<typename Pointee>
struct AlignedChecker<void, Pointee>
{
  static void
  test(Pointee* aPtr)
  {
  }
};

} // namespace detail

/**
 * Determines whether |aPtr| points at an object in the range [aBegin, aEnd).
 *
 * |aPtr| must have the same alignment as |aBegin| and |aEnd|.  This usually
 * should be achieved by ensuring |aPtr| points at a |U|, not just that it
 * points at a |T|.
 *
 * It is a usage error for any argument to be misaligned.
 *
 * It's okay for T* to be void*, and if so U* may also be void*.  In the latter
 * case no argument is required to be aligned (obviously, as void* implies no
 * particular alignment).
 */
template<typename T, typename U>
inline typename EnableIf<IsSame<T, U>::value ||
                         IsBaseOf<T, U>::value ||
                         IsVoid<T>::value,
                         bool>::Type
IsInRange(T* aPtr, U* aBegin, U* aEnd)
{
  MOZ_ASSERT(aBegin <= aEnd);
  detail::AlignedChecker<U, T>::test(aPtr);
  detail::AlignedChecker<U, U>::test(aBegin);
  detail::AlignedChecker<U, U>::test(aEnd);
  return aBegin <= static_cast<U*>(aPtr) && static_cast<U*>(aPtr) < aEnd;
}

/**
 * Convenience version of the above method when the valid range is specified as
 * uintptr_t values.  As above, |aPtr| must be aligned, and |aBegin| and |aEnd|
 * must be aligned with respect to |T|.
 */
template<typename T>
inline bool
IsInRange(T* aPtr, uintptr_t aBegin, uintptr_t aEnd)
{
  return IsInRange(aPtr,
                   reinterpret_cast<T*>(aBegin), reinterpret_cast<T*>(aEnd));
}

namespace detail {

/*
 * Helper for the MOZ_ARRAY_LENGTH() macro to make the length a typesafe
 * compile-time constant even on compilers lacking constexpr support.
 */
template <typename T, size_t N>
char (&ArrayLengthHelper(T (&array)[N]))[N];

} /* namespace detail */

} /* namespace mozilla */

#endif /* __cplusplus */

/*
 * MOZ_ARRAY_LENGTH() is an alternative to mozilla::ArrayLength() for C files
 * that can't use C++ template functions and for static_assert() calls that
 * can't call ArrayLength() when it is not a C++11 constexpr function.
 */
#ifdef __cplusplus
#  define MOZ_ARRAY_LENGTH(array)   sizeof(mozilla::detail::ArrayLengthHelper(array))
#else
#  define MOZ_ARRAY_LENGTH(array)   (sizeof(array)/sizeof((array)[0]))
#endif

#endif /* mozilla_ArrayUtils_h */
