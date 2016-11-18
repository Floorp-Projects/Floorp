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

#if !defined(MOZILLA_EXTERNAL_LINKAGE)
#include "mozilla/dom/RTCPeerConnectionBinding.h"
#include "mozilla/Preferences.h"
#include <mozilla/Types.h>
#endif

#include "nsNetCID.h" // NS_SOCKETTRANSPORTSERVICE_CONTRACTID
#include "nsServiceManagerUtils.h" // do_GetService
#include "nsIObserverService.h"
#include "nsIObserver.h"
#include "nsIIOService.h" // NS_IOSERVICE_*
#include "mozilla/Services.h"
#include "mozilla/StaticPtr.h"

#include "nsCRTGlue.h"

#include "gmp-video-decode.h" // GMP_API_VIDEO_DECODER
#include "gmp-video-encode.h" // GMP_API_VIDEO_ENCODER

static const char* logTag = "PeerConnectionCtx";

namespace mozilla {

using namespace dom;

class PeerConnectionCtxObserver : public nsIObserver
{
public:
  NS_DECL_ISUPPORTS

  PeerConnectionCtxObserver() {}

  void Init()
    {
      nsCOMPtr<nsIObserverService> observerService =
        services::GetObserverService();
      if (!observerService)
        return;

      nsresult rv = NS_OK;

#ifdef MOZILLA_INTERNAL_API
      rv = observerService->AddObserver(this,
                                        NS_XPCOM_SHUTDOWN_OBSERVER_ID,
                                        false);
      MOZ_ALWAYS_SUCCEEDS(rv);
      rv = observerService->AddObserver(this,
                                        NS_IOSERVICE_OFFLINE_STATUS_TOPIC,
                                        false);
      MOZ_ALWAYS_SUCCEEDS(rv);
#endif
      (void) rv;
    }

  NS_IMETHOD Observe(nsISupports* aSubject, const char* aTopic,
                     const char16_t* aData) override {
    if (strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID) == 0) {
      CSFLogDebug(logTag, "Shutting down PeerConnectionCtx");
      PeerConnectionCtx::Destroy();

      nsCOMPtr<nsIObserverService> observerService =
        services::GetObserverService();
      if (!observerService)
        return NS_ERROR_FAILURE;

      nsresult rv = observerService->RemoveObserver(this,
                                           NS_IOSERVICE_OFFLINE_STATUS_TOPIC);
      MOZ_ALWAYS_SUCCEEDS(rv);
      rv = observerService->RemoveObserver(this,
                                           NS_XPCOM_SHUTDOWN_OBSERVER_ID);
      MOZ_ALWAYS_SUCCEEDS(rv);

      // Make sure we're not deleted while still inside ::Observe()
      RefPtr<PeerConnectionCtxObserver> kungFuDeathGrip(this);
      PeerConnectionCtx::gPeerConnectionCtxObserver = nullptr;
    }
    if (strcmp(aTopic, NS_IOSERVICE_OFFLINE_STATUS_TOPIC) == 0) {
      const nsLiteralString onlineString(u"" NS_IOSERVICE_ONLINE);
      const nsLiteralString offlineString(u"" NS_IOSERVICE_OFFLINE);
      if (NS_strcmp(aData, offlineString.get()) == 0) {
        CSFLogDebug(logTag, "Updating network state to offline");
        PeerConnectionCtx::UpdateNetworkState(false);
      } else if(NS_strcmp(aData, onlineString.get()) == 0) {
        CSFLogDebug(logTag, "Updating network state to online");
        PeerConnectionCtx::UpdateNetworkState(true);
      } else {
        CSFLogDebug(logTag, "Received unsupported network state event");
        MOZ_CRASH();
      }
    }
    return NS_OK;
  }

private:
  virtual ~PeerConnectionCtxObserver()
    {
      nsCOMPtr<nsIObserverService> observerService =
        services::GetObserverService();
      if (observerService)
        observerService->RemoveObserver(this, NS_IOSERVICE_OFFLINE_STATUS_TOPIC);
        observerService->RemoveObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID);
    }
};

