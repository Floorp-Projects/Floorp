/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef EnumeratedArrayCycleCollection_h_
#define EnumeratedArrayCycleCollection_h_

#include "mozilla/EnumeratedArray.h"
#include "nsCycleCollectionTraversalCallback.h"

template<typename IndexType,
         MOZ_TEMPLATE_ENUM_CLASS_ENUM_TYPE(IndexType) SizeAsEnumValue,
         typename ValueType>
inline void
ImplCycleCollectionUnlink(mozilla::EnumeratedArray<IndexType,
                                                   SizeAsEnumValue,
                                                   ValueType>& aField)
{
  for (size_t i = 0; i < size_t(SizeAsEnumValue); ++i) {
    aField[IndexType(i)] = nullptr;
  }
}

template<typename IndexType,
         MOZ_TEMPLATE_ENUM_CLASS_ENUM_TYPE(IndexType) SizeAsEnumValue,
         typename ValueType>
inline void
ImplCycleCollectionTraverse(nsCycleCollectionTraversalCallback& aCallback,
                            mozilla::EnumeratedArray<IndexType,
                                                     SizeAsEnumValue,
                                                     ValueType>& aField,
                            const char* aName,
                            uint32_t aFlags = 0)
{
  aFlags |= CycleCollectionEdgeNameArrayFlag;
  for (size_t i = 0; i < size_t(SizeAsEnumValue); ++i) {
    ImplCycleCollectionTraverse(aCallback, aField[IndexType(i)], aName, aFlags);
  }
}

#endif // EnumeratedArrayCycleCollection_h_
