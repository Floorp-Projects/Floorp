/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <iostream>
#include <map>
#include <algorithm>
#include <string>

#include "base/basictypes.h"
#include "logging.h"

#define GTEST_HAS_RTTI 0
#include "gtest/gtest.h"
#include "gtest_utils.h"

#include "nspr.h"
#include "nss.h"
#include "ssl.h"
#include "prthread.h"

#include "FakePCObserver.h"
#include "FakeMediaStreams.h"
#include "FakeMediaStreamsImpl.h"
#include "PeerConnectionImpl.h"
#include "PeerConnectionCtx.h"
#include "PeerConnectionMedia.h"
#include "MediaPipeline.h"
#include "runnable_utils.h"
#include "nsServiceManagerUtils.h"
#include "mozilla/Services.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsIDNSService.h"
#include "nsQueryObject.h"
#include "nsWeakReference.h"
#include "nricectx.h"
#include "rlogringbuffer.h"
#include "mozilla/SyncRunnable.h"
#include "logging.h"
#include "stunserver.h"
#include "stunserver.cpp"
#include "PeerConnectionImplEnumsBinding.cpp"

#include "ice_ctx.h"
#include "ice_peer_ctx.h"

#include "mtransport_test_utils.h"
#include "gtest_ringbuffer_dumper.h"
MtransportTestUtils *test_utils;
nsCOMPtr<nsIThread> gMainThread;
nsCOMPtr<nsIThread> gGtestThread;
bool gTestsComplete = false;

#ifndef USE_FAKE_MEDIA_STREAMS
#error USE_FAKE_MEDIA_STREAMS undefined
#endif
#ifndef USE_FAKE_PCOBSERVER
#error USE_FAKE_PCOBSERVER undefined
#endif

static int kDefaultTimeout = 10000;

static std::string callerName = "caller";
static std::string calleeName = "callee";

#define ARRAY_TO_STL(container, type, array) \
        (container<type>((array), (array) + PR_ARRAY_SIZE(array)))

#define ARRAY_TO_SET(type, array) ARRAY_TO_STL(std::set, type, array)

std::string g_stun_server_address((char *)"23.21.150.121");
uint16_t g_stun_server_port(3478);
std::string kBogusSrflxAddress((char *)"192.0.2.1");
uint16_t kBogusSrflxPort(1001);

// We can't use webidl bindings here because it uses nsString,
// so we pass options in using OfferOptions instead
class OfferOptions : public mozilla::JsepOfferOptions {
public:
  void setInt32Option(const char *namePtr, size_t value) {
    if (!strcmp(namePtr, "OfferToReceiveAudio")) {
      mOfferToReceiveAudio = mozilla::Some(value);
    } else if (!strcmp(namePtr, "OfferToReceiveVideo")) {
      mOfferToReceiveVideo = mozilla::Some(value);
    }
  }
private:
};

using namespace mozilla;
using namespace mozilla::dom;

// XXX Workaround for bug 998092 to maintain the existing broken semantics
template<>
struct nsISupportsWeakReference::COMTypeInfo<nsSupportsWeakReference, void> {
  static const nsIID kIID;
};
//const nsIID nsISupportsWeakReference::COMTypeInfo<nsSupportsWeakReference, void>::kIID = NS_ISUPPORTSWEAKREFERENCE_IID;

namespace test {

class SignalingAgent;

std::string indent(const std::string &s, int width = 4) {
  std::string prefix;
  std::string out;
  char previous = '\n';
  prefix.assign(width, ' ');
  for (std::string::const_iterator i = s.begin(); i != s.end(); i++) {
    if (previous == '\n') {
      out += prefix;
    }
    out += *i;
    previous = *i;
  }
  return out;
}

static const std::string strSampleSdpAudioVideoNoIce =
  "v=0\r\n"
  "o=Mozilla-SIPUA 4949 0 IN IP4 10.86.255.143\r\n"
  "s=SIP Call\r\n"
  "t=0 0\r\n"
  "a=ice-ufrag:qkEP\r\n"
  "a=ice-pwd:ed6f9GuHjLcoCN6sC/Eh7fVl\r\n"
  "m=audio 16384 RTP/AVP 0 8 9 101\r\n"
  "c=IN IP4 10.86.255.143\r\n"
  "a=rtpmap:0 PCMU/8000\r\n"
  "a=rtpmap:8 PCMA/8000\r\n"
  "a=rtpmap:9 G722/8000\r\n"
  "a=rtpmap:101 telephone-event/8000\r\n"
  "a=fmtp:101 0-15\r\n"
  "a=sendrecv\r\n"
  "a=candidate:1 1 UDP 2130706431 192.168.2.1 50005 typ host\r\n"
  "a=candidate:2 2 UDP 2130706431 192.168.2.2 50006 typ host\r\n"
  "m=video 1024 RTP/AVP 97\r\n"
  "c=IN IP4 10.86.255.143\r\n"
  "a=rtpmap:120 VP8/90000\r\n"
  "a=fmtp:97 profile-level-id=42E00C\r\n"
  "a=sendrecv\r\n"
  "a=candidate:1 1 UDP 2130706431 192.168.2.3 50007 typ host\r\n"
  "a=candidate:2 2 UDP 2130706431 192.168.2.4 50008 typ host\r\n";

static const std::string strSampleCandidate =
  "a=candidate:1 1 UDP 2130706431 192.168.2.1 50005 typ host\r\n";

static const std::string strSampleMid = "";

static const unsigned short nSamplelevel = 2;

static const std::string strG711SdpOffer =
    "v=0\r\n"
    "o=- 1 1 IN IP4 148.147.200.251\r\n"
    "s=-\r\n"
    "b=AS:64\r\n"
    "t=0 0\r\n"
    "a=fingerprint:sha-256 F3:FA:20:C0:CD:48:C4:5F:02:5F:A5:D3:21:D0:2D:48:"
      "7B:31:60:5C:5A:D8:0D:CD:78:78:6C:6D:CE:CC:0C:67\r\n"
    "m=audio 9000 RTP/AVP 0 8 126\r\n"
    "c=IN IP4 148.147.200.251\r\n"
    "b=TIAS:64000\r\n"
    "a=rtpmap:0 PCMU/8000\r\n"
    "a=rtpmap:8 PCMA/8000\r\n"
    "a=rtpmap:126 telephone-event/8000\r\n"
    "a=candidate:0 1 udp 2130706432 148.147.200.251 9000 typ host\r\n"
    "a=candidate:0 2 udp 2130706432 148.147.200.251 9005 typ host\r\n"
    "a=ice-ufrag:cYuakxkEKH+RApYE\r\n"
    "a=ice-pwd:bwtpzLZD+3jbu8vQHvEa6Xuq\r\n"
    "a=setup:active\r\n"
    "a=sendrecv\r\n";


enum sdpTestFlags
{
  HAS_ALL_CANDIDATES     = (1 << 0),
};

enum offerAnswerFlags
{
  OFFER_NONE  = 0, // Sugar to make function calls clearer.
  OFFER_AUDIO = (1<<0),
  OFFER_VIDEO = (1<<1),
  // Leaving some room here for other media types
  ANSWER_NONE  = 0, // Sugar to make function calls clearer.
  ANSWER_AUDIO = (1<<8),
  ANSWER_VIDEO = (1<<9),

  OFFER_AV = OFFER_AUDIO | OFFER_VIDEO,
  ANSWER_AV = ANSWER_AUDIO | ANSWER_VIDEO
};

 typedef enum {
   NO_TRICKLE = 0,
   OFFERER_TRICKLES = 1,
   ANSWERER_TRICKLES = 2,
   BOTH_TRICKLE = OFFERER_TRICKLES | ANSWERER_TRICKLES
 } TrickleType;

class TestObserver : public AFakePCObserver
{
protected:
  ~TestObserver() {}

public:
  TestObserver(PeerConnectionImpl *peerConnection,
               const std::string &aName) :
    AFakePCObserver(peerConnection, aName),
    lastAddIceStatusCode(PeerConnectionImpl::kNoError),
    peerAgent(nullptr),
    trickleCandidates(true)
    {}

  size_t MatchingCandidates(const std::string& cand) {
    size_t count = 0;

    for (size_t i=0; i<candidates.size(); ++i) {
      if (candidates[i].find(cand) != std::string::npos)
        ++count;
    }

    return count;
  }

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_IMETHOD OnCreateOfferSuccess(const char* offer, ER&) override;
  NS_IMETHOD OnCreateOfferError(uint32_t code, const char *msg, ER&) override;
  NS_IMETHOD OnCreateAnswerSuccess(const char* answer, ER&) override;
  NS_IMETHOD OnCreateAnswerError(uint32_t code, const char *msg, ER&) override;
  NS_IMETHOD OnSetLocalDescriptionSuccess(ER&) override;
  NS_IMETHOD OnSetRemoteDescriptionSuccess(ER&) override;
  NS_IMETHOD OnSetLocalDescriptionError(uint32_t code, const char *msg, ER&) override;
  NS_IMETHOD OnSetRemoteDescriptionError(uint32_t code, const char *msg, ER&) override;
  NS_IMETHOD NotifyDataChannel(nsIDOMDataChannel *channel, ER&) override;
  NS_IMETHOD OnStateChange(PCObserverStateType state_type, ER&, void*) override;
  NS_IMETHOD OnAddStream(DOMMediaStream &stream, ER&) override;
  NS_IMETHOD OnRemoveStream(DOMMediaStream &stream, ER&) override;
  NS_IMETHOD OnAddTrack(MediaStreamTrack &track, ER&) override;
  NS_IMETHOD OnRemoveTrack(MediaStreamTrack &track, ER&) override;
  NS_IMETHOD OnReplaceTrackSuccess(ER&) override;
  NS_IMETHOD OnReplaceTrackError(uint32_t code, const char *msg, ER&) override;
  NS_IMETHOD OnAddIceCandidateSuccess(ER&) override;
  NS_IMETHOD OnAddIceCandidateError(uint32_t code, const char *msg, ER&) override;
  NS_IMETHOD OnIceCandidate(uint16_t level, const char *mid, const char *cand, ER&) override;
  NS_IMETHOD OnNegotiationNeeded(ER&) override;

  // Hack because add_ice_candidates can happen asynchronously with respect
  // to the API calls. The whole test suite needs a refactor.
  ResponseState addIceCandidateState;
  PeerConnectionImpl::Error lastAddIceStatusCode;

  SignalingAgent* peerAgent;
  bool trickleCandidates;
};

NS_IMPL_ISUPPORTS(TestObserver, nsISupportsWeakReference)

NS_IMETHODIMP
TestObserver::OnCreateOfferSuccess(const char* offer, ER&)
{
  lastString = offer;
  state = stateSuccess;
  std::cout << name << ": onCreateOfferSuccess = " << std::endl << indent(offer)
            << std::endl;
  return NS_OK;
}

NS_IMETHODIMP
TestObserver::OnCreateOfferError(uint32_t code, const char *message, ER&)
{
  lastStatusCode = static_cast<PeerConnectionImpl::Error>(code);
  state = stateError;
  std::cout << name << ": onCreateOfferError = " << code
            << " (" << message << ")" << std::endl;
  return NS_OK;
}

NS_IMETHODIMP
TestObserver::OnCreateAnswerSuccess(const char* answer, ER&)
{
  lastString = answer;
  state = stateSuccess;
  std::cout << name << ": onCreateAnswerSuccess =" << std::endl
            << indent(answer) << std::endl;
  return NS_OK;
}

NS_IMETHODIMP
TestObserver::OnCreateAnswerError(uint32_t code, const char *message, ER&)
{
  lastStatusCode = static_cast<PeerConnectionImpl::Error>(code);
  std::cout << name << ": onCreateAnswerError = " << code
            << " (" << message << ")" << std::endl;
  state = stateError;
  return NS_OK;
}

NS_IMETHODIMP
TestObserver::OnSetLocalDescriptionSuccess(ER&)
{
  lastStatusCode = PeerConnectionImpl::kNoError;
  state = stateSuccess;
  std::cout << name << ": onSetLocalDescriptionSuccess" << std::endl;
  return NS_OK;
}

NS_IMETHODIMP
TestObserver::OnSetRemoteDescriptionSuccess(ER&)
{
  lastStatusCode = PeerConnectionImpl::kNoError;
  state = stateSuccess;
  std::cout << name << ": onSetRemoteDescriptionSuccess" << std::endl;
  return NS_OK;
}

NS_IMETHODIMP
TestObserver::OnSetLocalDescriptionError(uint32_t code, const char *message, ER&)
{
  lastStatusCode = static_cast<PeerConnectionImpl::Error>(code);
  state = stateError;
  std::cout << name << ": onSetLocalDescriptionError = " << code
            << " (" << message << ")" << std::endl;
  return NS_OK;
}

NS_IMETHODIMP
TestObserver::OnSetRemoteDescriptionError(uint32_t code, const char *message, ER&)
{
  lastStatusCode = static_cast<PeerConnectionImpl::Error>(code);
  state = stateError;
  std::cout << name << ": onSetRemoteDescriptionError = " << code
            << " (" << message << ")" << std::endl;
  return NS_OK;
}

NS_IMETHODIMP
TestObserver::NotifyDataChannel(nsIDOMDataChannel *channel, ER&)
{
  std::cout << name << ": NotifyDataChannel" << std::endl;
  return NS_OK;
}

NS_IMETHODIMP
TestObserver::OnStateChange(PCObserverStateType state_type, ER&, void*)
{
  nsresult rv;
  PCImplIceConnectionState gotice;
  PCImplIceGatheringState goticegathering;
  PCImplSignalingState gotsignaling;

  std::cout << name << ": ";

  switch (state_type)
  {
  case PCObserverStateType::IceConnectionState:
    MOZ_ASSERT(NS_IsMainThread());
    rv = pc->IceConnectionState(&gotice);
    NS_ENSURE_SUCCESS(rv, rv);
    std::cout << "ICE Connection State: "
              << PCImplIceConnectionStateValues::strings[int(gotice)].value
              << std::endl;
    break;
  case PCObserverStateType::IceGatheringState:
    MOZ_ASSERT(NS_IsMainThread());
    rv = pc->IceGatheringState(&goticegathering);
    NS_ENSURE_SUCCESS(rv, rv);
    std::cout
        << "ICE Gathering State: "
        << PCImplIceGatheringStateValues::strings[int(goticegathering)].value
        << std::endl;
    break;
  case PCObserverStateType::SdpState:
    std::cout << "SDP State: " << std::endl;
    // NS_ENSURE_SUCCESS(rv, rv);
    break;
  case PCObserverStateType::SignalingState:
    MOZ_ASSERT(NS_IsMainThread());
    rv = pc->SignalingState(&gotsignaling);
    NS_ENSURE_SUCCESS(rv, rv);
    std::cout << "Signaling State: "
              << PCImplSignalingStateValues::strings[int(gotsignaling)].value
              << std::endl;
    break;
  default:
    // Unknown State
    MOZ_CRASH("Unknown state change type.");
    break;
  }

  lastStateType = state_type;
  return NS_OK;
}


NS_IMETHODIMP
TestObserver::OnAddStream(DOMMediaStream &stream, ER&)
{
  std::cout << name << ": OnAddStream called hints=" << stream.GetHintContents()
            << " thread=" << PR_GetCurrentThread() << std::endl ;

  onAddStreamCalled = true;

  streams.push_back(&stream);

  // We know that the media stream is secretly a Fake_SourceMediaStream,
  // so now we can start it pulling from us
  nsRefPtr<Fake_SourceMediaStream> fs =
    static_cast<Fake_SourceMediaStream *>(stream.GetStream());

  test_utils->sts_target()->Dispatch(
    WrapRunnable(fs, &Fake_SourceMediaStream::Start),
    NS_DISPATCH_NORMAL);

  return NS_OK;
}

NS_IMETHODIMP
TestObserver::OnRemoveStream(DOMMediaStream &stream, ER&)
{
  state = stateSuccess;
  return NS_OK;
}

NS_IMETHODIMP
TestObserver::OnAddTrack(MediaStreamTrack &track, ER&)
{
  state = stateSuccess;
  return NS_OK;
}

NS_IMETHODIMP
TestObserver::OnRemoveTrack(MediaStreamTrack &track, ER&)
{
  state = stateSuccess;
  return NS_OK;
}

NS_IMETHODIMP
TestObserver::OnReplaceTrackSuccess(ER&)
{
  return NS_OK;
}

NS_IMETHODIMP
TestObserver::OnReplaceTrackError(uint32_t code, const char *message, ER&)
{
  return NS_OK;
}

NS_IMETHODIMP
TestObserver::OnAddIceCandidateSuccess(ER&)
{
  lastAddIceStatusCode = PeerConnectionImpl::kNoError;
  addIceCandidateState = TestObserver::stateSuccess;
  std::cout << name << ": onAddIceCandidateSuccess" << std::endl;
  addIceSuccessCount++;
  return NS_OK;
}

NS_IMETHODIMP
TestObserver::OnAddIceCandidateError(uint32_t code, const char *message, ER&)
{
  lastAddIceStatusCode = static_cast<PeerConnectionImpl::Error>(code);
  addIceCandidateState = TestObserver::stateError;
  std::cout << name << ": onAddIceCandidateError = " << code
            << " (" << message << ")" << std::endl;
  return NS_OK;
}

class ParsedSDP {
 public:

  explicit ParsedSDP(const std::string &sdp)
  {
    Parse(sdp);
  }

  void DeleteLines(const std::string &objType,
                   uint32_t limit = UINT32_MAX)
  {
    for (auto it = sdp_lines_.begin(); it != sdp_lines_.end() && limit;) {
      auto temp = it;
      ++it;
      if (temp->first == objType) {
        sdp_lines_.erase(temp);
        --limit;
      }
    }
  }

  void DeleteLine(const std::string &objType)
  {
    DeleteLines(objType, 1);
  }

  // Replaces the first instance of objType in the SDP with
  // a new string.
  // If content is an empty string then the line will be removed
  void ReplaceLine(const std::string &objType,
                   const std::string &content,
                   size_t index = 0)
  {
    auto it = FindLine(objType, index);
    if(it != sdp_lines_.end()) {
      if (content.empty()) {
        sdp_lines_.erase(it);
      } else {
        (*it) = MakeKeyValue(content);
      }
    }
  }

  void AddLine(const std::string &content)
  {
    sdp_lines_.push_back(MakeKeyValue(content));
  }

  static std::pair<std::string, std::string> MakeKeyValue(
      const std::string &content)
  {
    size_t whiteSpace = content.find(' ');
    std::string key;
    std::string value;
    if (whiteSpace == std::string::npos) {
      //this is the line with no extra contents
      //example, v=0, a=sendrecv
      key = content.substr(0,  content.size() - 2);
      value = "\r\n"; // Checking code assumes this is here.
    } else {
      key = content.substr(0, whiteSpace);
      value = content.substr(whiteSpace+1);
    }
    return std::make_pair(key, value);
  }

  std::list<std::pair<std::string, std::string>>::iterator FindLine(
      const std::string& objType,
      size_t index = 0)
  {
    for (auto it = sdp_lines_.begin(); it != sdp_lines_.end(); ++it) {
      if (it->first == objType) {
        if (index == 0) {
          return it;
        }
        --index;
      }
    }
    return sdp_lines_.end();
  }

  void InsertLineAfter(const std::string &objType,
                       const std::string &content,
                       size_t index = 0)
  {
    auto it = FindLine(objType, index);
    if (it != sdp_lines_.end()) {
      sdp_lines_.insert(++it, MakeKeyValue(content));
    }
  }

  // Returns the values for all lines of the indicated type
  // Removes trailing "\r\n" from values.
  std::vector<std::string> GetLines(std::string objType) const
  {
    std::vector<std::string> values;
    for (auto it = sdp_lines_.begin(); it != sdp_lines_.end(); ++it) {
      if (it->first == objType) {
        std::string value = it->second;
        if (value.find("\r") != std::string::npos) {
          value = value.substr(0, value.find("\r"));
        } else {
          ADD_FAILURE() << "SDP line had no endline; this should never happen.";
        }
        values.push_back(value);
      }
    }
    return values;
  }

  //Parse SDP as std::string into map that looks like:
  // key: sdp content till first space
  // value: sdp content after the first space, _including_ \r\n
  void Parse(const std::string &sdp)
  {
    size_t prev = 0;
    size_t found = 0;
    for(;;) {
      found = sdp.find('\n', found + 1);
      if (found == std::string::npos)
        break;
      std::string line = sdp.substr(prev, (found - prev) + 1);
      sdp_lines_.push_back(MakeKeyValue(line));

      prev = found + 1;
    }
  }

  //Convert Internal SDP representation into String representation
  std::string getSdp() const
  {
    std::string sdp;

    for (auto it = sdp_lines_.begin(); it != sdp_lines_.end(); ++it) {
      sdp += it->first;
      if (it->second != "\r\n") {
        sdp += " ";
      }
      sdp += it->second;
    }

    return sdp;
  }

  void IncorporateCandidate(uint16_t level, const std::string &candidate)
  {
    std::string candidate_attribute("a=" + candidate + "\r\n");
    // InsertLineAfter is 0 indexed, but level is 1 indexed
    // This assumes that we have only media-level c lines.
    InsertLineAfter("c=IN", candidate_attribute, level - 1);
  }

  std::list<std::pair<std::string, std::string>> sdp_lines_;
};


// This class wraps the PeerConnection object and ensures that all calls
// into it happen on the main thread.
class PCDispatchWrapper : public nsSupportsWeakReference
{
 protected:
  virtual ~PCDispatchWrapper() {}

 public:
  explicit PCDispatchWrapper(const nsRefPtr<PeerConnectionImpl>& peerConnection)
    : pc_(peerConnection) {}

  NS_DECL_THREADSAFE_ISUPPORTS

  PeerConnectionImpl *pcImpl() const {
    return pc_;
  }

  const nsRefPtr<PeerConnectionMedia>& media() const {
    return pc_->media();
  }

  NS_IMETHODIMP Initialize(TestObserver* aObserver,
                      nsGlobalWindow* aWindow,
                      const IceConfiguration& aConfiguration,
                      nsIThread* aThread) {
    nsresult rv;

    observer_ = aObserver;

    if (NS_IsMainThread()) {
      rv = pc_->Initialize(*aObserver, aWindow, aConfiguration, aThread);
    } else {
      // It would have been preferable here to dispatch directly to
      // PeerConnectionImpl::Initialize but since all the PC methods
      // have overrides clang will throw a 'couldn't infer template
      // argument' error.
      // Instead we are dispatching back to the same method for
      // all of these.
      gMainThread->Dispatch(
        WrapRunnableRet(&rv, this, &PCDispatchWrapper::Initialize,
          aObserver, aWindow, aConfiguration, aThread),
        NS_DISPATCH_SYNC);
      rv = NS_OK;
    }

    return rv;
  }

