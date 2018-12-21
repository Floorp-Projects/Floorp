/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CSFLog.h"

#include "PeerConnectionImpl.h"
#include "PeerConnectionCtx.h"
#include "runnable_utils.h"
#include "prcvar.h"

#include "mozilla/Telemetry.h"
#include "browser_logging/WebRtcLog.h"

#include "mozilla/dom/RTCPeerConnectionBinding.h"
#include "mozilla/Preferences.h"
#include <mozilla/Types.h>

#include "nsNetCID.h"               // NS_SOCKETTRANSPORTSERVICE_CONTRACTID
#include "nsServiceManagerUtils.h"  // do_GetService
#include "nsIObserverService.h"
#include "nsIObserver.h"
#include "nsIIOService.h"  // NS_IOSERVICE_*
#include "mozilla/Services.h"
#include "mozilla/StaticPtr.h"

#include "nsCRTGlue.h"

#include "gmp-video-decode.h"  // GMP_API_VIDEO_DECODER
#include "gmp-video-encode.h"  // GMP_API_VIDEO_ENCODER

static const char* pccLogTag = "PeerConnectionCtx";
#ifdef LOGTAG
#undef LOGTAG
#endif
#define LOGTAG pccLogTag

namespace mozilla {

using namespace dom;

class PeerConnectionCtxObserver : public nsIObserver {
 public:
  NS_DECL_ISUPPORTS

  PeerConnectionCtxObserver() {}

  void Init() {
    nsCOMPtr<nsIObserverService> observerService =
        services::GetObserverService();
    if (!observerService) return;

    nsresult rv = NS_OK;

    rv = observerService->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID,
                                      false);
    MOZ_ALWAYS_SUCCEEDS(rv);
    rv = observerService->AddObserver(this, NS_IOSERVICE_OFFLINE_STATUS_TOPIC,
                                      false);
    MOZ_ALWAYS_SUCCEEDS(rv);
    (void)rv;
  }

  NS_IMETHOD Observe(nsISupports* aSubject, const char* aTopic,
                     const char16_t* aData) override {
    if (strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID) == 0) {
      CSFLogDebug(LOGTAG, "Shutting down PeerConnectionCtx");
      PeerConnectionCtx::Destroy();

      nsCOMPtr<nsIObserverService> observerService =
          services::GetObserverService();
      if (!observerService) return NS_ERROR_FAILURE;

      nsresult rv = observerService->RemoveObserver(
          this, NS_IOSERVICE_OFFLINE_STATUS_TOPIC);
      MOZ_ALWAYS_SUCCEEDS(rv);
      rv = observerService->RemoveObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID);
      MOZ_ALWAYS_SUCCEEDS(rv);

      // Make sure we're not deleted while still inside ::Observe()
      RefPtr<PeerConnectionCtxObserver> kungFuDeathGrip(this);
      PeerConnectionCtx::gPeerConnectionCtxObserver = nullptr;
    }
    if (strcmp(aTopic, NS_IOSERVICE_OFFLINE_STATUS_TOPIC) == 0) {
      if (NS_strcmp(aData, u"" NS_IOSERVICE_OFFLINE) == 0) {
        CSFLogDebug(LOGTAG, "Updating network state to offline");
        PeerConnectionCtx::UpdateNetworkState(false);
      } else if (NS_strcmp(aData, u"" NS_IOSERVICE_ONLINE) == 0) {
        CSFLogDebug(LOGTAG, "Updating network state to online");
        PeerConnectionCtx::UpdateNetworkState(true);
      } else {
        CSFLogDebug(LOGTAG, "Received unsupported network state event");
        MOZ_CRASH();
      }
    }
    return NS_OK;
  }

 private:
  virtual ~PeerConnectionCtxObserver() {
    nsCOMPtr<nsIObserverService> observerService =
        services::GetObserverService();
    if (observerService) {
      observerService->RemoveObserver(this, NS_IOSERVICE_OFFLINE_STATUS_TOPIC);
      observerService->RemoveObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID);
    }
  }
};

