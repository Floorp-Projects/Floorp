/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * A compile-time constant-length array, with bounds-checking assertions -- but
 * unlike mozilla::Array, with indexes biased by a constant.
 *
 * Thus where mozilla::Array<int, 3> is a three-element array indexed by [0, 3),
 * mozilla::RangedArray<int, 8, 3> is a three-element array indexed by [8, 11).
 */

#ifndef mozilla_RangedArray_h
#define mozilla_RangedArray_h

#include "mozilla/Array.h"

namespace mozilla {

template<typename T, size_t MinIndex, size_t Length>
class RangedArray
{
public:
  T& operator[](size_t aIndex)
  {
    MOZ_ASSERT(aIndex == MinIndex || aIndex > MinIndex);
    return mArr[aIndex - MinIndex];
  }

  const T& operator[](size_t aIndex) const
  {
    MOZ_ASSERT(aIndex == MinIndex || aIndex > MinIndex);
    return mArr[aIndex - MinIndex];
  }

private:
  Array<T, Length> mArr;
};

} // namespace mozilla

#endif // mozilla_RangedArray_h