  NS_IMETHODIMP CreateOffer(const mozilla::JsepOfferOptions& aOptions) {
    nsresult rv;

    if (NS_IsMainThread()) {
      rv = pc_->CreateOffer(aOptions);
      EXPECT_EQ(NS_OK, rv);
      if (NS_FAILED(rv))
        return rv;
      EXPECT_EQ(TestObserver::stateSuccess, observer_->state);
      if (observer_->state != TestObserver::stateSuccess) {
        return NS_ERROR_FAILURE;
      }
    } else {
      gMainThread->Dispatch(
        WrapRunnableRet(&rv, this, &PCDispatchWrapper::CreateOffer, aOptions),
        NS_DISPATCH_SYNC);
    }

    return rv;
  }

  NS_IMETHODIMP CreateAnswer() {
    nsresult rv;

    if (NS_IsMainThread()) {
      rv = pc_->CreateAnswer();
    } else {
      gMainThread->Dispatch(
        WrapRunnableRet(&rv, this, &PCDispatchWrapper::CreateAnswer),
        NS_DISPATCH_SYNC);
    }

    return rv;
  }

  NS_IMETHODIMP SetLocalDescription (int32_t aAction, const char* aSDP) {
    nsresult rv;

    if (NS_IsMainThread()) {
      rv = pc_->SetLocalDescription(aAction, aSDP);
    } else {
      gMainThread->Dispatch(
        WrapRunnableRet(&rv, this, &PCDispatchWrapper::SetLocalDescription,
          aAction, aSDP),
        NS_DISPATCH_SYNC);
    }

    return rv;
  }

  NS_IMETHODIMP SetRemoteDescription (int32_t aAction, const char* aSDP) {
    nsresult rv;

    if (NS_IsMainThread()) {
      rv = pc_->SetRemoteDescription(aAction, aSDP);
    } else {
      gMainThread->Dispatch(
        WrapRunnableRet(&rv, this, &PCDispatchWrapper::SetRemoteDescription,
          aAction, aSDP),
        NS_DISPATCH_SYNC);
    }

    return rv;
  }

  NS_IMETHODIMP AddIceCandidate(const char* aCandidate, const char* aMid,
                                unsigned short aLevel) {
    nsresult rv;

    if (NS_IsMainThread()) {
      rv = pc_->AddIceCandidate(aCandidate, aMid, aLevel);
    } else {
      gMainThread->Dispatch(
        WrapRunnableRet(&rv, this, &PCDispatchWrapper::AddIceCandidate,
          aCandidate, aMid, aLevel),
        NS_DISPATCH_SYNC);
    }
    return rv;
  }

  NS_IMETHODIMP AddTrack(MediaStreamTrack *aTrack,
                         DOMMediaStream *aMediaStream)
  {
    nsresult rv;

    if (NS_IsMainThread()) {
      rv = pc_->AddTrack(*aTrack, *aMediaStream);
    } else {
      gMainThread->Dispatch(
        WrapRunnableRet(&rv, this, &PCDispatchWrapper::AddTrack, aTrack,
                        aMediaStream),
        NS_DISPATCH_SYNC);
    }

    return rv;
  }

  NS_IMETHODIMP RemoveTrack(MediaStreamTrack *aTrack) {
    nsresult rv;

    if (NS_IsMainThread()) {
      rv = pc_->RemoveTrack(*aTrack);
    } else {
      gMainThread->Dispatch(
        WrapRunnableRet(&rv, this, &PCDispatchWrapper::RemoveTrack, aTrack),
        NS_DISPATCH_SYNC);
    }

    return rv;
  }

  NS_IMETHODIMP GetLocalDescription(char** aSDP) {
    nsresult rv;

    if (NS_IsMainThread()) {
      rv = pc_->GetLocalDescription(aSDP);
    } else {
      gMainThread->Dispatch(
        WrapRunnableRet(&rv, this, &PCDispatchWrapper::GetLocalDescription,
          aSDP),
        NS_DISPATCH_SYNC);
    }

    return rv;
  }

  NS_IMETHODIMP GetRemoteDescription(char** aSDP) {
    nsresult rv;

    if (NS_IsMainThread()) {
      rv = pc_->GetRemoteDescription(aSDP);
    } else {
      gMainThread->Dispatch(
        WrapRunnableRet(&rv, this, &PCDispatchWrapper::GetRemoteDescription,
          aSDP),
        NS_DISPATCH_SYNC);
    }

    return rv;
  }

  mozilla::dom::PCImplSignalingState SignalingState() {
    mozilla::dom::PCImplSignalingState result;

    if (NS_IsMainThread()) {
      result = pc_->SignalingState();
    } else {
      gMainThread->Dispatch(
        WrapRunnableRet(&result, this, &PCDispatchWrapper::SignalingState),
        NS_DISPATCH_SYNC);
    }

    return result;
  }

  mozilla::dom::PCImplIceConnectionState IceConnectionState() {
    mozilla::dom::PCImplIceConnectionState result;

    if (NS_IsMainThread()) {
      result = pc_->IceConnectionState();
    } else {
      gMainThread->Dispatch(
        WrapRunnableRet(&result, this, &PCDispatchWrapper::IceConnectionState),
        NS_DISPATCH_SYNC);
    }

    return result;
  }

  mozilla::dom::PCImplIceGatheringState IceGatheringState() {
    mozilla::dom::PCImplIceGatheringState result;

    if (NS_IsMainThread()) {
      result = pc_->IceGatheringState();
    } else {
      gMainThread->Dispatch(
        WrapRunnableRet(&result, this, &PCDispatchWrapper::IceGatheringState),
        NS_DISPATCH_SYNC);
    }

    return result;
  }

  NS_IMETHODIMP Close() {
    nsresult rv;

    if (NS_IsMainThread()) {
      rv = pc_->Close();
    } else {
      gMainThread->Dispatch(
        WrapRunnableRet(&rv, this, &PCDispatchWrapper::Close),
        NS_DISPATCH_SYNC);
    }

    return rv;
  }

 private:
  nsRefPtr<PeerConnectionImpl> pc_;
  nsRefPtr<TestObserver> observer_;
};

NS_IMPL_ISUPPORTS(PCDispatchWrapper, nsISupportsWeakReference)


struct Msid
{
  std::string streamId;
  std::string trackId;
  bool operator<(const Msid& other) const {
    if (streamId < other.streamId) {
      return true;
    }

    if (streamId > other.streamId) {
      return false;
    }

    return trackId < other.trackId;
  }
};

class SignalingAgent {
 public:
  explicit SignalingAgent(const std::string &aName,
    const std::string stun_addr = g_stun_server_address,
    uint16_t stun_port = g_stun_server_port) :
    pc(nullptr),
    name(aName),
    mBundleEnabled(true),
    mExpectedFrameRequestType(VideoSessionConduit::FrameRequestPli),
    mExpectNack(true),
    mExpectRtcpMuxAudio(true),
    mExpectRtcpMuxVideo(true),
    mRemoteDescriptionSet(false) {
    cfg_.addStunServer(stun_addr, stun_port, kNrIceTransportUdp);
    cfg_.addStunServer(stun_addr, stun_port, kNrIceTransportTcp);

    PeerConnectionImpl *pcImpl =
      PeerConnectionImpl::CreatePeerConnection();
    EXPECT_TRUE(pcImpl);
    pcImpl->SetAllowIceLoopback(true);
    pcImpl->SetAllowIceLinkLocal(true);
    pc = new PCDispatchWrapper(pcImpl);
  }


  ~SignalingAgent() {
    mozilla::SyncRunnable::DispatchToThread(gMainThread,
      WrapRunnable(this, &SignalingAgent::Close));
  }

  void Init_m()
  {
    pObserver = new TestObserver(pc->pcImpl(), name);
    ASSERT_TRUE(pObserver);

    ASSERT_EQ(pc->Initialize(pObserver, nullptr, cfg_, gMainThread), NS_OK);
  }

  void Init()
  {
    mozilla::SyncRunnable::DispatchToThread(gMainThread,
      WrapRunnable(this, &SignalingAgent::Init_m));
  }

  void SetBundleEnabled(bool enabled)
  {
    mBundleEnabled = enabled;
  }

  void SetExpectedFrameRequestType(VideoSessionConduit::FrameRequestType type)
  {
    mExpectedFrameRequestType = type;
  }

  void WaitForGather() {
    ASSERT_TRUE_WAIT(ice_gathering_state() == PCImplIceGatheringState::Complete,
                     kDefaultTimeout);

    std::cout << name << ": Init Complete" << std::endl;

    // Check that the default candidate has been filled out with something
    std::string localSdp = getLocalDescription();

    std::cout << "Local SDP after gather: " << localSdp;
    ASSERT_EQ(std::string::npos, localSdp.find("c=IN IP4 0.0.0.0"));
    ASSERT_EQ(std::string::npos, localSdp.find("m=video 9 "));
    ASSERT_EQ(std::string::npos, localSdp.find("m=audio 9 "));

    // TODO(bug 1098584): Check for end-of-candidates attr
  }

  bool WaitForGatherAllowFail() {
    EXPECT_TRUE_WAIT(
        ice_gathering_state() == PCImplIceGatheringState::Complete ||
        ice_connection_state() == PCImplIceConnectionState::Failed,
        kDefaultTimeout);

    if (ice_connection_state() == PCImplIceConnectionState::Failed) {
      std::cout << name << ": Init Failed" << std::endl;
      return false;
    }

    std::cout << name << "Init Complete" << std::endl;
    return true;
  }

  void DropOutgoingTrickleCandidates() {
    pObserver->trickleCandidates = false;
  }

  PCImplIceConnectionState ice_connection_state()
  {
    return pc->IceConnectionState();
  }

  PCImplIceGatheringState ice_gathering_state()
  {
    return pc->IceGatheringState();
  }

  PCImplSignalingState signaling_state()
  {
    return pc->SignalingState();
  }

  void Close()
  {
    std::cout << name << ": Close" << std::endl;

    pc->Close();
    pc = nullptr;
    pObserver = nullptr;
  }

  bool OfferContains(const std::string& str) {
    return offer().find(str) != std::string::npos;
  }

  bool AnswerContains(const std::string& str) {
    return answer().find(str) != std::string::npos;
  }

  size_t MatchingCandidates(const std::string& cand) {
    return pObserver->MatchingCandidates(cand);
  }

  const std::string& offer() const { return offer_; }
  const std::string& answer() const { return answer_; }

  std::string getLocalDescription() const {
    char *sdp = nullptr;
    pc->GetLocalDescription(&sdp);
    if (!sdp) {
      return "";
    }
    std::string result(sdp);
    delete sdp;
    return result;
  }

  std::string getRemoteDescription() const {
    char *sdp = 0;
    pc->GetRemoteDescription(&sdp);
    if (!sdp) {
      return "";
    }
    std::string result(sdp);
    delete sdp;
    return result;
  }

  std::string RemoveBundle(const std::string& sdp) const {
    ParsedSDP parsed(sdp);
    parsed.DeleteLines("a=group:BUNDLE");
    return parsed.getSdp();
  }

  // Adds a stream to the PeerConnection.
  void AddStream(uint32_t hint =
         DOMMediaStream::HINT_CONTENTS_AUDIO |
         DOMMediaStream::HINT_CONTENTS_VIDEO,
       MediaStream *stream = nullptr) {

    if (!stream && (hint & DOMMediaStream::HINT_CONTENTS_AUDIO)) {
      // Useful default
      // Create a media stream as if it came from GUM
      Fake_AudioStreamSource *audio_stream =
        new Fake_AudioStreamSource();

      nsresult ret;
      mozilla::SyncRunnable::DispatchToThread(
        test_utils->sts_target(),
        WrapRunnableRet(&ret, audio_stream, &Fake_MediaStream::Start));

      ASSERT_TRUE(NS_SUCCEEDED(ret));
      stream = audio_stream;
    }

    nsRefPtr<DOMMediaStream> domMediaStream = new DOMMediaStream(stream);
    domMediaStream->SetHintContents(hint);

    nsTArray<nsRefPtr<MediaStreamTrack>> tracks;
    domMediaStream->GetTracks(tracks);
    for (uint32_t i = 0; i < tracks.Length(); i++) {
      Msid msid = {domMediaStream->GetId(), tracks[i]->GetId()};

      ASSERT_FALSE(mAddedTracks.count(msid))
        << msid.streamId << "/" << msid.trackId << " already added";

      mAddedTracks[msid] = (tracks[i]->AsVideoStreamTrack() ?
                            SdpMediaSection::kVideo :
                            SdpMediaSection::kAudio);

      ASSERT_EQ(pc->AddTrack(tracks[i], domMediaStream), NS_OK);
    }
    domMediaStreams_.push_back(domMediaStream);
  }

  // I would love to make this an overload of operator<<, but there's no way to
  // declare it in a way that works with gtest's header files.
  std::string DumpTracks(
      const std::map<Msid, SdpMediaSection::MediaType>& tracks) const
  {
    std::ostringstream oss;
    for (auto it = tracks.begin(); it != tracks.end(); ++it) {
      oss << it->first.streamId << "/" << it->first.trackId
          << " (" << it->second << ")" << std::endl;
    }

    return oss.str();
  }

  void ExpectMissingTracks(SdpMediaSection::MediaType type)
  {
    for (auto it = mAddedTracks.begin(); it != mAddedTracks.end();) {
      if (it->second == type) {
        auto temp = it;
        ++it;
        mAddedTracks.erase(temp);
      } else {
        ++it;
      }
    }
  }

  void CheckLocalPipeline(const std::string& streamId,
                          const std::string& trackId,
                          SdpMediaSection::MediaType type,
                          int pipelineCheckFlags = 0) const
  {
    LocalSourceStreamInfo* info;
    mozilla::SyncRunnable::DispatchToThread(
      gMainThread, WrapRunnableRet(&info,
        pc->media(), &PeerConnectionMedia::GetLocalStreamById,
        streamId));

    ASSERT_TRUE(info) << "No such local stream id: " << streamId;

    RefPtr<MediaPipeline> pipeline;

    mozilla::SyncRunnable::DispatchToThread(
        gMainThread,
        WrapRunnableRet(&pipeline, info,
                        &SourceStreamInfo::GetPipelineByTrackId_m,
                        trackId));

    ASSERT_TRUE(pipeline) << "No such local track id: " << trackId;

    if (type == SdpMediaSection::kVideo) {
      ASSERT_TRUE(pipeline->IsVideo()) << "Local track " << trackId
                                       << " was not video";
      ASSERT_EQ(mExpectRtcpMuxVideo, pipeline->IsDoingRtcpMux())
        << "Pipeline for remote track " << trackId
        << " is" << (mExpectRtcpMuxVideo ? " not " : " ") << "using rtcp-mux";
      // No checking for video RTP yet, since we don't have support for fake
      // video here yet. (bug 1142320)
    } else {
      ASSERT_FALSE(pipeline->IsVideo()) << "Local track " << trackId
                                        << " was not audio";
      WAIT(pipeline->rtp_packets_sent() >= 4 &&
           pipeline->rtcp_packets_received() >= 1,
           kDefaultTimeout);
      ASSERT_LE(4, pipeline->rtp_packets_sent())
        << "Local track " << trackId << " isn't sending RTP";
      ASSERT_LE(1, pipeline->rtcp_packets_received())
        << "Local track " << trackId << " isn't receiving RTCP";
      ASSERT_EQ(mExpectRtcpMuxAudio, pipeline->IsDoingRtcpMux())
        << "Pipeline for remote track " << trackId
        << " is" << (mExpectRtcpMuxAudio ? " not " : " ") << "using rtcp-mux";
    }
  }

  void CheckRemotePipeline(const std::string& streamId,
                           const std::string& trackId,
                           SdpMediaSection::MediaType type,
                           int pipelineCheckFlags = 0) const
  {
    RemoteSourceStreamInfo* info;
    mozilla::SyncRunnable::DispatchToThread(
      gMainThread, WrapRunnableRet(&info,
        pc->media(), &PeerConnectionMedia::GetRemoteStreamById,
        streamId));

    ASSERT_TRUE(info) << "No such remote stream id: " << streamId;

    RefPtr<MediaPipeline> pipeline;

    mozilla::SyncRunnable::DispatchToThread(
        gMainThread,
        WrapRunnableRet(&pipeline, info,
                        &SourceStreamInfo::GetPipelineByTrackId_m,
                        trackId));

    ASSERT_TRUE(pipeline) << "No such remote track id: " << trackId;

    if (type == SdpMediaSection::kVideo) {
      ASSERT_TRUE(pipeline->IsVideo()) << "Remote track " << trackId
                                       << " was not video";
      mozilla::MediaSessionConduit *conduit = pipeline->Conduit();
      ASSERT_TRUE(conduit);
      ASSERT_EQ(conduit->type(), mozilla::MediaSessionConduit::VIDEO);
      mozilla::VideoSessionConduit *video_conduit =
        static_cast<mozilla::VideoSessionConduit*>(conduit);
      ASSERT_EQ(mExpectNack, video_conduit->UsingNackBasic());
      ASSERT_EQ(mExpectedFrameRequestType,
                video_conduit->FrameRequestMethod());
      ASSERT_EQ(mExpectRtcpMuxVideo, pipeline->IsDoingRtcpMux())
        << "Pipeline for remote track " << trackId
        << " is" << (mExpectRtcpMuxVideo ? " not " : " ") << "using rtcp-mux";
      // No checking for video RTP yet, since we don't have support for fake
      // video here yet. (bug 1142320)
    } else {
      ASSERT_FALSE(pipeline->IsVideo()) << "Remote track " << trackId
                                        << " was not audio";
      WAIT(pipeline->rtp_packets_received() >= 4 &&
           pipeline->rtcp_packets_sent() >= 1,
           kDefaultTimeout);
      ASSERT_LE(4, pipeline->rtp_packets_received())
        << "Remote track " << trackId << " isn't receiving RTP";
      ASSERT_LE(1, pipeline->rtcp_packets_sent())
        << "Remote track " << trackId << " isn't sending RTCP";
      ASSERT_EQ(mExpectRtcpMuxAudio, pipeline->IsDoingRtcpMux())
        << "Pipeline for remote track " << trackId
        << " is" << (mExpectRtcpMuxAudio ? " not " : " ") << "using rtcp-mux";
    }
  }

  void RemoveTrack(size_t streamIndex, bool videoTrack = false)
  {
    ASSERT_LT(streamIndex, domMediaStreams_.size());
    nsTArray<nsRefPtr<MediaStreamTrack>> tracks;
    domMediaStreams_[streamIndex]->GetTracks(tracks);
    for (size_t i = 0; i < tracks.Length(); ++i) {
      if (!!tracks[i]->AsVideoStreamTrack() == videoTrack) {
        Msid msid;
        msid.streamId = domMediaStreams_[streamIndex]->GetId();
        msid.trackId = tracks[i]->GetId();
        mAddedTracks.erase(msid);
        ASSERT_EQ(pc->RemoveTrack(tracks[i]), NS_OK);
      }
    }
  }

  void RemoveStream(size_t index) {
    nsTArray<nsRefPtr<MediaStreamTrack>> tracks;
    domMediaStreams_[index]->GetTracks(tracks);
    for (uint32_t i = 0; i < tracks.Length(); i++) {
      ASSERT_EQ(pc->RemoveTrack(tracks[i]), NS_OK);
    }
    domMediaStreams_.erase(domMediaStreams_.begin() + index);
  }

  // Removes the stream that was most recently added to the PeerConnection.
  void RemoveLastStreamAdded() {
    ASSERT_FALSE(domMediaStreams_.empty());
    RemoveStream(domMediaStreams_.size() - 1);
  }

  void CreateOffer(OfferOptions& options,
                   uint32_t offerFlags,
                   PCImplSignalingState endState =
                     PCImplSignalingState::SignalingStable) {

    uint32_t aHintContents = 0;
    if (offerFlags & OFFER_AUDIO) {
      aHintContents |= DOMMediaStream::HINT_CONTENTS_AUDIO;
    }
    if (offerFlags & OFFER_VIDEO) {
      aHintContents |= DOMMediaStream::HINT_CONTENTS_VIDEO;
    }
    AddStream(aHintContents);

    // Now call CreateOffer as JS would
    pObserver->state = TestObserver::stateNoResponse;
    ASSERT_EQ(pc->CreateOffer(options), NS_OK);

    ASSERT_EQ(pObserver->state, TestObserver::stateSuccess);
    ASSERT_EQ(signaling_state(), endState);
    offer_ = pObserver->lastString;
    if (!mBundleEnabled) {
      offer_ = RemoveBundle(offer_);
    }
  }

  // sets the offer to match the local description
  // which isn't good if you are the answerer
  void UpdateOffer() {
    offer_ = getLocalDescription();
    if (!mBundleEnabled) {
      offer_ = RemoveBundle(offer_);
    }
  }

  void CreateAnswer(uint32_t offerAnswerFlags,
                    PCImplSignalingState endState =
                    PCImplSignalingState::SignalingHaveRemoteOffer) {
    // Create a media stream as if it came from GUM
    Fake_AudioStreamSource *audio_stream =
      new Fake_AudioStreamSource();

    nsresult ret;
    mozilla::SyncRunnable::DispatchToThread(
      test_utils->sts_target(),
      WrapRunnableRet(&ret, audio_stream, &Fake_MediaStream::Start));

    ASSERT_TRUE(NS_SUCCEEDED(ret));

    uint32_t aHintContents = 0;
    if (offerAnswerFlags & ANSWER_AUDIO) {
      aHintContents |= DOMMediaStream::HINT_CONTENTS_AUDIO;
    }
    if (offerAnswerFlags & ANSWER_VIDEO) {
      aHintContents |= DOMMediaStream::HINT_CONTENTS_VIDEO;
    }
    AddStream(aHintContents, audio_stream);

    // Decide if streams are disabled for offer or answer
    // then perform SDP checking based on which stream disabled
    pObserver->state = TestObserver::stateNoResponse;
    ASSERT_EQ(pc->CreateAnswer(), NS_OK);
    ASSERT_EQ(pObserver->state, TestObserver::stateSuccess);
    ASSERT_EQ(signaling_state(), endState);

    answer_ = pObserver->lastString;
    if (!mBundleEnabled) {
      answer_ = RemoveBundle(answer_);
    }
  }

  // sets the answer to match the local description
  // which isn't good if you are the offerer
  void UpdateAnswer() {
    answer_ = getLocalDescription();
    if (!mBundleEnabled) {
      answer_ = RemoveBundle(answer_);
    }
  }

  void CreateOfferRemoveTrack(OfferOptions& options, bool videoTrack) {

    RemoveTrack(0, videoTrack);

    // Now call CreateOffer as JS would
    pObserver->state = TestObserver::stateNoResponse;
    ASSERT_EQ(pc->CreateOffer(options), NS_OK);
    ASSERT_TRUE(pObserver->state == TestObserver::stateSuccess);
    offer_ = pObserver->lastString;
    if (!mBundleEnabled) {
      offer_ = RemoveBundle(offer_);
    }
  }

