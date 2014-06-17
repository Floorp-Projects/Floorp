/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebrtcGlobalInformation.h"

#include <deque>
#include <string>

#include "CSFLog.h"
#include "WebRtcLog.h"
#include "mozilla/dom/WebrtcGlobalInformationBinding.h"

#include "nsAutoPtr.h"
#include "nsNetCID.h" // NS_SOCKETTRANSPORTSERVICE_CONTRACTID
#include "nsServiceManagerUtils.h" // do_GetService
#include "mozilla/ErrorResult.h"
#include "mozilla/Vector.h"
#include "nsProxyRelease.h"
#include "mozilla/Telemetry.h"

#include "rlogringbuffer.h"
#include "runnable_utils.h"
#include "PeerConnectionCtx.h"
#include "PeerConnectionImpl.h"
#include "webrtc/system_wrappers/interface/trace.h"

using sipcc::PeerConnectionImpl;
using sipcc::PeerConnectionCtx;
using sipcc::RTCStatsQuery;

static const char* logTag = "WebrtcGlobalInformation";

namespace mozilla {
namespace dom {

typedef Vector<nsAutoPtr<RTCStatsQuery>> RTCStatsQueries;

static sipcc::PeerConnectionCtx* GetPeerConnectionCtx()
{
  if(PeerConnectionCtx::isActive()) {
    MOZ_ASSERT(PeerConnectionCtx::GetInstance());
    return PeerConnectionCtx::GetInstance();
  }
  return nullptr;
}

static void OnStatsReport_m(
  nsMainThreadPtrHandle<WebrtcGlobalStatisticsCallback> aStatsCallback,
  nsAutoPtr<RTCStatsQueries> aQueryList)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aQueryList);

  WebrtcGlobalStatisticsReport report;
  report.mReports.Construct();

  // Reports for the currently active PeerConnections
  for (auto q = aQueryList->begin(); q != aQueryList->end(); ++q) {
    MOZ_ASSERT(*q);
    report.mReports.Value().AppendElement(*(*q)->report);
  }

  PeerConnectionCtx* ctx = GetPeerConnectionCtx();
  if (ctx) {
    // Reports for closed/destroyed PeerConnections
    report.mReports.Value().AppendElements(ctx->mStatsForClosedPeerConnections);
  }

  ErrorResult rv;
  aStatsCallback.get()->Call(report, rv);

  if (rv.Failed()) {
    CSFLogError(logTag, "Error firing stats observer callback");
  }
}

static void GetAllStats_s(
  nsMainThreadPtrHandle<WebrtcGlobalStatisticsCallback> aStatsCallback,
  nsAutoPtr<RTCStatsQueries> aQueryList)
{
  MOZ_ASSERT(aQueryList);

  for (auto q = aQueryList->begin(); q != aQueryList->end(); ++q) {
    MOZ_ASSERT(*q);
    PeerConnectionImpl::ExecuteStatsQuery_s(*q);
  }

  NS_DispatchToMainThread(WrapRunnableNM(&OnStatsReport_m,
                                         aStatsCallback,
                                         aQueryList),
                          NS_DISPATCH_NORMAL);
}

static void OnGetLogging_m(
  nsMainThreadPtrHandle<WebrtcGlobalLoggingCallback> aLoggingCallback,
  const std::string& aPattern,
  nsAutoPtr<std::deque<std::string>> aLogList)
{
  ErrorResult rv;
  if (!aLogList->empty()) {
    Sequence<nsString> nsLogs;
    for (auto l = aLogList->begin(); l != aLogList->end(); ++l) {
      nsLogs.AppendElement(NS_ConvertUTF8toUTF16(l->c_str()));
    }
    aLoggingCallback.get()->Call(nsLogs, rv);
  }

  if (rv.Failed()) {
    CSFLogError(logTag, "Error firing logging observer callback");
  }
}

