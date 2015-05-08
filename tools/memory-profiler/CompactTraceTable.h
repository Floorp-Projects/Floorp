/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef memory_profiler_CompactTraceTable_h
#define memory_profiler_CompactTraceTable_h

#include "UncensoredAllocator.h"

#include <functional>
#include <utility>

#include "mozilla/HashFunctions.h"

namespace mozilla {

struct TrieNode final
{
  uint32_t parentIdx;
  uint32_t nameIdx;
  bool operator==(const TrieNode t) const
  {
    return parentIdx == t.parentIdx && nameIdx == t.nameIdx;
  }
};

} // namespace mozilla

namespace std {
template<>
struct hash<mozilla::TrieNode>
{
  size_t operator()(const mozilla::TrieNode& v) const
  {
    uint64_t k = static_cast<uint64_t>(v.parentIdx) << 32 | v.nameIdx;
    return std::hash<uint64_t>()(k);
  }
};
#ifdef MOZ_REPLACE_MALLOC
template<>
struct hash<mozilla::u_string>
{
  size_t operator()(const mozilla::u_string& v) const
  {
    return mozilla::HashString(v.c_str());
  }
};
#endif
} // namespace std

namespace mozilla {

// This class maps a Node of type T to its parent's index in the
// map. When serializing, the map is traversed and put into an ordered
// vector of Nodes.
template<typename T>
class NodeIndexMap final
{
public:
  uint32_t Insert(const T& e)
  {
    auto i = mMap.insert(std::make_pair(e, mMap.size()));
    return i.first->second;
  }

  u_vector<T> Serialize() const
  {
    u_vector<T> v(mMap.size());
    for (auto i: mMap) {
      v[i.second] = i.first;
    }
    return v;
  }

  uint32_t Size() const
  {
    return mMap.size();
  }

  void Clear()
  {
    mMap.clear();
  }
private:
  u_unordered_map<T, uint32_t> mMap;
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
    mNames.Insert("(unknown)");
    mTraces.Insert(TrieNode{0, 0});
  }

  u_vector<u_string> GetNames() const
  {
    return mNames.Serialize();
  }

  u_vector<TrieNode> GetTraces() const
  {
    return mTraces.Serialize();
  }

  // Returns an ID to a stacktrace.
  uint32_t Insert(const u_vector<u_string>& aRawStacktrace)
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
  NodeIndexMap<u_string> mNames;
  NodeIndexMap<TrieNode> mTraces;
};

} // namespace mozilla

#endif // memory_profiler_CompactTraceTable_h