  void SetRemote(TestObserver::Action action, const std::string& remote,
                 bool ignoreError = false,
                 PCImplSignalingState endState =
                 PCImplSignalingState::SignalingInvalid) {

    if (endState == PCImplSignalingState::SignalingInvalid) {
      endState = (action == TestObserver::OFFER ?
                  PCImplSignalingState::SignalingHaveRemoteOffer :
                  PCImplSignalingState::SignalingStable);
    }

    pObserver->state = TestObserver::stateNoResponse;
    ASSERT_EQ(pc->SetRemoteDescription(action, remote.c_str()), NS_OK);
    ASSERT_EQ(signaling_state(), endState);
    if (!ignoreError) {
      ASSERT_EQ(pObserver->state, TestObserver::stateSuccess);
    }

    mRemoteDescriptionSet = true;
    for (auto i = deferredCandidates_.begin();
         i != deferredCandidates_.end();
         ++i) {
      AddIceCandidate(i->candidate.c_str(),
                      i->mid.c_str(),
                      i->level,
                      i->expectSuccess);
    }
    deferredCandidates_.clear();
  }

  void SetLocal(TestObserver::Action action, const std::string& local,
                bool ignoreError = false,
                PCImplSignalingState endState =
                PCImplSignalingState::SignalingInvalid) {

    if (endState == PCImplSignalingState::SignalingInvalid) {
      endState = (action == TestObserver::OFFER ?
                  PCImplSignalingState::SignalingHaveLocalOffer :
                  PCImplSignalingState::SignalingStable);
    }

    pObserver->state = TestObserver::stateNoResponse;
    ASSERT_EQ(pc->SetLocalDescription(action, local.c_str()), NS_OK);
    ASSERT_EQ(signaling_state(), endState);
    if (!ignoreError) {
      ASSERT_EQ(pObserver->state, TestObserver::stateSuccess);
    }
  }

  typedef enum {
    NORMAL_ENCODING,
    CHROME_ENCODING
  } TrickleEncoding;

  bool IceCompleted() {
    return pc->IceConnectionState() == PCImplIceConnectionState::Connected;
  }

  void AddIceCandidateStr(const std::string& candidate, const std::string& mid,
                          unsigned short level) {
    if (!mRemoteDescriptionSet) {
      // Not time to add this, because the unit-test code hasn't set the
      // description yet.
      DeferredCandidate candidateStruct = {candidate, mid, level, true};
      deferredCandidates_.push_back(candidateStruct);
    } else {
      AddIceCandidate(candidate, mid, level, true);
    }
  }

  void AddIceCandidate(const std::string& candidate, const std::string& mid, unsigned short level,
                       bool expectSuccess) {
    PCImplSignalingState endState = signaling_state();
    pObserver->addIceCandidateState = TestObserver::stateNoResponse;
    pc->AddIceCandidate(candidate.c_str(), mid.c_str(), level);
    ASSERT_TRUE(pObserver->addIceCandidateState ==
                expectSuccess ? TestObserver::stateSuccess :
                                TestObserver::stateError
               );

    // Verify that adding ICE candidates does not change the signaling state
    ASSERT_EQ(signaling_state(), endState);
  }

  int GetPacketsReceived(const std::string& streamId) const
  {
    std::vector<DOMMediaStream *> streams = pObserver->GetStreams();

    for (size_t i = 0; i < streams.size(); ++i) {
      if (streams[i]->GetId() == streamId) {
        return GetPacketsReceived(i);
      }
    }

    EXPECT_TRUE(false);
    return 0;
  }

  int GetPacketsReceived(size_t stream) const {
    std::vector<DOMMediaStream *> streams = pObserver->GetStreams();

    if (streams.size() <= stream) {
      EXPECT_TRUE(false);
      return 0;
    }

    return streams[stream]->GetStream()->AsSourceStream()->GetSegmentsAdded();
  }

  int GetPacketsSent(const std::string& streamId) const
  {
    for (size_t i = 0; i < domMediaStreams_.size(); ++i) {
      if (domMediaStreams_[i]->GetId() == streamId) {
        return GetPacketsSent(i);
      }
    }

    EXPECT_TRUE(false);
    return 0;
  }

  int GetPacketsSent(size_t stream) const {
    if (stream >= domMediaStreams_.size()) {
      EXPECT_TRUE(false);
      return 0;
    }

    return static_cast<Fake_MediaStreamBase *>(
        domMediaStreams_[stream]->GetStream())->GetSegmentsAdded();
  }

  //Stops generating new audio data for transmission.
  //Should be called before Cleanup of the peer connection.
  void CloseSendStreams() {
    for (auto i = domMediaStreams_.begin(); i != domMediaStreams_.end(); ++i) {
      static_cast<Fake_MediaStream*>((*i)->GetStream())->StopStream();
    }
  }

  //Stops pulling audio data off the receivers.
  //Should be called before Cleanup of the peer connection.
  void CloseReceiveStreams() {
    std::vector<DOMMediaStream *> streams =
                            pObserver->GetStreams();
    for (size_t i = 0; i < streams.size(); i++) {
      streams[i]->GetStream()->AsSourceStream()->StopStream();
    }
  }

  // Right now we have no convenient way for this unit-test to learn the track
  // ids of the tracks, so they can be queried later. We could either expose
  // the JsepSessionImpl in some way, or we could parse the identifiers out of
  // the SDP. For now, we just specify audio/video, since a given DOMMediaStream
  // can have only one of each anyway. Once this is fixed, we will need to
  // pass a real track id if we want to test that case.
  mozilla::RefPtr<mozilla::MediaPipeline> GetMediaPipeline(
    bool local, size_t stream, bool video) {
    SourceStreamInfo* streamInfo;
    if (local) {
      mozilla::SyncRunnable::DispatchToThread(
        gMainThread, WrapRunnableRet(&streamInfo,
          pc->media(), &PeerConnectionMedia::GetLocalStreamByIndex,
          stream));
    } else {
      mozilla::SyncRunnable::DispatchToThread(
        gMainThread, WrapRunnableRet(&streamInfo,
          pc->media(), &PeerConnectionMedia::GetRemoteStreamByIndex,
          stream));
    }

    if (!streamInfo) {
      return nullptr;
    }

    const auto &pipelines = streamInfo->GetPipelines();

    for (auto i = pipelines.begin(); i != pipelines.end(); ++i) {
      if (i->second->IsVideo() == video) {
        std::cout << "Got MediaPipeline " << i->second->trackid();
        return i->second;
      }
    }
    return nullptr;
  }

  void SetPeer(SignalingAgent* peer) {
    pObserver->peerAgent = peer;
  }

public:
  nsRefPtr<PCDispatchWrapper> pc;
  nsRefPtr<TestObserver> pObserver;
  std::string offer_;
  std::string answer_;
  std::vector<nsRefPtr<DOMMediaStream>> domMediaStreams_;
  IceConfiguration cfg_;
  const std::string name;
  bool mBundleEnabled;
  VideoSessionConduit::FrameRequestType mExpectedFrameRequestType;
  bool mExpectNack;
  bool mExpectRtcpMuxAudio;
  bool mExpectRtcpMuxVideo;
  bool mRemoteDescriptionSet;

  std::map<Msid, SdpMediaSection::MediaType> mAddedTracks;

  typedef struct {
    std::string candidate;
    std::string mid;
    uint16_t level;
    bool expectSuccess;
  } DeferredCandidate;

  std::list<DeferredCandidate> deferredCandidates_;
};

static void AddIceCandidateToPeer(nsWeakPtr weak_observer,
                                  uint16_t level,
                                  const std::string &mid,
                                  const std::string &cand) {
  nsCOMPtr<nsISupportsWeakReference> tmp = do_QueryReferent(weak_observer);
  if (!tmp) {
    return;
  }

  nsRefPtr<nsSupportsWeakReference> tmp2 = do_QueryObject(tmp);
  nsRefPtr<TestObserver> observer = static_cast<TestObserver*>(&*tmp2);

  if (!observer) {
    return;
  }

  observer->candidates.push_back(cand);

  if (!observer->peerAgent || !observer->trickleCandidates) {
    return;
  }

  observer->peerAgent->AddIceCandidateStr(cand, mid, level);
}


NS_IMETHODIMP
TestObserver::OnIceCandidate(uint16_t level,
                             const char * mid,
                             const char * candidate, ER&)
{
  if (strlen(candidate) != 0) {
    std::cerr << name << ": got candidate: " << candidate << std::endl;
    // Forward back to myself to unwind stack.
    nsWeakPtr weak_this = do_GetWeakReference(this);
    gMainThread->Dispatch(
        WrapRunnableNM(
            &AddIceCandidateToPeer,
            weak_this,
            level,
            std::string(mid),
            std::string(candidate)),
        NS_DISPATCH_NORMAL);
  }
  return NS_OK;
}

NS_IMETHODIMP
TestObserver::OnNegotiationNeeded(ER&)
{
  return NS_OK;
}

class SignalingEnvironment : public ::testing::Environment {
 public:
  void TearDown() {
    // Signaling is shut down in XPCOM shutdown
  }
};

class SignalingAgentTest : public ::testing::Test {
 public:
  static void SetUpTestCase() {
  }

  void TearDown() {
    // Delete all the agents.
    for (size_t i=0; i < agents_.size(); i++) {
      delete agents_[i];
    }
  }

  bool CreateAgent() {
    return CreateAgent(g_stun_server_address, g_stun_server_port);
  }

  bool CreateAgent(const std::string stun_addr, uint16_t stun_port) {
    ScopedDeletePtr<SignalingAgent> agent(
        new SignalingAgent("agent", stun_addr, stun_port));

    agent->Init();

    agents_.push_back(agent.forget());

    return true;
  }

  void CreateAgentNoInit() {
    ScopedDeletePtr<SignalingAgent> agent(new SignalingAgent("agent"));
    agents_.push_back(agent.forget());
  }

  SignalingAgent *agent(size_t i) {
    return agents_[i];
  }

 private:
  std::vector<SignalingAgent *> agents_;
};


class SignalingTest : public ::testing::Test,
                      public ::testing::WithParamInterface<std::string>
{
public:
  SignalingTest()
      : init_(false),
        a1_(nullptr),
        a2_(nullptr),
        stun_addr_(g_stun_server_address),
        stun_port_(g_stun_server_port) {}

  SignalingTest(const std::string& stun_addr, uint16_t stun_port)
      : a1_(nullptr),
        a2_(nullptr),
        stun_addr_(stun_addr),
        stun_port_(stun_port) {}

  ~SignalingTest() {
    if (init_) {
      mozilla::SyncRunnable::DispatchToThread(gMainThread,
        WrapRunnable(this, &SignalingTest::Teardown_m));
    }
  }

  void Teardown_m() {
    a1_->SetPeer(nullptr);
    a2_->SetPeer(nullptr);
  }

  static void SetUpTestCase() {
  }

  void EnsureInit() {

    if (init_)
      return;

    a1_ = new SignalingAgent(callerName, stun_addr_, stun_port_);
    a2_ = new SignalingAgent(calleeName, stun_addr_, stun_port_);
    a1_->Init();
    a2_->Init();
    if (GetParam() == "no_bundle") {
      a1_->SetBundleEnabled(false);
    } else if(GetParam() == "reject_bundle") {
      a2_->SetBundleEnabled(false);
    }

    a1_->SetPeer(a2_.get());
    a2_->SetPeer(a1_.get());

    init_ = true;
  }

  void WaitForGather() {
    a1_->WaitForGather();
    a2_->WaitForGather();
  }

  static void TearDownTestCase() {
  }

  void CreateOffer(OfferOptions& options, uint32_t offerFlags) {
    EnsureInit();
    a1_->CreateOffer(options, offerFlags);
  }

  void CreateSetOffer(OfferOptions& options) {
    EnsureInit();
    a1_->CreateOffer(options, OFFER_AV);
    a1_->SetLocal(TestObserver::OFFER, a1_->offer());
  }

  // Home for checks that we cannot perform by inspecting the various signaling
  // classes. We should endeavor to make this function disappear, since SDP
  // checking does not belong in these tests. That's the job of
  // jsep_session_unittest.
  void SDPSanityCheck(const std::string& sdp, uint32_t flags, bool offer)
  {
    std::cout << "SDPSanityCheck flags for "
              << (offer ? "offer" : "answer")
              << " = " << std::hex << std::showbase
              << flags << std::dec
              << ((flags & HAS_ALL_CANDIDATES)?" HAS_ALL_CANDIDATES":"")
              << std::endl;

    if (flags & HAS_ALL_CANDIDATES) {
      ASSERT_NE(std::string::npos, sdp.find("a=candidate"))
                << "should have at least one candidate";
      ASSERT_NE(std::string::npos, sdp.find("a=end-of-candidates"));
      ASSERT_EQ(std::string::npos, sdp.find("c=IN IP4 0.0.0.0"));
    }
  }

  void CheckPipelines()
  {
    std::cout << "Checking pipelines..." << std::endl;
    for (auto it = a1_->mAddedTracks.begin();
         it != a1_->mAddedTracks.end();
         ++it) {
      a1_->CheckLocalPipeline(it->first.streamId, it->first.trackId, it->second);
      a2_->CheckRemotePipeline(it->first.streamId, it->first.trackId, it->second);
    }

    for (auto it = a2_->mAddedTracks.begin();
         it != a2_->mAddedTracks.end();
         ++it) {
      a2_->CheckLocalPipeline(it->first.streamId, it->first.trackId, it->second);
      a1_->CheckRemotePipeline(it->first.streamId, it->first.trackId, it->second);
    }
    std::cout << "Done checking pipelines." << std::endl;
  }

  void CheckStreams(SignalingAgent& sender, SignalingAgent& receiver)
  {
    for (auto it = sender.mAddedTracks.begin();
         it != sender.mAddedTracks.end();
         ++it) {
      // No checking for video yet, since we don't have support for fake video
      // here yet. (bug 1142320)
      if (it->second == SdpMediaSection::kAudio) {
        int sendExpect = sender.GetPacketsSent(it->first.streamId) + 2;
        int receiveExpect = receiver.GetPacketsReceived(it->first.streamId) + 2;

        // TODO: Once we support more than one of each track type per stream,
        // this will need to be updated.
        WAIT(sender.GetPacketsSent(it->first.streamId) >= sendExpect &&
             receiver.GetPacketsReceived(it->first.streamId) >= receiveExpect,
             kDefaultTimeout);
        ASSERT_LE(sendExpect, sender.GetPacketsSent(it->first.streamId))
          << "Local track " << it->first.streamId << "/" << it->first.trackId
          << " is not sending audio segments.";
        ASSERT_LE(receiveExpect, receiver.GetPacketsReceived(it->first.streamId))
          << "Remote track " << it->first.streamId << "/" << it->first.trackId
          << " is not receiving audio segments.";
      }
    }
  }

  void CheckStreams()
  {
    std::cout << "Checking streams..." << std::endl;
    CheckStreams(*a1_, *a2_);
    CheckStreams(*a2_, *a1_);
    std::cout << "Done checking streams." << std::endl;
  }

  void Offer(OfferOptions& options,
             uint32_t offerAnswerFlags,
             TrickleType trickleType = BOTH_TRICKLE) {
    EnsureInit();
    a1_->CreateOffer(options, offerAnswerFlags);
    bool trickle = !!(trickleType & OFFERER_TRICKLES);
    if (!trickle) {
      a1_->pObserver->trickleCandidates = false;
    }
    a2_->mRemoteDescriptionSet = false;
    a1_->SetLocal(TestObserver::OFFER, a1_->offer());
    if (!trickle) {
      a1_->WaitForGather();
      a1_->UpdateOffer();
      SDPSanityCheck(a1_->getLocalDescription(), HAS_ALL_CANDIDATES, true);
    }
    a2_->SetRemote(TestObserver::OFFER, a1_->offer());
  }

  void Answer(OfferOptions& options,
              uint32_t offerAnswerFlags,
              TrickleType trickleType = BOTH_TRICKLE) {

    a2_->CreateAnswer(offerAnswerFlags);
    bool trickle = !!(trickleType & ANSWERER_TRICKLES);
    if (!trickle) {
      a2_->pObserver->trickleCandidates = false;
    }
    a1_->mRemoteDescriptionSet = false;
    a2_->SetLocal(TestObserver::ANSWER, a2_->answer());
    if (!trickle) {
      a2_->WaitForGather();
      a2_->UpdateAnswer();
      SDPSanityCheck(a2_->getLocalDescription(), HAS_ALL_CANDIDATES, false);
    }
    a1_->SetRemote(TestObserver::ANSWER, a2_->answer());
  }

  void WaitForCompleted() {
    ASSERT_TRUE_WAIT(a1_->IceCompleted() == true, kDefaultTimeout);
    ASSERT_TRUE_WAIT(a2_->IceCompleted() == true, kDefaultTimeout);
  }

  void OfferAnswer(OfferOptions& options,
                   uint32_t offerAnswerFlags,
                   TrickleType trickleType = BOTH_TRICKLE) {
    EnsureInit();
    Offer(options, offerAnswerFlags, trickleType);
    Answer(options, offerAnswerFlags, trickleType);
    WaitForCompleted();
    CheckPipelines();
    CheckStreams();
  }

  void OfferAnswerTrickleChrome(OfferOptions& options,
                                uint32_t offerAnswerFlags) {
    EnsureInit();
    Offer(options, offerAnswerFlags);
    Answer(options, offerAnswerFlags);
    WaitForCompleted();
    CheckPipelines();
    CheckStreams();
  }

  void CreateOfferRemoveTrack(OfferOptions& options, bool videoTrack) {
    EnsureInit();
    OfferOptions aoptions;
    aoptions.setInt32Option("OfferToReceiveAudio", 1);
    aoptions.setInt32Option("OfferToReceiveVideo", 1);
    a1_->CreateOffer(aoptions, OFFER_AV);
    a1_->CreateOfferRemoveTrack(options, videoTrack);
  }

  void CreateOfferAudioOnly(OfferOptions& options) {
    EnsureInit();
    a1_->CreateOffer(options, OFFER_AUDIO);
  }

  void CreateOfferAddCandidate(OfferOptions& options,
                               const std::string& candidate, const std::string& mid,
                               unsigned short level) {
    EnsureInit();
    a1_->CreateOffer(options, OFFER_AV);
    a1_->AddIceCandidate(candidate, mid, level, true);
  }

  void AddIceCandidateEarly(const std::string& candidate, const std::string& mid,
                            unsigned short level) {
    EnsureInit();
    a1_->AddIceCandidate(candidate, mid, level, false);
  }

  std::string SwapMsids(const std::string& sdp, bool swapVideo) const
  {
    SipccSdpParser parser;
    UniquePtr<Sdp> parsed = parser.Parse(sdp);

    SdpMediaSection* previousMsection = nullptr;
    bool swapped = false;
    for (size_t i = 0; i < parsed->GetMediaSectionCount(); ++i) {
      SdpMediaSection* currentMsection = &parsed->GetMediaSection(i);
      bool isVideo = currentMsection->GetMediaType() == SdpMediaSection::kVideo;
      if (swapVideo == isVideo) {
        if (previousMsection) {
          UniquePtr<SdpMsidAttributeList> prevMsid(
            new SdpMsidAttributeList(
                previousMsection->GetAttributeList().GetMsid()));
          UniquePtr<SdpMsidAttributeList> currMsid(
            new SdpMsidAttributeList(
                currentMsection->GetAttributeList().GetMsid()));
          previousMsection->GetAttributeList().SetAttribute(currMsid.release());
          currentMsection->GetAttributeList().SetAttribute(prevMsid.release());
          swapped = true;
        }
        previousMsection = currentMsection;
      }
    }

    EXPECT_TRUE(swapped);

    return parsed->ToString();
  }

  void CheckRtcpFbSdp(const std::string &sdp,
                      const std::set<std::string>& expected) {

    std::set<std::string>::const_iterator it;

    // Iterate through the list of expected feedback types and ensure
    // that none of them are missing.
    for (it = expected.begin(); it != expected.end(); ++it) {
      std::string attr = std::string("\r\na=rtcp-fb:120 ") + (*it) + "\r\n";
      std::cout << " - Checking for a=rtcp-fb: '" << *it << "'" << std::endl;
      ASSERT_NE(sdp.find(attr), std::string::npos);
    }

    // Iterate through all of the rtcp-fb lines in the SDP and ensure
    // that all of them are expected.
    ParsedSDP sdpWrapper(sdp);
    std::vector<std::string> values = sdpWrapper.GetLines("a=rtcp-fb:120");
    std::vector<std::string>::iterator it2;
    for (it2 = values.begin(); it2 != values.end(); ++it2) {
      std::cout << " - Verifying that rtcp-fb is okay: '" << *it2
                << "'" << std::endl;
      ASSERT_NE(0U, expected.count(*it2));
    }
  }

  std::string HardcodeRtcpFb(const std::string& sdp,
                             const std::set<std::string>& feedback) {
    ParsedSDP sdpWrapper(sdp);

    // Strip out any existing rtcp-fb lines
    sdpWrapper.DeleteLines("a=rtcp-fb:120");
    sdpWrapper.DeleteLines("a=rtcp-fb:126");
    sdpWrapper.DeleteLines("a=rtcp-fb:97");

    // Add rtcp-fb lines for the desired feedback types
    // We know that the video section is generated second (last),
    // so appending these to the end of the SDP has the desired effect.
    std::set<std::string>::const_iterator it;
    for (it = feedback.begin(); it != feedback.end(); ++it) {
      sdpWrapper.AddLine(std::string("a=rtcp-fb:120 ") + (*it) + "\r\n");
      sdpWrapper.AddLine(std::string("a=rtcp-fb:126 ") + (*it) + "\r\n");
      sdpWrapper.AddLine(std::string("a=rtcp-fb:97 ") + (*it) + "\r\n");
    }

    std::cout << "Modified SDP " << std::endl
              << indent(sdpWrapper.getSdp()) << std::endl;

    // Double-check that the offered SDP matches what we expect
    CheckRtcpFbSdp(sdpWrapper.getSdp(), feedback);

    return sdpWrapper.getSdp();
  }

  void TestRtcpFbAnswer(const std::set<std::string>& feedback,
      bool expectNack,
      VideoSessionConduit::FrameRequestType frameRequestType) {
    EnsureInit();
    OfferOptions options;

    a1_->CreateOffer(options, OFFER_AV);
    a1_->SetLocal(TestObserver::OFFER, a1_->offer());

    a2_->SetRemote(TestObserver::OFFER, a1_->offer());
    a2_->CreateAnswer(OFFER_AV | ANSWER_AV);

    a2_->SetLocal(TestObserver::ANSWER, a2_->answer());

    std::string modifiedAnswer(HardcodeRtcpFb(a2_->answer(), feedback));

    a1_->SetRemote(TestObserver::ANSWER, modifiedAnswer);

    a1_->SetExpectedFrameRequestType(frameRequestType);
    a1_->mExpectNack = expectNack;
    // Since we don't support rewriting rtcp-fb in answers, a2 still thinks it
    // will be doing all of the normal rtcp-fb

    WaitForCompleted();
    CheckPipelines();

    CloseStreams();
  }

  void TestRtcpFbOffer(
      const std::set<std::string>& feedback,
      bool expectNack,
      VideoSessionConduit::FrameRequestType frameRequestType) {
    EnsureInit();
    OfferOptions options;

    a1_->CreateOffer(options, OFFER_AV);
    a1_->SetLocal(TestObserver::OFFER, a1_->offer());

    std::string modifiedOffer = HardcodeRtcpFb(a1_->offer(), feedback);

    a2_->SetRemote(TestObserver::OFFER, modifiedOffer);
    a1_->SetExpectedFrameRequestType(frameRequestType);
    a1_->mExpectNack = expectNack;
    a2_->SetExpectedFrameRequestType(frameRequestType);
    a2_->mExpectNack = expectNack;

    a2_->CreateAnswer(OFFER_AV | ANSWER_AV);

    a2_->SetLocal(TestObserver::ANSWER, a2_->answer());
    a1_->SetRemote(TestObserver::ANSWER, a2_->answer());

    WaitForCompleted();

    CheckPipelines();
    CloseStreams();
  }

  void SetTestStunServer() {
    stun_addr_ = TestStunServer::GetInstance()->addr();
    stun_port_ = TestStunServer::GetInstance()->port();

    TestStunServer::GetInstance()->SetActive(false);
    TestStunServer::GetInstance()->SetResponseAddr(
        kBogusSrflxAddress, kBogusSrflxPort);
  }

  // Check max-fs and max-fr in SDP
  void CheckMaxFsFrSdp(const std::string sdp,
                       int format,
                       int max_fs,
                       int max_fr) {
    ParsedSDP sdpWrapper(sdp);
    std::stringstream ss;
    ss << "a=fmtp:" << format;
    std::vector<std::string> lines = sdpWrapper.GetLines(ss.str());

    // Both max-fs and max-fr not exist
    if (lines.empty()) {
      ASSERT_EQ(max_fs, 0);
      ASSERT_EQ(max_fr, 0);
      return;
    }

    // At most one instance allowed for each format
    ASSERT_EQ(lines.size(), 1U);

    std::string line = lines.front();

    // Make sure that max-fs doesn't exist
    if (max_fs == 0) {
      ASSERT_EQ(line.find("max-fs="), std::string::npos);
    }
    // Check max-fs value
    if (max_fs > 0) {
      std::stringstream ss;
      ss << "max-fs=" << max_fs;
      ASSERT_NE(line.find(ss.str()), std::string::npos);
    }
    // Make sure that max-fr doesn't exist
    if (max_fr == 0) {
      ASSERT_EQ(line.find("max-fr="), std::string::npos);
    }
    // Check max-fr value
    if (max_fr > 0) {
      std::stringstream ss;
      ss << "max-fr=" << max_fr;
      ASSERT_NE(line.find(ss.str()), std::string::npos);
    }
  }

  void CloseStreams()
  {
    a1_->CloseSendStreams();
    a2_->CloseSendStreams();
    a1_->CloseReceiveStreams();
    a2_->CloseReceiveStreams();
  }

 protected:
  bool init_;
  ScopedDeletePtr<SignalingAgent> a1_;  // Canonically "caller"
  ScopedDeletePtr<SignalingAgent> a2_;  // Canonically "callee"
  std::string stun_addr_;
  uint16_t stun_port_;
};

#if !defined(MOZILLA_XPCOMRT_API)
// FIXME XPCOMRT doesn't support nsPrefService
// See Bug 1129188 - Create standalone libpref for use in standalone WebRTC
static void SetIntPrefOnMainThread(nsCOMPtr<nsIPrefBranch> prefs,
  const char *pref_name,
  int new_value) {
  MOZ_ASSERT(NS_IsMainThread());
  prefs->SetIntPref(pref_name, new_value);
}

static void SetMaxFsFr(nsCOMPtr<nsIPrefBranch> prefs,
  int max_fs,
  int max_fr) {
  gMainThread->Dispatch(
    WrapRunnableNM(SetIntPrefOnMainThread,
      prefs,
      "media.navigator.video.max_fs",
      max_fs),
    NS_DISPATCH_SYNC);

  gMainThread->Dispatch(
    WrapRunnableNM(SetIntPrefOnMainThread,
      prefs,
      "media.navigator.video.max_fr",
      max_fr),
    NS_DISPATCH_SYNC);
}

class FsFrPrefClearer {
  public:
    explicit FsFrPrefClearer(nsCOMPtr<nsIPrefBranch> prefs): mPrefs(prefs) {}
    ~FsFrPrefClearer() {
      gMainThread->Dispatch(
        WrapRunnableNM(FsFrPrefClearer::ClearUserPrefOnMainThread,
          mPrefs,
          "media.navigator.video.max_fs"),
        NS_DISPATCH_SYNC);
      gMainThread->Dispatch(
        WrapRunnableNM(FsFrPrefClearer::ClearUserPrefOnMainThread,
          mPrefs,
          "media.navigator.video.max_fr"),
        NS_DISPATCH_SYNC);
    }

