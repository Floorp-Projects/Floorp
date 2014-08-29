/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TEST_PCOBSERVER_H_
#define TEST_PCOBSERVER_H_

#include "nsNetCID.h"
#include "nsITimer.h"
#include "nsComponentManagerUtils.h"
#include "nsIComponentManager.h"
#include "nsIComponentRegistrar.h"

#include "mozilla/Mutex.h"
#include "AudioSegment.h"
#include "MediaSegment.h"
#include "StreamBuffer.h"
#include "nsTArray.h"
#include "nsIRunnable.h"
#include "nsISupportsImpl.h"
#include "nsIDOMMediaStream.h"
#include "mozilla/dom/PeerConnectionObserverEnumsBinding.h"
#include "PeerConnectionImpl.h"
#include "nsWeakReference.h"

namespace sipcc {
class PeerConnectionImpl;
}

class nsIDOMWindow;
class nsIDOMDataChannel;

namespace test {

class AFakePCObserver : public nsSupportsWeakReference
{
protected:
  typedef mozilla::ErrorResult ER;
public:
  enum Action {
    OFFER,
    ANSWER
  };

  enum ResponseState {
    stateNoResponse,
    stateSuccess,
    stateError
  };

  AFakePCObserver(sipcc::PeerConnectionImpl *peerConnection,
                  const std::string &aName) :
    state(stateNoResponse), addIceSuccessCount(0),
    onAddStreamCalled(false),
    name(aName),
    pc(peerConnection) {
  }

  virtual ~AFakePCObserver() {}

  std::vector<mozilla::DOMMediaStream *> GetStreams() { return streams; }

  ResponseState state;
  std::string lastString;
  sipcc::PeerConnectionImpl::Error lastStatusCode;
  mozilla::dom::PCObserverStateType lastStateType;
  int addIceSuccessCount;
  bool onAddStreamCalled;
  std::string name;
  std::vector<std::string> candidates;

  virtual NS_IMETHODIMP OnCreateOfferSuccess(const char* offer, ER&) = 0;
  virtual NS_IMETHODIMP OnCreateOfferError(uint32_t code, const char *msg, ER&) = 0;
  virtual NS_IMETHODIMP OnCreateAnswerSuccess(const char* answer, ER&) = 0;
  virtual NS_IMETHODIMP OnCreateAnswerError(uint32_t code, const char *msg, ER&) = 0;
  virtual NS_IMETHODIMP OnSetLocalDescriptionSuccess(ER&) = 0;
  virtual NS_IMETHODIMP OnSetRemoteDescriptionSuccess(ER&) = 0;
  virtual NS_IMETHODIMP OnSetLocalDescriptionError(uint32_t code, const char *msg, ER&) = 0;
  virtual NS_IMETHODIMP OnSetRemoteDescriptionError(uint32_t code, const char *msg, ER&) = 0;
  virtual NS_IMETHODIMP NotifyDataChannel(nsIDOMDataChannel *channel, ER&) = 0;
  virtual NS_IMETHODIMP OnStateChange(mozilla::dom::PCObserverStateType state_type, ER&,
                                      void* = nullptr) = 0;
  virtual NS_IMETHODIMP OnAddStream(nsIDOMMediaStream *stream, ER&) = 0;
  virtual NS_IMETHODIMP OnRemoveStream(ER&) = 0;
  virtual NS_IMETHODIMP OnAddTrack(ER&) = 0;
  virtual NS_IMETHODIMP OnRemoveTrack(ER&) = 0;
  virtual NS_IMETHODIMP OnReplaceTrackSuccess(ER&) = 0;
  virtual NS_IMETHODIMP OnReplaceTrackError(uint32_t code, const char *msg, ER&) = 0;
  virtual NS_IMETHODIMP OnAddIceCandidateSuccess(ER&) = 0;
  virtual NS_IMETHODIMP OnAddIceCandidateError(uint32_t code, const char *msg, ER&) = 0;
  virtual NS_IMETHODIMP OnIceCandidate(uint16_t level, const char *mid,
                                       const char *candidate, ER&) = 0;
protected:
  sipcc::PeerConnectionImpl *pc;
  std::vector<mozilla::DOMMediaStream *> streams;
};
}

namespace mozilla {
namespace dom {
typedef test::AFakePCObserver PeerConnectionObserver;
}
}

#endif
