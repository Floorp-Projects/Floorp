/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CSFLog.h"

#include "base/histogram.h"
#include "CallControlManager.h"
#include "CC_Device.h"
#include "CC_Call.h"
#include "CC_Observer.h"
#include "ccapi_call_info.h"
#include "CC_SIPCCCallInfo.h"
#include "ccapi_device_info.h"
#include "CC_SIPCCDeviceInfo.h"
#include "vcm.h"
#include "VcmSIPCCBinding.h"
#include "PeerConnectionImpl.h"
#include "PeerConnectionCtx.h"
#include "runnable_utils.h"
#include "cpr_socket.h"
#include "debug-psipcc-types.h"
#include "prcvar.h"

#include "mozilla/Telemetry.h"

#ifdef MOZILLA_INTERNAL_API
#include "mozilla/dom/RTCPeerConnectionBinding.h"
#include "mozilla/Preferences.h"
#include <mozilla/Types.h>
#endif

#include "nsNetCID.h" // NS_SOCKETTRANSPORTSERVICE_CONTRACTID
#include "nsServiceManagerUtils.h" // do_GetService
#include "nsIObserverService.h"
#include "nsIObserver.h"
#include "mozilla/Services.h"
#include "StaticPtr.h"
extern "C" {
#include "../sipcc/core/common/thread_monitor.h"
}

static const char* logTag = "PeerConnectionCtx";

extern "C" {
extern PRCondVar *ccAppReadyToStartCond;
extern PRLock *ccAppReadyToStartLock;
extern char ccAppReadyToStart;
}

namespace mozilla {

using namespace dom;

// Convert constraints to C structures

#ifdef MOZILLA_INTERNAL_API
static void
Apply(const Optional<bool> &aSrc, cc_boolean_constraint_t *aDst,
      bool mandatory = false) {
  if (aSrc.WasPassed() && (mandatory || !aDst->was_passed)) {
    aDst->was_passed = true;
    aDst->value = aSrc.Value();
    aDst->mandatory = mandatory;
  }
}
#endif

MediaConstraintsExternal::MediaConstraintsExternal() {
  memset(&mConstraints, 0, sizeof(mConstraints));
}

MediaConstraintsExternal::MediaConstraintsExternal(
    const MediaConstraintsInternal &aSrc) {
  cc_media_constraints_t* c = &mConstraints;
  memset(c, 0, sizeof(*c));
#ifdef MOZILLA_INTERNAL_API
  Apply(aSrc.mMandatory.mOfferToReceiveAudio, &c->offer_to_receive_audio, true);
  Apply(aSrc.mMandatory.mOfferToReceiveVideo, &c->offer_to_receive_video, true);
  if (!Preferences::GetBool("media.peerconnection.video.enabled", true)) {
    c->offer_to_receive_video.was_passed = true;
    c->offer_to_receive_video.value = false;
  }
  Apply(aSrc.mMandatory.mMozDontOfferDataChannel, &c->moz_dont_offer_datachannel,
        true);
  Apply(aSrc.mMandatory.mMozBundleOnly, &c->moz_bundle_only, true);
  if (aSrc.mOptional.WasPassed()) {
    const Sequence<MediaConstraintSet> &array = aSrc.mOptional.Value();
    for (uint32_t i = 0; i < array.Length(); i++) {
      Apply(array[i].mOfferToReceiveAudio, &c->offer_to_receive_audio);
      Apply(array[i].mOfferToReceiveVideo, &c->offer_to_receive_video);
      Apply(array[i].mMozDontOfferDataChannel, &c->moz_dont_offer_datachannel);
      Apply(array[i].mMozBundleOnly, &c->moz_bundle_only);
    }
  }
#endif
}

cc_media_constraints_t*
MediaConstraintsExternal::build() const {
  cc_media_constraints_t* cc  = (cc_media_constraints_t*)
    cpr_malloc(sizeof(cc_media_constraints_t));
  if (cc) {
    *cc = mConstraints;
  }
  return cc;
}

class PeerConnectionCtxShutdown : public nsIObserver
{
public:
  NS_DECL_ISUPPORTS

  PeerConnectionCtxShutdown() {}

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
      MOZ_ALWAYS_TRUE(NS_SUCCEEDED(rv));
#endif
      (void) rv;
    }

