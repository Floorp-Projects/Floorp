/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TupleCycleCollection_h
#define TupleCycleCollection_h

#include "mozilla/Tuple.h"
#include "nsCycleCollectionNoteChild.h"

class nsCycleCollectionTraversalCallback;

template <typename... Elements>
inline void ImplCycleCollectionUnlink(mozilla::Tuple<Elements...>& aField) {
  ForEach(aField, [](auto& aElem) { ImplCycleCollectionUnlink(aElem); });
}

template <typename... Elements>
inline void ImplCycleCollectionTraverse(
    nsCycleCollectionTraversalCallback& aCallback,
    mozilla::Tuple<Elements...>& aField, const char* aName,
    uint32_t aFlags = 0) {
  aFlags |= CycleCollectionEdgeNameArrayFlag;
  ForEach(aField, [&](auto& aElem) {
    ImplCycleCollectionTraverse(aCallback, aElem, aName, aFlags);
  });
}

#endif  // TupleCycleCollection_h