static void GetLogging_s(
  nsMainThreadPtrHandle<WebrtcGlobalLoggingCallback> aLoggingCallback,
  const std::string& aPattern)
{
  RLogRingBuffer* logs = RLogRingBuffer::GetInstance();
  nsAutoPtr<std::deque<std::string>> result(new std::deque<std::string>);
  // Might not exist yet.
  if (logs) {
    logs->Filter(aPattern, 0, result);
  }
  NS_DispatchToMainThread(WrapRunnableNM(&OnGetLogging_m,
                                         aLoggingCallback,
                                         aPattern,
                                         result),
                          NS_DISPATCH_NORMAL);
}


void
WebrtcGlobalInformation::GetAllStats(
  const GlobalObject& aGlobal,
  WebrtcGlobalStatisticsCallback& aStatsCallback,
  const Optional<nsAString>& pcIdFilter,
  ErrorResult& aRv)
{
  if (!NS_IsMainThread()) {
    aRv.Throw(NS_ERROR_NOT_SAME_THREAD);
    return;
  }

  nsresult rv;
  nsCOMPtr<nsIEventTarget> stsThread =
    do_GetService(NS_SOCKETTRANSPORTSERVICE_CONTRACTID, &rv);

  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return;
  }

  if (!stsThread) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return;
  }

  nsAutoPtr<RTCStatsQueries> queries(new RTCStatsQueries);

  // If there is no PeerConnectionCtx, go through the same motions, since
  // the API consumer doesn't care why there are no PeerConnectionImpl.
  PeerConnectionCtx *ctx = GetPeerConnectionCtx();
  if (ctx) {
    for (auto p = ctx->mPeerConnections.begin();
         p != ctx->mPeerConnections.end();
         ++p) {
      MOZ_ASSERT(p->second);

      if (!pcIdFilter.WasPassed() ||
          pcIdFilter.Value().EqualsASCII(p->second->GetIdAsAscii().c_str())) {
        if (p->second->HasMedia()) {
          queries->append(nsAutoPtr<RTCStatsQuery>(new RTCStatsQuery(true)));
          p->second->BuildStatsQuery_m(nullptr, // all tracks
                                       queries->back());
        }
      }
    }
  }

  // CallbackObject does not support threadsafe refcounting, and must be
  // destroyed on main.
  nsMainThreadPtrHandle<WebrtcGlobalStatisticsCallback> callbackHandle(
    new nsMainThreadPtrHolder<WebrtcGlobalStatisticsCallback>(&aStatsCallback));

  rv = RUN_ON_THREAD(stsThread,
                     WrapRunnableNM(&GetAllStats_s, callbackHandle, queries),
                     NS_DISPATCH_NORMAL);

  aRv = rv;
}

void
WebrtcGlobalInformation::GetLogging(
  const GlobalObject& aGlobal,
  const nsAString& aPattern,
  WebrtcGlobalLoggingCallback& aLoggingCallback,
  ErrorResult& aRv)
{
  if (!NS_IsMainThread()) {
    aRv.Throw(NS_ERROR_NOT_SAME_THREAD);
    return;
  }

  nsresult rv;
  nsCOMPtr<nsIEventTarget> stsThread =
    do_GetService(NS_SOCKETTRANSPORTSERVICE_CONTRACTID, &rv);

  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return;
  }

  if (!stsThread) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return;
  }

  std::string pattern(NS_ConvertUTF16toUTF8(aPattern).get());

  // CallbackObject does not support threadsafe refcounting, and must be
  // destroyed on main.
  nsMainThreadPtrHandle<WebrtcGlobalLoggingCallback> callbackHandle(
    new nsMainThreadPtrHolder<WebrtcGlobalLoggingCallback>(&aLoggingCallback));

  rv = RUN_ON_THREAD(stsThread,
                     WrapRunnableNM(&GetLogging_s, callbackHandle, pattern),
                     NS_DISPATCH_NORMAL);

  if (NS_FAILED(rv)) {
    aLoggingCallback.Release();
  }

  aRv = rv;
}

static int32_t sLastSetLevel = 0;
static bool sLastAECDebug = false;

