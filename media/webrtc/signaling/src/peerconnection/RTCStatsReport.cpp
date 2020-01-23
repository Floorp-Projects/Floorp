
/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "RTCStatsReport.h"
#include "mozilla/dom/Performance.h"
#include "nsRFPService.h"

namespace mozilla {
namespace dom {

RTCStatsTimestampMaker::RTCStatsTimestampMaker(const GlobalObject* aGlobal) {
  nsCOMPtr<nsPIDOMWindowInner> window =
      do_QueryInterface(aGlobal->GetAsSupports());
  if (window) {
    mRandomTimelineSeed = window->GetPerformance()->GetRandomTimelineSeed();
    mStartWallClock = window->GetPerformance()->TimeOrigin();
    mStartMonotonic = window->GetPerformance()->CreationTimeStamp();
  }
}

DOMHighResTimeStamp RTCStatsTimestampMaker::GetNow() const {
  // webrtc-pc says to use performance.timeOrigin + performance.now(), but
  // keeping a Performance object around is difficult because it is
  // main-thread-only. So, we perform the same calculation here. Note that this
  // can be very different from the current wall-clock time because of changes
  // to the wall clock, or monotonic clock drift over long periods of time.
  TimeStamp nowMonotonic = TimeStamp::NowUnfuzzed();
  DOMHighResTimeStamp msSinceStart =
      (nowMonotonic - mStartMonotonic).ToMilliseconds();
  DOMHighResTimeStamp rawTime = mStartWallClock + msSinceStart;
  return nsRFPService::ReduceTimePrecisionAsMSecs(rawTime, mRandomTimelineSeed);
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
