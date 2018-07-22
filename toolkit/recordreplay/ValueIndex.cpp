/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ValueIndex.h"

#include "mozilla/Assertions.h"

namespace mozilla {
namespace recordreplay {

size_t
ValueIndex::Insert(const void* aValue)
{
  MOZ_RELEASE_ASSERT(!Contains(aValue));

  size_t index = mIndexCount++;
  mValueToIndex.insert(ValueToIndexMap::value_type(aValue, index));
  mIndexToValue.insert(IndexToValueMap::value_type(index, aValue));
  return index;
}

void
ValueIndex::Remove(const void* aValue)
{
  size_t index;
  if (!MaybeGetIndex(aValue, &index)) {
    return;
  }

  mValueToIndex.erase(aValue);
  mIndexToValue.erase(index);
}

size_t
ValueIndex::GetIndex(const void* aValue)
{
  size_t index;
  if (!MaybeGetIndex(aValue, &index)) {
    MOZ_CRASH();
  }
  return index;
}

bool
ValueIndex::MaybeGetIndex(const void* aValue, size_t* aIndex)
{
  ValueToIndexMap::const_iterator iter = mValueToIndex.find(aValue);
  if (iter != mValueToIndex.end()) {
    *aIndex = iter->second;
    return true;
  }
  return false;
}

bool
ValueIndex::Contains(const void* aValue)
{
  size_t index;
  return MaybeGetIndex(aValue, &index);
}

const void*
ValueIndex::GetValue(size_t aIndex)
{
  IndexToValueMap::const_iterator iter = mIndexToValue.find(aIndex);
  MOZ_RELEASE_ASSERT(iter != mIndexToValue.end());
  return iter->second;
}

bool
ValueIndex::IsEmpty()
{
  MOZ_ASSERT(mValueToIndex.empty() == mIndexToValue.empty());
  return mValueToIndex.empty();
}

const ValueIndex::ValueToIndexMap&
ValueIndex::GetValueToIndexMap()
{
  return mValueToIndex;
}

} // namespace recordreplay
} // namespace mozilla
