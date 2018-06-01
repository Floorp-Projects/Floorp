/* -*-  Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef HangReports_h__
#define HangReports_h__

#include <vector>
#include "mozilla/HangAnnotations.h"
#include "ProcessedStack.h"
#include "nsTArray.h"
#include "nsString.h"
#include "nsClassHashtable.h"
#include "CombinedStacks.h"

namespace mozilla {
namespace Telemetry {

nsresult
ComputeAnnotationsKey(const HangMonitor::HangAnnotations& aAnnotations, nsAString& aKeyOut);

class HangReports {
public:
  /**
   * This struct encapsulates information for an individual ChromeHang annotation.
   * mHangIndex is the index of the corresponding ChromeHang.
   */
  struct AnnotationInfo {
    AnnotationInfo(uint32_t aHangIndex,
                   HangMonitor::HangAnnotations&& aAnnotations)
      : mAnnotations(std::move(aAnnotations))
    {
      mHangIndices.AppendElement(aHangIndex);
    }
    AnnotationInfo(AnnotationInfo&& aOther)
      : mHangIndices(aOther.mHangIndices)
      , mAnnotations(std::move(aOther.mAnnotations))
    {}
    ~AnnotationInfo() = default;
    AnnotationInfo& operator=(AnnotationInfo&& aOther)
    {
      mHangIndices = aOther.mHangIndices;
      mAnnotations = std::move(aOther.mAnnotations);
      return *this;
    }
    // To save memory, a single AnnotationInfo can be associated to multiple chrome
    // hangs. The following array holds the index of each related chrome hang.
    nsTArray<uint32_t> mHangIndices;
    HangMonitor::HangAnnotations mAnnotations;

  private:
    // Force move constructor
    AnnotationInfo(const AnnotationInfo& aOther) = delete;
    void operator=(const AnnotationInfo& aOther) = delete;
  };
  size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;
#if defined(MOZ_GECKO_PROFILER)
  void AddHang(const Telemetry::ProcessedStack& aStack, uint32_t aDuration,
               int32_t aSystemUptime, int32_t aFirefoxUptime,
               HangMonitor::HangAnnotations&& aAnnotations);
  void PruneStackReferences(const size_t aRemovedStackIndex);
#endif
  uint32_t GetDuration(unsigned aIndex) const;
  int32_t GetSystemUptime(unsigned aIndex) const;
  int32_t GetFirefoxUptime(unsigned aIndex) const;
  const nsClassHashtable<nsStringHashKey, AnnotationInfo>& GetAnnotationInfo() const;
  const CombinedStacks& GetStacks() const;
private:
  /**
   * This struct encapsulates the data for an individual ChromeHang, excluding
   * annotations.
   */
  struct HangInfo {
    // Hang duration (in seconds)
    uint32_t mDuration;
    // System uptime (in minutes) at the time of the hang
    int32_t mSystemUptime;
    // Firefox uptime (in minutes) at the time of the hang
    int32_t mFirefoxUptime;
  };
  std::vector<HangInfo> mHangInfo;
  nsClassHashtable<nsStringHashKey, AnnotationInfo> mAnnotationInfo;
  CombinedStacks mStacks;
};

} // namespace Telemetry
} // namespace mozilla

#endif // CombinedStacks_h__