void
WebrtcGlobalInformation::SetDebugLevel(const GlobalObject& aGlobal, int32_t aLevel)
{
  StartWebRtcLog(webrtc::TraceLevel(aLevel));
  sLastSetLevel = aLevel;
}

int32_t
WebrtcGlobalInformation::DebugLevel(const GlobalObject& aGlobal)
{
  return sLastSetLevel;
}

void
WebrtcGlobalInformation::SetAecDebug(const GlobalObject& aGlobal, bool aEnable)
{
  StartWebRtcLog(sLastSetLevel); // to make it read the aec path
  webrtc::Trace::set_aec_debug(aEnable);
  sLastAECDebug = aEnable;
}

bool
WebrtcGlobalInformation::AecDebug(const GlobalObject& aGlobal)
{
  return sLastAECDebug;
}


struct StreamResult {
  StreamResult() : candidateTypeBitpattern(0), streamSucceeded(false) {}
  uint8_t candidateTypeBitpattern;
  bool streamSucceeded;
};

static void StoreLongTermICEStatisticsImpl_m(
    nsresult result,
    nsAutoPtr<RTCStatsQuery> query) {

  using namespace Telemetry;

  if (NS_FAILED(result) ||
      !query->error.empty() ||
      !query->report->mIceCandidateStats.WasPassed()) {
    return;
  }

  query->report->mClosed.Construct(true);

  // First, store stuff in telemetry
  enum {
    REMOTE_GATHERED_SERVER_REFLEXIVE = 1,
    REMOTE_GATHERED_TURN = 1 << 1,
    LOCAL_GATHERED_SERVER_REFLEXIVE = 1 << 2,
    LOCAL_GATHERED_TURN_UDP = 1 << 3,
    LOCAL_GATHERED_TURN_TCP = 1 << 4,
    LOCAL_GATHERED_TURN_TLS = 1 << 5,
    LOCAL_GATHERED_TURN_HTTPS = 1 << 6,
  };

  // TODO(bcampen@mozilla.com): Do we need to watch out for cases where the
  // components within a stream didn't have the same types of relayed
  // candidates? I have a feeling that late trickle could cause this, but right
  // now we don't have enough information to detect it (we would need to know
  // the ICE component id for each candidate pair and candidate)

  std::map<std::string, StreamResult> streamResults;

  // Build list of streams, and whether or not they failed.
  for (size_t i = 0;
       i < query->report->mIceCandidatePairStats.Value().Length();
       ++i) {
    const RTCIceCandidatePairStats &pair =
      query->report->mIceCandidatePairStats.Value()[i];

    if (!pair.mState.WasPassed() || !pair.mComponentId.WasPassed()) {
      MOZ_CRASH();
      continue;
    }

    // Note: this is not a "component" in the ICE definition, this is really a
    // stream ID. This is just the way the stats API is standardized right now.
    // Very confusing.
    std::string streamId(
      NS_ConvertUTF16toUTF8(pair.mComponentId.Value()).get());

    streamResults[streamId].streamSucceeded |=
      pair.mState.Value() == RTCStatsIceCandidatePairState::Succeeded;
  }

  for (size_t i = 0;
       i < query->report->mIceCandidateStats.Value().Length();
       ++i) {
    const RTCIceCandidateStats &cand =
      query->report->mIceCandidateStats.Value()[i];

    if (!cand.mType.WasPassed() ||
        !cand.mCandidateType.WasPassed() ||
        !cand.mComponentId.WasPassed()) {
      // Crash on debug, ignore this candidate otherwise.
      MOZ_CRASH();
      continue;
    }

    // Note: this is not a "component" in the ICE definition, this is really a
    // stream ID. This is just the way the stats API is standardized right now
    // Very confusing.
    std::string streamId(
      NS_ConvertUTF16toUTF8(cand.mComponentId.Value()).get());

    if (cand.mCandidateType.Value() == RTCStatsIceCandidateType::Relayed) {
      if (cand.mType.Value() == RTCStatsType::Localcandidate) {
        NS_ConvertUTF16toUTF8 transport(cand.mMozLocalTransport.Value());
        if (transport == kNrIceTransportUdp) {
          streamResults[streamId].candidateTypeBitpattern |=
            LOCAL_GATHERED_TURN_UDP;
        } else if (transport == kNrIceTransportTcp) {
          streamResults[streamId].candidateTypeBitpattern |=
            LOCAL_GATHERED_TURN_TCP;
        }
      } else {
        streamResults[streamId].candidateTypeBitpattern |= REMOTE_GATHERED_TURN;
      }
    } else if (cand.mCandidateType.Value() ==
               RTCStatsIceCandidateType::Serverreflexive) {
      if (cand.mType.Value() == RTCStatsType::Localcandidate) {
        streamResults[streamId].candidateTypeBitpattern |=
          LOCAL_GATHERED_SERVER_REFLEXIVE;
      } else {
        streamResults[streamId].candidateTypeBitpattern |=
          REMOTE_GATHERED_SERVER_REFLEXIVE;
      }
    }
  }

  for (auto i = streamResults.begin(); i != streamResults.end(); ++i) {
    if (i->second.streamSucceeded) {
      Telemetry::Accumulate(Telemetry::WEBRTC_CANDIDATE_TYPES_GIVEN_SUCCESS,
                            i->second.candidateTypeBitpattern);
    } else {
      Telemetry::Accumulate(Telemetry::WEBRTC_CANDIDATE_TYPES_GIVEN_FAILURE,
                            i->second.candidateTypeBitpattern);
    }
  }

  // Beyond ICE, accumulate telemetry for various PER_CALL settings here.

  if (query->report->mOutboundRTPStreamStats.WasPassed()) {
    auto& array = query->report->mOutboundRTPStreamStats.Value();
    for (decltype(array.Length()) i = 0; i < array.Length(); i++) {
      auto& s = array[i];
      bool isVideo = (s.mId.Value().Find("video") != -1);
      if (!isVideo || s.mIsRemote) {
        continue;
      }
      if (s.mBitrateMean.WasPassed()) {
        Accumulate(WEBRTC_VIDEO_ENCODER_BITRATE_AVG_PER_CALL_KBPS,
                   uint32_t(s.mBitrateMean.Value() / 1000));
      }
      if (s.mBitrateStdDev.WasPassed()) {
        Accumulate(WEBRTC_VIDEO_ENCODER_BITRATE_STD_DEV_PER_CALL_KBPS,
                   uint32_t(s.mBitrateStdDev.Value() / 1000));
      }
      if (s.mFramerateMean.WasPassed()) {
        Accumulate(WEBRTC_VIDEO_ENCODER_FRAMERATE_AVG_PER_CALL,
                   uint32_t(s.mFramerateMean.Value()));
      }
      if (s.mFramerateStdDev.WasPassed()) {
        Accumulate(WEBRTC_VIDEO_ENCODER_FRAMERATE_10X_STD_DEV_PER_CALL,
                   uint32_t(s.mFramerateStdDev.Value() * 10));
      }
      if (s.mDroppedFrames.WasPassed()) {
        double mins = (TimeStamp::Now() - query->iceStartTime).ToSeconds() / 60;
        if (mins > 0) {
          Accumulate(WEBRTC_VIDEO_ENCODER_DROPPED_FRAMES_PER_CALL_FPM,
                     uint32_t(double(s.mDroppedFrames.Value()) / mins));
        }
      }
    }
  }

  if (query->report->mInboundRTPStreamStats.WasPassed()) {
    auto& array = query->report->mInboundRTPStreamStats.Value();
    for (decltype(array.Length()) i = 0; i < array.Length(); i++) {
      auto& s = array[i];
      bool isVideo = (s.mId.Value().Find("video") != -1);
      if (!isVideo || s.mIsRemote) {
        continue;
      }
      if (s.mBitrateMean.WasPassed()) {
        Accumulate(WEBRTC_VIDEO_DECODER_BITRATE_AVG_PER_CALL_KBPS,
                   uint32_t(s.mBitrateMean.Value() / 1000));
      }
      if (s.mBitrateStdDev.WasPassed()) {
        Accumulate(WEBRTC_VIDEO_DECODER_BITRATE_STD_DEV_PER_CALL_KBPS,
                   uint32_t(s.mBitrateStdDev.Value() / 1000));
      }
      if (s.mFramerateMean.WasPassed()) {
        Accumulate(WEBRTC_VIDEO_DECODER_FRAMERATE_AVG_PER_CALL,
                   uint32_t(s.mFramerateMean.Value()));
      }
      if (s.mFramerateStdDev.WasPassed()) {
        Accumulate(WEBRTC_VIDEO_DECODER_FRAMERATE_10X_STD_DEV_PER_CALL,
                   uint32_t(s.mFramerateStdDev.Value() * 10));
      }
      if (s.mDiscardedPackets.WasPassed()) {
        double mins = (TimeStamp::Now() - query->iceStartTime).ToSeconds() / 60;
        if (mins > 0) {
          Accumulate(WEBRTC_VIDEO_DECODER_DISCARDED_PACKETS_PER_CALL_PPM,
                     uint32_t(double(s.mDiscardedPackets.Value()) / mins));
        }
      }
    }
  }

  // Finally, store the stats

  PeerConnectionCtx *ctx = GetPeerConnectionCtx();
  if (ctx) {
    ctx->mStatsForClosedPeerConnections.AppendElement(*query->report);
  }
}

