/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#include "CallControlManager.h"
#include "CC_Device.h"
#include "CC_Call.h"
#include "CC_Observer.h"
#include "ccapi_call_info.h"
#include "CC_SIPCCCallInfo.h"
#include "ccapi_device_info.h"
#include "CC_SIPCCDeviceInfo.h"
#include "CSFLog.h"
#include "vcm.h"
#include "VcmSIPCCBinding.h"
#include "PeerConnectionImpl.h"
#include "PeerConnectionCtx.h"
#include "runnable_utils.h"
#include "cpr_socket.h"

#include "nsIObserverService.h"
#include "nsIObserver.h"
#include "mozilla/Services.h"
#include "StaticPtr.h"

static const char* logTag = "PeerConnectionCtx";

namespace mozilla {
class PeerConnectionCtxShutdown : public nsIObserver
{
public:
  NS_DECL_ISUPPORTS

  PeerConnectionCtxShutdown() {}

  void Init()
    {
      nsCOMPtr<nsIObserverService> observerService =
        mozilla::services::GetObserverService();
      if (!observerService)
        return;

      nsresult rv = observerService->AddObserver(this,
                                                 NS_XPCOM_SHUTDOWN_OBSERVER_ID,
                                                 false);
      MOZ_ASSERT(rv == NS_OK);
      (void) rv;
    }

  virtual ~PeerConnectionCtxShutdown()
    {
      nsCOMPtr<nsIObserverService> observerService =
        mozilla::services::GetObserverService();
      if (observerService)
        observerService->RemoveObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID);
    }

  NS_IMETHODIMP Observe(nsISupports* aSubject, const char* aTopic,
                        const PRUnichar* aData) {
    if (strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID) == 0) {
      CSFLogDebug(logTag, "Shutting down PeerConnectionCtx");
      sipcc::PeerConnectionCtx::Destroy();

      nsCOMPtr<nsIObserverService> observerService =
        mozilla::services::GetObserverService();
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

NS_IMPL_ISUPPORTS1(PeerConnectionCtxShutdown, nsIObserver);
}

namespace sipcc {

PeerConnectionCtx* PeerConnectionCtx::gInstance;
nsIThread* PeerConnectionCtx::gMainThread;
StaticRefPtr<mozilla::PeerConnectionCtxShutdown> PeerConnectionCtx::gPeerConnectionCtxShutdown;

nsresult PeerConnectionCtx::InitializeGlobal(nsIThread *mainThread) {
  if (!gMainThread) {
    gMainThread = mainThread;
    CSF::VcmSIPCCBinding::setMainThread(gMainThread);
  } else {
#ifdef MOZILLA_INTERNAL_API
    MOZ_ASSERT(gMainThread == mainThread);
#endif
  }

  nsresult res;

#ifdef MOZILLA_INTERNAL_API
  // This check fails on the unit tests because they do not
  // have the right thread behavior.
  bool on;
  res = gMainThread->IsOnCurrentThread(&on);
  NS_ENSURE_SUCCESS(res, res);
  MOZ_ASSERT(on);
#endif

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

void PeerConnectionCtx::Destroy() {
  CSFLogDebug(logTag, "%s", __FUNCTION__);

  gInstance->Cleanup();
  delete gInstance;
  gInstance = NULL;
}

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

  //codecMask |= VCM_CODEC_RESOURCE_H264;
  codecMask |= VCM_CODEC_RESOURCE_VP8;
  //codecMask |= VCM_CODEC_RESOURCE_I420;
  mCCM->setVideoCodecs(codecMask);

  if (!mCCM->startSDPMode())
    return NS_ERROR_FAILURE;

  mDevice = mCCM->getActiveDevice();
  mCCM->addCCObserver(this);
  NS_ENSURE_TRUE(mDevice.get(), NS_ERROR_FAILURE);
  ChangeSipccState(PeerConnectionImpl::kStarting);
  return NS_OK;
}

nsresult PeerConnectionCtx::Cleanup() {
  CSFLogDebug(logTag, "%s", __FUNCTION__);

  mCCM->destroy();
  mCCM->removeCCObserver(this);
  return NS_OK;
}

CSF::CC_CallPtr PeerConnectionCtx::createCall() {
  return mDevice->createCall();
}

void PeerConnectionCtx::onDeviceEvent(ccapi_device_event_e aDeviceEvent,
                                      CSF::CC_DevicePtr aDevice,
                                      CSF::CC_DeviceInfoPtr aInfo ) {
  cc_service_state_t state = aInfo->getServiceState();

  CSFLogDebug(logTag, "%s - %d : %d", __FUNCTION__, state, mSipccState);

  if (CC_STATE_INS == state) {
    // SIPCC is up
    if (PeerConnectionImpl::kStarting == mSipccState ||
        PeerConnectionImpl::kIdle == mSipccState) {
      ChangeSipccState(PeerConnectionImpl::kStarted);
    } else {
      CSFLogError(logTag, "%s PeerConnection already started", __FUNCTION__);
    }
  } else {
    NS_NOTREACHED("Unsupported Signaling State Transition");
  }
}

void PeerConnectionCtx::onCallEvent(ccapi_call_event_e aCallEvent,
                                      CSF::CC_CallPtr aCall,
                                      CSF::CC_CallInfoPtr aInfo) {
  // This is called on a SIPCC thread.
  // WARNING: Do not make this NS_DISPATCH_NORMAL.
  // CC_*Ptr is not thread-safe so we must not manipulate
  // the ref count on multiple threads at once.
  // NS_DISPATCH_SYNC enforces this and because this is
  // not a real nsThread, we don't have to worry about
  // reentrancy.
  RUN_ON_THREAD(gMainThread,
                WrapRunnable(this,
                             &PeerConnectionCtx::onCallEvent_m,
                             aCallEvent, aCall, aInfo),
                NS_DISPATCH_SYNC);
}

// Demux the call event to the right PeerConnection
void PeerConnectionCtx::onCallEvent_m(ccapi_call_event_e aCallEvent,
                                      CSF::CC_CallPtr aCall,
                                      CSF::CC_CallInfoPtr aInfo) {
  CSFLogDebug(logTag, "onCallEvent()");
  PeerConnectionWrapper pc(aCall->getPeerConnection());
  if (!pc.impl())  // This must be an event on a dead PC. Ignore
    return;

  CSFLogDebug(logTag, "Calling PC");
  pc.impl()->onCallEvent(aCallEvent, aCall, aInfo);
}

}  // namespace sipcc