    static void ClearUserPrefOnMainThread(nsCOMPtr<nsIPrefBranch> prefs,
      const char *pref_name) {
      MOZ_ASSERT(NS_IsMainThread());
      prefs->ClearUserPref(pref_name);
    }
  private:
    nsCOMPtr<nsIPrefBranch> mPrefs;
};
#endif // !defined(MOZILLA_XPCOMRT_API)

TEST_P(SignalingTest, JustInit)
{
}

TEST_P(SignalingTest, CreateSetOffer)
{
  OfferOptions options;
  CreateSetOffer(options);
}

TEST_P(SignalingTest, CreateOfferAudioVideoOptionUndefined)
{
  OfferOptions options;
  CreateOffer(options, OFFER_AV);
}

TEST_P(SignalingTest, CreateOfferNoVideoStreamRecvVideo)
{
  OfferOptions options;
  options.setInt32Option("OfferToReceiveAudio", 1);
  options.setInt32Option("OfferToReceiveVideo", 1);
  CreateOffer(options, OFFER_AUDIO);
}

TEST_P(SignalingTest, CreateOfferNoAudioStreamRecvAudio)
{
  OfferOptions options;
  options.setInt32Option("OfferToReceiveAudio", 1);
  options.setInt32Option("OfferToReceiveVideo", 1);
  CreateOffer(options, OFFER_VIDEO);
}

TEST_P(SignalingTest, CreateOfferNoVideoStream)
{
  OfferOptions options;
  options.setInt32Option("OfferToReceiveAudio", 1);
  options.setInt32Option("OfferToReceiveVideo", 0);
  CreateOffer(options, OFFER_AUDIO);
}

TEST_P(SignalingTest, CreateOfferNoAudioStream)
{
  OfferOptions options;
  options.setInt32Option("OfferToReceiveAudio", 0);
  options.setInt32Option("OfferToReceiveVideo", 1);
  CreateOffer(options, OFFER_VIDEO);
}

TEST_P(SignalingTest, CreateOfferDontReceiveAudio)
{
  OfferOptions options;
  options.setInt32Option("OfferToReceiveAudio", 0);
  options.setInt32Option("OfferToReceiveVideo", 1);
  CreateOffer(options, OFFER_AV);
}

TEST_P(SignalingTest, CreateOfferDontReceiveVideo)
{
  OfferOptions options;
  options.setInt32Option("OfferToReceiveAudio", 1);
  options.setInt32Option("OfferToReceiveVideo", 0);
  CreateOffer(options, OFFER_AV);
}

TEST_P(SignalingTest, CreateOfferRemoveAudioTrack)
{
  OfferOptions options;
  options.setInt32Option("OfferToReceiveAudio", 1);
  options.setInt32Option("OfferToReceiveVideo", 1);
  CreateOfferRemoveTrack(options, false);
}

TEST_P(SignalingTest, CreateOfferDontReceiveAudioRemoveAudioTrack)
{
  OfferOptions options;
  options.setInt32Option("OfferToReceiveAudio", 0);
  options.setInt32Option("OfferToReceiveVideo", 1);
  CreateOfferRemoveTrack(options, false);
}

TEST_P(SignalingTest, CreateOfferDontReceiveVideoRemoveVideoTrack)
{
  OfferOptions options;
  options.setInt32Option("OfferToReceiveAudio", 1);
  options.setInt32Option("OfferToReceiveVideo", 0);
  CreateOfferRemoveTrack(options, true);
}

TEST_P(SignalingTest, OfferAnswerNothingDisabled)
{
  OfferOptions options;
  OfferAnswer(options, OFFER_AV | ANSWER_AV);
}

TEST_P(SignalingTest, OfferAnswerNoTrickle)
{
  OfferOptions options;
  OfferAnswer(options, OFFER_AV | ANSWER_AV, NO_TRICKLE);
}

TEST_P(SignalingTest, OfferAnswerOffererTrickles)
{
  OfferOptions options;
  OfferAnswer(options, OFFER_AV | ANSWER_AV, OFFERER_TRICKLES);
}

TEST_P(SignalingTest, OfferAnswerAnswererTrickles)
{
  OfferOptions options;
  OfferAnswer(options, OFFER_AV | ANSWER_AV, ANSWERER_TRICKLES);
}

TEST_P(SignalingTest, OfferAnswerBothTrickle)
{
  OfferOptions options;
  OfferAnswer(options, OFFER_AV | ANSWER_AV, BOTH_TRICKLE);
}

TEST_P(SignalingTest, OfferAnswerAudioBothTrickle)
{
  OfferOptions options;
  OfferAnswer(options, OFFER_AUDIO | ANSWER_AUDIO, BOTH_TRICKLE);
}


TEST_P(SignalingTest, OfferAnswerNothingDisabledFullCycle)
{
  OfferOptions options;
  OfferAnswer(options, OFFER_AV | ANSWER_AV);
  // verify the default codec priorities
  ASSERT_NE(a1_->getLocalDescription().find("RTP/SAVPF 109 9 0 8\r"), std::string::npos);
  ASSERT_NE(a2_->getLocalDescription().find("RTP/SAVPF 109\r"), std::string::npos);
}

TEST_P(SignalingTest, OfferAnswerAudioInactive)
{
  OfferOptions options;
  options.setInt32Option("OfferToReceiveAudio", 1);
  options.setInt32Option("OfferToReceiveVideo", 1);
  OfferAnswer(options, OFFER_VIDEO | ANSWER_VIDEO);
}

TEST_P(SignalingTest, OfferAnswerVideoInactive)
{
  OfferOptions options;
  options.setInt32Option("OfferToReceiveAudio", 1);
  options.setInt32Option("OfferToReceiveVideo", 1);
  OfferAnswer(options, OFFER_AUDIO | ANSWER_AUDIO);
  CloseStreams();
}

TEST_P(SignalingTest, CreateOfferAddCandidate)
{
  OfferOptions options;
  CreateOfferAddCandidate(options, strSampleCandidate,
                          strSampleMid, nSamplelevel);
}

TEST_P(SignalingTest, AddIceCandidateEarly)
{
  OfferOptions options;
  AddIceCandidateEarly(strSampleCandidate,
                       strSampleMid, nSamplelevel);
}

TEST_P(SignalingTest, OfferAnswerDontAddAudioStreamOnAnswerNoOptions)
{
  OfferOptions options;
  options.setInt32Option("OfferToReceiveAudio", 1);
  options.setInt32Option("OfferToReceiveVideo", 1);
  OfferAnswer(options, OFFER_AV | ANSWER_VIDEO);
}

TEST_P(SignalingTest, OfferAnswerDontAddVideoStreamOnAnswerNoOptions)
{
  OfferOptions options;
  options.setInt32Option("OfferToReceiveAudio", 1);
  options.setInt32Option("OfferToReceiveVideo", 1);
  OfferAnswer(options, OFFER_AV | ANSWER_AUDIO);
}

TEST_P(SignalingTest, OfferAnswerDontAddAudioVideoStreamsOnAnswerNoOptions)
{
  OfferOptions options;
  options.setInt32Option("OfferToReceiveAudio", 1);
  options.setInt32Option("OfferToReceiveVideo", 1);
  OfferAnswer(options, OFFER_AV | ANSWER_NONE);
}

TEST_P(SignalingTest, RenegotiationOffererAddsTracks)
{
  OfferOptions options;
  OfferAnswer(options, OFFER_AV | ANSWER_AV);
  // OFFER_AV causes a new stream + tracks to be added
  OfferAnswer(options, OFFER_AV);
  CloseStreams();
}

TEST_P(SignalingTest, RenegotiationOffererRemovesTrack)
{
  OfferOptions options;
  OfferAnswer(options, OFFER_AV | ANSWER_AV);

  a1_->RemoveTrack(0, false);

  OfferAnswer(options, OFFER_NONE);

  CloseStreams();
}

TEST_P(SignalingTest, RenegotiationBothRemoveThenAddTrack)
{
  OfferOptions options;
  OfferAnswer(options, OFFER_AV | ANSWER_AV);

  a1_->RemoveTrack(0, false);
  a2_->RemoveTrack(0, false);

  OfferAnswer(options, OFFER_NONE);

  // OFFER_AUDIO causes a new audio track to be added on both sides
  OfferAnswer(options, OFFER_AUDIO);

  CloseStreams();
}

TEST_P(SignalingTest, RenegotiationOffererReplacesTrack)
{
  OfferOptions options;
  OfferAnswer(options, OFFER_AV | ANSWER_AV);

  a1_->RemoveTrack(0, false);

  // OFFER_AUDIO causes a new audio track to be added on both sides
  OfferAnswer(options, OFFER_AUDIO);

  CloseStreams();
}

TEST_P(SignalingTest, RenegotiationOffererSwapsMsids)
{
  OfferOptions options;

  EnsureInit();
  a1_->AddStream(DOMMediaStream::HINT_CONTENTS_AUDIO |
                 DOMMediaStream::HINT_CONTENTS_VIDEO);

  OfferAnswer(options, OFFER_AV | ANSWER_AV);

  a1_->CreateOffer(options, OFFER_NONE);
  a1_->SetLocal(TestObserver::OFFER, a1_->offer());
  std::string audioSwapped = SwapMsids(a1_->offer(), false);
  std::string audioAndVideoSwapped = SwapMsids(audioSwapped, true);
  std::cout << "Msids swapped: " << std::endl << audioAndVideoSwapped << std::endl;
  a2_->SetRemote(TestObserver::OFFER, audioAndVideoSwapped);
  Answer(options, OFFER_NONE, BOTH_TRICKLE);
  WaitForCompleted();

  CheckPipelines();
  CheckStreams();

  CloseStreams();
}

TEST_P(SignalingTest, RenegotiationAnswererAddsTracks)
{
  OfferOptions options;
  OfferAnswer(options, OFFER_AV | ANSWER_AV);

  options.setInt32Option("OfferToReceiveAudio", 2);
  options.setInt32Option("OfferToReceiveVideo", 2);

  // ANSWER_AV causes a new stream + tracks to be added
  OfferAnswer(options, ANSWER_AV);

  CloseStreams();
}

TEST_P(SignalingTest, RenegotiationAnswererRemovesTrack)
{
  OfferOptions options;
  OfferAnswer(options, OFFER_AV | ANSWER_AV);

  a2_->RemoveTrack(0, false);

  OfferAnswer(options, OFFER_NONE);

  CloseStreams();
}

TEST_P(SignalingTest, RenegotiationAnswererReplacesTrack)
{
  OfferOptions options;
  OfferAnswer(options, OFFER_AV | ANSWER_AV);

  a2_->RemoveTrack(0, false);

  // ANSWER_AUDIO causes a new audio track to be added
  OfferAnswer(options, ANSWER_AUDIO);

  CloseStreams();
}

TEST_P(SignalingTest, BundleRenegotiation)
{
  if (GetParam() == "bundle") {
    // We don't support ICE restart, which is a prereq for renegotiating bundle
    // off.
    return;
  }

  OfferOptions options;
  OfferAnswer(options, OFFER_AV | ANSWER_AV);

  // If we did bundle before, turn it off, if not, turn it on
  if (a1_->mBundleEnabled && a2_->mBundleEnabled) {
    a1_->SetBundleEnabled(false);
  } else {
    a1_->SetBundleEnabled(true);
    a2_->SetBundleEnabled(true);
  }

  OfferAnswer(options, OFFER_NONE);
}

TEST_P(SignalingTest, FullCallAudioOnly)
{
  OfferOptions options;
  OfferAnswer(options, OFFER_AUDIO | ANSWER_AUDIO);

  CloseStreams();
}

TEST_P(SignalingTest, FullCallVideoOnly)
{
  OfferOptions options;
  OfferAnswer(options, OFFER_VIDEO | ANSWER_VIDEO);

  CloseStreams();
}

TEST_P(SignalingTest, OfferAndAnswerWithExtraCodec)
{
  EnsureInit();
  OfferOptions options;
  Offer(options, OFFER_AUDIO);

  a2_->CreateAnswer(OFFER_AUDIO | ANSWER_AUDIO);
  a2_->SetLocal(TestObserver::ANSWER, a2_->answer());
  ParsedSDP sdpWrapper(a2_->answer());
  sdpWrapper.ReplaceLine("m=audio", "m=audio 65375 RTP/SAVPF 109 8\r\n");
  sdpWrapper.AddLine("a=rtpmap:8 PCMA/8000\r\n");
  std::cout << "Modified SDP " << std::endl
            << indent(sdpWrapper.getSdp()) << std::endl;

  a1_->SetRemote(TestObserver::ANSWER, sdpWrapper.getSdp());

  WaitForCompleted();

  CheckPipelines();
  CheckStreams();

  CloseStreams();
}

TEST_P(SignalingTest, FullCallTrickle)
{
  OfferOptions options;
  OfferAnswer(options, OFFER_AV | ANSWER_AV);

  std::cerr << "ICE handshake completed" << std::endl;

  CloseStreams();
}

// Offer answer with trickle but with chrome-style candidates
TEST_P(SignalingTest, DISABLED_FullCallTrickleChrome)
{
  OfferOptions options;
  OfferAnswerTrickleChrome(options, OFFER_AV | ANSWER_AV);

  std::cerr << "ICE handshake completed" << std::endl;

  CloseStreams();
}

TEST_P(SignalingTest, FullCallTrickleBeforeSetLocal)
{
  OfferOptions options;
  Offer(options, OFFER_AV | ANSWER_AV);
  // ICE will succeed even if one side fails to trickle, so we need to disable
  // one side before performing a test that might cause candidates to be
  // dropped
  a2_->DropOutgoingTrickleCandidates();
  // Wait until all of a1's candidates have been trickled to a2, _before_ a2
  // has called CreateAnswer/SetLocal (ie; the ICE stack is not running yet)
  a1_->WaitForGather();
  Answer(options, OFFER_AV | ANSWER_AV);
  WaitForCompleted();

  CheckPipelines();
  CheckStreams();

  std::cerr << "ICE handshake completed" << std::endl;

  CloseStreams();
}

// This test comes from Bug 810220
// TODO: Move this to jsep_session_unittest
TEST_P(SignalingTest, AudioOnlyG711Call)
{
  EnsureInit();

  OfferOptions options;
  const std::string& offer(strG711SdpOffer);

  std::cout << "Setting offer to:" << std::endl << indent(offer) << std::endl;
  a2_->SetRemote(TestObserver::OFFER, offer);

  std::cout << "Creating answer:" << std::endl;
  a2_->CreateAnswer(OFFER_AUDIO | ANSWER_AUDIO);

  std::string answer = a2_->answer();

  // They didn't offer opus, so our answer shouldn't include it.
  ASSERT_EQ(answer.find(" opus/"), std::string::npos);

  // They also didn't offer video or application
  ASSERT_EQ(answer.find("video"), std::string::npos);
  ASSERT_EQ(answer.find("application"), std::string::npos);

  // We should answer with PCMU and telephone-event
  ASSERT_NE(answer.find(" PCMU/8000"), std::string::npos);

  // Double-check the directionality
  ASSERT_NE(answer.find("\r\na=sendrecv"), std::string::npos);

}

TEST_P(SignalingTest, IncomingOfferIceLite)
{
  EnsureInit();

  std::string offer =
    "v=0\r\n"
    "o=- 1936463 1936463 IN IP4 148.147.200.251\r\n"
    "s=-\r\n"
    "c=IN IP4 148.147.200.251\r\n"
    "t=0 0\r\n"
    "a=ice-lite\r\n"
    "a=fingerprint:sha-1 "
      "E7:FA:17:DA:3F:3C:1E:D8:E4:9C:8C:4C:13:B9:2E:D5:C6:78:AB:B3\r\n"
    "m=audio 40014 RTP/SAVPF 8 0 101\r\n"
    "a=rtpmap:8 PCMA/8000\r\n"
    "a=rtpmap:0 PCMU/8000\r\n"
    "a=rtpmap:101 telephone-event/8000\r\n"
    "a=fmtp:101 0-15\r\n"
    "a=ptime:20\r\n"
    "a=sendrecv\r\n"
    "a=ice-ufrag:bf2LAgqBZdiWFR2r\r\n"
    "a=ice-pwd:ScxgaNzdBOYScR0ORleAvt1x\r\n"
    "a=candidate:1661181211 1 udp 10 148.147.200.251 40014 typ host\r\n"
    "a=candidate:1661181211 2 udp 9 148.147.200.251 40015 typ host\r\n"
    "a=setup:actpass\r\n";

  std::cout << "Setting offer to:" << std::endl << indent(offer) << std::endl;
  a2_->SetRemote(TestObserver::OFFER, offer);

  std::cout << "Creating answer:" << std::endl;
  a2_->CreateAnswer(OFFER_AUDIO | ANSWER_AUDIO);
  a2_->SetLocal(TestObserver::ANSWER, a2_->answer());

  ASSERT_EQ(a2_->pc->media()->ice_ctx()->GetControlling(),
            NrIceCtx::ICE_CONTROLLING);
}

// This test comes from Bug814038
TEST_P(SignalingTest, ChromeOfferAnswer)
{
  EnsureInit();

  // This is captured SDP from an early interop attempt with Chrome.
  std::string offer =
    "v=0\r\n"
    "o=- 1713781661 2 IN IP4 127.0.0.1\r\n"
    "s=-\r\n"
    "t=0 0\r\n"
    "a=group:BUNDLE audio video\r\n"

    "m=audio 1 RTP/SAVPF 103 104 111 0 8 107 106 105 13 126\r\n"
    "a=fingerprint:sha-1 4A:AD:B9:B1:3F:82:18:3B:54:02:12:DF:3E:"
      "5D:49:6B:19:E5:7C:AB\r\n"
    "a=setup:active\r\n"
    "c=IN IP4 0.0.0.0\r\n"
    "a=rtcp:1 IN IP4 0.0.0.0\r\n"
    "a=ice-ufrag:lBrbdDfrVBH1cldN\r\n"
    "a=ice-pwd:rzh23jet4QpCaEoj9Sl75pL3\r\n"
    "a=ice-options:google-ice\r\n"
    "a=sendrecv\r\n"
    "a=mid:audio\r\n"
    "a=rtcp-mux\r\n"
    "a=crypto:1 AES_CM_128_HMAC_SHA1_80 inline:"
      "RzrYlzpkTsvgYFD1hQqNCzQ7y4emNLKI1tODsjim\r\n"
    "a=rtpmap:103 ISAC/16000\r\n"
    "a=rtpmap:104 ISAC/32000\r\n"
    // NOTE: the actual SDP that Chrome sends at the moment
    // doesn't indicate two channels. I've amended their SDP
    // here, under the assumption that the constraints
    // described in draft-spittka-payload-rtp-opus will
    // eventually be implemented by Google.
    "a=rtpmap:111 opus/48000/2\r\n"
    "a=rtpmap:0 PCMU/8000\r\n"
    "a=rtpmap:8 PCMA/8000\r\n"
    "a=rtpmap:107 CN/48000\r\n"
    "a=rtpmap:106 CN/32000\r\n"
    "a=rtpmap:105 CN/16000\r\n"
    "a=rtpmap:13 CN/8000\r\n"
    "a=rtpmap:126 telephone-event/8000\r\n"
    "a=ssrc:661333377 cname:KIXaNxUlU5DP3fVS\r\n"
    "a=ssrc:661333377 msid:A5UL339RyGxT7zwgyF12BFqesxkmbUsaycp5 a0\r\n"
    "a=ssrc:661333377 mslabel:A5UL339RyGxT7zwgyF12BFqesxkmbUsaycp5\r\n"
    "a=ssrc:661333377 label:A5UL339RyGxT7zwgyF12BFqesxkmbUsaycp5a0\r\n"

    "m=video 1 RTP/SAVPF 100 101 102\r\n"
    "a=fingerprint:sha-1 4A:AD:B9:B1:3F:82:18:3B:54:02:12:DF:3E:5D:49:"
      "6B:19:E5:7C:AB\r\n"
    "a=setup:active\r\n"
    "c=IN IP4 0.0.0.0\r\n"
    "a=rtcp:1 IN IP4 0.0.0.0\r\n"
    "a=ice-ufrag:lBrbdDfrVBH1cldN\r\n"
    "a=ice-pwd:rzh23jet4QpCaEoj9Sl75pL3\r\n"
    "a=ice-options:google-ice\r\n"
    "a=sendrecv\r\n"
    "a=mid:video\r\n"
    "a=rtcp-mux\r\n"
    "a=crypto:1 AES_CM_128_HMAC_SHA1_80 inline:"
      "RzrYlzpkTsvgYFD1hQqNCzQ7y4emNLKI1tODsjim\r\n"
    "a=rtpmap:100 VP8/90000\r\n"
    "a=rtpmap:101 red/90000\r\n"
    "a=rtpmap:102 ulpfec/90000\r\n"
    "a=rtcp-fb:100 nack\r\n"
    "a=rtcp-fb:100 ccm fir\r\n"
    "a=ssrc:3012607008 cname:KIXaNxUlU5DP3fVS\r\n"
    "a=ssrc:3012607008 msid:A5UL339RyGxT7zwgyF12BFqesxkmbUsaycp5 v0\r\n"
    "a=ssrc:3012607008 mslabel:A5UL339RyGxT7zwgyF12BFqesxkmbUsaycp5\r\n"
    "a=ssrc:3012607008 label:A5UL339RyGxT7zwgyF12BFqesxkmbUsaycp5v0\r\n";


  std::cout << "Setting offer to:" << std::endl << indent(offer) << std::endl;
  a2_->SetRemote(TestObserver::OFFER, offer);

  std::cout << "Creating answer:" << std::endl;
  a2_->CreateAnswer(OFFER_AUDIO | ANSWER_AUDIO);

  std::string answer = a2_->answer();
}


TEST_P(SignalingTest, FullChromeHandshake)
{
  EnsureInit();

  std::string offer = "v=0\r\n"
      "o=- 3835809413 2 IN IP4 127.0.0.1\r\n"
      "s=-\r\n"
      "t=0 0\r\n"
      "a=group:BUNDLE audio video\r\n"
      "a=msid-semantic: WMS ahheYQXHFU52slYMrWNtKUyHCtWZsOJgjlOH\r\n"
      "m=audio 1 RTP/SAVPF 103 104 111 0 8 107 106 105 13 126\r\n"
      "c=IN IP4 1.1.1.1\r\n"
      "a=rtcp:1 IN IP4 1.1.1.1\r\n"
      "a=ice-ufrag:jz9UBk9RT8eCQXiL\r\n"
      "a=ice-pwd:iscXxsdU+0gracg0g5D45orx\r\n"
      "a=ice-options:google-ice\r\n"
      "a=fingerprint:sha-256 A8:76:8C:4C:FA:2E:67:D7:F8:1D:28:4E:90:24:04:"
        "12:EB:B4:A6:69:3D:05:92:E4:91:C3:EA:F9:B7:54:D3:09\r\n"
      "a=setup:active\r\n"
      "a=sendrecv\r\n"
      "a=mid:audio\r\n"
      "a=rtcp-mux\r\n"
      "a=crypto:1 AES_CM_128_HMAC_SHA1_80 inline:/he/v44FKu/QvEhex86zV0pdn2V"
        "4Y7wB2xaZ8eUy\r\n"
      "a=rtpmap:103 ISAC/16000\r\n"
      "a=rtpmap:104 ISAC/32000\r\n"
      "a=rtpmap:111 opus/48000/2\r\n"
      "a=rtpmap:0 PCMU/8000\r\n"
      "a=rtpmap:8 PCMA/8000\r\n"
      "a=rtpmap:107 CN/48000\r\n"
      "a=rtpmap:106 CN/32000\r\n"
      "a=rtpmap:105 CN/16000\r\n"
      "a=rtpmap:13 CN/8000\r\n"
      "a=rtpmap:126 telephone-event/8000\r\n"
      "a=ssrc:3389377748 cname:G5I+Jxz4rcaq8IIK\r\n"
      "a=ssrc:3389377748 msid:ahheYQXHFU52slYMrWNtKUyHCtWZsOJgjlOH a0\r\n"
      "a=ssrc:3389377748 mslabel:ahheYQXHFU52slYMrWNtKUyHCtWZsOJgjlOH\r\n"
      "a=ssrc:3389377748 label:ahheYQXHFU52slYMrWNtKUyHCtWZsOJgjlOHa0\r\n"
      "m=video 1 RTP/SAVPF 100 116 117\r\n"
      "c=IN IP4 1.1.1.1\r\n"
      "a=rtcp:1 IN IP4 1.1.1.1\r\n"
      "a=ice-ufrag:jz9UBk9RT8eCQXiL\r\n"
      "a=ice-pwd:iscXxsdU+0gracg0g5D45orx\r\n"
      "a=ice-options:google-ice\r\n"
      "a=fingerprint:sha-256 A8:76:8C:4C:FA:2E:67:D7:F8:1D:28:4E:90:24:04:"
        "12:EB:B4:A6:69:3D:05:92:E4:91:C3:EA:F9:B7:54:D3:09\r\n"
      "a=setup:active\r\n"
      "a=sendrecv\r\n"
      "a=mid:video\r\n"
      "a=rtcp-mux\r\n"
      "a=crypto:1 AES_CM_128_HMAC_SHA1_80 inline:/he/v44FKu/QvEhex86zV0pdn2V"
        "4Y7wB2xaZ8eUy\r\n"
      "a=rtpmap:100 VP8/90000\r\n"
      "a=rtpmap:116 red/90000\r\n"
      "a=rtpmap:117 ulpfec/90000\r\n"
      "a=ssrc:3613537198 cname:G5I+Jxz4rcaq8IIK\r\n"
      "a=ssrc:3613537198 msid:ahheYQXHFU52slYMrWNtKUyHCtWZsOJgjlOH v0\r\n"
      "a=ssrc:3613537198 mslabel:ahheYQXHFU52slYMrWNtKUyHCtWZsOJgjlOH\r\n"
      "a=ssrc:3613537198 label:ahheYQXHFU52slYMrWNtKUyHCtWZsOJgjlOHv0\r\n";

  std::cout << "Setting offer to:" << std::endl << indent(offer) << std::endl;
  a2_->SetRemote(TestObserver::OFFER, offer);

  std::cout << "Creating answer:" << std::endl;
  a2_->CreateAnswer(OFFER_AUDIO | ANSWER_AUDIO);

  std::cout << "Setting answer" << std::endl;
  a2_->SetLocal(TestObserver::ANSWER, a2_->answer());

  std::string answer = a2_->answer();
  ASSERT_NE(answer.find("111 opus/"), std::string::npos);
}

// Disabled pending resolution of bug 818640.
// Actually, this test is completely broken; you can't just call
// SetRemote/CreateAnswer over and over again.
// If we were to test this sort of thing, it would belong in
// jsep_session_unitest
TEST_P(SignalingTest, DISABLED_OfferAllDynamicTypes)
{
  EnsureInit();

  std::string offer;
  for (int i = 96; i < 128; i++)
  {
    std::stringstream ss;
    ss << i;
    std::cout << "Trying dynamic pt = " << i << std::endl;
    offer =
      "v=0\r\n"
      "o=- 1 1 IN IP4 148.147.200.251\r\n"
      "s=-\r\n"
      "b=AS:64\r\n"
      "t=0 0\r\n"
      "a=fingerprint:sha-256 F3:FA:20:C0:CD:48:C4:5F:02:5F:A5:D3:21:D0:2D:48:"
        "7B:31:60:5C:5A:D8:0D:CD:78:78:6C:6D:CE:CC:0C:67\r\n"
      "m=audio 9000 RTP/AVP " + ss.str() + "\r\n"
      "c=IN IP4 148.147.200.251\r\n"
      "b=TIAS:64000\r\n"
      "a=rtpmap:" + ss.str() +" opus/48000/2\r\n"
      "a=candidate:0 1 udp 2130706432 148.147.200.251 9000 typ host\r\n"
      "a=candidate:0 2 udp 2130706432 148.147.200.251 9005 typ host\r\n"
      "a=ice-ufrag:cYuakxkEKH+RApYE\r\n"
      "a=ice-pwd:bwtpzLZD+3jbu8vQHvEa6Xuq\r\n"
      "a=sendrecv\r\n";

      /*
      std::cout << "Setting offer to:" << std::endl
                << indent(offer) << std::endl;
      */
      a2_->SetRemote(TestObserver::OFFER, offer);

      //std::cout << "Creating answer:" << std::endl;
      a2_->CreateAnswer(OFFER_AUDIO | ANSWER_AUDIO);

      std::string answer = a2_->answer();

      ASSERT_NE(answer.find(ss.str() + " opus/"), std::string::npos);
  }

}

// TODO: Move to jsep_session_unittest
TEST_P(SignalingTest, ipAddrAnyOffer)
{
  EnsureInit();

  std::string offer =
    "v=0\r\n"
    "o=- 1 1 IN IP4 127.0.0.1\r\n"
    "s=-\r\n"
    "b=AS:64\r\n"
    "t=0 0\r\n"
    "a=fingerprint:sha-256 F3:FA:20:C0:CD:48:C4:5F:02:5F:A5:D3:21:D0:2D:48:"
      "7B:31:60:5C:5A:D8:0D:CD:78:78:6C:6D:CE:CC:0C:67\r\n"
    "m=audio 9000 RTP/AVP 99\r\n"
    "c=IN IP4 0.0.0.0\r\n"
    "a=rtpmap:99 opus/48000/2\r\n"
    "a=ice-ufrag:cYuakxkEKH+RApYE\r\n"
    "a=ice-pwd:bwtpzLZD+3jbu8vQHvEa6Xuq\r\n"
    "a=setup:active\r\n"
    "a=sendrecv\r\n";

    a2_->SetRemote(TestObserver::OFFER, offer);
    ASSERT_TRUE(a2_->pObserver->state == TestObserver::stateSuccess);
    a2_->CreateAnswer(OFFER_AUDIO | ANSWER_AUDIO);
    ASSERT_TRUE(a2_->pObserver->state == TestObserver::stateSuccess);
    std::string answer = a2_->answer();
    ASSERT_NE(answer.find("a=sendrecv"), std::string::npos);
}

static void CreateSDPForBigOTests(std::string& offer, const std::string& number) {
  offer =
    "v=0\r\n"
    "o=- ";
  offer += number;
  offer += " ";
  offer += number;
  offer += " IN IP4 127.0.0.1\r\n"
    "s=-\r\n"
    "b=AS:64\r\n"
    "t=0 0\r\n"
    "a=fingerprint:sha-256 F3:FA:20:C0:CD:48:C4:5F:02:5F:A5:D3:21:D0:2D:48:"
      "7B:31:60:5C:5A:D8:0D:CD:78:78:6C:6D:CE:CC:0C:67\r\n"
    "m=audio 9000 RTP/AVP 99\r\n"
    "c=IN IP4 0.0.0.0\r\n"
    "a=rtpmap:99 opus/48000/2\r\n"
    "a=ice-ufrag:cYuakxkEKH+RApYE\r\n"
    "a=ice-pwd:bwtpzLZD+3jbu8vQHvEa6Xuq\r\n"
    "a=setup:active\r\n"
    "a=sendrecv\r\n";
}

// TODO: Move to jsep_session_unittest
TEST_P(SignalingTest, BigOValues)
{
  EnsureInit();

  std::string offer;

  CreateSDPForBigOTests(offer, "12345678901234567");

  a2_->SetRemote(TestObserver::OFFER, offer);
  ASSERT_EQ(a2_->pObserver->state, TestObserver::stateSuccess);
}

// TODO: Move to jsep_session_unittest
// We probably need to retain at least one test case for each API entry point
// that verifies that errors are propagated correctly, though.
TEST_P(SignalingTest, BigOValuesExtraChars)
{
  EnsureInit();

  std::string offer;

  CreateSDPForBigOTests(offer, "12345678901234567FOOBAR");

  // The signaling state will remain "stable" because the unparsable
  // SDP leads to a failure in SetRemoteDescription.
  a2_->SetRemote(TestObserver::OFFER, offer, true,
                 PCImplSignalingState::SignalingStable);
  ASSERT_TRUE(a2_->pObserver->state == TestObserver::stateError);
}

// TODO: Move to jsep_session_unittest
TEST_P(SignalingTest, BigOValuesTooBig)
{
  EnsureInit();

  std::string offer;

  CreateSDPForBigOTests(offer, "18446744073709551615");

  // The signaling state will remain "stable" because the unparsable
  // SDP leads to a failure in SetRemoteDescription.
  a2_->SetRemote(TestObserver::OFFER, offer, true,
                 PCImplSignalingState::SignalingStable);
  ASSERT_TRUE(a2_->pObserver->state == TestObserver::stateError);
}

// TODO: Move to jsep_session_unittest
TEST_P(SignalingTest, SetLocalAnswerInStable)
{
  EnsureInit();

  OfferOptions options;
  CreateOffer(options, OFFER_AUDIO);

  // The signaling state will remain "stable" because the
  // SetLocalDescription call fails.
  a1_->SetLocal(TestObserver::ANSWER, a1_->offer(), true,
                PCImplSignalingState::SignalingStable);
  ASSERT_EQ(a1_->pObserver->lastStatusCode,
            PeerConnectionImpl::kInvalidState);
}

// TODO: Move to jsep_session_unittest
TEST_P(SignalingTest, SetRemoteAnswerInStable) {
  EnsureInit();

  // The signaling state will remain "stable" because the
  // SetRemoteDescription call fails.
  a1_->SetRemote(TestObserver::ANSWER, strSampleSdpAudioVideoNoIce, true,
                PCImplSignalingState::SignalingStable);
  ASSERT_EQ(a1_->pObserver->lastStatusCode,
            PeerConnectionImpl::kInvalidState);
}

// TODO: Move to jsep_session_unittest
TEST_P(SignalingTest, SetLocalAnswerInHaveLocalOffer) {
  OfferOptions options;
  CreateOffer(options, OFFER_AUDIO);
  a1_->SetLocal(TestObserver::OFFER, a1_->offer());
  ASSERT_EQ(a1_->pObserver->lastStatusCode,
            PeerConnectionImpl::kNoError);

  // The signaling state will remain "have-local-offer" because the
  // SetLocalDescription call fails.
  a1_->SetLocal(TestObserver::ANSWER, a1_->offer(), true,
                PCImplSignalingState::SignalingHaveLocalOffer);
  ASSERT_EQ(a1_->pObserver->lastStatusCode,
            PeerConnectionImpl::kInvalidState);
}

// TODO: Move to jsep_session_unittest
TEST_P(SignalingTest, SetRemoteOfferInHaveLocalOffer) {
  OfferOptions options;
  CreateOffer(options, OFFER_AUDIO);
  a1_->SetLocal(TestObserver::OFFER, a1_->offer());
  ASSERT_EQ(a1_->pObserver->lastStatusCode,
            PeerConnectionImpl::kNoError);

  // The signaling state will remain "have-local-offer" because the
  // SetRemoteDescription call fails.
  a1_->SetRemote(TestObserver::OFFER, a1_->offer(), true,
                 PCImplSignalingState::SignalingHaveLocalOffer);
  ASSERT_EQ(a1_->pObserver->lastStatusCode,
            PeerConnectionImpl::kInvalidState);
}

// TODO: Move to jsep_session_unittest
TEST_P(SignalingTest, SetLocalOfferInHaveRemoteOffer) {
  OfferOptions options;
  CreateOffer(options, OFFER_AUDIO);
  a2_->SetRemote(TestObserver::OFFER, a1_->offer());
  ASSERT_EQ(a2_->pObserver->lastStatusCode,
            PeerConnectionImpl::kNoError);

  // The signaling state will remain "have-remote-offer" because the
  // SetLocalDescription call fails.
  a2_->SetLocal(TestObserver::OFFER, a1_->offer(), true,
                PCImplSignalingState::SignalingHaveRemoteOffer);
  ASSERT_EQ(a2_->pObserver->lastStatusCode,
            PeerConnectionImpl::kInvalidState);
}

// TODO: Move to jsep_session_unittest
TEST_P(SignalingTest, SetRemoteAnswerInHaveRemoteOffer) {
  OfferOptions options;
  CreateOffer(options, OFFER_AUDIO);
  a2_->SetRemote(TestObserver::OFFER, a1_->offer());
  ASSERT_EQ(a2_->pObserver->lastStatusCode,
            PeerConnectionImpl::kNoError);

  // The signaling state will remain "have-remote-offer" because the
  // SetRemoteDescription call fails.
  a2_->SetRemote(TestObserver::ANSWER, a1_->offer(), true,
               PCImplSignalingState::SignalingHaveRemoteOffer);
  ASSERT_EQ(a2_->pObserver->lastStatusCode,
            PeerConnectionImpl::kInvalidState);
}

// Disabled until the spec adds a failure callback to addStream
// Actually, this is allowed I think, it just triggers a negotiationneeded
TEST_P(SignalingTest, DISABLED_AddStreamInHaveLocalOffer) {
  OfferOptions options;
  CreateOffer(options, OFFER_AUDIO);
  a1_->SetLocal(TestObserver::OFFER, a1_->offer());
  ASSERT_EQ(a1_->pObserver->lastStatusCode,
            PeerConnectionImpl::kNoError);
  a1_->AddStream();
  ASSERT_EQ(a1_->pObserver->lastStatusCode,
            PeerConnectionImpl::kInvalidState);
}

// Disabled until the spec adds a failure callback to removeStream
// Actually, this is allowed I think, it just triggers a negotiationneeded
TEST_P(SignalingTest, DISABLED_RemoveStreamInHaveLocalOffer) {
  OfferOptions options;
  CreateOffer(options, OFFER_AUDIO);
  a1_->SetLocal(TestObserver::OFFER, a1_->offer());
  ASSERT_EQ(a1_->pObserver->lastStatusCode,
            PeerConnectionImpl::kNoError);
  a1_->RemoveLastStreamAdded();
  ASSERT_EQ(a1_->pObserver->lastStatusCode,
            PeerConnectionImpl::kInvalidState);
}

TEST_P(SignalingTest, AddCandidateInHaveLocalOffer) {
  OfferOptions options;
  CreateOffer(options, OFFER_AUDIO);
  a1_->SetLocal(TestObserver::OFFER, a1_->offer());
  ASSERT_EQ(a1_->pObserver->lastAddIceStatusCode,
            PeerConnectionImpl::kNoError);
  a1_->AddIceCandidate(strSampleCandidate,
                      strSampleMid, nSamplelevel, false);
  ASSERT_EQ(PeerConnectionImpl::kInvalidState,
            a1_->pObserver->lastAddIceStatusCode);
}

TEST_F(SignalingAgentTest, CreateOffer) {
  CreateAgent(TestStunServer::GetInstance()->addr(),
              TestStunServer::GetInstance()->port());
  OfferOptions options;
  agent(0)->CreateOffer(options, OFFER_AUDIO);
}

TEST_F(SignalingAgentTest, SetLocalWithoutCreateOffer) {
  CreateAgent(TestStunServer::GetInstance()->addr(),
              TestStunServer::GetInstance()->port());
  CreateAgent(TestStunServer::GetInstance()->addr(),
              TestStunServer::GetInstance()->port());
  OfferOptions options;
  agent(0)->CreateOffer(options, OFFER_AUDIO);
  agent(1)->SetLocal(TestObserver::OFFER,
                     agent(0)->offer(),
                     true,
                     PCImplSignalingState::SignalingStable);
}

TEST_F(SignalingAgentTest, SetLocalWithoutCreateAnswer) {
  CreateAgent(TestStunServer::GetInstance()->addr(),
              TestStunServer::GetInstance()->port());
  CreateAgent(TestStunServer::GetInstance()->addr(),
              TestStunServer::GetInstance()->port());
  CreateAgent(TestStunServer::GetInstance()->addr(),
              TestStunServer::GetInstance()->port());
  OfferOptions options;
  agent(0)->CreateOffer(options, OFFER_AUDIO);
  agent(1)->SetRemote(TestObserver::OFFER, agent(0)->offer());
  agent(1)->CreateAnswer(ANSWER_AUDIO);
  agent(2)->SetRemote(TestObserver::OFFER, agent(0)->offer());
  // Use agent 1's answer on agent 2, should fail
  agent(2)->SetLocal(TestObserver::ANSWER,
                     agent(1)->answer(),
                     true,
                     PCImplSignalingState::SignalingHaveRemoteOffer);
}

TEST_F(SignalingAgentTest, CreateOfferSetLocalTrickleTestServer) {
  TestStunServer::GetInstance()->SetActive(false);
  TestStunServer::GetInstance()->SetResponseAddr(
      kBogusSrflxAddress, kBogusSrflxPort);

  CreateAgent(
      TestStunServer::GetInstance()->addr(),
      TestStunServer::GetInstance()->port());

  OfferOptions options;
  agent(0)->CreateOffer(options, OFFER_AUDIO);

  // Verify that the bogus addr is not there.
  ASSERT_FALSE(agent(0)->OfferContains(kBogusSrflxAddress));

  // Now enable the STUN server.
  TestStunServer::GetInstance()->SetActive(true);

  agent(0)->SetLocal(TestObserver::OFFER, agent(0)->offer());
  agent(0)->WaitForGather();

  // Verify that we got our candidates.
  ASSERT_LE(2U, agent(0)->MatchingCandidates(kBogusSrflxAddress));

  // Verify that the candidates appear in the offer.
  size_t match;
  match = agent(0)->getLocalDescription().find(kBogusSrflxAddress);
  ASSERT_LT(0U, match);
}


TEST_F(SignalingAgentTest, CreateAnswerSetLocalTrickleTestServer) {
  TestStunServer::GetInstance()->SetActive(false);
  TestStunServer::GetInstance()->SetResponseAddr(
      kBogusSrflxAddress, kBogusSrflxPort);

  CreateAgent(
      TestStunServer::GetInstance()->addr(),
      TestStunServer::GetInstance()->port());

  std::string offer(strG711SdpOffer);
  agent(0)->SetRemote(TestObserver::OFFER, offer, true,
                 PCImplSignalingState::SignalingHaveRemoteOffer);
  ASSERT_EQ(agent(0)->pObserver->lastStatusCode,
            PeerConnectionImpl::kNoError);

  agent(0)->CreateAnswer(ANSWER_AUDIO);

  // Verify that the bogus addr is not there.
  ASSERT_FALSE(agent(0)->AnswerContains(kBogusSrflxAddress));

  // Now enable the STUN server.
  TestStunServer::GetInstance()->SetActive(true);

  agent(0)->SetLocal(TestObserver::ANSWER, agent(0)->answer());
  agent(0)->WaitForGather();

  // Verify that we got our candidates.
  ASSERT_LE(2U, agent(0)->MatchingCandidates(kBogusSrflxAddress));

  // Verify that the candidates appear in the answer.
  size_t match;
  match = agent(0)->getLocalDescription().find(kBogusSrflxAddress);
  ASSERT_LT(0U, match);
}



TEST_F(SignalingAgentTest, CreateLotsAndWait) {
  int i;

  for (i=0; i < 100; i++) {
    if (!CreateAgent())
      break;
    std::cerr << "Created agent " << i << std::endl;
  }
  PR_Sleep(1000);  // Wait to see if we crash
}

// Test for bug 856433.
TEST_F(SignalingAgentTest, CreateNoInit) {
  CreateAgentNoInit();
}


/*
 * Test for Bug 843595
 */
TEST_P(SignalingTest, missingUfrag)
{
  EnsureInit();

  OfferOptions options;
  std::string offer =
    "v=0\r\n"
    "o=Mozilla-SIPUA 2208 0 IN IP4 0.0.0.0\r\n"
    "s=SIP Call\r\n"
    "t=0 0\r\n"
    "a=ice-pwd:4450d5a4a5f097855c16fa079893be18\r\n"
    "a=fingerprint:sha-256 23:9A:2E:43:94:42:CF:46:68:FC:62:F9:F4:48:61:DB:"
      "2F:8C:C9:FF:6B:25:54:9D:41:09:EF:83:A8:19:FC:B6\r\n"
    "m=audio 56187 RTP/SAVPF 109 0 8 101\r\n"
    "c=IN IP4 77.9.79.167\r\n"
    "a=rtpmap:109 opus/48000/2\r\n"
    "a=ptime:20\r\n"
    "a=rtpmap:0 PCMU/8000\r\n"
    "a=rtpmap:8 PCMA/8000\r\n"
    "a=rtpmap:101 telephone-event/8000\r\n"
    "a=fmtp:101 0-15\r\n"
    "a=sendrecv\r\n"
    "a=candidate:0 1 UDP 2113601791 192.168.178.20 56187 typ host\r\n"
    "a=candidate:1 1 UDP 1694236671 77.9.79.167 56187 typ srflx raddr "
      "192.168.178.20 rport 56187\r\n"
    "a=candidate:0 2 UDP 2113601790 192.168.178.20 52955 typ host\r\n"
    "a=candidate:1 2 UDP 1694236670 77.9.79.167 52955 typ srflx raddr "
      "192.168.178.20 rport 52955\r\n"
    "m=video 49929 RTP/SAVPF 120\r\n"
    "c=IN IP4 77.9.79.167\r\n"
    "a=rtpmap:120 VP8/90000\r\n"
    "a=recvonly\r\n"
    "a=candidate:0 1 UDP 2113601791 192.168.178.20 49929 typ host\r\n"
    "a=candidate:1 1 UDP 1694236671 77.9.79.167 49929 typ srflx raddr "
      "192.168.178.20 rport 49929\r\n"
    "a=candidate:0 2 UDP 2113601790 192.168.178.20 50769 typ host\r\n"
    "a=candidate:1 2 UDP 1694236670 77.9.79.167 50769 typ srflx raddr "
      "192.168.178.20 rport 50769\r\n"
    "m=application 54054 DTLS/SCTP 5000\r\n"
    "c=IN IP4 77.9.79.167\r\n"
    "a=fmtp:HuRUu]Dtcl\\zM,7(OmEU%O$gU]x/z\tD protocol=webrtc-datachannel;"
      "streams=16\r\n"
    "a=sendrecv\r\n";

  // Need to create an offer, since that's currently required by our
  // FSM. This may change in the future.
  a1_->CreateOffer(options, OFFER_AV);
  a1_->SetLocal(TestObserver::OFFER, a1_->offer(), true);
  // We now detect the missing ICE parameters at SetRemoteDescription
  a2_->SetRemote(TestObserver::OFFER, offer, true,
                 PCImplSignalingState::SignalingStable);
  ASSERT_TRUE(a2_->pObserver->state == TestObserver::stateError);
}

TEST_P(SignalingTest, AudioOnlyCalleeNoRtcpMux)
{
  EnsureInit();

  OfferOptions options;

  a1_->CreateOffer(options, OFFER_AUDIO);
  a1_->SetLocal(TestObserver::OFFER, a1_->offer(), false);
  ParsedSDP sdpWrapper(a1_->offer());
  sdpWrapper.DeleteLine("a=rtcp-mux");
  std::cout << "Modified SDP " << std::endl
            << indent(sdpWrapper.getSdp()) << std::endl;
  a2_->SetRemote(TestObserver::OFFER, sdpWrapper.getSdp(), false);
  a2_->CreateAnswer(OFFER_AUDIO | ANSWER_AUDIO);
  a2_->SetLocal(TestObserver::ANSWER, a2_->answer(), false);
  a1_->SetRemote(TestObserver::ANSWER, a2_->answer(), false);

  a1_->mExpectRtcpMuxAudio = false;
  a2_->mExpectRtcpMuxAudio = false;

  // Answer should not have a=rtcp-mux
  ASSERT_EQ(a2_->getLocalDescription().find("\r\na=rtcp-mux"),
            std::string::npos) << "SDP was: " << a2_->getLocalDescription();

  WaitForCompleted();

  CheckPipelines();
  CheckStreams();

  CloseStreams();
}



TEST_P(SignalingTest, AudioOnlyG722Only)
{
  EnsureInit();

  OfferOptions options;

  a1_->CreateOffer(options, OFFER_AUDIO);
  a1_->SetLocal(TestObserver::OFFER, a1_->offer(), false);
  ParsedSDP sdpWrapper(a1_->offer());
  sdpWrapper.ReplaceLine("m=audio",
                         "m=audio 65375 RTP/SAVPF 9\r\n");
  std::cout << "Modified SDP " << std::endl
            << indent(sdpWrapper.getSdp()) << std::endl;
  a2_->SetRemote(TestObserver::OFFER, sdpWrapper.getSdp(), false);
  a2_->CreateAnswer(OFFER_AUDIO | ANSWER_AUDIO);
  a2_->SetLocal(TestObserver::ANSWER, a2_->answer(), false);
  a1_->SetRemote(TestObserver::ANSWER, a2_->answer(), false);
  ASSERT_NE(a2_->getLocalDescription().find("RTP/SAVPF 9\r"), std::string::npos);
  ASSERT_NE(a2_->getLocalDescription().find("a=rtpmap:9 G722/8000"), std::string::npos);

  WaitForCompleted();

  CheckPipelines();
  CheckStreams();

  CloseStreams();
}

TEST_P(SignalingTest, AudioOnlyG722MostPreferred)
{
  EnsureInit();

  OfferOptions options;

  a1_->CreateOffer(options, OFFER_AUDIO);
  a1_->SetLocal(TestObserver::OFFER, a1_->offer(), false);
  ParsedSDP sdpWrapper(a1_->offer());
  sdpWrapper.ReplaceLine("m=audio",
                         "m=audio 65375 RTP/SAVPF 9 0 8 109\r\n");
  std::cout << "Modified SDP " << std::endl
            << indent(sdpWrapper.getSdp()) << std::endl;
  a2_->SetRemote(TestObserver::OFFER, sdpWrapper.getSdp(), false);
  a2_->CreateAnswer(OFFER_AUDIO | ANSWER_AUDIO);
  a2_->SetLocal(TestObserver::ANSWER, a2_->answer(), false);
  a1_->SetRemote(TestObserver::ANSWER, a2_->answer(), false);
  ASSERT_NE(a2_->getLocalDescription().find("RTP/SAVPF 9"), std::string::npos);
  ASSERT_NE(a2_->getLocalDescription().find("a=rtpmap:9 G722/8000"), std::string::npos);

  CheckPipelines();
  CheckStreams();

  CloseStreams();
}

TEST_P(SignalingTest, AudioOnlyG722Rejected)
{
  EnsureInit();

  OfferOptions options;

  a1_->CreateOffer(options, OFFER_AUDIO);
  // creating different SDPs as a workaround for rejecting codecs
  // this way the answerer should pick a codec with lower priority
  a1_->SetLocal(TestObserver::OFFER, a1_->offer(), false);
  ParsedSDP sdpWrapper(a1_->offer());
  sdpWrapper.ReplaceLine("m=audio",
                         "m=audio 65375 RTP/SAVPF 0 8\r\n");
  std::cout << "Modified SDP offer " << std::endl
            << indent(sdpWrapper.getSdp()) << std::endl;
  a2_->SetRemote(TestObserver::OFFER, sdpWrapper.getSdp(), false);
  a2_->CreateAnswer(OFFER_AUDIO | ANSWER_AUDIO);
  a2_->SetLocal(TestObserver::ANSWER, a2_->answer(), false);
  a1_->SetRemote(TestObserver::ANSWER, a2_->answer(), false);
  // TODO(bug 1099351): Use commented out code instead.
  ASSERT_NE(a2_->getLocalDescription().find("RTP/SAVPF 0\r"), std::string::npos);
  // ASSERT_NE(a2_->getLocalDescription().find("RTP/SAVPF 0 8\r"), std::string::npos);
  ASSERT_NE(a2_->getLocalDescription().find("a=rtpmap:0 PCMU/8000"), std::string::npos);
  ASSERT_EQ(a2_->getLocalDescription().find("a=rtpmap:109 opus/48000/2"), std::string::npos);
  ASSERT_EQ(a2_->getLocalDescription().find("a=rtpmap:9 G722/8000"), std::string::npos);

  CheckPipelines();
  CheckStreams();

  CloseStreams();
}

TEST_P(SignalingTest, FullCallAudioNoMuxVideoMux)
{
  if (GetParam() == "bundle") {
    // This test doesn't make sense for bundle
    return;
  }

  EnsureInit();

  OfferOptions options;

  a1_->CreateOffer(options, OFFER_AV);
  a1_->SetLocal(TestObserver::OFFER, a1_->offer(), false);
  ParsedSDP sdpWrapper(a1_->offer());
  sdpWrapper.DeleteLine("a=rtcp-mux");
  std::cout << "Modified SDP " << std::endl
            << indent(sdpWrapper.getSdp()) << std::endl;
  a2_->SetRemote(TestObserver::OFFER, sdpWrapper.getSdp(), false);
  a2_->CreateAnswer(OFFER_AV | ANSWER_AV);
  a2_->SetLocal(TestObserver::ANSWER, a2_->answer(), false);
  a1_->SetRemote(TestObserver::ANSWER, a2_->answer(), false);

  // Answer should have only one a=rtcp-mux line
  size_t match = a2_->getLocalDescription().find("\r\na=rtcp-mux");
  ASSERT_NE(match, std::string::npos);
  match = a2_->getLocalDescription().find("\r\na=rtcp-mux", match + 1);
  ASSERT_EQ(match, std::string::npos);

  a1_->mExpectRtcpMuxAudio = false;
  a2_->mExpectRtcpMuxAudio = false;

  WaitForCompleted();
  CheckPipelines();
  CheckStreams();

  CloseStreams();
}

// TODO: Move to jsep_sesion_unittest
TEST_P(SignalingTest, RtcpFbInOffer)
{
  EnsureInit();
  OfferOptions options;
  a1_->CreateOffer(options, OFFER_AV);
  const char *expected[] = { "nack", "nack pli", "ccm fir" };
  CheckRtcpFbSdp(a1_->offer(), ARRAY_TO_SET(std::string, expected));
}

TEST_P(SignalingTest, RtcpFbOfferAll)
{
  const char *feedbackTypes[] = { "nack", "nack pli", "ccm fir" };
  TestRtcpFbOffer(ARRAY_TO_SET(std::string, feedbackTypes),
                  true,
                  VideoSessionConduit::FrameRequestPli);
}

TEST_P(SignalingTest, RtcpFbOfferNoNackBasic)
{
  const char *feedbackTypes[] = { "nack pli", "ccm fir" };
  TestRtcpFbOffer(ARRAY_TO_SET(std::string, feedbackTypes),
                  false,
                  VideoSessionConduit::FrameRequestPli);
}

TEST_P(SignalingTest, RtcpFbOfferNoNackPli)
{
  const char *feedbackTypes[] = { "nack", "ccm fir" };
  TestRtcpFbOffer(ARRAY_TO_SET(std::string, feedbackTypes),
                  true,
                  VideoSessionConduit::FrameRequestFir);
}

TEST_P(SignalingTest, RtcpFbOfferNoCcmFir)
{
  const char *feedbackTypes[] = { "nack", "nack pli" };
  TestRtcpFbOffer(ARRAY_TO_SET(std::string, feedbackTypes),
                  true,
                  VideoSessionConduit::FrameRequestPli);
}

TEST_P(SignalingTest, RtcpFbOfferNoNack)
{
  const char *feedbackTypes[] = { "ccm fir" };
  TestRtcpFbOffer(ARRAY_TO_SET(std::string, feedbackTypes),
                  false,
                  VideoSessionConduit::FrameRequestFir);
}

TEST_P(SignalingTest, RtcpFbOfferNoFrameRequest)
{
  const char *feedbackTypes[] = { "nack" };
  TestRtcpFbOffer(ARRAY_TO_SET(std::string, feedbackTypes),
                  true,
                  VideoSessionConduit::FrameRequestNone);
}

TEST_P(SignalingTest, RtcpFbOfferPliOnly)
{
  const char *feedbackTypes[] = { "nack pli" };
  TestRtcpFbOffer(ARRAY_TO_SET(std::string, feedbackTypes),
                  false,
                  VideoSessionConduit::FrameRequestPli);
}

TEST_P(SignalingTest, RtcpFbOfferNoFeedback)
{
  const char *feedbackTypes[] = { };
  TestRtcpFbOffer(ARRAY_TO_SET(std::string, feedbackTypes),
                  false,
                  VideoSessionConduit::FrameRequestNone);
}

TEST_P(SignalingTest, RtcpFbAnswerAll)
{
  const char *feedbackTypes[] = { "nack", "nack pli", "ccm fir" };
  TestRtcpFbAnswer(ARRAY_TO_SET(std::string, feedbackTypes),
                  true,
                  VideoSessionConduit::FrameRequestPli);
}

TEST_P(SignalingTest, RtcpFbAnswerNoNackBasic)
{
  const char *feedbackTypes[] = { "nack pli", "ccm fir" };
  TestRtcpFbAnswer(ARRAY_TO_SET(std::string, feedbackTypes),
                  false,
                  VideoSessionConduit::FrameRequestPli);
}

TEST_P(SignalingTest, RtcpFbAnswerNoNackPli)
{
  const char *feedbackTypes[] = { "nack", "ccm fir" };
  TestRtcpFbAnswer(ARRAY_TO_SET(std::string, feedbackTypes),
                  true,
                  VideoSessionConduit::FrameRequestFir);
}

TEST_P(SignalingTest, RtcpFbAnswerNoCcmFir)
{
  const char *feedbackTypes[] = { "nack", "nack pli" };
  TestRtcpFbAnswer(ARRAY_TO_SET(std::string, feedbackTypes),
                  true,
                  VideoSessionConduit::FrameRequestPli);
}

TEST_P(SignalingTest, RtcpFbAnswerNoNack)
{
  const char *feedbackTypes[] = { "ccm fir" };
  TestRtcpFbAnswer(ARRAY_TO_SET(std::string, feedbackTypes),
                  false,
                  VideoSessionConduit::FrameRequestFir);
}

TEST_P(SignalingTest, RtcpFbAnswerNoFrameRequest)
{
  const char *feedbackTypes[] = { "nack" };
  TestRtcpFbAnswer(ARRAY_TO_SET(std::string, feedbackTypes),
                  true,
                  VideoSessionConduit::FrameRequestNone);
}

TEST_P(SignalingTest, RtcpFbAnswerPliOnly)
{
  const char *feedbackTypes[] = { "nack pli" };
  TestRtcpFbAnswer(ARRAY_TO_SET(std::string, feedbackTypes),
                  0,
                  VideoSessionConduit::FrameRequestPli);
}

TEST_P(SignalingTest, RtcpFbAnswerNoFeedback)
{
  const char *feedbackTypes[] = { };
  TestRtcpFbAnswer(ARRAY_TO_SET(std::string, feedbackTypes),
                  0,
                  VideoSessionConduit::FrameRequestNone);
}

// In this test we will change the offer SDP's a=setup value
// from actpass to passive.  This will make the answer do active.
TEST_P(SignalingTest, AudioCallForceDtlsRoles)
{
  EnsureInit();

  OfferOptions options;
  size_t match;

  a1_->CreateOffer(options, OFFER_AUDIO);

  // By default the offer should give actpass
  std::string offer(a1_->offer());
  match = offer.find("\r\na=setup:actpass");
  ASSERT_NE(match, std::string::npos);
  // Now replace the actpass with passive so that the answer will
  // return active
  offer.replace(match, strlen("\r\na=setup:actpass"),
    "\r\na=setup:passive");
  std::cout << "Modified SDP " << std::endl
            << indent(offer) << std::endl;

  a1_->SetLocal(TestObserver::OFFER, offer, false);
  a2_->SetRemote(TestObserver::OFFER, offer, false);
  a2_->CreateAnswer(OFFER_AUDIO | ANSWER_AUDIO);

  // Now the answer should contain a=setup:active
  std::string answer(a2_->answer());
  match = answer.find("\r\na=setup:active");
  ASSERT_NE(match, std::string::npos);

  // This should setup the DTLS with the same roles
  // as the regular tests above.
  a2_->SetLocal(TestObserver::ANSWER, a2_->answer(), false);
  a1_->SetRemote(TestObserver::ANSWER, a2_->answer(), false);

  WaitForCompleted();

  CheckPipelines();
  CheckStreams();

  CloseStreams();
}

// In this test we will change the offer SDP's a=setup value
// from actpass to active.  This will make the answer do passive
TEST_P(SignalingTest, AudioCallReverseDtlsRoles)
{
  EnsureInit();

  OfferOptions options;
  size_t match;

  a1_->CreateOffer(options, OFFER_AUDIO);

  // By default the offer should give actpass
  std::string offer(a1_->offer());
  match = offer.find("\r\na=setup:actpass");
  ASSERT_NE(match, std::string::npos);
  // Now replace the actpass with active so that the answer will
  // return passive
  offer.replace(match, strlen("\r\na=setup:actpass"),
    "\r\na=setup:active");
  std::cout << "Modified SDP " << std::endl
            << indent(offer) << std::endl;

  a1_->SetLocal(TestObserver::OFFER, offer, false);
  a2_->SetRemote(TestObserver::OFFER, offer, false);
  a2_->CreateAnswer(OFFER_AUDIO | ANSWER_AUDIO);

  // Now the answer should contain a=setup:passive
  std::string answer(a2_->answer());
  match = answer.find("\r\na=setup:passive");
  ASSERT_NE(match, std::string::npos);

  // This should setup the DTLS with the opposite roles
  // than the regular tests above.
  a2_->SetLocal(TestObserver::ANSWER, a2_->answer(), false);
  a1_->SetRemote(TestObserver::ANSWER, a2_->answer(), false);

  WaitForCompleted();

  CheckPipelines();
  CheckStreams();

  CloseStreams();
}

// In this test we will change the answer SDP's a=setup value
// from active to passive.  This will make both sides do
// active and should not connect.
TEST_P(SignalingTest, AudioCallMismatchDtlsRoles)
{
  EnsureInit();

  OfferOptions options;
  size_t match;

  a1_->CreateOffer(options, OFFER_AUDIO);

  // By default the offer should give actpass
  std::string offer(a1_->offer());
  match = offer.find("\r\na=setup:actpass");
  ASSERT_NE(match, std::string::npos);
  a1_->SetLocal(TestObserver::OFFER, offer, false);
  a2_->SetRemote(TestObserver::OFFER, offer, false);
  a2_->CreateAnswer(OFFER_AUDIO | ANSWER_AUDIO);

  // Now the answer should contain a=setup:active
  std::string answer(a2_->answer());
  match = answer.find("\r\na=setup:active");
  ASSERT_NE(match, std::string::npos);
  a2_->SetLocal(TestObserver::ANSWER, answer, false);

  // Now replace the active with passive so that the offerer will
  // also do active.
  answer.replace(match, strlen("\r\na=setup:active"),
    "\r\na=setup:passive");
  std::cout << "Modified SDP " << std::endl
            << indent(answer) << std::endl;

  // This should setup the DTLS with both sides playing active
  a1_->SetRemote(TestObserver::ANSWER, answer, false);

  WaitForCompleted();

  // Not using ASSERT_TRUE_WAIT here because we expect failure
  PR_Sleep(500); // Wait for some data to get written

  CloseStreams();

  ASSERT_GE(a1_->GetPacketsSent(0), 4);
  // In this case we should receive nothing.
  ASSERT_EQ(a2_->GetPacketsReceived(0), 0);
}

// In this test we will change the offer SDP's a=setup value
// from actpass to garbage.  It should ignore the garbage value
// and respond with setup:active
TEST_P(SignalingTest, AudioCallGarbageSetup)
{
  EnsureInit();

  OfferOptions options;
  size_t match;

  a1_->CreateOffer(options, OFFER_AUDIO);

  // By default the offer should give actpass
  std::string offer(a1_->offer());
  match = offer.find("\r\na=setup:actpass");
  ASSERT_NE(match, std::string::npos);
  // Now replace the actpass with a garbage value
  offer.replace(match, strlen("\r\na=setup:actpass"),
    "\r\na=setup:G4rb4g3V4lu3");
  std::cout << "Modified SDP " << std::endl
            << indent(offer) << std::endl;

  a1_->SetLocal(TestObserver::OFFER, offer, false);
  a2_->SetRemote(TestObserver::OFFER, offer, false);
  a2_->CreateAnswer(OFFER_AUDIO | ANSWER_AUDIO);

  // Now the answer should contain a=setup:active
  std::string answer(a2_->answer());
  match = answer.find("\r\na=setup:active");
  ASSERT_NE(match, std::string::npos);

  // This should setup the DTLS with the same roles
  // as the regular tests above.
  a2_->SetLocal(TestObserver::ANSWER, a2_->answer(), false);
  a1_->SetRemote(TestObserver::ANSWER, a2_->answer(), false);

  WaitForCompleted();

  CheckPipelines();
  CheckStreams();

  CloseStreams();
}

// In this test we will change the offer SDP to remove the
// a=setup line.  Answer should respond with a=setup:active.
TEST_P(SignalingTest, AudioCallOfferNoSetupOrConnection)
{
  EnsureInit();

  OfferOptions options;
  size_t match;

  a1_->CreateOffer(options, OFFER_AUDIO);

  std::string offer(a1_->offer());
  a1_->SetLocal(TestObserver::OFFER, offer, false);

  // By default the offer should give setup:actpass
  match = offer.find("\r\na=setup:actpass");
  ASSERT_NE(match, std::string::npos);
  // Remove the a=setup line
  offer.replace(match, strlen("\r\na=setup:actpass"), "");
  std::cout << "Modified SDP " << std::endl
            << indent(offer) << std::endl;

  a2_->SetRemote(TestObserver::OFFER, offer, false);
  a2_->CreateAnswer(OFFER_AUDIO | ANSWER_AUDIO);

  // Now the answer should contain a=setup:active
  std::string answer(a2_->answer());
  match = answer.find("\r\na=setup:active");
  ASSERT_NE(match, std::string::npos);

  // This should setup the DTLS with the same roles
  // as the regular tests above.
  a2_->SetLocal(TestObserver::ANSWER, a2_->answer(), false);
  a1_->SetRemote(TestObserver::ANSWER, a2_->answer(), false);

  WaitForCompleted();

  CheckPipelines();
  CheckStreams();

  CloseStreams();
}

// In this test we will change the answer SDP to remove the
// a=setup line.  ICE should still connect since active will
// be assumed.
TEST_P(SignalingTest, AudioCallAnswerNoSetupOrConnection)
{
  EnsureInit();

  OfferOptions options;
  size_t match;

  a1_->CreateOffer(options, OFFER_AUDIO);

  // By default the offer should give setup:actpass
  std::string offer(a1_->offer());
  match = offer.find("\r\na=setup:actpass");
  ASSERT_NE(match, std::string::npos);

  a1_->SetLocal(TestObserver::OFFER, offer, false);
  a2_->SetRemote(TestObserver::OFFER, offer, false);
  a2_->CreateAnswer(OFFER_AUDIO | ANSWER_AUDIO);

  // Now the answer should contain a=setup:active
  std::string answer(a2_->answer());
  match = answer.find("\r\na=setup:active");
  ASSERT_NE(match, std::string::npos);
  // Remove the a=setup line
  answer.replace(match, strlen("\r\na=setup:active"), "");
  std::cout << "Modified SDP " << std::endl
            << indent(answer) << std::endl;

  // This should setup the DTLS with the same roles
  // as the regular tests above.
  a2_->SetLocal(TestObserver::ANSWER, answer, false);
  a1_->SetRemote(TestObserver::ANSWER, answer, false);

  WaitForCompleted();

  CheckPipelines();
  CheckStreams();

  CloseStreams();
}


TEST_P(SignalingTest, FullCallRealTrickle)
{
  OfferOptions options;
  OfferAnswer(options, OFFER_AV | ANSWER_AV);
  CloseStreams();
}

TEST_P(SignalingTest, FullCallRealTrickleTestServer)
{
  SetTestStunServer();

  OfferOptions options;
  OfferAnswer(options, OFFER_AV | ANSWER_AV);

  TestStunServer::GetInstance()->SetActive(true);

  CloseStreams();
}

TEST_P(SignalingTest, hugeSdp)
{
  EnsureInit();

  OfferOptions options;
  std::string offer =
    "v=0\r\n"
    "o=- 1109973417102828257 2 IN IP4 127.0.0.1\r\n"
    "s=-\r\n"
    "t=0 0\r\n"
    "a=group:BUNDLE audio video\r\n"
    "a=msid-semantic: WMS 1PBxet5BYh0oYodwsvNM4k6KiO2eWCX40VIP\r\n"
    "m=audio 32952 RTP/SAVPF 111 103 104 0 8 107 106 105 13 126\r\n"
    "c=IN IP4 128.64.32.16\r\n"
    "a=rtcp:32952 IN IP4 128.64.32.16\r\n"
    "a=candidate:77142221 1 udp 2113937151 192.168.137.1 54081 typ host generation 0\r\n"
    "a=candidate:77142221 2 udp 2113937151 192.168.137.1 54081 typ host generation 0\r\n"
    "a=candidate:983072742 1 udp 2113937151 172.22.0.56 54082 typ host generation 0\r\n"
    "a=candidate:983072742 2 udp 2113937151 172.22.0.56 54082 typ host generation 0\r\n"
    "a=candidate:2245074553 1 udp 1845501695 32.64.128.1 62397 typ srflx raddr 192.168.137.1 rport 54081 generation 0\r\n"
    "a=candidate:2245074553 2 udp 1845501695 32.64.128.1 62397 typ srflx raddr 192.168.137.1 rport 54081 generation 0\r\n"
    "a=candidate:2479353907 1 udp 1845501695 32.64.128.1 54082 typ srflx raddr 172.22.0.56 rport 54082 generation 0\r\n"
    "a=candidate:2479353907 2 udp 1845501695 32.64.128.1 54082 typ srflx raddr 172.22.0.56 rport 54082 generation 0\r\n"
    "a=candidate:1243276349 1 tcp 1509957375 192.168.137.1 0 typ host generation 0\r\n"
    "a=candidate:1243276349 2 tcp 1509957375 192.168.137.1 0 typ host generation 0\r\n"
    "a=candidate:1947960086 1 tcp 1509957375 172.22.0.56 0 typ host generation 0\r\n"
    "a=candidate:1947960086 2 tcp 1509957375 172.22.0.56 0 typ host generation 0\r\n"
    "a=candidate:1808221584 1 udp 33562367 128.64.32.16 32952 typ relay raddr 32.64.128.1 rport 62398 generation 0\r\n"
    "a=candidate:1808221584 2 udp 33562367 128.64.32.16 32952 typ relay raddr 32.64.128.1 rport 62398 generation 0\r\n"
    "a=candidate:507872740 1 udp 33562367 128.64.32.16 40975 typ relay raddr 32.64.128.1 rport 54085 generation 0\r\n"
    "a=candidate:507872740 2 udp 33562367 128.64.32.16 40975 typ relay raddr 32.64.128.1 rport 54085 generation 0\r\n"
    "a=ice-ufrag:xQuJwjX3V3eMA81k\r\n"
    "a=ice-pwd:ZUiRmjS2GDhG140p73dAsSVP\r\n"
    "a=ice-options:google-ice\r\n"
    "a=fingerprint:sha-256 59:4A:8B:73:A7:73:53:71:88:D7:4D:58:28:0C:79:72:31:29:9B:05:37:DD:58:43:C2:D4:85:A2:B3:66:38:7A\r\n"
    "a=setup:active\r\n"
    "a=extmap:1 urn:ietf:params:rtp-hdrext:ssrc-audio-level\r\n"
    "a=sendrecv\r\n"
    "a=mid:audio\r\n"
    "a=rtcp-mux\r\n"
    "a=crypto:1 AES_CM_128_HMAC_SHA1_80 inline:/U44g3ULdtapeiSg+T3n6dDLBKIjpOhb/NXAL/2b\r\n"
    "a=rtpmap:111 opus/48000/2\r\n"
    "a=fmtp:111 minptime=10\r\n"
    "a=rtpmap:103 ISAC/16000\r\n"
    "a=rtpmap:104 ISAC/32000\r\n"
    "a=rtpmap:0 PCMU/8000\r\n"
    "a=rtpmap:8 PCMA/8000\r\n"
    "a=rtpmap:107 CN/48000\r\n"
    "a=rtpmap:106 CN/32000\r\n"
    "a=rtpmap:105 CN/16000\r\n"
    "a=rtpmap:13 CN/8000\r\n"
    "a=rtpmap:126 telephone-event/8000\r\n"
    "a=maxptime:60\r\n"
    "a=ssrc:2271517329 cname:mKDNt7SQf6pwDlIn\r\n"
    "a=ssrc:2271517329 msid:1PBxet5BYh0oYodwsvNM4k6KiO2eWCX40VIP 1PBxet5BYh0oYodwsvNM4k6KiO2eWCX40VIPa0\r\n"
    "a=ssrc:2271517329 mslabel:1PBxet5BYh0oYodwsvNM4k6KiO2eWCX40VIP\r\n"
    "a=ssrc:2271517329 label:1PBxet5BYh0oYodwsvNM4k6KiO2eWCX40VIPa0\r\n"
    "m=video 32952 RTP/SAVPF 100 116 117\r\n"
    "c=IN IP4 128.64.32.16\r\n"
    "a=rtcp:32952 IN IP4 128.64.32.16\r\n"
    "a=candidate:77142221 1 udp 2113937151 192.168.137.1 54081 typ host generation 0\r\n"
    "a=candidate:77142221 2 udp 2113937151 192.168.137.1 54081 typ host generation 0\r\n"
    "a=candidate:983072742 1 udp 2113937151 172.22.0.56 54082 typ host generation 0\r\n"
    "a=candidate:983072742 2 udp 2113937151 172.22.0.56 54082 typ host generation 0\r\n"
    "a=candidate:2245074553 1 udp 1845501695 32.64.128.1 62397 typ srflx raddr 192.168.137.1 rport 54081 generation 0\r\n"
    "a=candidate:2245074553 2 udp 1845501695 32.64.128.1 62397 typ srflx raddr 192.168.137.1 rport 54081 generation 0\r\n"
    "a=candidate:2479353907 1 udp 1845501695 32.64.128.1 54082 typ srflx raddr 172.22.0.56 rport 54082 generation 0\r\n"
    "a=candidate:2479353907 2 udp 1845501695 32.64.128.1 54082 typ srflx raddr 172.22.0.56 rport 54082 generation 0\r\n"
    "a=candidate:1243276349 1 tcp 1509957375 192.168.137.1 0 typ host generation 0\r\n"
    "a=candidate:1243276349 2 tcp 1509957375 192.168.137.1 0 typ host generation 0\r\n"
    "a=candidate:1947960086 1 tcp 1509957375 172.22.0.56 0 typ host generation 0\r\n"
    "a=candidate:1947960086 2 tcp 1509957375 172.22.0.56 0 typ host generation 0\r\n"
    "a=candidate:1808221584 1 udp 33562367 128.64.32.16 32952 typ relay raddr 32.64.128.1 rport 62398 generation 0\r\n"
    "a=candidate:1808221584 2 udp 33562367 128.64.32.16 32952 typ relay raddr 32.64.128.1 rport 62398 generation 0\r\n"
    "a=candidate:507872740 1 udp 33562367 128.64.32.16 40975 typ relay raddr 32.64.128.1 rport 54085 generation 0\r\n"
    "a=candidate:507872740 2 udp 33562367 128.64.32.16 40975 typ relay raddr 32.64.128.1 rport 54085 generation 0\r\n"
    "a=ice-ufrag:xQuJwjX3V3eMA81k\r\n"
    "a=ice-pwd:ZUiRmjS2GDhG140p73dAsSVP\r\n"
    "a=ice-options:google-ice\r\n"
    "a=fingerprint:sha-256 59:4A:8B:73:A7:73:53:71:88:D7:4D:58:28:0C:79:72:31:29:9B:05:37:DD:58:43:C2:D4:85:A2:B3:66:38:7A\r\n"
    "a=setup:active\r\n"
    "a=extmap:2 urn:ietf:params:rtp-hdrext:toffset\r\n"
    "a=extmap:3 http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time\r\n"
    "a=sendrecv\r\n"
    "a=mid:video\r\n"
    "a=rtcp-mux\r\n"
    "a=crypto:1 AES_CM_128_HMAC_SHA1_80 inline:/U44g3ULdtapeiSg+T3n6dDLBKIjpOhb/NXAL/2b\r\n"
    "a=rtpmap:100 VP8/90000\r\n"
    "a=rtcp-fb:100 ccm fir\r\n"
    "a=rtcp-fb:100 nack\r\n"
    "a=rtcp-fb:100 goog-remb\r\n"
    "a=rtpmap:116 red/90000\r\n"
    "a=rtpmap:117 ulpfec/90000\r\n"
    "a=ssrc:54724160 cname:mKDNt7SQf6pwDlIn\r\n"
    "a=ssrc:54724160 msid:1PBxet5BYh0oYodwsvNM4k6KiO2eWCX40VIP 1PBxet5BYh0oYodwsvNM4k6KiO2eWCX40VIPv0\r\n"
    "a=ssrc:54724160 mslabel:1PBxet5BYh0oYodwsvNM4k6KiO2eWCX40VIP\r\n"
    "a=ssrc:54724160 label:1PBxet5BYh0oYodwsvNM4k6KiO2eWCX40VIPv0\r\n";

  a1_->CreateOffer(options, OFFER_AV);
  a1_->SetLocal(TestObserver::OFFER, a1_->offer(), true);

  a2_->SetRemote(TestObserver::OFFER, offer, true);
  ASSERT_GE(a2_->getRemoteDescription().length(), 4096U);
  a2_->CreateAnswer(OFFER_AV);
}

#if !defined(MOZILLA_XPCOMRT_API)
// FIXME XPCOMRT doesn't support nsPrefService
// See Bug 1129188 - Create standalone libpref for use in standalone WebRTC

// Test max_fs and max_fr prefs have proper impact on SDP offer
TEST_P(SignalingTest, MaxFsFrInOffer)
{
  EnsureInit();

  OfferOptions options;

  nsCOMPtr<nsIPrefBranch> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
  ASSERT_TRUE(prefs);
  FsFrPrefClearer prefClearer(prefs);

  SetMaxFsFr(prefs, 300, 30);

  a1_->CreateOffer(options, OFFER_AV);

  // Verify that SDP contains correct max-fs and max-fr
  CheckMaxFsFrSdp(a1_->offer(), 120, 300, 30);
}

// Test max_fs and max_fr prefs have proper impact on SDP answer
TEST_P(SignalingTest, MaxFsFrInAnswer)
{
  EnsureInit();

  OfferOptions options;

  nsCOMPtr<nsIPrefBranch> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
  ASSERT_TRUE(prefs);
  FsFrPrefClearer prefClearer(prefs);

  a1_->CreateOffer(options, OFFER_AV);

  SetMaxFsFr(prefs, 600, 60);

  a2_->SetRemote(TestObserver::OFFER, a1_->offer());

  a2_->CreateAnswer(OFFER_AV | ANSWER_AV);

  // Verify that SDP contains correct max-fs and max-fr
  CheckMaxFsFrSdp(a2_->answer(), 120, 600, 60);
}

// Test SDP offer has proper impact on callee's codec configuration
TEST_P(SignalingTest, MaxFsFrCalleeCodec)
{
  EnsureInit();

  OfferOptions options;

  nsCOMPtr<nsIPrefBranch> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
  ASSERT_TRUE(prefs);
  FsFrPrefClearer prefClearer(prefs);

  SetMaxFsFr(prefs, 300, 30);
  a1_->CreateOffer(options, OFFER_AV);

  CheckMaxFsFrSdp(a1_->offer(), 120, 300, 30);

  a1_->SetLocal(TestObserver::OFFER, a1_->offer());

  SetMaxFsFr(prefs, 3601, 31);
  a2_->SetRemote(TestObserver::OFFER, a1_->offer());

  a2_->CreateAnswer(OFFER_AV | ANSWER_AV);

  CheckMaxFsFrSdp(a2_->answer(), 120, 3601, 31);

  a2_->SetLocal(TestObserver::ANSWER, a2_->answer());
  a1_->SetRemote(TestObserver::ANSWER, a2_->answer());

  WaitForCompleted();

  CheckPipelines();
  CheckStreams();

  // Checking callee's video sending configuration does respect max-fs and
  // max-fr in SDP offer.
  mozilla::RefPtr<mozilla::MediaPipeline> pipeline =
    a2_->GetMediaPipeline(1, 0, 1);
  ASSERT_TRUE(pipeline);
  mozilla::MediaSessionConduit *conduit = pipeline->Conduit();
  ASSERT_TRUE(conduit);
  ASSERT_EQ(conduit->type(), mozilla::MediaSessionConduit::VIDEO);
  mozilla::VideoSessionConduit *video_conduit =
    static_cast<mozilla::VideoSessionConduit*>(conduit);

  ASSERT_EQ(video_conduit->SendingMaxFs(), (unsigned short) 300);
  ASSERT_EQ(video_conduit->SendingMaxFr(), (unsigned short) 30);
}

// Test SDP answer has proper impact on caller's codec configuration
TEST_P(SignalingTest, MaxFsFrCallerCodec)
{
  EnsureInit();

  OfferOptions options;

  nsCOMPtr<nsIPrefBranch> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
  ASSERT_TRUE(prefs);
  FsFrPrefClearer prefClearer(prefs);

  a1_->CreateOffer(options, OFFER_AV);
  a1_->SetLocal(TestObserver::OFFER, a1_->offer());

  SetMaxFsFr(prefs, 600, 60);
  a2_->SetRemote(TestObserver::OFFER, a1_->offer());

  a2_->CreateAnswer(OFFER_AV | ANSWER_AV);

  // Double confirm that SDP answer contains correct max-fs and max-fr
  CheckMaxFsFrSdp(a2_->answer(), 120, 600, 60);

  a2_->SetLocal(TestObserver::ANSWER, a2_->answer());
  a1_->SetRemote(TestObserver::ANSWER, a2_->answer());

  WaitForCompleted();

  CheckPipelines();
  CheckStreams();

  // Checking caller's video sending configuration does respect max-fs and
  // max-fr in SDP answer.
  mozilla::RefPtr<mozilla::MediaPipeline> pipeline =
    a1_->GetMediaPipeline(1, 0, 1);
  ASSERT_TRUE(pipeline);
  mozilla::MediaSessionConduit *conduit = pipeline->Conduit();
  ASSERT_TRUE(conduit);
  ASSERT_EQ(conduit->type(), mozilla::MediaSessionConduit::VIDEO);
  mozilla::VideoSessionConduit *video_conduit =
    static_cast<mozilla::VideoSessionConduit*>(conduit);

  ASSERT_EQ(video_conduit->SendingMaxFs(), (unsigned short) 600);
  ASSERT_EQ(video_conduit->SendingMaxFr(), (unsigned short) 60);
}
#endif // !defined(MOZILLA_XPCOMRT_API)

// Validate offer with multiple video codecs
TEST_P(SignalingTest, ValidateMultipleVideoCodecsInOffer)
{
  EnsureInit();
  OfferOptions options;

  a1_->CreateOffer(options, OFFER_AV);
  std::string offer = a1_->offer();

#ifdef H264_P0_SUPPORTED
  ASSERT_NE(offer.find("RTP/SAVPF 120 126 97"), std::string::npos);
#else
  ASSERT_NE(offer.find("RTP/SAVPF 120 126"), std::string::npos);
#endif
  ASSERT_NE(offer.find("a=rtpmap:120 VP8/90000"), std::string::npos);
  ASSERT_NE(offer.find("a=rtpmap:126 H264/90000"), std::string::npos);
  ASSERT_NE(offer.find("a=fmtp:126 profile-level-id="), std::string::npos);
  ASSERT_NE(offer.find("a=rtcp-fb:120 nack"), std::string::npos);
  ASSERT_NE(offer.find("a=rtcp-fb:120 nack pli"), std::string::npos);
  ASSERT_NE(offer.find("a=rtcp-fb:120 ccm fir"), std::string::npos);
  ASSERT_NE(offer.find("a=rtcp-fb:126 nack"), std::string::npos);
  ASSERT_NE(offer.find("a=rtcp-fb:126 nack pli"), std::string::npos);
  ASSERT_NE(offer.find("a=rtcp-fb:126 ccm fir"), std::string::npos);
#ifdef H264_P0_SUPPORTED
  ASSERT_NE(offer.find("a=rtpmap:97 H264/90000"), std::string::npos);
  ASSERT_NE(offer.find("a=fmtp:97 profile-level-id="), std::string::npos);
  ASSERT_NE(offer.find("a=rtcp-fb:97 nack"), std::string::npos);
  ASSERT_NE(offer.find("a=rtcp-fb:97 nack pli"), std::string::npos);
  ASSERT_NE(offer.find("a=rtcp-fb:97 ccm fir"), std::string::npos);
#endif
}

// Remove VP8 from offer and check that answer negotiates H264 P1 correctly and ignores unknown params
TEST_P(SignalingTest, RemoveVP8FromOfferWithP1First)
{
  EnsureInit();

  OfferOptions options;
  size_t match;

  a1_->CreateOffer(options, OFFER_AV);

  // Remove VP8 from offer
  std::string offer = a1_->offer();
  match = offer.find("RTP/SAVPF 120");
  ASSERT_NE(std::string::npos, match);
  offer.replace(match, strlen("RTP/SAVPF 120"), "RTP/SAVPF");

  match = offer.find("profile-level-id");
  ASSERT_NE(std::string::npos, match);
  offer.replace(match,
                strlen("profile-level-id"),
                "max-foo=1234;profile-level-id");

  ParsedSDP sdpWrapper(offer);
  sdpWrapper.DeleteLines("a=rtcp-fb:120");
  sdpWrapper.DeleteLine("a=rtpmap:120");

  std::cout << "Modified SDP " << std::endl
            << indent(sdpWrapper.getSdp()) << std::endl;

  // P1 should be offered first
  ASSERT_NE(offer.find("RTP/SAVPF 126"), std::string::npos);

  a1_->SetLocal(TestObserver::OFFER, sdpWrapper.getSdp());
  a2_->SetRemote(TestObserver::OFFER, sdpWrapper.getSdp(), false);
  a2_->CreateAnswer(OFFER_AV|ANSWER_AV);

  std::string answer(a2_->answer());

  // Validate answer SDP
  ASSERT_NE(answer.find("RTP/SAVPF 126"), std::string::npos);
  ASSERT_NE(answer.find("a=rtpmap:126 H264/90000"), std::string::npos);
  ASSERT_NE(answer.find("a=rtcp-fb:126 nack"), std::string::npos);
  ASSERT_NE(answer.find("a=rtcp-fb:126 nack pli"), std::string::npos);
  ASSERT_NE(answer.find("a=rtcp-fb:126 ccm fir"), std::string::npos);
  // Ensure VP8 removed
  ASSERT_EQ(answer.find("a=rtpmap:120 VP8/90000"), std::string::npos);
  ASSERT_EQ(answer.find("a=rtcp-fb:120"), std::string::npos);
}

// Insert H.264 before VP8 in Offer, check answer selects H.264
TEST_P(SignalingTest, OfferWithH264BeforeVP8)
{
  EnsureInit();

  OfferOptions options;
  size_t match;

  a1_->CreateOffer(options, OFFER_AV);

  // Swap VP8 and P1 in offer
  std::string offer = a1_->offer();
#ifdef H264_P0_SUPPORTED
  match = offer.find("RTP/SAVPF 120 126 97");
  ASSERT_NE(std::string::npos, match);
  offer.replace(match,
                strlen("RTP/SAVPF 126 120 97"),
                "RTP/SAVPF 126 120 97");
#else
  match = offer.find("RTP/SAVPF 120 126");
  ASSERT_NE(std::string::npos, match);
  offer.replace(match,
                strlen("RTP/SAVPF 126 120"),
                "RTP/SAVPF 126 120");
#endif

  match = offer.find("a=rtpmap:126 H264/90000");
  ASSERT_NE(std::string::npos, match);
  offer.replace(match,
                strlen("a=rtpmap:120 VP8/90000"),
                "a=rtpmap:120 VP8/90000");

  match = offer.find("a=rtpmap:120 VP8/90000");
  ASSERT_NE(std::string::npos, match);
  offer.replace(match,
                strlen("a=rtpmap:126 H264/90000"),
                "a=rtpmap:126 H264/90000");

  std::cout << "Modified SDP " << std::endl
            << indent(offer) << std::endl;

  // P1 should be offered first
#ifdef H264_P0_SUPPORTED
  ASSERT_NE(offer.find("RTP/SAVPF 126 120 97"), std::string::npos);
#else
  ASSERT_NE(offer.find("RTP/SAVPF 126 120"), std::string::npos);
#endif

  a1_->SetLocal(TestObserver::OFFER, offer);
  a2_->SetRemote(TestObserver::OFFER, offer, false);
  a2_->CreateAnswer(OFFER_AV|ANSWER_AV);

  std::string answer(a2_->answer());

  // Validate answer SDP
  ASSERT_NE(answer.find("RTP/SAVPF 126"), std::string::npos);
  ASSERT_NE(answer.find("a=rtpmap:126 H264/90000"), std::string::npos);
  ASSERT_NE(answer.find("a=rtcp-fb:126 nack"), std::string::npos);
  ASSERT_NE(answer.find("a=rtcp-fb:126 nack pli"), std::string::npos);
  ASSERT_NE(answer.find("a=rtcp-fb:126 ccm fir"), std::string::npos);
}

#ifdef H264_P0_SUPPORTED
// Remove H.264 P1 and VP8 from offer, check answer negotiates H.264 P0
TEST_P(SignalingTest, OfferWithOnlyH264P0)
{
  EnsureInit();

  OfferOptions options;
  size_t match;

  a1_->CreateOffer(options, OFFER_AV);

  // Remove VP8 from offer
  std::string offer = a1_->offer();
  match = offer.find("RTP/SAVPF 120 126");
  ASSERT_NE(std::string::npos, match);
  offer.replace(match,
                strlen("RTP/SAVPF 120 126"),
                "RTP/SAVPF");

  ParsedSDP sdpWrapper(offer);
  sdpWrapper.DeleteLines("a=rtcp-fb:120");
  sdpWrapper.DeleteLine("a=rtpmap:120");
  sdpWrapper.DeleteLines("a=rtcp-fb:126");
  sdpWrapper.DeleteLine("a=rtpmap:126");
  sdpWrapper.DeleteLine("a=fmtp:126");

  std::cout << "Modified SDP " << std::endl
            << indent(sdpWrapper.getSdp()) << std::endl;

  // Offer shouldn't have P1 or VP8 now
  offer = sdpWrapper.getSdp();
  ASSERT_EQ(offer.find("a=rtpmap:126 H264/90000"), std::string::npos);
  ASSERT_EQ(offer.find("a=rtpmap:120 VP8/90000"), std::string::npos);

  // P0 should be offered first
  ASSERT_NE(offer.find("RTP/SAVPF 97"), std::string::npos);

  a1_->SetLocal(TestObserver::OFFER, offer);
  a2_->SetRemote(TestObserver::OFFER, offer, false);
  a2_->CreateAnswer(OFFER_AV|ANSWER_AV);

  std::string answer(a2_->answer());

  // validate answer SDP
  ASSERT_NE(answer.find("RTP/SAVPF 97"), std::string::npos);
  ASSERT_NE(answer.find("a=rtpmap:97 H264/90000"), std::string::npos);
  ASSERT_NE(answer.find("a=rtcp-fb:97 nack"), std::string::npos);
  ASSERT_NE(answer.find("a=rtcp-fb:97 nack pli"), std::string::npos);
  ASSERT_NE(answer.find("a=rtcp-fb:97 ccm fir"), std::string::npos);
  // Ensure VP8 and P1 removed
  ASSERT_EQ(answer.find("a=rtpmap:126 H264/90000"), std::string::npos);
  ASSERT_EQ(answer.find("a=rtpmap:120 VP8/90000"), std::string::npos);
  ASSERT_EQ(answer.find("a=rtcp-fb:120"), std::string::npos);
  ASSERT_EQ(answer.find("a=rtcp-fb:126"), std::string::npos);
}
#endif

// Test negotiating an answer which has only H.264 P1
// Which means replace VP8 with H.264 P1 in answer
TEST_P(SignalingTest, AnswerWithoutVP8)
{
  EnsureInit();

  OfferOptions options;

  a1_->CreateOffer(options, OFFER_AV);
  a1_->SetLocal(TestObserver::OFFER, a1_->offer());
  a2_->SetRemote(TestObserver::OFFER, a1_->offer(), false);
  a2_->CreateAnswer(OFFER_AV|ANSWER_AV);

  std::string answer(a2_->answer());

  // Ensure answer has VP8
  ASSERT_NE(answer.find("\r\na=rtpmap:120 VP8/90000"), std::string::npos);

  // Replace VP8 with H.264 P1
  ParsedSDP sdpWrapper(a2_->answer());
  sdpWrapper.AddLine("a=fmtp:126 profile-level-id=42e00c;level-asymmetry-allowed=1;packetization-mode=1\r\n");
  size_t match;
  answer = sdpWrapper.getSdp();

  match = answer.find("RTP/SAVPF 120");
  ASSERT_NE(std::string::npos, match);
  answer.replace(match, strlen("RTP/SAVPF 120"), "RTP/SAVPF 126");

  match = answer.find("\r\na=rtpmap:120 VP8/90000");
  ASSERT_NE(std::string::npos, match);
  answer.replace(match,
                 strlen("\r\na=rtpmap:126 H264/90000"),
                 "\r\na=rtpmap:126 H264/90000");

  match = answer.find("\r\na=rtcp-fb:120 nack");
  ASSERT_NE(std::string::npos, match);
  answer.replace(match,
                 strlen("\r\na=rtcp-fb:126 nack"),
                 "\r\na=rtcp-fb:126 nack");

  match = answer.find("\r\na=rtcp-fb:120 nack pli");
  ASSERT_NE(std::string::npos, match);
  answer.replace(match,
                 strlen("\r\na=rtcp-fb:126 nack pli"),
                 "\r\na=rtcp-fb:126 nack pli");

  match = answer.find("\r\na=rtcp-fb:120 ccm fir");
  ASSERT_NE(std::string::npos, match);
  answer.replace(match,
                 strlen("\r\na=rtcp-fb:126 ccm fir"),
                 "\r\na=rtcp-fb:126 ccm fir");

  std::cout << "Modified SDP " << std::endl << indent(answer) << std::endl;

  a2_->SetLocal(TestObserver::ANSWER, answer, false);

  ASSERT_EQ(a2_->pObserver->lastStatusCode,
            PeerConnectionImpl::kNoError);

  a1_->SetRemote(TestObserver::ANSWER, answer, false);

  ASSERT_EQ(a1_->pObserver->lastStatusCode,
            PeerConnectionImpl::kNoError);

  WaitForCompleted();

  // We cannot check pipelines/streams since the H264 stuff won't init.

  CloseStreams();
}

// Test using a non preferred dynamic video payload type on answer negotiation
TEST_P(SignalingTest, UseNonPrefferedPayloadTypeOnAnswer)
{
  EnsureInit();

  OfferOptions options;
  a1_->CreateOffer(options, OFFER_AV);
  a1_->SetLocal(TestObserver::OFFER, a1_->offer());
  a2_->SetRemote(TestObserver::OFFER, a1_->offer(), false);
  a2_->CreateAnswer(OFFER_AV|ANSWER_AV);

  std::string answer(a2_->answer());

  // Ensure answer has VP8
  ASSERT_NE(answer.find("\r\na=rtpmap:120 VP8/90000"), std::string::npos);

  // Replace VP8 Payload Type with a non preferred value
  size_t match;
  match = answer.find("RTP/SAVPF 120");
  ASSERT_NE(std::string::npos, match);
  answer.replace(match, strlen("RTP/SAVPF 121"), "RTP/SAVPF 121");

  match = answer.find("\r\na=rtpmap:120 VP8/90000");
  ASSERT_NE(std::string::npos, match);
  answer.replace(match,
                 strlen("\r\na=rtpmap:121 VP8/90000"),
                 "\r\na=rtpmap:121 VP8/90000");

  match = answer.find("\r\na=rtcp-fb:120 nack");
  ASSERT_NE(std::string::npos, match);
  answer.replace(match,
                 strlen("\r\na=rtcp-fb:121 nack"),
                 "\r\na=rtcp-fb:121 nack");

  match = answer.find("\r\na=rtcp-fb:120 nack pli");
  ASSERT_NE(std::string::npos, match);
  answer.replace(match,
                 strlen("\r\na=rtcp-fb:121 nack pli"),
                 "\r\na=rtcp-fb:121 nack pli");

  match = answer.find("\r\na=rtcp-fb:120 ccm fir");
  ASSERT_NE(std::string::npos, match);
  answer.replace(match,
                 strlen("\r\na=rtcp-fb:121 ccm fir"),
                 "\r\na=rtcp-fb:121 ccm fir");

  std::cout << "Modified SDP " << std::endl
            << indent(answer) << std::endl;

  a2_->SetLocal(TestObserver::ANSWER, answer, false);

  ASSERT_EQ(a2_->pObserver->lastStatusCode,
            PeerConnectionImpl::kNoError);

  a1_->SetRemote(TestObserver::ANSWER, answer, false);

  ASSERT_EQ(a1_->pObserver->lastStatusCode,
            PeerConnectionImpl::kNoError);

  WaitForCompleted();

  CheckPipelines();
  CheckStreams();

  CloseStreams();
}

TEST_P(SignalingTest, VideoNegotiationFails)
{
  EnsureInit();

  OfferOptions options;

  a1_->CreateOffer(options, OFFER_AV);
  a1_->SetLocal(TestObserver::OFFER, a1_->offer());

  ParsedSDP parsedOffer(a1_->offer());
  parsedOffer.DeleteLines("a=rtcp-fb:120");
  parsedOffer.DeleteLines("a=rtcp-fb:126");
  parsedOffer.DeleteLines("a=rtcp-fb:97");
  parsedOffer.DeleteLines("a=rtpmap:120");
  parsedOffer.DeleteLines("a=rtpmap:126");
  parsedOffer.DeleteLines("a=rtpmap:97");
  parsedOffer.AddLine("a=rtpmap:120 VP9/90000\r\n");
  parsedOffer.AddLine("a=rtpmap:126 VP10/90000\r\n");
  parsedOffer.AddLine("a=rtpmap:97 H265/90000\r\n");

  std::cout << "Modified offer: " << std::endl << parsedOffer.getSdp()
    << std::endl;

  a2_->SetRemote(TestObserver::OFFER, parsedOffer.getSdp(), false);
  a2_->CreateAnswer(OFFER_AV|ANSWER_AUDIO);

  a2_->SetLocal(TestObserver::ANSWER, a2_->answer(), false);

  ASSERT_EQ(a2_->pObserver->lastStatusCode,
            PeerConnectionImpl::kNoError);

  a1_->SetRemote(TestObserver::ANSWER, a2_->answer(), false);

  ASSERT_EQ(a1_->pObserver->lastStatusCode,
            PeerConnectionImpl::kNoError);

  a1_->ExpectMissingTracks(SdpMediaSection::kVideo);
  a2_->ExpectMissingTracks(SdpMediaSection::kVideo);

  WaitForCompleted();

  CheckPipelines();
  // TODO: (bug 1140089) a2 is not seeing audio segments in this test.
  // CheckStreams();

  CloseStreams();
}

TEST_P(SignalingTest, AudioNegotiationFails)
{
  EnsureInit();

  OfferOptions options;

  a1_->CreateOffer(options, OFFER_AV);
  a1_->SetLocal(TestObserver::OFFER, a1_->offer());

  ParsedSDP parsedOffer(a1_->offer());
  parsedOffer.ReplaceLine("a=rtpmap:0", "a=rtpmap:0 G728/8000");
  parsedOffer.ReplaceLine("a=rtpmap:8", "a=rtpmap:8 G729/8000");
  parsedOffer.ReplaceLine("a=rtpmap:9", "a=rtpmap:9 GSM/8000");
  parsedOffer.ReplaceLine("a=rtpmap:109", "a=rtpmap:109 LPC/8000");

  a2_->SetRemote(TestObserver::OFFER, parsedOffer.getSdp(), false);
  a2_->CreateAnswer(OFFER_AV|ANSWER_VIDEO);

  a2_->SetLocal(TestObserver::ANSWER, a2_->answer(), false);

  ASSERT_EQ(a2_->pObserver->lastStatusCode,
            PeerConnectionImpl::kNoError);

  a1_->SetRemote(TestObserver::ANSWER, a2_->answer(), false);

  ASSERT_EQ(a1_->pObserver->lastStatusCode,
            PeerConnectionImpl::kNoError);

  a1_->ExpectMissingTracks(SdpMediaSection::kAudio);
  a2_->ExpectMissingTracks(SdpMediaSection::kAudio);

  WaitForCompleted();

  CheckPipelines();
  CheckStreams();

  CloseStreams();
}

TEST_P(SignalingTest, BundleStreamCorrelationBySsrc)
{
  if (GetParam() != "bundle") {
    return;
  }

  EnsureInit();

  a1_->AddStream(DOMMediaStream::HINT_CONTENTS_AUDIO);
  a1_->AddStream(DOMMediaStream::HINT_CONTENTS_AUDIO);

  OfferOptions options;

  a1_->CreateOffer(options, OFFER_NONE);
  ParsedSDP parsedOffer(a1_->offer());

  // Sabotage mid-based matching
  std::string modifiedOffer = parsedOffer.getSdp();
  size_t midExtStart =
    modifiedOffer.find("urn:ietf:params:rtp-hdrext:sdes:mid");
  if (midExtStart != std::string::npos) {
    // Just garble it a little
    modifiedOffer[midExtStart] = 'q';
  }

  a1_->SetLocal(TestObserver::OFFER, modifiedOffer);

  a2_->SetRemote(TestObserver::OFFER, modifiedOffer, false);
  a2_->CreateAnswer(ANSWER_AUDIO);

  a2_->SetLocal(TestObserver::ANSWER, a2_->answer(), false);

  ASSERT_EQ(a2_->pObserver->lastStatusCode,
            PeerConnectionImpl::kNoError);

  a1_->SetRemote(TestObserver::ANSWER, a2_->answer(), false);

  ASSERT_EQ(a1_->pObserver->lastStatusCode,
            PeerConnectionImpl::kNoError);

  WaitForCompleted();

  CheckPipelines();
  CheckStreams();

  CloseStreams();
}

TEST_P(SignalingTest, BundleStreamCorrelationByUniquePt)
{
  if (GetParam() != "bundle") {
    return;
  }

  EnsureInit();

  OfferOptions options;

  a1_->CreateOffer(options, OFFER_AV);
  ParsedSDP parsedOffer(a1_->offer());

  std::string modifiedOffer = parsedOffer.getSdp();
  // Sabotage ssrc matching
  size_t ssrcStart =
    modifiedOffer.find("a=ssrc:");
  ASSERT_NE(std::string::npos, ssrcStart);
  // Garble
  modifiedOffer[ssrcStart+2] = 'q';

  // Sabotage mid-based matching
  size_t midExtStart =
    modifiedOffer.find("urn:ietf:params:rtp-hdrext:sdes:mid");
  if (midExtStart != std::string::npos) {
    // Just garble it a little
    modifiedOffer[midExtStart] = 'q';
  }

  a1_->SetLocal(TestObserver::OFFER, modifiedOffer);

  a2_->SetRemote(TestObserver::OFFER, modifiedOffer, false);
  a2_->CreateAnswer(OFFER_AV|ANSWER_AV);

  a2_->SetLocal(TestObserver::ANSWER, a2_->answer(), false);

  ASSERT_EQ(a2_->pObserver->lastStatusCode,
            PeerConnectionImpl::kNoError);

  a1_->SetRemote(TestObserver::ANSWER, a2_->answer(), false);

  ASSERT_EQ(a1_->pObserver->lastStatusCode,
            PeerConnectionImpl::kNoError);

  WaitForCompleted();

  CheckPipelines();
  CheckStreams();

  CloseStreams();
}

INSTANTIATE_TEST_CASE_P(Variants, SignalingTest,
                        ::testing::Values("bundle",
                                          "no_bundle",
                                          "reject_bundle"));

} // End namespace test.

