// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDFIUM_THIRD_PARTY_BASE_STL_UTIL_H_
#define PDFIUM_THIRD_PARTY_BASE_STL_UTIL_H_

#include <algorithm>
#include <memory>
#include <set>

#include "third_party/base/numerics/safe_conversions.h"

namespace pdfium {

// Test to see if a set, map, hash_set or hash_map contains a particular key.
// Returns true if the key is in the collection.
template <typename Collection, typename Key>
bool ContainsKey(const Collection& collection, const Key& key) {
  return collection.find(key) != collection.end();
}

// Test to see if a collection like a vector contains a particular value.
// Returns true if the value is in the collection.
template <typename Collection, typename Value>
bool ContainsValue(const Collection& collection, const Value& value) {
  return std::find(collection.begin(), collection.end(), value) !=
         collection.end();
}

// Means of generating a key for searching STL collections of std::unique_ptr
// that avoids the side effect of deleting the pointer.
template <class T>
class FakeUniquePtr : public std::unique_ptr<T> {
 public:
  using std::unique_ptr<T>::unique_ptr;
  ~FakeUniquePtr() { std::unique_ptr<T>::release(); }
};

// Convenience routine for "int-fected" code, so that the stl collection
// size_t size() method return values will be checked.
template <typename ResultType, typename Collection>
ResultType CollectionSize(const Collection& collection) {
  return pdfium::base::checked_cast<ResultType>(collection.size());
}

// Track the addition of an object to a set, removing it automatically when
// the ScopedSetInsertion goes out of scope.
template <typename T>
class ScopedSetInsertion {
 public:
  ScopedSetInsertion(std::set<T>* org_set, T elem)
      : m_Set(org_set), m_Entry(elem) {
    m_Set->insert(m_Entry);
  }
  ~ScopedSetInsertion() { m_Set->erase(m_Entry); }

 private:
  std::set<T>* const m_Set;
  const T m_Entry;
};

}  // namespace pdfium

#endif  // PDFIUM_THIRD_PARTY_BASE_STL_UTIL_H_
