
/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "RTCStatsReport.h"
#include "mozilla/dom/Performance.h"
#include "mozilla/dom/PerformanceService.h"
#include "nsRFPService.h"

namespace mozilla {
namespace dom {

RTCStatsTimestampMaker::RTCStatsTimestampMaker(const GlobalObject* aGlobal) {
  nsCOMPtr<nsPIDOMWindowInner> window =
      do_QueryInterface(aGlobal->GetAsSupports());
  if (window) {
    mRandomTimelineSeed = window->GetPerformance()->GetRandomTimelineSeed();
    mStartMonotonic = window->GetPerformance()->CreationTimeStamp();
    // Ugh. Performance::TimeOrigin is not constant, which means we need to
    // emulate this weird behavior so our time stamps are consistent with JS
    // timeOrigin. This is based on the code here:
    // https://searchfox.org/mozilla-central/rev/
    // 053826b10f838f77c27507e5efecc96e34718541/dom/performance/Performance.cpp#111-117
    mStartWallClockRaw =
        PerformanceService::GetOrCreate()->TimeOrigin(mStartMonotonic);
    MOZ_ASSERT(window->AsGlobal());
    mCrossOriginIsolated = window->AsGlobal()->CrossOriginIsolated();
  }
}

DOMHighResTimeStamp RTCStatsTimestampMaker::GetNow() const {
  // webrtc-pc says to use performance.timeOrigin + performance.now(), but
  // keeping a Performance object around is difficult because it is
  // main-thread-only. So, we perform the same calculation here. Note that this
  // can be very different from the current wall-clock time because of changes
  // to the wall clock, or monotonic clock drift over long periods of time.
  // We are very careful to do exactly what Performance does, to avoid timestamp
  // discrepancies.
  DOMHighResTimeStamp msSinceStart =
      (TimeStamp::NowUnfuzzed() - mStartMonotonic).ToMilliseconds();
  // mRandomTimelineSeed is not set in the unit-tests.
  if (mRandomTimelineSeed) {
    msSinceStart = nsRFPService::ReduceTimePrecisionAsMSecs(
        msSinceStart, mRandomTimelineSeed, /* aIsSystemPrincipal */ false,
        mCrossOriginIsolated);
  }
  return msSinceStart + nsRFPService::ReduceTimePrecisionAsMSecs(
                            mStartWallClockRaw, 0,
                            /* aIsSystemPrincipal */ false,
                            mCrossOriginIsolated);
}

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(RTCStatsReport, mParent)

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(RTCStatsReport, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(RTCStatsReport, Release)

RTCStatsReport::RTCStatsReport(nsPIDOMWindowInner* aParent)
    : mParent(aParent) {}

/*static*/
already_AddRefed<RTCStatsReport> RTCStatsReport::Constructor(
    const GlobalObject& aGlobal) {
  nsCOMPtr<nsPIDOMWindowInner> window(
      do_QueryInterface(aGlobal.GetAsSupports()));
  RefPtr<RTCStatsReport> report(new RTCStatsReport(window));
  return report.forget();
}

JSObject* RTCStatsReport::WrapObject(JSContext* aCx,
                                     JS::Handle<JSObject*> aGivenProto) {
  return RTCStatsReport_Binding::Wrap(aCx, this, aGivenProto);
}

void RTCStatsReport::Incorporate(RTCStatsCollection& aStats) {
  SetRTCStats(aStats.mIceCandidatePairStats);
  SetRTCStats(aStats.mIceCandidateStats);
  SetRTCStats(aStats.mInboundRtpStreamStats);
  SetRTCStats(aStats.mOutboundRtpStreamStats);
  SetRTCStats(aStats.mRemoteInboundRtpStreamStats);
  SetRTCStats(aStats.mRemoteOutboundRtpStreamStats);
  SetRTCStats(aStats.mRtpContributingSourceStats);
  SetRTCStats(aStats.mTrickledIceCandidateStats);
}

void RTCStatsReport::Set(const nsAString& aKey, JS::Handle<JSObject*> aValue,
                         ErrorResult& aRv) {
  RTCStatsReport_Binding::MaplikeHelpers::Set(this, aKey, aValue, aRv);
}

}  // namespace dom
}  // namespace mozilla