bool is_color_terminal(const char *terminal) {
  if (!terminal) {
    return false;
  }
  const char *color_terms[] = {
    "xterm",
    "xterm-color",
    "xterm-256color",
    "screen",
    "linux",
    "cygwin",
    0
  };
  const char **p = color_terms;
  while (*p) {
    if (!strcmp(terminal, *p)) {
      return true;
    }
    p++;
  }
  return false;
}

static std::string get_environment(const char *name) {
  char *value = getenv(name);

  if (!value)
    return "";

  return value;
}

// This exists to send as an event to trigger shutdown.
static void tests_complete() {
  gTestsComplete = true;
}

// The GTest thread runs this instead of the main thread so it can
// do things like ASSERT_TRUE_WAIT which you could not do on the main thread.
static int gtest_main(int argc, char **argv) {
  MOZ_ASSERT(!NS_IsMainThread());

  ::testing::InitGoogleTest(&argc, argv);

  for(int i=0; i<argc; i++) {
    if (!strcmp(argv[i],"-t")) {
      kDefaultTimeout = 20000;
    }
   }

  ::testing::AddGlobalTestEnvironment(new test::SignalingEnvironment);
  int result = RUN_ALL_TESTS();

  test_utils->sts_target()->Dispatch(
    WrapRunnableNM(&TestStunServer::ShutdownInstance), NS_DISPATCH_SYNC);

  // Set the global shutdown flag and tickle the main thread
  // The main thread did not go through Init() so calling Shutdown()
  // on it will not work.
  gMainThread->Dispatch(WrapRunnableNM(tests_complete), NS_DISPATCH_SYNC);

  return result;
}


