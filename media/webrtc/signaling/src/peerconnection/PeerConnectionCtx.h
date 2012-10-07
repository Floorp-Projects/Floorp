/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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

// Currently SIPCC only allows a single stack instance to exist in a process
// at once. This class implements a singleton object that wraps that
// instance. It also hosts the observer class that demuxes events onto
// individual PCs.
class PeerConnectionCtx : public CSF::CC_Observer {
 public:
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

 private:
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

  // SIPCC objects
  PeerConnectionImpl::SipccState mSipccState;  // TODO(ekr@rtfm.com): refactor this out? What does it do?
  CSF::CallControlManagerPtr mCCM;
  CSF::CC_DevicePtr mDevice;

  static PeerConnectionCtx *instance;
};

}  // namespace sipcc

#endif
