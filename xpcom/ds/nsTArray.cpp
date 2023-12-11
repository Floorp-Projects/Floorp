/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsTArray.h"
#include "nsXPCOM.h"
#include "nsCycleCollectionNoteChild.h"
#include "nsDebug.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/IntegerPrintfMacros.h"

// Ensure this is sufficiently aligned so that Elements() and co don't create
// unaligned pointers, or slices with unaligned pointers for empty arrays, see
// https://github.com/servo/servo/issues/22613.
alignas(8) const nsTArrayHeader sEmptyTArrayHeader = {0, 0, 0};

bool IsTwiceTheRequiredBytesRepresentableAsUint32(size_t aCapacity,
                                                  size_t aElemSize) {
  using mozilla::CheckedUint32;
  return ((CheckedUint32(aCapacity) * aElemSize) * 2).isValid();
}

void ::detail::SetCycleCollectionArrayFlag(uint32_t& aFlags) {
  aFlags |= CycleCollectionEdgeNameArrayFlag;
}
