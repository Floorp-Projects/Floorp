/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef peerconnectionctx_h___h__
#define peerconnectionctx_h___h__

#include <string>

#include "mozilla/Attributes.h"
#include "CallControlManager.h"
#include "CC_Device.h"
#include "CC_Call.h"
#include "CC_Observer.h"

#include "PeerConnectionImpl.h"

namespace sipcc {

// A class to hold some of the singleton objects we need:
// * The global PeerConnectionImpl table and its associated lock.
// * Currently SIPCC only allows a single stack instance to exist in a process
//   at once. This class implements a singleton object that wraps that.
// * The observer class that demuxes events onto individual PCs.
class PeerConnectionCtx : public CSF::CC_Observer {
 public:
  static nsresult InitializeGlobal(nsIThread *mainThread);
  static PeerConnectionCtx* GetInstance();
  static void Destroy();

  // Implementations of CC_Observer methods
  virtual void onDeviceEvent(ccapi_device_event_e deviceEvent, CSF::CC_DevicePtr device, CSF::CC_DeviceInfoPtr info);
  virtual void onFeatureEvent(ccapi_device_event_e deviceEvent, CSF::CC_DevicePtr device, CSF::CC_FeatureInfoPtr feature_info) {}
  virtual void onLineEvent(ccapi_line_event_e lineEvent, CSF::CC_LinePtr line, CSF::CC_LineInfoPtr info) {}
  virtual void onCallEvent(ccapi_call_event_e callEvent, CSF::CC_CallPtr call, CSF::CC_CallInfoPtr info);

  // Create a SIPCC Call
  CSF::CC_CallPtr createCall();

  PeerConnectionImpl::SipccState sipcc_state() { return mSipccState; }

  // Make these classes friend so that they can access mPeerconnections.
  friend class PeerConnectionImpl;
  friend class PeerConnectionWrapper;

 private:
  // We could make these available only via accessors but it's too much trouble.
  std::map<const std::string, PeerConnectionImpl *> mPeerConnections;

  PeerConnectionCtx() :  mSipccState(PeerConnectionImpl::kIdle),
                         mCCM(NULL), mDevice(NULL) {}
  // This is a singleton, so don't copy construct it, etc.
  PeerConnectionCtx(const PeerConnectionCtx& other) MOZ_DELETE;
  void operator=(const PeerConnectionCtx& other) MOZ_DELETE;
  virtual ~PeerConnectionCtx() {};

  nsresult Initialize();
  nsresult Cleanup();

  void ChangeSipccState(PeerConnectionImpl::SipccState aState) {
    mSipccState = aState;
  }

  virtual void onCallEvent_m(ccapi_call_event_e callEvent,
			     CSF::CC_CallPtr call,
			     CSF::CC_CallInfoPtr info);

  // SIPCC objects
  PeerConnectionImpl::SipccState mSipccState;  // TODO(ekr@rtfm.com): refactor this out? What does it do?
  CSF::CallControlManagerPtr mCCM;
  CSF::CC_DevicePtr mDevice;

  static PeerConnectionCtx *gInstance;
  static nsIThread *gMainThread;
};

}  // namespace sipcc

#endif
