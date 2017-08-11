/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "HangReports.h"

namespace mozilla {
namespace Telemetry {

using namespace HangMonitor;

// This utility function generates a string key that is used to index the annotations
// in a hash map from |HangReports::AddHang|.
nsresult
ComputeAnnotationsKey(const HangAnnotationsPtr& aAnnotations, nsAString& aKeyOut)
{
  UniquePtr<HangAnnotations::Enumerator> annotationsEnum = aAnnotations->GetEnumerator();
  if (!annotationsEnum) {
    return NS_ERROR_FAILURE;
  }

  // Append all the attributes to the key, to uniquely identify this annotation.
  nsAutoString  key;
  nsAutoString  value;
  while (annotationsEnum->Next(key, value)) {
    aKeyOut.Append(key);
    aKeyOut.Append(value);
  }

  return NS_OK;
}

#if defined(MOZ_GECKO_PROFILER)
/** The maximum number of stacks that we're keeping for hang reports. */
const size_t kMaxHangStacksKept = 50;

void
HangReports::AddHang(const Telemetry::ProcessedStack& aStack,
                     uint32_t aDuration,
                     int32_t aSystemUptime,
                     int32_t aFirefoxUptime,
                     HangAnnotationsPtr aAnnotations) {
  // Append the new stack to the stack's circular queue.
  size_t hangIndex = mStacks.AddStack(aStack);
  // Append the hang info at the same index, in mHangInfo.
  HangInfo info = { aDuration, aSystemUptime, aFirefoxUptime };
  if (mHangInfo.size() < kMaxHangStacksKept) {
    mHangInfo.push_back(info);
  } else {
    mHangInfo[hangIndex] = info;
    // Remove any reference to the stack overwritten in the circular queue
    // from the annotations.
    PruneStackReferences(hangIndex);
  }

  if (!aAnnotations) {
    return;
  }

  nsAutoString annotationsKey;
  // Generate a key to index aAnnotations in the hash map.
  nsresult rv = ComputeAnnotationsKey(aAnnotations, annotationsKey);
  if (NS_FAILED(rv)) {
    return;
  }

  AnnotationInfo* annotationsEntry = mAnnotationInfo.Get(annotationsKey);
  if (annotationsEntry) {
    // If the key is already in the hash map, append the index of the chrome hang
    // to its indices.
    annotationsEntry->mHangIndices.AppendElement(hangIndex);
    return;
  }

  // If the key was not found, add the annotations to the hash map.
  mAnnotationInfo.Put(annotationsKey, new AnnotationInfo(hangIndex, Move(aAnnotations)));
}

/**
 * This function removes links to discarded chrome hangs stacks and prunes unused
 * annotations.
 */
void
HangReports::PruneStackReferences(const size_t aRemovedStackIndex) {
  // We need to adjust the indices that link annotations to chrome hangs. Since we
  // removed a stack, we must remove all references to it and prune annotations
  // linked to no stacks.
  for (auto iter = mAnnotationInfo.Iter(); !iter.Done(); iter.Next()) {
    nsTArray<uint32_t>& stackIndices = iter.Data()->mHangIndices;
    size_t toRemove = stackIndices.NoIndex;
    for (size_t k = 0; k < stackIndices.Length(); k++) {
      // Is this index referencing the removed stack?
      if (stackIndices[k] == aRemovedStackIndex) {
        toRemove = k;
        break;
      }
    }

    // Remove the index referencing the old stack from the annotation.
    if (toRemove != stackIndices.NoIndex) {
      stackIndices.RemoveElementAt(toRemove);
    }

    // If this annotation no longer references any stack, drop it.
    if (!stackIndices.Length()) {
      iter.Remove();
    }
  }
}
#endif

size_t
HangReports::SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const {
  size_t n = 0;
  n += mStacks.SizeOfExcludingThis();
  // This is a crude approximation. See comment on
  // CombinedStacks::SizeOfExcludingThis.
  n += mHangInfo.capacity() * sizeof(HangInfo);
  n += mAnnotationInfo.ShallowSizeOfExcludingThis(aMallocSizeOf);
  n += mAnnotationInfo.Count() * sizeof(AnnotationInfo);
  for (auto iter = mAnnotationInfo.ConstIter(); !iter.Done(); iter.Next()) {
    n += iter.Key().SizeOfExcludingThisIfUnshared(aMallocSizeOf);
    n += iter.Data()->mAnnotations->SizeOfIncludingThis(aMallocSizeOf);
  }
  return n;
}

const CombinedStacks&
HangReports::GetStacks() const {
  return mStacks;
}

uint32_t
HangReports::GetDuration(unsigned aIndex) const {
  return mHangInfo[aIndex].mDuration;
}

int32_t
HangReports::GetSystemUptime(unsigned aIndex) const {
  return mHangInfo[aIndex].mSystemUptime;
}

int32_t
HangReports::GetFirefoxUptime(unsigned aIndex) const {
  return mHangInfo[aIndex].mFirefoxUptime;
}

const nsClassHashtable<nsStringHashKey, HangReports::AnnotationInfo>&
HangReports::GetAnnotationInfo() const {
  return mAnnotationInfo;
}

} // namespace Telemetry
} // namespace mozilla