static void GetStatsForLongTermStorage_s(
    nsAutoPtr<RTCStatsQuery> query) {

  MOZ_ASSERT(query);

  nsresult rv = PeerConnectionImpl::ExecuteStatsQuery_s(query.get());

  // Check whether packets were dropped due to rate limiting during
  // this call. (These calls must be made on STS)
  unsigned char rate_limit_bit_pattern = 0;
  if (!mozilla::nr_socket_short_term_violation_time().IsNull() &&
      mozilla::nr_socket_short_term_violation_time() >= query->iceStartTime) {
    rate_limit_bit_pattern |= 1;
  }
  if (!mozilla::nr_socket_long_term_violation_time().IsNull() &&
      mozilla::nr_socket_long_term_violation_time() >= query->iceStartTime) {
    rate_limit_bit_pattern |= 2;
  }

  if (query->failed) {
    Telemetry::Accumulate(
        Telemetry::WEBRTC_STUN_RATE_LIMIT_EXCEEDED_BY_TYPE_GIVEN_FAILURE,
        rate_limit_bit_pattern);
  } else {
    Telemetry::Accumulate(
        Telemetry::WEBRTC_STUN_RATE_LIMIT_EXCEEDED_BY_TYPE_GIVEN_SUCCESS,
        rate_limit_bit_pattern);
  }

  // Even if Telemetry::Accumulate is threadsafe, we still need to send the
  // query back to main, since that is where it must be destroyed.
  NS_DispatchToMainThread(
      WrapRunnableNM(
          &StoreLongTermICEStatisticsImpl_m,
          rv,
          query),
      NS_DISPATCH_NORMAL);
}

void WebrtcGlobalInformation::StoreLongTermICEStatistics(
    sipcc::PeerConnectionImpl& aPc) {
  Telemetry::Accumulate(Telemetry::WEBRTC_ICE_FINAL_CONNECTION_STATE,
                        static_cast<uint32_t>(aPc.IceConnectionState()));

  if (aPc.IceConnectionState() == PCImplIceConnectionState::New) {
    // ICE has not started; we won't have any remote candidates, so recording
    // statistics on gathered candidates is pointless.
    return;
  }

  nsAutoPtr<RTCStatsQuery> query(new RTCStatsQuery(true));

  nsresult rv = aPc.BuildStatsQuery_m(nullptr, query.get());

  NS_ENSURE_SUCCESS_VOID(rv);

  RUN_ON_THREAD(aPc.GetSTSThread(),
                WrapRunnableNM(&GetStatsForLongTermStorage_s,
                               query),
                NS_DISPATCH_NORMAL);
}


} // namespace dom
} // namespace mozilla