NS_IMPL_ISUPPORTS(PeerConnectionCtxObserver, nsIObserver);
}

namespace mozilla {

PeerConnectionCtx* PeerConnectionCtx::gInstance;
nsIThread* PeerConnectionCtx::gMainThread;
StaticRefPtr<PeerConnectionCtxObserver> PeerConnectionCtx::gPeerConnectionCtxObserver;

const std::map<const std::string, PeerConnectionImpl *>&
PeerConnectionCtx::mGetPeerConnections()
{
  return mPeerConnections;
}

nsresult PeerConnectionCtx::InitializeGlobal(nsIThread *mainThread,
  nsIEventTarget* stsThread) {
  if (!gMainThread) {
    gMainThread = mainThread;
  } else {
    MOZ_ASSERT(gMainThread == mainThread);
  }

  nsresult res;

  MOZ_ASSERT(NS_IsMainThread());

  if (!gInstance) {
    CSFLogDebug(logTag, "Creating PeerConnectionCtx");
    PeerConnectionCtx *ctx = new PeerConnectionCtx();

    res = ctx->Initialize();
    PR_ASSERT(NS_SUCCEEDED(res));
    if (!NS_SUCCEEDED(res))
      return res;

    gInstance = ctx;

    if (!PeerConnectionCtx::gPeerConnectionCtxObserver) {
      PeerConnectionCtx::gPeerConnectionCtxObserver = new PeerConnectionCtxObserver();
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

bool PeerConnectionCtx::isActive() {
  return gInstance;
}

void PeerConnectionCtx::Destroy() {
  CSFLogDebug(logTag, "%s", __FUNCTION__);

  if (gInstance) {
    gInstance->Cleanup();
    delete gInstance;
    gInstance = nullptr;
  }

  StopWebRtcLog();
}

#if !defined(MOZILLA_EXTERNAL_LINKAGE)
typedef Vector<nsAutoPtr<RTCStatsQuery>> RTCStatsQueries;

// Telemetry reporting every second after start of first call.
// The threading model around the media pipelines is weird:
// - The pipelines are containers,
// - containers that are only safe on main thread, with members only safe on STS,
// - hence the there and back again approach.

static auto
FindId(const Sequence<RTCInboundRTPStreamStats>& aArray,
       const nsString &aId) -> decltype(aArray.Length()) {
  for (decltype(aArray.Length()) i = 0; i < aArray.Length(); i++) {
    if (aArray[i].mId.Value() == aId) {
      return i;
    }
  }
  return aArray.NoIndex;
}

static auto
FindId(const nsTArray<nsAutoPtr<RTCStatsReportInternal>>& aArray,
       const nsString &aId) -> decltype(aArray.Length()) {
  for (decltype(aArray.Length()) i = 0; i < aArray.Length(); i++) {
    if (aArray[i]->mPcid == aId) {
      return i;
    }
  }
  return aArray.NoIndex;
}

static void
FreeOnMain_m(nsAutoPtr<RTCStatsQueries> aQueryList) {
  MOZ_ASSERT(NS_IsMainThread());
}

static void
EverySecondTelemetryCallback_s(nsAutoPtr<RTCStatsQueries> aQueryList) {
  using namespace Telemetry;

  if(!PeerConnectionCtx::isActive()) {
    return;
  }
  PeerConnectionCtx *ctx = PeerConnectionCtx::GetInstance();

  for (auto q = aQueryList->begin(); q != aQueryList->end(); ++q) {
    PeerConnectionImpl::ExecuteStatsQuery_s(*q);
    auto& r = *(*q)->report;
    if (r.mInboundRTPStreamStats.WasPassed()) {
      // First, get reports from a second ago, if any, for calculations below
      const Sequence<RTCInboundRTPStreamStats> *lastInboundStats = nullptr;
      {
        auto i = FindId(ctx->mLastReports, r.mPcid);
        if (i != ctx->mLastReports.NoIndex) {
          lastInboundStats = &ctx->mLastReports[i]->mInboundRTPStreamStats.Value();
        }
      }
      // Then, look for the things we want telemetry on
      auto& array = r.mInboundRTPStreamStats.Value();
      for (decltype(array.Length()) i = 0; i < array.Length(); i++) {
        auto& s = array[i];
        bool isAudio = (s.mId.Value().Find("audio") != -1);
        if (s.mPacketsLost.WasPassed() && s.mPacketsReceived.WasPassed() &&
            (s.mPacketsLost.Value() + s.mPacketsReceived.Value()) != 0) {
          ID id;
          if (s.mIsRemote) {
            id = isAudio ? WEBRTC_AUDIO_QUALITY_OUTBOUND_PACKETLOSS_RATE :
                           WEBRTC_VIDEO_QUALITY_OUTBOUND_PACKETLOSS_RATE;
          } else {
            id = isAudio ? WEBRTC_AUDIO_QUALITY_INBOUND_PACKETLOSS_RATE :
                           WEBRTC_VIDEO_QUALITY_INBOUND_PACKETLOSS_RATE;
          }
          // *1000 so we can read in 10's of a percent (permille)
          Accumulate(id,
                     (s.mPacketsLost.Value() * 1000) /
                     (s.mPacketsLost.Value() + s.mPacketsReceived.Value()));
        }
        if (s.mJitter.WasPassed()) {
          ID id;
          if (s.mIsRemote) {
            id = isAudio ? WEBRTC_AUDIO_QUALITY_OUTBOUND_JITTER :
                           WEBRTC_VIDEO_QUALITY_OUTBOUND_JITTER;
          } else {
            id = isAudio ? WEBRTC_AUDIO_QUALITY_INBOUND_JITTER :
                           WEBRTC_VIDEO_QUALITY_INBOUND_JITTER;
          }
          Accumulate(id, s.mJitter.Value());
        }
        if (s.mMozRtt.WasPassed()) {
          MOZ_ASSERT(s.mIsRemote);
          ID id = isAudio ? WEBRTC_AUDIO_QUALITY_OUTBOUND_RTT :
                            WEBRTC_VIDEO_QUALITY_OUTBOUND_RTT;
          Accumulate(id, s.mMozRtt.Value());
        }
        if (lastInboundStats && s.mBytesReceived.WasPassed()) {
          auto& laststats = *lastInboundStats;
          auto i = FindId(laststats, s.mId.Value());
          if (i != laststats.NoIndex) {
            auto& lasts = laststats[i];
            if (lasts.mBytesReceived.WasPassed()) {
              auto delta_ms = int32_t(s.mTimestamp.Value() -
                                      lasts.mTimestamp.Value());
              // In theory we're called every second, so delta *should* be in that range.
              // Small deltas could cause errors due to division
              if (delta_ms > 500 && delta_ms < 60000) {
                ID id;
                if (s.mIsRemote) {
                  id = isAudio ? WEBRTC_AUDIO_QUALITY_OUTBOUND_BANDWIDTH_KBITS :
                                 WEBRTC_VIDEO_QUALITY_OUTBOUND_BANDWIDTH_KBITS;
                } else {
                  id = isAudio ? WEBRTC_AUDIO_QUALITY_INBOUND_BANDWIDTH_KBITS :
                                 WEBRTC_VIDEO_QUALITY_INBOUND_BANDWIDTH_KBITS;
                }
                Accumulate(id, ((s.mBytesReceived.Value() -
                                 lasts.mBytesReceived.Value()) * 8) / delta_ms);
              }
              // We could accumulate values until enough time has passed
              // and then Accumulate() but this isn't that important.
            }
          }
        }
      }
    }
  }
  // Steal and hang on to reports for the next second
  ctx->mLastReports.Clear();
  for (auto q = aQueryList->begin(); q != aQueryList->end(); ++q) {
    ctx->mLastReports.AppendElement((*q)->report.forget()); // steal avoids copy
  }
  // Container must be freed back on main thread
  NS_DispatchToMainThread(WrapRunnableNM(&FreeOnMain_m, aQueryList),
                          NS_DISPATCH_NORMAL);
}

void
PeerConnectionCtx::EverySecondTelemetryCallback_m(nsITimer* timer, void *closure) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(PeerConnectionCtx::isActive());
  auto ctx = static_cast<PeerConnectionCtx*>(closure);
  if (ctx->mPeerConnections.empty()) {
    return;
  }
  nsresult rv;
  nsCOMPtr<nsIEventTarget> stsThread =
      do_GetService(NS_SOCKETTRANSPORTSERVICE_CONTRACTID, &rv);
  if (NS_FAILED(rv)) {
    return;
  }
  MOZ_ASSERT(stsThread);

  nsAutoPtr<RTCStatsQueries> queries(new RTCStatsQueries);
  for (auto p = ctx->mPeerConnections.begin();
        p != ctx->mPeerConnections.end(); ++p) {
    if (p->second->HasMedia()) {
      if (!queries->append(nsAutoPtr<RTCStatsQuery>(new RTCStatsQuery(true)))) {
	return;
      }
      if (NS_WARN_IF(NS_FAILED(p->second->BuildStatsQuery_m(nullptr, // all tracks
                                                            queries->back())))) {
        queries->popBack();
      } else {
        MOZ_ASSERT(queries->back()->report);
      }
    }
  }
  if (!queries->empty()) {
    rv = RUN_ON_THREAD(stsThread,
                       WrapRunnableNM(&EverySecondTelemetryCallback_s, queries),
                       NS_DISPATCH_NORMAL);
    NS_ENSURE_SUCCESS_VOID(rv);
  }
}
#endif

void
PeerConnectionCtx::UpdateNetworkState(bool online) {
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

#if !defined(MOZILLA_EXTERNAL_LINKAGE)
  mTelemetryTimer = do_CreateInstance(NS_TIMER_CONTRACTID);
  MOZ_ASSERT(mTelemetryTimer);
  nsresult rv = mTelemetryTimer->SetTarget(gMainThread);
  NS_ENSURE_SUCCESS(rv, rv);
  mTelemetryTimer->InitWithFuncCallback(EverySecondTelemetryCallback_m, this, 1000,
                                        nsITimer::TYPE_REPEATING_PRECISE_CAN_SKIP);

  if (XRE_IsContentProcess()) {
    WebrtcGlobalChild::Create();
  }
#endif // MOZILLA_INTERNAL_API

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

void PeerConnectionCtx::initGMP()
{
  mGMPService = do_GetService("@mozilla.org/gecko-media-plugin-service;1");

  if (!mGMPService) {
    CSFLogError(logTag, "%s failed to get the gecko-media-plugin-service",
                __FUNCTION__);
    return;
  }

  nsCOMPtr<nsIThread> thread;
  nsresult rv = mGMPService->GetThread(getter_AddRefs(thread));

  if (NS_FAILED(rv)) {
    mGMPService = nullptr;
    CSFLogError(logTag,
                "%s failed to get the gecko-media-plugin thread, err=%u",
                __FUNCTION__,
                static_cast<unsigned>(rv));
    return;
  }

  // presumes that all GMP dir scans have been queued for the GMPThread
  thread->Dispatch(WrapRunnableNM(&GMPReady), NS_DISPATCH_NORMAL);
}

nsresult PeerConnectionCtx::Cleanup() {
  CSFLogDebug(logTag, "%s", __FUNCTION__);

  mQueuedJSEPOperations.Clear();
  mGMPService = nullptr;
  return NS_OK;
}

PeerConnectionCtx::~PeerConnectionCtx() {
    // ensure mTelemetryTimer ends on main thread
  MOZ_ASSERT(NS_IsMainThread());
#if !defined(MOZILLA_EXTERNAL_LINKAGE)
  if (mTelemetryTimer) {
    mTelemetryTimer->Cancel();
  }
#endif
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
                                    &tags,
                                    &has_gmp);
  if (NS_FAILED(rv) || !has_gmp) {
    return false;
  }

  rv = mGMPService->HasPluginForAPI(NS_LITERAL_CSTRING(GMP_API_VIDEO_DECODER),
                                    &tags,
                                    &has_gmp);
  if (NS_FAILED(rv) || !has_gmp) {
    return false;
  }

  return true;
}

} // namespace mozilla