  virtual ~PeerConnectionCtxShutdown()
    {
      nsCOMPtr<nsIObserverService> observerService =
        services::GetObserverService();
      if (observerService)
        observerService->RemoveObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID);
    }

  NS_IMETHODIMP Observe(nsISupports* aSubject, const char* aTopic,
                        const char16_t* aData) {
    if (strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID) == 0) {
      CSFLogDebug(logTag, "Shutting down PeerConnectionCtx");
      sipcc::PeerConnectionCtx::Destroy();

      nsCOMPtr<nsIObserverService> observerService =
        services::GetObserverService();
      if (!observerService)
        return NS_ERROR_FAILURE;

      nsresult rv = observerService->RemoveObserver(this,
                                                    NS_XPCOM_SHUTDOWN_OBSERVER_ID);
      MOZ_ALWAYS_TRUE(NS_SUCCEEDED(rv));

      // Make sure we're not deleted while still inside ::Observe()
      nsRefPtr<PeerConnectionCtxShutdown> kungFuDeathGrip(this);
      sipcc::PeerConnectionCtx::gPeerConnectionCtxShutdown = nullptr;
    }
    return NS_OK;
  }
};

NS_IMPL_ISUPPORTS(PeerConnectionCtxShutdown, nsIObserver);
}

using namespace mozilla;
namespace sipcc {

PeerConnectionCtx* PeerConnectionCtx::gInstance;
nsIThread* PeerConnectionCtx::gMainThread;
StaticRefPtr<PeerConnectionCtxShutdown> PeerConnectionCtx::gPeerConnectionCtxShutdown;

// Since we have a pointer to main-thread, help make it safe for lower-level
// SIPCC threads to use SyncRunnable without deadlocking, by exposing main's
// dispatcher and waiter functions. See sipcc/core/common/thread_monitor.c.

static void thread_ended_dispatcher(thread_ended_funct func, thread_monitor_id_t id)
{
  nsresult rv = PeerConnectionCtx::gMainThread->Dispatch(WrapRunnableNM(func, id),
                                                         NS_DISPATCH_NORMAL);
  if (NS_FAILED(rv)) {
    CSFLogError( logTag, "%s(): Could not dispatch to main thread", __FUNCTION__);
  }
}

static void join_waiter() {
  NS_ProcessPendingEvents(PeerConnectionCtx::gMainThread);
}

nsresult PeerConnectionCtx::InitializeGlobal(nsIThread *mainThread,
  nsIEventTarget* stsThread) {
  if (!gMainThread) {
    gMainThread = mainThread;
    CSF::VcmSIPCCBinding::setMainThread(gMainThread);
    init_thread_monitor(&thread_ended_dispatcher, &join_waiter);
  } else {
    MOZ_ASSERT(gMainThread == mainThread);
  }

  CSF::VcmSIPCCBinding::setSTSThread(stsThread);

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

    if (!sipcc::PeerConnectionCtx::gPeerConnectionCtxShutdown) {
      sipcc::PeerConnectionCtx::gPeerConnectionCtxShutdown = new PeerConnectionCtxShutdown();
      sipcc::PeerConnectionCtx::gPeerConnectionCtxShutdown->Init();
    }
  }

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
}

