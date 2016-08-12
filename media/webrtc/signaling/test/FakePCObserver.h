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
#include "StreamTracks.h"
#include "nsTArray.h"
#include "nsIRunnable.h"
#include "nsISupportsImpl.h"
#include "mozilla/dom/PeerConnectionObserverEnumsBinding.h"
#include "PeerConnectionImpl.h"
#include "nsWeakReference.h"

namespace mozilla {
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

  AFakePCObserver(mozilla::PeerConnectionImpl *peerConnection,
                  const std::string &aName) :
    state(stateNoResponse), addIceSuccessCount(0),
    onAddStreamCalled(false),
    name(aName),
    pc(peerConnection) {
  }

  AFakePCObserver() :
    state(stateNoResponse), addIceSuccessCount(0),
    onAddStreamCalled(false),
    name(""),
    pc(nullptr) {
  }

  virtual ~AFakePCObserver() {}

  std::vector<mozilla::DOMMediaStream *> GetStreams() { return streams; }

  ResponseState state;
  std::string lastString;
  mozilla::PeerConnectionImpl::Error lastStatusCode;
  mozilla::dom::PCObserverStateType lastStateType;
  int addIceSuccessCount;
  bool onAddStreamCalled;
  std::string name;
  std::vector<std::string> candidates;

  NS_IMETHOD OnCreateOfferSuccess(const char* offer, ER&) = 0;
  NS_IMETHOD OnCreateOfferError(uint32_t code, const char *msg, ER&) = 0;
  NS_IMETHOD OnCreateAnswerSuccess(const char* answer, ER&) = 0;
  NS_IMETHOD OnCreateAnswerError(uint32_t code, const char *msg, ER&) = 0;
  NS_IMETHOD OnSetLocalDescriptionSuccess(ER&) = 0;
  NS_IMETHOD OnSetRemoteDescriptionSuccess(ER&) = 0;
  NS_IMETHOD OnSetLocalDescriptionError(uint32_t code, const char *msg, ER&) = 0;
  NS_IMETHOD OnSetRemoteDescriptionError(uint32_t code, const char *msg, ER&) = 0;
  NS_IMETHOD NotifyDataChannel(nsIDOMDataChannel *channel, ER&) = 0;
  NS_IMETHOD OnStateChange(mozilla::dom::PCObserverStateType state_type, ER&,
                                      void* = nullptr) = 0;
  NS_IMETHOD OnAddStream(mozilla::DOMMediaStream &stream, ER&) = 0;
  NS_IMETHOD OnRemoveStream(mozilla::DOMMediaStream &stream, ER&) = 0;
  NS_IMETHOD OnAddTrack(mozilla::dom::MediaStreamTrack &track, ER&) = 0;
  NS_IMETHOD OnRemoveTrack(mozilla::dom::MediaStreamTrack &track, ER&) = 0;
  NS_IMETHOD OnReplaceTrackSuccess(ER&) = 0;
  NS_IMETHOD OnReplaceTrackError(uint32_t code, const char *msg, ER&) = 0;
  NS_IMETHOD OnAddIceCandidateSuccess(ER&) = 0;
  NS_IMETHOD OnAddIceCandidateError(uint32_t code, const char *msg, ER&) = 0;
  NS_IMETHOD OnIceCandidate(uint16_t level, const char *mid,
                                       const char *candidate, ER&) = 0;
  NS_IMETHOD OnNegotiationNeeded(ER&) = 0;
protected:
  mozilla::PeerConnectionImpl *pc;
  std::vector<mozilla::DOMMediaStream *> streams;
};
}

namespace mozilla {
namespace dom {
typedef test::AFakePCObserver PeerConnectionObserver;
}
}

#endif
