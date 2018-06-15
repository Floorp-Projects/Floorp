#include "RtpSourceObserver.h"
#include "nsThreadUtils.h"
#include "webrtc/system_wrappers/include/clock.h"
#include "webrtc/modules/include/module_common_types.h"

namespace mozilla {

using EntryType = dom::RTCRtpSourceEntryType;

double
RtpSourceObserver::RtpSourceEntry::ToLinearAudioLevel() const
{
  // Spec indicates that a value of 127 should be set to 0
  if (audioLevel == 127) {
    return 0;
  }
  // All other values are calculated as 10^(-rfc_level/20)
  return std::pow(10, -static_cast<double>(audioLevel) / 20);
}

RtpSourceObserver::RtpSourceObserver() :
  mMaxJitterWindow(0),
  mLevelGuard("RtpSourceObserver::mLevelGuard") {}

void
RtpSourceObserver::OnRtpPacket(const webrtc::WebRtcRTPHeader* aHeader,
                                     const int64_t aTimestamp,
                                     const uint32_t aJitter)
{
  auto& header = aHeader->header;
  MutexAutoLock lock(mLevelGuard);
  {
    mMaxJitterWindow = std::max(mMaxJitterWindow,
                                static_cast<int64_t>(aJitter) * 2);
    const auto jitterAdjusted = aTimestamp + aJitter;
    auto& hist = mRtpSources[GetKey(header.ssrc, EntryType::Synchronization)];
    hist.Prune(aTimestamp);
    // ssrc-audio-level handling
    hist.Insert(aTimestamp, jitterAdjusted,
                header.extension.hasAudioLevel,
                header.extension.audioLevel);

    // csrc-audio-level handling
    const auto& list = header.extension.csrcAudioLevels;
    for (uint8_t i = 0; i < header.numCSRCs; i++) {
      const uint32_t& csrc = header.arrOfCSRCs[i];
      auto& hist = mRtpSources[GetKey(csrc, EntryType::Contributing)];
      hist.Prune(aTimestamp);
      bool hasLevel = i < list.numAudioLevels;
      uint8_t level = hasLevel ? list.arrOfAudioLevels[i] : 0;
      hist.Insert(aTimestamp, jitterAdjusted, hasLevel, level);
    }
  }
}

void
RtpSourceObserver::GetRtpSources(const int64_t aTimeNow,
    nsTArray<dom::RTCRtpSourceEntry>& outSources) const
{
  MutexAutoLock lock(mLevelGuard);
  outSources.Clear();
  for (const auto& it : mRtpSources) {
    const RtpSourceEntry* entry = it.second.FindClosestNotAfter(aTimeNow);
    if (entry) {
      dom::RTCRtpSourceEntry domEntry;
      domEntry.mSource = GetSourceFromKey(it.first);
      domEntry.mSourceType = GetTypeFromKey(it.first);
      domEntry.mTimestamp = entry->jitterAdjustedTimestamp;
      if (entry->hasAudioLevel) {
        domEntry.mAudioLevel.Construct(entry->ToLinearAudioLevel());
      }
      outSources.AppendElement(std::move(domEntry));
    }
  }
}

int64_t RtpSourceObserver::NowInReportClockTime() {
  return webrtc::Clock::GetRealTimeClock()->TimeInMilliseconds();
}

const RtpSourceObserver::RtpSourceEntry*
RtpSourceObserver::RtpSourceHistory::FindClosestNotAfter(int64_t aTime) const {
  // This method scans the history for the entry whose timestamp is closest to a
  // given timestamp but no greater. Because it is scanning forward, it keeps
  // track of the closest entry it has found so far in case it overshoots.
  // There is no before map.begin() which complicates things, so found tracks
  // if something was really found.
  auto lastFound = mDetailedHistory.cbegin();
  bool found = false;
  for (const auto& it : mDetailedHistory) {
    if (it.second.jitterAdjustedTimestamp > aTime) {
      break;
    }
    // lastFound can't start before begin, so the first inc must be skipped
    if (found) {
      lastFound++;
    }
    found = true;
  }
  if (found) {
    return &lastFound->second;
  }
  if (HasEvicted() && aTime >= mLatestEviction.jitterAdjustedTimestamp) {
    return &mLatestEviction;
  }
  return nullptr;
}

void
RtpSourceObserver::RtpSourceHistory::Prune(const int64_t aTimeNow) {
  const auto aTimeT = aTimeNow - mMaxJitterWindow;
  const auto aTimePrehistory = aTimeNow - kHistoryWindow;
  bool found = false;
  // New lower bound of the map
  auto lower = mDetailedHistory.begin();
  for (auto& it : mDetailedHistory) {
    if (it.second.jitterAdjustedTimestamp > aTimeT) {
      found = true;
      break;
    }
    if (found) {
      lower++;
    }
    found = true;
  }
  if (found) {
    if (lower->second.jitterAdjustedTimestamp > aTimePrehistory) {
      mLatestEviction = lower->second;
      mHasEvictedEntry = true;
    }
    lower++;
    mDetailedHistory.erase(mDetailedHistory.begin(), lower);
  }
  if (HasEvicted() &&
      (mLatestEviction.jitterAdjustedTimestamp + kHistoryWindow) < aTimeNow) {
    mHasEvictedEntry = false;
  }
}

void
RtpSourceObserver::RtpSourceHistory::Insert(const int64_t aTimeNow,
                                            const int64_t aTimestamp,
                                            const bool aHasAudioLevel,
                                            const uint8_t aAudioLevel)
{
  Insert(aTimeNow, aTimestamp).Update(aTimestamp, aHasAudioLevel, aAudioLevel);
}

RtpSourceObserver::RtpSourceEntry&
RtpSourceObserver::RtpSourceHistory::Insert(const int64_t aTimeNow,
                                            const int64_t aTimestamp)
{
  // Time T is the oldest time inside the jitter window (now - jitter)
  // Time J is the newest time inside the jitter window (now + jitter)
  // Time x is the jitter adjusted entry time
  // Time Z is the time of the long term storage element
  // Times A, B, C are times of entries in the jitter window buffer
  // x-axis: time
  // x or x        T   J
  //  |------Z-----|ABC| -> |------Z-----|ABC|
  if ((aTimestamp + kHistoryWindow) < aTimeNow ||
      aTimestamp < mLatestEviction.jitterAdjustedTimestamp) {
    return mPrehistory; // A.K.A. /dev/null
  }
  mMaxJitterWindow = std::max(mMaxJitterWindow,
                              (aTimestamp - aTimeNow) * 2);
  const int64_t aTimeT = aTimeNow - mMaxJitterWindow;
  //           x  T   J
  // |------Z-----|ABC| -> |--------x---|ABC|
  if (aTimestamp < aTimeT) {
    mHasEvictedEntry = true;
    return mLatestEviction;
  }
  //              T  X J
  // |------Z-----|AB-C| -> |--------x---|ABXC|
  return mDetailedHistory[aTimestamp];
}

}