#ifdef MOZILLA_INTERNAL_API
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
        if (s.mPacketsLost.WasPassed()) {
          Accumulate(s.mIsRemote?
                     (isAudio? WEBRTC_AUDIO_QUALITY_OUTBOUND_PACKETLOSS :
                               WEBRTC_VIDEO_QUALITY_OUTBOUND_PACKETLOSS) :
                     (isAudio? WEBRTC_AUDIO_QUALITY_INBOUND_PACKETLOSS :
                               WEBRTC_VIDEO_QUALITY_INBOUND_PACKETLOSS),
                      s.mPacketsLost.Value());
        }
        if (s.mJitter.WasPassed()) {
          Accumulate(s.mIsRemote?
                     (isAudio? WEBRTC_AUDIO_QUALITY_OUTBOUND_JITTER :
                               WEBRTC_VIDEO_QUALITY_OUTBOUND_JITTER) :
                     (isAudio? WEBRTC_AUDIO_QUALITY_INBOUND_JITTER :
                               WEBRTC_VIDEO_QUALITY_INBOUND_JITTER),
                      s.mJitter.Value());
        }
        if (s.mMozRtt.WasPassed()) {
          MOZ_ASSERT(s.mIsRemote);
          Accumulate(isAudio? WEBRTC_AUDIO_QUALITY_OUTBOUND_RTT :
                              WEBRTC_VIDEO_QUALITY_OUTBOUND_RTT,
                      s.mMozRtt.Value());
        }
        if (lastInboundStats && s.mBytesReceived.WasPassed()) {
          auto& laststats = *lastInboundStats;
          auto i = FindId(laststats, s.mId.Value());
          if (i != laststats.NoIndex) {
            auto& lasts = laststats[i];
            if (lasts.mBytesReceived.WasPassed()) {
              auto delta_ms = int32_t(s.mTimestamp.Value() -
                                      lasts.mTimestamp.Value());
              if (delta_ms > 0 && delta_ms < 60000) {
                Accumulate(s.mIsRemote?
                           (isAudio? WEBRTC_AUDIO_QUALITY_OUTBOUND_BANDWIDTH_KBITS :
                                     WEBRTC_VIDEO_QUALITY_OUTBOUND_BANDWIDTH_KBITS) :
                           (isAudio? WEBRTC_AUDIO_QUALITY_INBOUND_BANDWIDTH_KBITS :
                                     WEBRTC_VIDEO_QUALITY_INBOUND_BANDWIDTH_KBITS),
                           ((s.mBytesReceived.Value() -
                             lasts.mBytesReceived.Value()) * 8) / delta_ms);
              }
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
      queries->append(nsAutoPtr<RTCStatsQuery>(new RTCStatsQuery(true)));
      p->second->BuildStatsQuery_m(nullptr, // all tracks
                                   queries->back());
    }
  }
  rv = RUN_ON_THREAD(stsThread,
                     WrapRunnableNM(&EverySecondTelemetryCallback_s, queries),
                     NS_DISPATCH_NORMAL);
  NS_ENSURE_SUCCESS_VOID(rv);
}
#endif

nsresult PeerConnectionCtx::Initialize() {
  mCCM = CSF::CallControlManager::create();

  NS_ENSURE_TRUE(mCCM.get(), NS_ERROR_FAILURE);

  // Add the local audio codecs
  // FIX - Get this list from MediaEngine instead
  int codecMask = 0;
  codecMask |= VCM_CODEC_RESOURCE_G711;
  codecMask |= VCM_CODEC_RESOURCE_OPUS;
  //codecMask |= VCM_CODEC_RESOURCE_LINEAR;
  //codecMask |= VCM_CODEC_RESOURCE_G722;
  //codecMask |= VCM_CODEC_RESOURCE_iLBC;
  //codecMask |= VCM_CODEC_RESOURCE_iSAC;
  mCCM->setAudioCodecs(codecMask);

  //Add the local video codecs
  // FIX - Get this list from MediaEngine instead
  // Turning them all on for now
  codecMask = 0;
  // Only adding codecs supported
  //codecMask |= VCM_CODEC_RESOURCE_H263;

#ifdef MOZILLA_INTERNAL_API
  if (Preferences::GetBool("media.peerconnection.video.h264_enabled")) {
    codecMask |= VCM_CODEC_RESOURCE_H264;
  }
#else
  // Outside MOZILLA_INTERNAL_API ensures H.264 available in unit tests
  codecMask |= VCM_CODEC_RESOURCE_H264;
#endif

  codecMask |= VCM_CODEC_RESOURCE_VP8;
  //codecMask |= VCM_CODEC_RESOURCE_I420;
  mCCM->setVideoCodecs(codecMask);

  ccAppReadyToStartLock = PR_NewLock();
  if (!ccAppReadyToStartLock) {
    return NS_ERROR_FAILURE;
  }

  ccAppReadyToStartCond = PR_NewCondVar(ccAppReadyToStartLock);
  if (!ccAppReadyToStartCond) {
    return NS_ERROR_FAILURE;
  }

  if (!mCCM->startSDPMode())
    return NS_ERROR_FAILURE;

  mDevice = mCCM->getActiveDevice();
  mCCM->addCCObserver(this);
  NS_ENSURE_TRUE(mDevice.get(), NS_ERROR_FAILURE);
  ChangeSipccState(dom::PCImplSipccState::Starting);

  // Now that everything is set up, we let the CCApp thread
  // know that it's okay to start processing messages.
  PR_Lock(ccAppReadyToStartLock);
  ccAppReadyToStart = 1;
  PR_NotifyAllCondVar(ccAppReadyToStartCond);
  PR_Unlock(ccAppReadyToStartLock);

#ifdef MOZILLA_INTERNAL_API
  mConnectionCounter = 0;
  Telemetry::GetHistogramById(Telemetry::WEBRTC_CALL_COUNT)->Add(0);

  mTelemetryTimer = do_CreateInstance(NS_TIMER_CONTRACTID);
  MOZ_ASSERT(mTelemetryTimer);
  nsresult rv = mTelemetryTimer->SetTarget(gMainThread);
  NS_ENSURE_SUCCESS(rv, rv);
  mTelemetryTimer->InitWithFuncCallback(EverySecondTelemetryCallback_m, this, 1000,
                                        nsITimer::TYPE_REPEATING_PRECISE_CAN_SKIP);
#endif
  return NS_OK;
}

