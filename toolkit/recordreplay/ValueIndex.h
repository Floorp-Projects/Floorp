/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_recordreplay_ValueIndex_h
#define mozilla_recordreplay_ValueIndex_h

#include "mozilla/Types.h"

#include <unordered_map>

namespace mozilla {
namespace recordreplay {

// ValueIndexes are a bidirectional map between arbitrary pointers and indexes.
// These are used while recording and replaying to handle the general issue
// that pointer values are not preserved during replay: recording a pointer and
// replaying its bits later will not yield a pointer to the same heap value,
// but rather a pointer to garbage that must not be dereferenced.
//
// When entries are added to a ValueIndex at consistent points between
// recording and replaying, then the resulting indexes will be consistent, and
// that index can be recorded and later replayed and used to find the
// replay-specific pointer value corresponding to the pointer used at that
// point in the recording. Entries can be removed from the ValueIndex at
// different points in the recording and replay without affecting the indexes
// that will be generated later.
//
// This is a helper class that is used in various places to help record/replay
// pointers to heap data.
class ValueIndex
{
public:
  ValueIndex()
    : mIndexCount(0)
  {}

  typedef std::unordered_map<const void*, size_t> ValueToIndexMap;

  // Add a new entry to the map.
  size_t Insert(const void* aValue);

  // Remove an entry from the map, unless there is no entry for aValue.
  void Remove(const void* aValue);

  // Get the index for an entry in the map. The entry must exist in the map.
  size_t GetIndex(const void* aValue);

  // Get the index for an entry in the map if there is one, otherwise return
  // false.
  bool MaybeGetIndex(const void* aValue, size_t* aIndex);

  // Return whether there is an entry for aValue.
  bool Contains(const void* aValue);

  // Get the value associated with an index. The index must exist in the map.
  const void* GetValue(size_t aIndex);

  // Whether the map is empty.
  bool IsEmpty();

  // Raw read-only access to the map contents.
  const ValueToIndexMap& GetValueToIndexMap();

private:
  typedef std::unordered_map<size_t, const void*> IndexToValueMap;

  // Map from pointer values to indexes.
  ValueToIndexMap mValueToIndex;

  // Map from indexes to pointer values.
  IndexToValueMap mIndexToValue;

  // The total number of entries that have ever been added to this map.
  size_t mIndexCount;
};

} // namespace recordreplay
} // namespace mozilla

#endif // mozilla_recordreplay_ValueIndex_h
