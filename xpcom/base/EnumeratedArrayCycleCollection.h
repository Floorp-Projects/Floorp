/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef EnumeratedArrayCycleCollection_h_
#define EnumeratedArrayCycleCollection_h_

#include "mozilla/EnumeratedArray.h"
#include "nsCycleCollectionTraversalCallback.h"

template <typename IndexType, typename ValueType, size_t Size>
inline void ImplCycleCollectionUnlink(
    mozilla::EnumeratedArray<IndexType, ValueType, Size>& aField) {
  for (size_t i = 0; i < Size; ++i) {
    aField[IndexType(i)] = nullptr;
  }
}

template <typename IndexType, typename ValueType, size_t Size>
inline void ImplCycleCollectionTraverse(
    nsCycleCollectionTraversalCallback& aCallback,
    mozilla::EnumeratedArray<IndexType, ValueType, Size>& aField,
    const char* aName, uint32_t aFlags = 0) {
  aFlags |= CycleCollectionEdgeNameArrayFlag;
  for (size_t i = 0; i < Size; ++i) {
    ImplCycleCollectionTraverse(aCallback, aField[IndexType(i)], aName, aFlags);
  }
}

#endif  // EnumeratedArrayCycleCollection_h_