NS_IMPL_ISUPPORTS(PeerConnectionCtxObserver, nsIObserver);
}  // namespace mozilla

namespace mozilla {

PeerConnectionCtx* PeerConnectionCtx::gInstance;
nsIThread* PeerConnectionCtx::gMainThread;
StaticRefPtr<PeerConnectionCtxObserver>
    PeerConnectionCtx::gPeerConnectionCtxObserver;

const std::map<const std::string, PeerConnectionImpl*>&
PeerConnectionCtx::mGetPeerConnections() {
  return mPeerConnections;
}

nsresult PeerConnectionCtx::InitializeGlobal(nsIThread* mainThread,
                                             nsIEventTarget* stsThread) {
  if (!gMainThread) {
    gMainThread = mainThread;
  } else {
    MOZ_ASSERT(gMainThread == mainThread);
  }

  nsresult res;

  MOZ_ASSERT(NS_IsMainThread());

  if (!gInstance) {
    CSFLogDebug(LOGTAG, "Creating PeerConnectionCtx");
    PeerConnectionCtx* ctx = new PeerConnectionCtx();

    res = ctx->Initialize();
    PR_ASSERT(NS_SUCCEEDED(res));
    if (!NS_SUCCEEDED(res)) return res;

    gInstance = ctx;

    if (!PeerConnectionCtx::gPeerConnectionCtxObserver) {
      PeerConnectionCtx::gPeerConnectionCtxObserver =
          new PeerConnectionCtxObserver();
      PeerConnectionCtx::gPeerConnectionCtxObserver->Init();
    }
  }

  EnableWebRtcLog();
  return NS_OK;
}

PeerConnectionCtx* PeerConnectionCtx::GetInstance() {
  MOZ_ASSERT(gInstance);
  return gInstance;
}

bool PeerConnectionCtx::isActive() { return gInstance; }

void PeerConnectionCtx::Destroy() {
  CSFLogDebug(LOGTAG, "%s", __FUNCTION__);

  if (gInstance) {
    gInstance->Cleanup();
    delete gInstance;
    gInstance = nullptr;
  }

  StopWebRtcLog();
}

// Telemetry reporting every second after start of first call.
// The threading model around the media pipelines is weird:
// - The pipelines are containers,
// - containers that are only safe on main thread, with members only safe on
//   STS,
// - hence the there and back again approach.

static auto FindId(const Sequence<RTCInboundRTPStreamStats>& aArray,
                   const nsString& aId) -> decltype(aArray.Length()) {
  for (decltype(aArray.Length()) i = 0; i < aArray.Length(); i++) {
    if (aArray[i].mId.Value() == aId) {
      return i;
    }
  }
  return aArray.NoIndex;
}

void PeerConnectionCtx::DeliverStats(RTCStatsQuery& aQuery) {
  using namespace Telemetry;

  std::unique_ptr<dom::RTCStatsReportInternal> report(aQuery.report.forget());
  // First, get reports from a second ago, if any, for calculations below
  std::unique_ptr<dom::RTCStatsReportInternal> lastReport;
  {
    auto i = mLastReports.find(report->mPcid);
    if (i != mLastReports.end()) {
      lastReport = std::move(i->second);
    }
  }

  if (report->mInboundRTPStreamStats.WasPassed()) {
    // Then, look for the things we want telemetry on
    for (auto& s : report->mInboundRTPStreamStats.Value()) {
      bool isRemote = s.mType.Value() == dom::RTCStatsType::Remote_inbound_rtp;
      bool isAudio = (s.mId.Value().Find("audio") != -1);
      if (s.mPacketsLost.WasPassed() && s.mPacketsReceived.WasPassed() &&
          (s.mPacketsLost.Value() + s.mPacketsReceived.Value()) != 0) {
        HistogramID id;
        if (isRemote) {
          id = isAudio ? WEBRTC_AUDIO_QUALITY_OUTBOUND_PACKETLOSS_RATE
                       : WEBRTC_VIDEO_QUALITY_OUTBOUND_PACKETLOSS_RATE;
        } else {
          id = isAudio ? WEBRTC_AUDIO_QUALITY_INBOUND_PACKETLOSS_RATE
                       : WEBRTC_VIDEO_QUALITY_INBOUND_PACKETLOSS_RATE;
        }
        // *1000 so we can read in 10's of a percent (permille)
        Accumulate(id,
                   (s.mPacketsLost.Value() * 1000) /
                       (s.mPacketsLost.Value() + s.mPacketsReceived.Value()));
      }
      if (s.mJitter.WasPassed()) {
        HistogramID id;
        if (isRemote) {
          id = isAudio ? WEBRTC_AUDIO_QUALITY_OUTBOUND_JITTER
                       : WEBRTC_VIDEO_QUALITY_OUTBOUND_JITTER;
        } else {
          id = isAudio ? WEBRTC_AUDIO_QUALITY_INBOUND_JITTER
                       : WEBRTC_VIDEO_QUALITY_INBOUND_JITTER;
        }
        Accumulate(id, s.mJitter.Value());
      }
      if (s.mRoundTripTime.WasPassed()) {
        MOZ_ASSERT(isRemote);
        HistogramID id = isAudio ? WEBRTC_AUDIO_QUALITY_OUTBOUND_RTT
                                 : WEBRTC_VIDEO_QUALITY_OUTBOUND_RTT;
        Accumulate(id, s.mRoundTripTime.Value());
      }
      if (lastReport && lastReport->mInboundRTPStreamStats.WasPassed() &&
          s.mBytesReceived.WasPassed()) {
        auto& laststats = lastReport->mInboundRTPStreamStats.Value();
        auto i = FindId(laststats, s.mId.Value());
        if (i != laststats.NoIndex) {
          auto& lasts = laststats[i];
          if (lasts.mBytesReceived.WasPassed()) {
            auto delta_ms =
                int32_t(s.mTimestamp.Value() - lasts.mTimestamp.Value());
            // In theory we're called every second, so delta *should* be in that
            // range. Small deltas could cause errors due to division
            if (delta_ms > 500 && delta_ms < 60000) {
              HistogramID id;
              if (isRemote) {
                id = isAudio ? WEBRTC_AUDIO_QUALITY_OUTBOUND_BANDWIDTH_KBITS
                             : WEBRTC_VIDEO_QUALITY_OUTBOUND_BANDWIDTH_KBITS;
              } else {
                id = isAudio ? WEBRTC_AUDIO_QUALITY_INBOUND_BANDWIDTH_KBITS
                             : WEBRTC_VIDEO_QUALITY_INBOUND_BANDWIDTH_KBITS;
              }
              Accumulate(id, ((s.mBytesReceived.Value() -
                               lasts.mBytesReceived.Value()) *
                              8) /
                                 delta_ms);
            }
            // We could accumulate values until enough time has passed
            // and then Accumulate() but this isn't that important.
          }
        }
      }
    }
  }

  mLastReports[report->mPcid] = std::move(report);
}

void PeerConnectionCtx::EverySecondTelemetryCallback_m(nsITimer* timer,
                                                       void* closure) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(PeerConnectionCtx::isActive());

