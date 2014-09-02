/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef peerconnectionctx_h___h__
#define peerconnectionctx_h___h__

#include <string>

#include "mozilla/Attributes.h"
#include "CallControlManager.h"
#include "CC_Device.h"
#include "CC_DeviceInfo.h"
#include "CC_Call.h"
#include "CC_CallInfo.h"
#include "CC_Line.h"
#include "CC_LineInfo.h"
#include "CC_Observer.h"
#include "CC_FeatureInfo.h"
#include "cpr_stdlib.h"

#include "StaticPtr.h"
#include "PeerConnectionImpl.h"
#include "mozIGeckoMediaPluginService.h"

namespace mozilla {
class PeerConnectionCtxShutdown;

namespace dom {
class WebrtcGlobalInformation;
}

// Unit-test helper, because cc_media_options_t is hard to forward-declare

class SipccOfferOptions {
public:
  SipccOfferOptions();
  explicit SipccOfferOptions(const dom::RTCOfferOptions &aOther);
  cc_media_options_t* build() const;
protected:
  cc_media_options_t mOptions;
};
}

namespace sipcc {

class OnCallEventArgs {
public:
  OnCallEventArgs(ccapi_call_event_e aCallEvent, CSF::CC_CallInfoPtr aInfo)
  : mCallEvent(aCallEvent), mInfo(aInfo) {}

  ccapi_call_event_e mCallEvent;
  CSF::CC_CallInfoPtr mInfo;
};

// A class to hold some of the singleton objects we need:
// * The global PeerConnectionImpl table and its associated lock.
// * Currently SIPCC only allows a single stack instance to exist in a process
//   at once. This class implements a singleton object that wraps that.
// * The observer class that demuxes events onto individual PCs.
class PeerConnectionCtx : public CSF::CC_Observer {
 public:
  static nsresult InitializeGlobal(nsIThread *mainThread, nsIEventTarget *stsThread);
  static PeerConnectionCtx* GetInstance();
  static bool isActive();
  static void Destroy();

  // Implementations of CC_Observer methods
  virtual void onDeviceEvent(ccapi_device_event_e deviceEvent, CSF::CC_DevicePtr device, CSF::CC_DeviceInfoPtr info);
  virtual void onFeatureEvent(ccapi_device_event_e deviceEvent, CSF::CC_DevicePtr device, CSF::CC_FeatureInfoPtr feature_info) {}
  virtual void onLineEvent(ccapi_line_event_e lineEvent, CSF::CC_LinePtr line, CSF::CC_LineInfoPtr info) {}
  virtual void onCallEvent(ccapi_call_event_e callEvent, CSF::CC_CallPtr call, CSF::CC_CallInfoPtr info);

  // Create a SIPCC Call
  CSF::CC_CallPtr createCall();

  mozilla::dom::PCImplSipccState sipcc_state() { return mSipccState; }

  bool isReady() {
    // If mGMPService is not set, we aren't using GMP.
    if (mGMPService) {
      return mGMPReady;
    }
    return true;
  }

  void queueJSEPOperation(nsRefPtr<nsIRunnable> aJSEPOperation);
  void onGMPReady();

  // Make these classes friend so that they can access mPeerconnections.
  friend class PeerConnectionImpl;
  friend class PeerConnectionWrapper;
  friend class mozilla::dom::WebrtcGlobalInformation;

#ifdef MOZILLA_INTERNAL_API
  // WebrtcGlobalInformation uses this; we put it here so we don't need to
  // create another shutdown observer class.
  mozilla::dom::Sequence<mozilla::dom::RTCStatsReportInternal>
    mStatsForClosedPeerConnections;
#endif

 private:
  // We could make these available only via accessors but it's too much trouble.
  std::map<const std::string, PeerConnectionImpl *> mPeerConnections;

  PeerConnectionCtx() :  mSipccState(mozilla::dom::PCImplSipccState::Idle),
                         mCCM(nullptr), mDevice(nullptr), mGMPReady(false) {}
  // This is a singleton, so don't copy construct it, etc.
  PeerConnectionCtx(const PeerConnectionCtx& other) MOZ_DELETE;
  void operator=(const PeerConnectionCtx& other) MOZ_DELETE;
  virtual ~PeerConnectionCtx();

  nsresult Initialize();
  nsresult Cleanup();

  void ChangeSipccState(mozilla::dom::PCImplSipccState aState) {
    mSipccState = aState;
  }

  void initGMP();

  static void
  EverySecondTelemetryCallback_m(nsITimer* timer, void *);

#ifdef MOZILLA_INTERNAL_API
  // Telemetry Peer conection counter
  int mConnectionCounter;

  nsCOMPtr<nsITimer> mTelemetryTimer;

public:
  // TODO(jib): If we ever enable move semantics on std::map...
  //std::map<nsString,nsAutoPtr<mozilla::dom::RTCStatsReportInternal>> mLastReports;
  nsTArray<nsAutoPtr<mozilla::dom::RTCStatsReportInternal>> mLastReports;
private:
#endif

  // SIPCC objects
  mozilla::dom::PCImplSipccState mSipccState;  // TODO(ekr@rtfm.com): refactor this out? What does it do?
  CSF::CallControlManagerPtr mCCM;
  CSF::CC_DevicePtr mDevice;

  // We cannot form offers/answers properly until the Gecko Media Plugin stuff
  // has been initted, which is a complicated mess of thread dispatches,
  // including sync dispatches to main. So, we need to be able to queue up
  // offer creation (or SetRemote, when we're the answerer) until all of this is
  // ready to go, since blocking on this init is just begging for deadlock.
  nsCOMPtr<mozIGeckoMediaPluginService> mGMPService;
  bool mGMPReady;
  nsTArray<nsRefPtr<nsIRunnable>> mQueuedJSEPOperations;

  static PeerConnectionCtx *gInstance;
public:
  static nsIThread *gMainThread;
  static mozilla::StaticRefPtr<mozilla::PeerConnectionCtxShutdown> gPeerConnectionCtxShutdown;
};

}  // namespace sipcc

#endif
