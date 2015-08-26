/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef memory_profiler_CompactTraceTable_h
#define memory_profiler_CompactTraceTable_h

#include "mozilla/HashFunctions.h"

#include "nsDataHashtable.h"
#include "nsTArray.h"

namespace mozilla {

struct TrieNode final
{
  uint32_t parentIdx;
  uint32_t nameIdx;
  bool operator==(const TrieNode t) const
  {
    return parentIdx == t.parentIdx && nameIdx == t.nameIdx;
  }
  uint32_t Hash() const
  {
    return HashGeneric(parentIdx, nameIdx);
  }
};

// This class maps a Node of type T to its parent's index in the
// map. When serializing, the map is traversed and put into an ordered
// array of Nodes.
template<typename KeyClass, typename T>
class NodeIndexMap final
{
public:
  uint32_t Insert(const T& e)
  {
    uint32_t index = mMap.Count();
    if (!mMap.Get(e, &index)) {
      mMap.Put(e, index);
    }
    return index;
  }

  nsTArray<T> Serialize() const
  {
    nsTArray<T> v;
    v.SetLength(mMap.Count());
    for (auto iter = mMap.ConstIter(); !iter.Done(); iter.Next()) {
      v[iter.Data()] = iter.Key();
    }
    return v;
  }

  uint32_t Size() const
  {
    return mMap.Count();
  }

  void Clear()
  {
    mMap.Clear();
  }
private:
  nsDataHashtable<KeyClass, uint32_t> mMap;
};

// Backtraces are stored in a trie to save spaces.
// Function names are stored in an unique table and TrieNodes contain indexes
// into that table.
// The trie is implemented with a hash table; children are stored in
// traces[TrieNode{parent node index, branch/function name index}].
class CompactTraceTable final
{
public:
  CompactTraceTable()
  {
    mNames.Insert(nsAutoCString("(unknown)"));
    mTraces.Insert(TrieNode{0, 0});
  }

  nsTArray<nsCString> GetNames() const
  {
    return mNames.Serialize();
  }

  nsTArray<TrieNode> GetTraces() const
  {
    return mTraces.Serialize();
  }

  // Returns an ID to a stacktrace.
  uint32_t Insert(const nsTArray<nsCString>& aRawStacktrace)
  {
    uint32_t parent = 0;
    for (auto& frame: aRawStacktrace) {
      parent = mTraces.Insert(TrieNode{parent, mNames.Insert(frame)});
    }
    return parent;
  }

  void Reset()
  {
    mNames.Clear();
    mTraces.Clear();
  }
private:
  NodeIndexMap<nsCStringHashKey, nsCString> mNames;
  NodeIndexMap<nsGenericHashKey<TrieNode>, TrieNode> mTraces;
};

} // namespace mozilla

#endif // memory_profiler_CompactTraceTable_h
