/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Math operations that implement wraparound semantics on overflow or underflow.
 */

#ifndef mozilla_WrappingOperations_h
#define mozilla_WrappingOperations_h

#include "mozilla/Attributes.h"
#include "mozilla/TypeTraits.h"

#include <limits.h>

namespace mozilla {

namespace detail {

template<typename UnsignedType>
struct WrapToSignedHelper
{
  static_assert(mozilla::IsUnsigned<UnsignedType>::value,
                "WrapToSigned must be passed an unsigned type");

  using SignedType = typename mozilla::MakeSigned<UnsignedType>::Type;

  static constexpr SignedType MaxValue =
    (UnsignedType(1) << (CHAR_BIT * sizeof(SignedType) - 1)) - 1;
  static constexpr SignedType MinValue = -MaxValue - 1;

  static constexpr UnsignedType MinValueUnsigned =
    static_cast<UnsignedType>(MinValue);
  static constexpr UnsignedType MaxValueUnsigned =
    static_cast<UnsignedType>(MaxValue);

  // Overflow-correctness was proven in bug 1432646 and is explained in the
  // comment below.  This function is very hot, both at compile time and
  // runtime, so disable all overflow checking in it.
  MOZ_NO_SANITIZE_UNSIGNED_OVERFLOW MOZ_NO_SANITIZE_SIGNED_OVERFLOW
  static constexpr SignedType compute(UnsignedType aValue)
  {
    // This algorithm was originally provided here:
    // https://stackoverflow.com/questions/13150449/efficient-unsigned-to-signed-cast-avoiding-implementation-defined-behavior
    //
    // If the value is in the non-negative signed range, just cast.
    //
    // If the value will be negative, compute its delta from the first number
    // past the max signed integer, then add that to the minimum signed value.
    //
    // At the low end: if |u| is the maximum signed value plus one, then it has
    // the same mathematical value as |MinValue| cast to unsigned form.  The
    // delta is zero, so the signed form of |u| is |MinValue| -- exactly the
    // result of adding zero delta to |MinValue|.
    //
    // At the high end: if |u| is the maximum *unsigned* value, then it has all
    // bits set.  |MinValue| cast to unsigned form is purely the high bit set.
    // So the delta is all bits but high set -- exactly |MaxValue|.  And as
    // |MinValue = -MaxValue - 1|, we have |MaxValue + (-MaxValue - 1)| to
    // equal -1.
    //
    // Thus the delta below is in signed range, the corresponding cast is safe,
    // and this computation produces values spanning [MinValue, 0): exactly the
    // desired range of all negative signed integers.
    return (aValue <= MaxValueUnsigned)
           ? static_cast<SignedType>(aValue)
           : static_cast<SignedType>(aValue - MinValueUnsigned) + MinValue;
  }
};

} // namespace detail

/**
 * Convert an unsigned value to signed, if necessary wrapping around.
 *
 * This is the behavior normal C++ casting will perform in most implementations
 * these days -- but this function makes explicit that such conversion is
 * happening.
 */
template<typename UnsignedType>
inline constexpr typename detail::WrapToSignedHelper<UnsignedType>::SignedType
WrapToSigned(UnsignedType aValue)
{
  return detail::WrapToSignedHelper<UnsignedType>::compute(aValue);
}

} /* namespace mozilla */

#endif /* mozilla_WrappingOperations_h */