  for (auto& idAndPc : GetInstance()->mPeerConnections) {
    if (idAndPc.second->HasMedia()) {
      idAndPc.second->GetStats(nullptr, true)
          ->Then(GetMainThreadSerialEventTarget(), __func__,
                 [=](UniquePtr<RTCStatsQuery>&& aQuery) {
                   if (PeerConnectionCtx::isActive()) {
                     PeerConnectionCtx::GetInstance()->DeliverStats(*aQuery);
                   }
                 },
                 [=](nsresult aError) {});
    }
  }
}

void PeerConnectionCtx::UpdateNetworkState(bool online) {
  auto ctx = GetInstance();
  if (ctx->mPeerConnections.empty()) {
    return;
  }
  for (auto pc : ctx->mPeerConnections) {
    pc.second->UpdateNetworkState(online);
  }
}

nsresult PeerConnectionCtx::Initialize() {
  initGMP();

  nsresult rv = NS_NewTimerWithFuncCallback(
      getter_AddRefs(mTelemetryTimer), EverySecondTelemetryCallback_m, this,
      1000, nsITimer::TYPE_REPEATING_PRECISE_CAN_SKIP,
      "EverySecondTelemetryCallback_m",
      SystemGroup::EventTargetFor(TaskCategory::Other));
  NS_ENSURE_SUCCESS(rv, rv);

  if (XRE_IsContentProcess()) {
    WebrtcGlobalChild::Create();
  }

  return NS_OK;
}

