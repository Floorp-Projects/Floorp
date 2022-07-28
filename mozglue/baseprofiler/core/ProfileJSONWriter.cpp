/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/BaseProfileJSONWriter.h"

namespace mozilla::baseprofiler {

UniqueJSONStrings::UniqueJSONStrings() { mStringTableWriter.StartBareList(); }

UniqueJSONStrings::UniqueJSONStrings(const UniqueJSONStrings& aOther,
                                     ProgressLogger aProgressLogger) {
  using namespace mozilla::literals::ProportionValue_literals;  // For `10_pc`.

  mStringTableWriter.StartBareList();
  uint32_t count = aOther.mStringHashToIndexMap.count();
  if (count != 0) {
    MOZ_ALWAYS_TRUE(mStringHashToIndexMap.reserve(count));
    auto iter = aOther.mStringHashToIndexMap.iter();
    for (auto&& [unusedIndex, progressLogger] :
         aProgressLogger.CreateLoopSubLoggersFromTo(
             10_pc, 90_pc, count, "Copying unique strings...")) {
      (void)unusedIndex;
      if (iter.done()) {
        break;
      }
      mStringHashToIndexMap.putNewInfallible(iter.get().key(),
                                             iter.get().value());
      iter.next();
    }
    aProgressLogger.SetLocalProgress(90_pc, "Copied unique strings");
    mStringTableWriter.CopyAndSplice(
        aOther.mStringTableWriter.ChunkedWriteFunc());
    aProgressLogger.SetLocalProgress(100_pc, "Spliced unique strings");
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
