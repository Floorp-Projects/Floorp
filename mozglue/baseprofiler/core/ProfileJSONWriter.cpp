/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/BaseProfileJSONWriter.h"

namespace mozilla::baseprofiler {

UniqueJSONStrings::UniqueJSONStrings(JSONWriter::CollectionStyle aStyle) {
  mStringTableWriter.StartBareList(aStyle);
}

UniqueJSONStrings::UniqueJSONStrings(const UniqueJSONStrings& aOther,
                                     JSONWriter::CollectionStyle aStyle) {
  mStringTableWriter.StartBareList(aStyle);
  uint32_t count = aOther.mStringHashToIndexMap.count();
  if (count != 0) {
    MOZ_RELEASE_ASSERT(mStringHashToIndexMap.reserve(count));
    for (auto iter = aOther.mStringHashToIndexMap.iter(); !iter.done();
         iter.next()) {
      mStringHashToIndexMap.putNewInfallible(iter.get().key(),
                                             iter.get().value());
    }
    mStringTableWriter.CopyAndSplice(
        aOther.mStringTableWriter.ChunkedWriteFunc());
  }
}

UniqueJSONStrings::~UniqueJSONStrings() = default;

void UniqueJSONStrings::SpliceStringTableElements(
    SpliceableJSONWriter& aWriter) {
  aWriter.TakeAndSplice(mStringTableWriter.TakeChunkedWriteFunc());
}

uint32_t UniqueJSONStrings::GetOrAddIndex(const Span<const char>& aStr) {
  uint32_t count = mStringHashToIndexMap.count();
  HashNumber hash = HashString(aStr.data(), aStr.size());
  auto entry = mStringHashToIndexMap.lookupForAdd(hash);
  if (entry) {
    MOZ_ASSERT(entry->value() < count);
    return entry->value();
  }

  MOZ_RELEASE_ASSERT(mStringHashToIndexMap.add(entry, hash, count));
  mStringTableWriter.StringElement(aStr);
  return count;
}

}  // namespace mozilla::baseprofiler
