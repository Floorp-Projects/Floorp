/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _RTCDTMFSender_h_
#define _RTCDTMFSender_h_

#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/RefPtr.h"
#include "js/RootingAPI.h"

class nsPIDOMWindowInner;
class nsITimer;

namespace mozilla {
class AudioSessionConduit;
class TransceiverImpl;

namespace dom {

class RTCDTMFSender : public DOMEventTargetHelper, public nsITimerCallback {
 public:
  explicit RTCDTMFSender(nsPIDOMWindowInner* aWindow,
                         TransceiverImpl* aTransceiver,
                         AudioSessionConduit* aConduit);

  // nsISupports
  NS_DECL_NSITIMERCALLBACK
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(RTCDTMFSender, DOMEventTargetHelper)

  // webidl
  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;
  void InsertDTMF(const nsAString& aTones, uint32_t aDuration,
                  uint32_t aInterToneGap, ErrorResult& aRv);
  void GetToneBuffer(nsAString& aOutToneBuffer);
  IMPL_EVENT_HANDLER(tonechange)

  nsPIDOMWindowInner* GetParentObject() const;

  void StopPlayout();

 private:
  virtual ~RTCDTMFSender() = default;

  void StartPlayout(uint32_t aDelay);

  nsCOMPtr<nsPIDOMWindowInner> mWindow;
  RefPtr<TransceiverImpl> mTransceiver;
  RefPtr<AudioSessionConduit> mConduit;
  nsString mToneBuffer;
  uint32_t mDuration = 0;
  uint32_t mInterToneGap = 0;
  nsCOMPtr<nsITimer> mSendTimer;
};

}  // namespace dom
}  // namespace mozilla
#endif  // _RTCDTMFSender_h_