static void GMPReady_m() {
  if (PeerConnectionCtx::isActive()) {
    PeerConnectionCtx::GetInstance()->onGMPReady();
  }
};

static void GMPReady() {
  PeerConnectionCtx::gMainThread->Dispatch(WrapRunnableNM(&GMPReady_m),
                                           NS_DISPATCH_NORMAL);
};

void PeerConnectionCtx::initGMP() {
  mGMPService = do_GetService("@mozilla.org/gecko-media-plugin-service;1");

  if (!mGMPService) {
    CSFLogError(LOGTAG, "%s failed to get the gecko-media-plugin-service",
                __FUNCTION__);
    return;
  }

  nsCOMPtr<nsIThread> thread;
  nsresult rv = mGMPService->GetThread(getter_AddRefs(thread));

  if (NS_FAILED(rv)) {
    mGMPService = nullptr;
    CSFLogError(LOGTAG,
                "%s failed to get the gecko-media-plugin thread, err=%u",
                __FUNCTION__, static_cast<unsigned>(rv));
    return;
  }

  // presumes that all GMP dir scans have been queued for the GMPThread
  thread->Dispatch(WrapRunnableNM(&GMPReady), NS_DISPATCH_NORMAL);
}

nsresult PeerConnectionCtx::Cleanup() {
  CSFLogDebug(LOGTAG, "%s", __FUNCTION__);

  mQueuedJSEPOperations.Clear();
  mGMPService = nullptr;
  return NS_OK;
}

PeerConnectionCtx::~PeerConnectionCtx() {
  // ensure mTelemetryTimer ends on main thread
  MOZ_ASSERT(NS_IsMainThread());
  if (mTelemetryTimer) {
    mTelemetryTimer->Cancel();
  }
};

void PeerConnectionCtx::queueJSEPOperation(nsIRunnable* aOperation) {
  mQueuedJSEPOperations.AppendElement(aOperation);
}

void PeerConnectionCtx::onGMPReady() {
  mGMPReady = true;
  for (size_t i = 0; i < mQueuedJSEPOperations.Length(); ++i) {
    mQueuedJSEPOperations[i]->Run();
  }
  mQueuedJSEPOperations.Clear();
}

bool PeerConnectionCtx::gmpHasH264() {
  if (!mGMPService) {
    return false;
  }

  // XXX I'd prefer if this was all known ahead of time...

  nsTArray<nsCString> tags;
  tags.AppendElement(NS_LITERAL_CSTRING("h264"));

  bool has_gmp;
  nsresult rv;
  rv = mGMPService->HasPluginForAPI(NS_LITERAL_CSTRING(GMP_API_VIDEO_ENCODER),
                                    &tags, &has_gmp);
  if (NS_FAILED(rv) || !has_gmp) {
    return false;
  }

  rv = mGMPService->HasPluginForAPI(NS_LITERAL_CSTRING(GMP_API_VIDEO_DECODER),
                                    &tags, &has_gmp);
  if (NS_FAILED(rv) || !has_gmp) {
    return false;
  }

  return true;
}

}  // namespace mozilla