int main(int argc, char **argv) {

  // This test can cause intermittent oranges on the builders
  CHECK_ENVIRONMENT_FLAG("MOZ_WEBRTC_TESTS")

  if (isatty(STDOUT_FILENO) && is_color_terminal(getenv("TERM"))) {
    std::string ansiMagenta = "\x1b[35m";
    std::string ansiCyan = "\x1b[36m";
    std::string ansiColorOff = "\x1b[0m";
    callerName = ansiCyan + callerName + ansiColorOff;
    calleeName = ansiMagenta + calleeName + ansiColorOff;
  }

  std::string tmp = get_environment("STUN_SERVER_ADDRESS");
  if (tmp != "")
    g_stun_server_address = tmp;

  tmp = get_environment("STUN_SERVER_PORT");
  if (tmp != "")
      g_stun_server_port = atoi(tmp.c_str());

  test_utils = new MtransportTestUtils();
  NSS_NoDB_Init(nullptr);
  NSS_SetDomesticPolicy();

  ::testing::TestEventListeners& listeners =
        ::testing::UnitTest::GetInstance()->listeners();
  // Adds a listener to the end.  Google Test takes the ownership.
  listeners.Append(new test::RingbufferDumper(test_utils));
  test_utils->sts_target()->Dispatch(
    WrapRunnableNM(&TestStunServer::GetInstance, AF_INET), NS_DISPATCH_SYNC);

  // Set the main thread global which is this thread.
  nsIThread *thread;
  NS_GetMainThread(&thread);
  gMainThread = thread;
  MOZ_ASSERT(NS_IsMainThread());

  // Now create the GTest thread and run all of the tests on it
  // When it is complete it will set gTestsComplete
  NS_NewNamedThread("gtest_thread", &thread);
  gGtestThread = thread;

  int result;
  gGtestThread->Dispatch(
    WrapRunnableNMRet(&result, gtest_main, argc, argv), NS_DISPATCH_NORMAL);

  // Here we handle the event queue for dispatches to the main thread
  // When the GTest thread is complete it will send one more dispatch
  // with gTestsComplete == true.
  while (!gTestsComplete && NS_ProcessNextEvent());

  gGtestThread->Shutdown();

  PeerConnectionCtx::Destroy();
  delete test_utils;

  return result;
}
