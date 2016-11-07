/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_Range_h
#define mozilla_Range_h

#include "mozilla/RangedPtr.h"
#include "mozilla/TypeTraits.h"

#include <stddef.h>

namespace mozilla {

// Range<T> is a tuple containing a pointer and a length.
template <typename T>
class Range
{
  const RangedPtr<T> mStart;
  const RangedPtr<T> mEnd;

public:
  Range() : mStart(nullptr, 0), mEnd(nullptr, 0) {}
  Range(T* aPtr, size_t aLength)
    : mStart(aPtr, aPtr, aPtr + aLength),
      mEnd(aPtr + aLength, aPtr, aPtr + aLength)
  {}
  Range(const RangedPtr<T>& aStart, const RangedPtr<T>& aEnd)
    : mStart(aStart.get(), aStart.get(), aEnd.get()),
      mEnd(aEnd.get(), aStart.get(), aEnd.get())
  {
    // Only accept two RangedPtrs within the same range.
    aStart.checkIdenticalRange(aEnd);
    MOZ_ASSERT(aStart <= aEnd);
  }

  template<typename U,
           class = typename EnableIf<IsConvertible<U (*)[], T (*)[]>::value,
                                     int>::Type>
  Range(const Range<U>& aOther)
    : mStart(aOther.mStart),
      mEnd(aOther.mEnd)
  {}

  RangedPtr<T> begin() const { return mStart; }
  RangedPtr<T> end() const { return mEnd; }
  size_t length() const { return mEnd - mStart; }

  T& operator[](size_t aOffset) const { return mStart[aOffset]; }

  explicit operator bool() const { return mStart != nullptr; }
};

} // namespace mozilla

#endif /* mozilla_Range_h */