nsresult PeerConnectionCtx::Cleanup() {
  CSFLogDebug(logTag, "%s", __FUNCTION__);

  mCCM->destroy();
  mCCM->removeCCObserver(this);
  return NS_OK;
}

PeerConnectionCtx::~PeerConnectionCtx() {
    // ensure mTelemetryTimer ends on main thread
  MOZ_ASSERT(NS_IsMainThread());
#ifdef MOZILLA_INTERNAL_API
  if (mTelemetryTimer) {
    mTelemetryTimer->Cancel();
  }
#endif
};

CSF::CC_CallPtr PeerConnectionCtx::createCall() {
  return mDevice->createCall();
}

void PeerConnectionCtx::onDeviceEvent(ccapi_device_event_e aDeviceEvent,
                                      CSF::CC_DevicePtr aDevice,
                                      CSF::CC_DeviceInfoPtr aInfo ) {
  cc_service_state_t state = aInfo->getServiceState();
  // We are keeping this in a local var to avoid a data race
  // with ChangeSipccState in the debug message and compound if below
  dom::PCImplSipccState currentSipccState = mSipccState;

  switch (aDeviceEvent) {
    case CCAPI_DEVICE_EV_STATE:
      CSFLogDebug(logTag, "%s - %d : %d", __FUNCTION__, state,
                  static_cast<uint32_t>(currentSipccState));

      if (CC_STATE_INS == state) {
        // SIPCC is up
        if (dom::PCImplSipccState::Starting == currentSipccState ||
            dom::PCImplSipccState::Idle == currentSipccState) {
          ChangeSipccState(dom::PCImplSipccState::Started);
        } else {
          CSFLogError(logTag, "%s PeerConnection already started", __FUNCTION__);
        }
      } else {
        NS_NOTREACHED("Unsupported Signaling State Transition");
      }
      break;
    default:
      CSFLogDebug(logTag, "%s: Ignoring event: %s\n",__FUNCTION__,
                  device_event_getname(aDeviceEvent));
  }
}

static void onCallEvent_m(nsAutoPtr<std::string> peerconnection,
                          ccapi_call_event_e aCallEvent,
                          CSF::CC_CallInfoPtr aInfo);

void PeerConnectionCtx::onCallEvent(ccapi_call_event_e aCallEvent,
                                    CSF::CC_CallPtr aCall,
                                    CSF::CC_CallInfoPtr aInfo) {
  // This is called on a SIPCC thread.
  //
  // We cannot use SyncRunnable to main thread, as that would deadlock on
  // shutdown. Instead, we dispatch asynchronously. We copy getPeerConnection(),
  // a "weak ref" to the PC, which is safe in shutdown, and CC_CallInfoPtr (an
  // nsRefPtr) is thread-safe and keeps aInfo alive.
  nsAutoPtr<std::string> pcDuped(new std::string(aCall->getPeerConnection()));

  // DISPATCH_NORMAL with duped string
  nsresult rv = gMainThread->Dispatch(WrapRunnableNM(&onCallEvent_m, pcDuped,
                                                     aCallEvent, aInfo),
                                      NS_DISPATCH_NORMAL);
  if (NS_FAILED(rv)) {
    CSFLogError( logTag, "%s(): Could not dispatch to main thread", __FUNCTION__);
  }
}

// Demux the call event to the right PeerConnection
static void onCallEvent_m(nsAutoPtr<std::string> peerconnection,
                          ccapi_call_event_e aCallEvent,
                          CSF::CC_CallInfoPtr aInfo) {
  CSFLogDebug(logTag, "onCallEvent()");
  PeerConnectionWrapper pc(peerconnection->c_str());
  if (!pc.impl())  // This must be an event on a dead PC. Ignore
    return;
  CSFLogDebug(logTag, "Calling PC");
  pc.impl()->onCallEvent(OnCallEventArgs(aCallEvent, aInfo));
}

}  // namespace sipcc
