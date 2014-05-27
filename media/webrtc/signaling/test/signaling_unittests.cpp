/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <iostream>
#include <map>
#include <algorithm>
#include <string>
#include <unistd.h>

#include "base/basictypes.h"
#include "logging.h"

#define GTEST_HAS_RTTI 0
#include "gtest/gtest.h"
#include "gtest_utils.h"

#include "nspr.h"
#include "nss.h"
#include "ssl.h"
#include "prthread.h"

#include "cpr_stdlib.h"
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
#include "nsNetUtil.h"
#include "nsIIOService.h"
#include "nsIDNSService.h"
#include "nsWeakReference.h"
#include "nricectx.h"
#include "rlogringbuffer.h"
#include "mozilla/SyncRunnable.h"
#include "logging.h"
#include "stunserver.h"
#include "stunserver.cpp"
#include "PeerConnectionImplEnumsBinding.cpp"

#include "mtransport_test_utils.h"
#include "gtest_ringbuffer_dumper.h"
MtransportTestUtils *test_utils;
nsCOMPtr<nsIThread> gThread;

#ifndef USE_FAKE_MEDIA_STREAMS
#error USE_FAKE_MEDIA_STREAMS undefined
#endif
#ifndef USE_FAKE_PCOBSERVER
#error USE_FAKE_PCOBSERVER undefined
#endif

static int kDefaultTimeout = 5000;
static bool fRtcpMux = true;

static std::string callerName = "caller";
static std::string calleeName = "callee";

#define ARRAY_TO_STL(container, type, array) \
        (container<type>((array), (array) + PR_ARRAY_SIZE(array)))

#define ARRAY_TO_SET(type, array) ARRAY_TO_STL(std::set, type, array)

std::string g_stun_server_address((char *)"23.21.150.121");
uint16_t g_stun_server_port(3478);
std::string kBogusSrflxAddress((char *)"192.0.2.1");
uint16_t kBogusSrflxPort(1001);

namespace sipcc {

// We can't use mozilla/dom/MediaConstraintsBinding.h here because it uses
// nsString, so we pass constraints in using MediaConstraintsExternal instead

class MediaConstraints : public MediaConstraintsExternal {
public:
  void setBooleanConstraint(const char *namePtr, bool value, bool mandatory) {
    cc_boolean_constraint_t &member (getMember(namePtr));
    member.was_passed = true;
    member.value = value;
    member.mandatory = mandatory;
  }
private:
  cc_boolean_constraint_t &getMember(const char *namePtr) {
    if (strcmp(namePtr, "OfferToReceiveAudio") == 0) {
        return mConstraints.offer_to_receive_audio;
    }
    if (strcmp(namePtr, "OfferToReceiveVideo") == 0) {
        return mConstraints.offer_to_receive_video;
    }
    MOZ_ASSERT(false);
    return mConstraints.moz_dont_offer_datachannel;
  }
};
}

using namespace mozilla;
using namespace mozilla::dom;

namespace test {
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
    "a=sendrecv\r\n";


enum sdpTestFlags
{
  SHOULD_SEND_AUDIO     = (1<<0),
  SHOULD_RECV_AUDIO     = (1<<1),
  SHOULD_INACTIVE_AUDIO = (1<<2),
  SHOULD_REJECT_AUDIO   = (1<<3),
  SHOULD_OMIT_AUDIO     = (1<<4),
  DONT_CHECK_AUDIO      = (1<<5),
  SHOULD_CHECK_AUDIO    = (1<<6),

  SHOULD_SEND_VIDEO     = (1<<8),
  SHOULD_RECV_VIDEO     = (1<<9),
  SHOULD_INACTIVE_VIDEO = (1<<10),
  SHOULD_REJECT_VIDEO   = (1<<11),
  SHOULD_OMIT_VIDEO     = (1<<12),
  DONT_CHECK_VIDEO      = (1<<13),
  SHOULD_CHECK_VIDEO    = (1<<14),

  SHOULD_INCLUDE_DATA   = (1 << 16),
  DONT_CHECK_DATA       = (1 << 17),

  SHOULD_SENDRECV_AUDIO = SHOULD_SEND_AUDIO | SHOULD_RECV_AUDIO,
  SHOULD_SENDRECV_VIDEO = SHOULD_SEND_VIDEO | SHOULD_RECV_VIDEO,
  SHOULD_SENDRECV_AV = SHOULD_SENDRECV_AUDIO | SHOULD_SENDRECV_VIDEO,
  SHOULD_CHECK_AV = SHOULD_CHECK_AUDIO | SHOULD_CHECK_VIDEO,

  AUDIO_FLAGS = SHOULD_SEND_AUDIO | SHOULD_RECV_AUDIO
                | SHOULD_INACTIVE_AUDIO | SHOULD_REJECT_AUDIO
                | DONT_CHECK_AUDIO | SHOULD_OMIT_AUDIO
                | SHOULD_CHECK_AUDIO,

  VIDEO_FLAGS = SHOULD_SEND_VIDEO | SHOULD_RECV_VIDEO
                | SHOULD_INACTIVE_VIDEO | SHOULD_REJECT_VIDEO
                | DONT_CHECK_VIDEO | SHOULD_OMIT_VIDEO
                | SHOULD_CHECK_VIDEO
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

enum mediaPipelineFlags
{
  PIPELINE_LOCAL = (1<<0),
  PIPELINE_RTCP_MUX = (1<<1),
  PIPELINE_SEND = (1<<2),
  PIPELINE_VIDEO = (1<<3),
  PIPELINE_RTCP_NACK = (1<<4)
};


static bool SetupGlobalThread() {
  if (!gThread) {
    nsIThread *thread;

    nsresult rv = NS_NewNamedThread("pseudo-main",&thread);
    if (NS_FAILED(rv))
      return false;

    gThread = thread;
  }
  return true;
}

class TestObserver : public AFakePCObserver
{
public:
  TestObserver(sipcc::PeerConnectionImpl *peerConnection,
               const std::string &aName) :
    AFakePCObserver(peerConnection, aName) {}

  size_t MatchingCandidates(const std::string& cand) {
    size_t count = 0;

    for (size_t i=0; i<candidates.size(); ++i) {
      if (candidates[i].find(cand) != std::string::npos)
        ++count;
    }

    return count;
  }

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_IMETHODIMP OnCreateOfferSuccess(const char* offer, ER&);
  NS_IMETHODIMP OnCreateOfferError(uint32_t code, const char *msg, ER&);
  NS_IMETHODIMP OnCreateAnswerSuccess(const char* answer, ER&);
  NS_IMETHODIMP OnCreateAnswerError(uint32_t code, const char *msg, ER&);
  NS_IMETHODIMP OnSetLocalDescriptionSuccess(ER&);
  NS_IMETHODIMP OnSetRemoteDescriptionSuccess(ER&);
  NS_IMETHODIMP OnSetLocalDescriptionError(uint32_t code, const char *msg, ER&);
  NS_IMETHODIMP OnSetRemoteDescriptionError(uint32_t code, const char *msg, ER&);
  NS_IMETHODIMP NotifyConnection(ER&);
  NS_IMETHODIMP NotifyClosedConnection(ER&);
  NS_IMETHODIMP NotifyDataChannel(nsIDOMDataChannel *channel, ER&);
  NS_IMETHODIMP OnStateChange(PCObserverStateType state_type, ER&, void*);
  NS_IMETHODIMP OnAddStream(nsIDOMMediaStream *stream, ER&);
  NS_IMETHODIMP OnRemoveStream(ER&);
  NS_IMETHODIMP OnAddTrack(ER&);
  NS_IMETHODIMP OnRemoveTrack(ER&);
  NS_IMETHODIMP OnAddIceCandidateSuccess(ER&);
  NS_IMETHODIMP OnAddIceCandidateError(uint32_t code, const char *msg, ER&);
  NS_IMETHODIMP OnIceCandidate(uint16_t level, const char *mid, const char *cand, ER&);
};

NS_IMPL_ISUPPORTS1(TestObserver, nsISupportsWeakReference)

NS_IMETHODIMP
TestObserver::OnCreateOfferSuccess(const char* offer, ER&)
{
  lastString = strdup(offer);
  state = stateSuccess;
  std::cout << name << ": onCreateOfferSuccess = " << std::endl << indent(offer)
            << std::endl;
  return NS_OK;
}

NS_IMETHODIMP
TestObserver::OnCreateOfferError(uint32_t code, const char *message, ER&)
{
  lastStatusCode = static_cast<sipcc::PeerConnectionImpl::Error>(code);
  state = stateError;
  std::cout << name << ": onCreateOfferError = " << code
            << " (" << message << ")" << std::endl;
  return NS_OK;
}

NS_IMETHODIMP
TestObserver::OnCreateAnswerSuccess(const char* answer, ER&)
{
  lastString = strdup(answer);
  state = stateSuccess;
  std::cout << name << ": onCreateAnswerSuccess =" << std::endl
            << indent(answer) << std::endl;
  return NS_OK;
}

NS_IMETHODIMP
TestObserver::OnCreateAnswerError(uint32_t code, const char *message, ER&)
{
  lastStatusCode = static_cast<sipcc::PeerConnectionImpl::Error>(code);
  std::cout << name << ": onCreateAnswerError = " << code
            << " (" << message << ")" << std::endl;
  state = stateError;
  return NS_OK;
}

NS_IMETHODIMP
TestObserver::OnSetLocalDescriptionSuccess(ER&)
{
  lastStatusCode = sipcc::PeerConnectionImpl::kNoError;
  state = stateSuccess;
  std::cout << name << ": onSetLocalDescriptionSuccess" << std::endl;
  return NS_OK;
}

NS_IMETHODIMP
TestObserver::OnSetRemoteDescriptionSuccess(ER&)
{
  lastStatusCode = sipcc::PeerConnectionImpl::kNoError;
  state = stateSuccess;
  std::cout << name << ": onSetRemoteDescriptionSuccess" << std::endl;
  return NS_OK;
}

NS_IMETHODIMP
TestObserver::OnSetLocalDescriptionError(uint32_t code, const char *message, ER&)
{
  lastStatusCode = static_cast<sipcc::PeerConnectionImpl::Error>(code);
  state = stateError;
  std::cout << name << ": onSetLocalDescriptionError = " << code
            << " (" << message << ")" << std::endl;
  return NS_OK;
}

NS_IMETHODIMP
TestObserver::OnSetRemoteDescriptionError(uint32_t code, const char *message, ER&)
{
  lastStatusCode = static_cast<sipcc::PeerConnectionImpl::Error>(code);
  state = stateError;
  std::cout << name << ": onSetRemoteDescriptionError = " << code
            << " (" << message << ")" << std::endl;
  return NS_OK;
}

NS_IMETHODIMP
TestObserver::NotifyConnection(ER&)
{
  std::cout << name << ": NotifyConnection" << std::endl;
  return NS_OK;
}

NS_IMETHODIMP
TestObserver::NotifyClosedConnection(ER&)
{
  std::cout << name << ": NotifyClosedConnection" << std::endl;
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
  PCImplReadyState gotready;
  PCImplIceConnectionState gotice;
  PCImplIceGatheringState goticegathering;
  PCImplSipccState gotsipcc;
  PCImplSignalingState gotsignaling;

  std::cout << name << ": ";

  switch (state_type)
  {
  case PCObserverStateType::ReadyState:
    rv = pc->ReadyState(&gotready);
    NS_ENSURE_SUCCESS(rv, rv);
    std::cout << "Ready State: "
              << PCImplReadyStateValues::strings[int(gotready)].value
              << std::endl;
    break;
  case PCObserverStateType::IceConnectionState:
    rv = pc->IceConnectionState(&gotice);
    NS_ENSURE_SUCCESS(rv, rv);
    std::cout << "ICE Connection State: "
              << PCImplIceConnectionStateValues::strings[int(gotice)].value
              << std::endl;
    break;
  case PCObserverStateType::IceGatheringState:
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
  case PCObserverStateType::SipccState:
    rv = pc->SipccState(&gotsipcc);
    NS_ENSURE_SUCCESS(rv, rv);
    std::cout << "SIPCC State: "
              << PCImplSipccStateValues::strings[int(gotsipcc)].value
              << std::endl;
    break;
  case PCObserverStateType::SignalingState:
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
TestObserver::OnAddStream(nsIDOMMediaStream *stream, ER&)
{
  PR_ASSERT(stream);

  DOMMediaStream *ms = static_cast<DOMMediaStream *>(stream);

  std::cout << name << ": OnAddStream called hints=" << ms->GetHintContents()
            << " thread=" << PR_GetCurrentThread() << std::endl ;

  onAddStreamCalled = true;

  streams.push_back(ms);

  // We know that the media stream is secretly a Fake_SourceMediaStream,
  // so now we can start it pulling from us
  Fake_SourceMediaStream *fs = static_cast<Fake_SourceMediaStream *>(ms->GetStream());

  test_utils->sts_target()->Dispatch(
    WrapRunnable(fs, &Fake_SourceMediaStream::Start),
    NS_DISPATCH_NORMAL);

  return NS_OK;
}

NS_IMETHODIMP
TestObserver::OnRemoveStream(ER&)
{
  state = stateSuccess;
  return NS_OK;
}

NS_IMETHODIMP
TestObserver::OnAddTrack(ER&)
{
  state = stateSuccess;
  return NS_OK;
}

NS_IMETHODIMP
TestObserver::OnRemoveTrack(ER&)
{
  state = stateSuccess;
  return NS_OK;
}

NS_IMETHODIMP
TestObserver::OnIceCandidate(uint16_t level,
                             const char * mid,
                             const char * candidate, ER&)
{
  std::cout << name << ": onIceCandidate [" << level << "/"
            << mid << "] " << candidate << std::endl;
  candidates.push_back(candidate);
  return NS_OK;
}

NS_IMETHODIMP
TestObserver::OnAddIceCandidateSuccess(ER&)
{
  lastStatusCode = sipcc::PeerConnectionImpl::kNoError;
  state = stateSuccess;
  std::cout << name << ": onAddIceCandidateSuccess" << std::endl;
  addIceSuccessCount++;
  return NS_OK;
}

NS_IMETHODIMP
TestObserver::OnAddIceCandidateError(uint32_t code, const char *message, ER&)
{
  lastStatusCode = static_cast<sipcc::PeerConnectionImpl::Error>(code);
  state = stateError;
  std::cout << name << ": onAddIceCandidateError = " << code
            << " (" << message << ")" << std::endl;
  return NS_OK;
}

class ParsedSDP {
 public:
  //Line number with the corresponding SDP line.
  typedef std::pair<int, std::string> SdpLine;

  ParsedSDP(std::string sdp):
    sdp_(),
    sdp_without_ice_(),
    ice_candidates_(),
    levels_(0),
    num_lines(0)
  {
    sdp_ = sdp;
    Parse();
  }

  void DeleteAllLines(std::string objType)
  {
    int count = sdp_map_.count(objType);
    std::cout << "Removing " << count << " lines from SDP (" << objType
              << ")" << std::endl;

    for (int i = 0; i < count; i++) {
      DeleteLine(objType);
    }
  }

  void DeleteLine(std::string objType)
  {
    ReplaceLine(objType, "");
  }

  // Replaces the first instance of objType in the SDP with
  // a new string.
  // If content is an empty string then the line will be removed
  void ReplaceLine(std::string objType, std::string content)
  {
    std::multimap<std::string, SdpLine>::iterator it;
    it = sdp_map_.find(objType);
    if(it != sdp_map_.end()) {
      SdpLine sdp_line_pair = (*it).second;
      int line_no = sdp_line_pair.first;
      sdp_map_.erase(it);
      if(content.empty()) {
        return;
      }
      std::string value = content.substr(objType.length() + 1);
      sdp_map_.insert(std::pair<std::string, SdpLine>(objType,
        std::make_pair(line_no,value)));
    }
  }

  void AddLine(std::string content)
  {
    size_t whiteSpace = content.find(' ');
    std::string key;
    std::string value;
    if(whiteSpace == std::string::npos) {
      key = content.substr(0,  content.size() - 2);
      value = "";
    } else {
      key = content.substr(0, whiteSpace);
      value = content.substr(whiteSpace+1);
    }
    sdp_map_.insert(std::pair<std::string, SdpLine>(key,
      std::make_pair(num_lines,value)));
    num_lines++;
  }

  // Returns the values for all lines of the indicated type
  // Removes trailing "\r\n" from values.
  std::vector<std::string> GetLines(std::string objType) const
  {
    std::multimap<std::string, SdpLine>::const_iterator it, end;
    std::vector<std::string> values;
    it = sdp_map_.lower_bound(objType);
    end = sdp_map_.upper_bound(objType);
    while (it != end) {
      std::string value = it->second.second;
      if (value.find("\r") != std::string::npos) {
        value = value.substr(0, value.find("\r"));
      } else {
        ADD_FAILURE();
      }
      values.push_back(value);
      ++it;
    }
    return values;
  }

  //Parse SDP as std::string into map that looks like:
  // key: sdp content till first space
  // value : <line_number, sdp content after the first space>
  void Parse()
  {
    size_t prev = 0;
    size_t found = 0;
    num_lines = 0;
    for(;;) {
      found = sdp_.find('\n', found + 1);
      if (found == std::string::npos)
        break;
      std::string line = sdp_.substr(prev, (found - prev) + 1);
      size_t whiteSpace = line.find(' ');
      std::string key;
      std::string value;
      if(whiteSpace == std::string::npos) {
        //this is the line with no extra contents
        //example, v=0, a=sendrecv
        key = line.substr(0, line.size() - 2);
        //<line_no>:<valeu>
        value = "";
      } else {
        key = line.substr(0, whiteSpace);
        //<line_no>:<value>
        value = line.substr(whiteSpace+1);
      }
      SdpLine sdp_line_pair = std::make_pair(num_lines,value);
      sdp_map_.insert(std::pair<std::string, SdpLine>(key, sdp_line_pair));
      num_lines++;
      //storing ice candidates separately for quick acesss as needed
      //for the trickle unit tests
      if (line.find("a=candidate") == 0) {
        // This is a candidate, strip of a= and \r\n
        std::string cand = line.substr(2, line.size() - 4);
        ice_candidates_.insert(std::pair<int, std::string>(levels_, cand));
       } else {
        sdp_without_ice_ += line;
      }
      if (line.find("m=") == 0) {
        // This is an m-line
        ++levels_;
      }
      prev = found + 1;
    }
  }

  //Convert Internal SDP representation into String representation
  std::string getSdp()
  {
     std::vector<std::string> sdp_lines(num_lines);
     for (std::multimap<std::string, SdpLine>::iterator it = sdp_map_.begin();
         it != sdp_map_.end(); ++it) {

      SdpLine sdp_line_pair = (*it).second;
      std::string value;
      if(sdp_line_pair.second.length() == 0) {
        value = (*it).first + "\r\n";
        sdp_lines[sdp_line_pair.first] = value;
      } else {
        value = (*it).first + ' ' + sdp_line_pair.second;
        sdp_lines[sdp_line_pair.first] = value;
      }
   }

    //generate our final sdp in std::string format
    std::string sdp;
    for (size_t i = 0; i < sdp_lines.size(); i++)
    {
      sdp += sdp_lines[i];
    }

    return sdp;
  }



  std::string sdp_;
  std::string sdp_without_ice_;
  std::multimap<int, std::string> ice_candidates_;
  std::multimap<std::string, SdpLine> sdp_map_;
  int levels_;
  int num_lines;
};

class SignalingAgent {
 public:
  SignalingAgent(const std::string &aName) : pc(nullptr), name(aName) {
    cfg_.addStunServer(g_stun_server_address, g_stun_server_port);

    pc = sipcc::PeerConnectionImpl::CreatePeerConnection();
    EXPECT_TRUE(pc);
  }

  SignalingAgent(const std::string &aName, const std::string stun_addr,
                 uint16_t stun_port) : pc(nullptr), name(aName) {
    cfg_.addStunServer(stun_addr, stun_port);

    pc = sipcc::PeerConnectionImpl::CreatePeerConnection();
    EXPECT_TRUE(pc);
  }


  ~SignalingAgent() {
    mozilla::SyncRunnable::DispatchToThread(gThread,
      WrapRunnable(this, &SignalingAgent::Close));
  }

  void Init_m(nsCOMPtr<nsIThread> thread)
  {
    pObserver = new TestObserver(pc, name);
    ASSERT_TRUE(pObserver);

    ASSERT_EQ(pc->Initialize(*pObserver, nullptr, cfg_, thread), NS_OK);
  }

  void Init(nsCOMPtr<nsIThread> thread)
  {
    mozilla::SyncRunnable::DispatchToThread(thread,
      WrapRunnable(this, &SignalingAgent::Init_m, thread));

    ASSERT_TRUE_WAIT(sipcc_state() == PCImplSipccState::Started,
                     kDefaultTimeout);
  }

  void WaitForGather() {
    ASSERT_TRUE_WAIT(ice_gathering_state() == PCImplIceGatheringState::Complete,
                     5000);

    std::cout << name << ": Init Complete" << std::endl;
  }

  bool WaitForGatherAllowFail() {
    EXPECT_TRUE_WAIT(
        ice_gathering_state() == PCImplIceGatheringState::Complete ||
        ice_connection_state() == PCImplIceConnectionState::Failed,
        5000);

    if (ice_connection_state() == PCImplIceConnectionState::Failed) {
      std::cout << name << ": Init Failed" << std::endl;
      return false;
    }

    std::cout << name << "Init Complete" << std::endl;
    return true;
  }

  PCImplSipccState sipcc_state()
  {
    return pc->SipccState();
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
    if (pc) {
      std::cout << name << ": Close" << std::endl;

      pc->Close();
      pc = nullptr;
    }

    // Shutdown is synchronous evidently.
    // ASSERT_TRUE(pObserver->WaitForObserverCall());
    // ASSERT_EQ(pc->sipcc_state(), sipcc::PeerConnectionInterface::kIdle);
  }

  bool OfferContains(const std::string& str) {
    std::string o(offer());

    return o.find(str) != std::string::npos;
  }

  bool AnswerContains(const std::string& str) {
    std::string o(answer());

    return o.find(str) != std::string::npos;
  }

  size_t MatchingCandidates(const std::string& cand) {
    return pObserver->MatchingCandidates(cand);
  }

  char* offer() const { return offer_; }
  char* answer() const { return answer_; }

  std::string getLocalDescription() const {
    char *sdp = nullptr;
    pc->GetLocalDescription(&sdp);
    if (!sdp) {
      return "";
    }
    return sdp;
  }

  std::string getRemoteDescription() const {
    char *sdp = 0;
    pc->GetRemoteDescription(&sdp);
    if (!sdp) {
      return "";
    }
    return sdp;
  }

  // Adds a stream to the PeerConnection.
  void AddStream(uint32_t hint =
         DOMMediaStream::HINT_CONTENTS_AUDIO |
         DOMMediaStream::HINT_CONTENTS_VIDEO,
       MediaStream *stream = nullptr,
       sipcc::MediaConstraints *constraints = nullptr
       ) {

    sipcc::MediaConstraints noConstraints;
    if (!constraints) {
      constraints = &noConstraints;
    }

    nsRefPtr<DOMMediaStream> domMediaStream;
    if (stream) {
      domMediaStream = new DOMMediaStream(stream);
    } else {
      domMediaStream = new DOMMediaStream();
    }

    domMediaStream->SetHintContents(hint);
    ASSERT_EQ(pc->AddStream(*domMediaStream, *constraints), NS_OK);
    domMediaStream_ = domMediaStream;
  }


  // Removes a stream from the PeerConnection. If the stream
  // parameter is absent, removes the stream that was most
  // recently added to the PeerConnection.
  void RemoveLastStreamAdded() {
    ASSERT_EQ(pc->RemoveStream(*domMediaStream_), NS_OK);
  }

  void CreateOffer(sipcc::MediaConstraints& constraints,
                   uint32_t offerFlags, uint32_t sdpCheck,
                   PCImplSignalingState endState =
                     PCImplSignalingState::SignalingStable) {

    // Create a media stream as if it came from GUM
    Fake_AudioStreamSource *audio_stream =
      new Fake_AudioStreamSource();

    nsresult ret;
    mozilla::SyncRunnable::DispatchToThread(
      test_utils->sts_target(),
      WrapRunnableRet(audio_stream, &Fake_MediaStream::Start, &ret));

    ASSERT_TRUE(NS_SUCCEEDED(ret));

    uint32_t aHintContents = 0;
    if (offerFlags & OFFER_AUDIO) {
      aHintContents |= DOMMediaStream::HINT_CONTENTS_AUDIO;
    }
    if (offerFlags & OFFER_VIDEO) {
      aHintContents |= DOMMediaStream::HINT_CONTENTS_VIDEO;
    }
    AddStream(aHintContents, audio_stream);

    // Now call CreateOffer as JS would
    pObserver->state = TestObserver::stateNoResponse;
    ASSERT_EQ(pc->CreateOffer(constraints), NS_OK);
    ASSERT_TRUE_WAIT(pObserver->state != TestObserver::stateNoResponse,
                     kDefaultTimeout);
    ASSERT_EQ(pObserver->state, TestObserver::stateSuccess);
    SDPSanityCheck(pObserver->lastString, sdpCheck, true);
    ASSERT_EQ(signaling_state(), endState);
    offer_ = pObserver->lastString;
  }

void CreateAnswer(sipcc::MediaConstraints& constraints, std::string offer,
                    uint32_t offerAnswerFlags,
                    uint32_t sdpCheck = DONT_CHECK_AUDIO|
                                        DONT_CHECK_VIDEO|
                                        DONT_CHECK_DATA,
                    PCImplSignalingState endState =
                    PCImplSignalingState::SignalingHaveRemoteOffer) {

    uint32_t aHintContents = 0;
    if (offerAnswerFlags & ANSWER_AUDIO) {
      aHintContents |= DOMMediaStream::HINT_CONTENTS_AUDIO;
    }
    if (offerAnswerFlags & ANSWER_VIDEO) {
      aHintContents |= DOMMediaStream::HINT_CONTENTS_VIDEO;
    }
    AddStream(aHintContents);

    // Decide if streams are disabled for offer or answer
    // then perform SDP checking based on which stream disabled
    pObserver->state = TestObserver::stateNoResponse;
    ASSERT_EQ(pc->CreateAnswer(constraints), NS_OK);
    ASSERT_TRUE_WAIT(pObserver->state != TestObserver::stateNoResponse,
                     kDefaultTimeout);
    ASSERT_EQ(pObserver->state, TestObserver::stateSuccess);
    SDPSanityCheck(pObserver->lastString, sdpCheck, false);
    ASSERT_EQ(signaling_state(), endState);

    answer_ = pObserver->lastString;
  }

  // At present, we use the hints field in a stream to find and
  // remove it. This only works if the specified hints flags are
  // unique among all streams in the PeerConnection. This is not
  // generally true, and will need significant revision once
  // multiple streams are supported.
  void CreateOfferRemoveStream(sipcc::MediaConstraints& constraints,
                               uint32_t hints, uint32_t sdpCheck) {

    domMediaStream_->SetHintContents(hints);

    // This currently "removes" a stream that has the same audio/video
    // hints as were passed in.
    // When complete RemoveStream will remove and entire stream and its tracks
    // not just disable a track as this is currently doing
    ASSERT_EQ(pc->RemoveStream(*domMediaStream_), NS_OK);

    // Now call CreateOffer as JS would
    pObserver->state = TestObserver::stateNoResponse;
    ASSERT_EQ(pc->CreateOffer(constraints), NS_OK);
    ASSERT_TRUE_WAIT(pObserver->state != TestObserver::stateNoResponse,
                     kDefaultTimeout);
    ASSERT_TRUE(pObserver->state == TestObserver::stateSuccess);
    SDPSanityCheck(pObserver->lastString, sdpCheck, true);
    offer_ = pObserver->lastString;
  }

  void SetRemote(TestObserver::Action action, std::string remote,
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
    ASSERT_TRUE_WAIT(pObserver->state != TestObserver::stateNoResponse,
                     kDefaultTimeout);
    ASSERT_EQ(signaling_state(), endState);
    if (!ignoreError) {
      ASSERT_EQ(pObserver->state, TestObserver::stateSuccess);
    }
  }

  void SetLocal(TestObserver::Action action, std::string local,
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
    ASSERT_TRUE_WAIT(pObserver->state != TestObserver::stateNoResponse,
                     kDefaultTimeout);
    ASSERT_EQ(signaling_state(), endState);
    if (!ignoreError) {
      ASSERT_EQ(pObserver->state, TestObserver::stateSuccess);
    }
  }

  void DoTrickleIce(ParsedSDP &sdp) {
    int expectAddIce = 0;
    pObserver->addIceSuccessCount = 0;
    for (std::multimap<int, std::string>::iterator it =
           sdp.ice_candidates_.begin();
         it != sdp.ice_candidates_.end(); ++it) {
      if ((*it).first != 0) {
        std::cerr << "Adding trickle ICE candidate " << (*it).second
                  << std::endl;
        ASSERT_TRUE(NS_SUCCEEDED(pc->AddIceCandidate((*it).second.c_str(), "", (*it).first)));
        expectAddIce++;
      }
    }
    ASSERT_TRUE_WAIT(pObserver->addIceSuccessCount == expectAddIce,
                     kDefaultTimeout);
  }


  void DoTrickleIceChrome(ParsedSDP &sdp) {
    int expectAddIce = 0;
    pObserver->addIceSuccessCount = 0;
    for (std::multimap<int, std::string>::iterator it =
           sdp.ice_candidates_.begin();
         it != sdp.ice_candidates_.end(); ++it) {
      if ((*it).first != 0) {
        std::string candidate = "a=" + (*it).second + "\r\n";
        std::cerr << "Adding trickle ICE candidate " << candidate << std::endl;

        ASSERT_TRUE(NS_SUCCEEDED(pc->AddIceCandidate(candidate.c_str(), "", (*it).first)));
        expectAddIce++;
      }
    }
    ASSERT_TRUE_WAIT(pObserver->addIceSuccessCount == expectAddIce,
                     kDefaultTimeout);
  }


  bool IceCompleted() {
    return pc->IceConnectionState() == PCImplIceConnectionState::Connected;
  }

  void AddIceCandidate(const char* candidate, const char* mid, unsigned short level,
                       bool expectSuccess) {
    PCImplSignalingState endState = signaling_state();
    pObserver->state = TestObserver::stateNoResponse;
    pc->AddIceCandidate(candidate, mid, level);
    ASSERT_TRUE_WAIT(pObserver->state != TestObserver::stateNoResponse,
                     kDefaultTimeout);
    ASSERT_TRUE(pObserver->state ==
                expectSuccess ? TestObserver::stateSuccess :
                                TestObserver::stateError
               );

    // Verify that adding ICE candidates does not change the signaling state
    ASSERT_EQ(signaling_state(), endState);
  }

  int GetPacketsReceived(int stream) {
    std::vector<DOMMediaStream *> streams = pObserver->GetStreams();

    if ((int) streams.size() <= stream) {
      return 0;
    }

    return streams[stream]->GetStream()->AsSourceStream()->GetSegmentsAdded();
  }

  int GetPacketsSent(int stream) {
    return static_cast<Fake_MediaStreamBase *>(
        domMediaStream_->GetStream())->GetSegmentsAdded();
  }

  //Stops generating new audio data for transmission.
  //Should be called before Cleanup of the peer connection.
  void CloseSendStreams() {
    static_cast<Fake_MediaStream*>(
        domMediaStream_->GetStream())->StopStream();
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

  mozilla::RefPtr<mozilla::MediaPipeline> GetMediaPipeline(
    bool local, int stream, int track) {
    sipcc::SourceStreamInfo *streamInfo;

    if (local) {
      streamInfo = pc->media()->GetLocalStream(stream);
    } else {
      streamInfo = pc->media()->GetRemoteStream(stream);
    }

    if (!streamInfo) {
      return nullptr;
    }

    const auto &pipelines = streamInfo->GetPipelines();
    auto it = pipelines.find(track);
    return (it == pipelines.end())? nullptr : it->second;
  }

  void CheckMediaPipeline(int stream, int track, uint32_t flags,
    VideoSessionConduit::FrameRequestType frameRequestMethod =
      VideoSessionConduit::FrameRequestNone) {

    std::cout << name << ": Checking media pipeline settings for "
              << ((flags & PIPELINE_LOCAL) ? "local " : "remote ")
              << ((flags & PIPELINE_SEND) ? "sending " : "receiving ")
              << ((flags & PIPELINE_VIDEO) ? "video" : "audio")
              << " pipeline (stream " << stream
              << ", track " << track << "); expect "
              << ((flags & PIPELINE_RTCP_MUX) ? "MUX, " : "no MUX, ")
              << ((flags & PIPELINE_RTCP_NACK) ? "NACK." : "no NACK.")
              << std::endl;

    mozilla::RefPtr<mozilla::MediaPipeline> pipeline =
      GetMediaPipeline((flags & PIPELINE_LOCAL), stream, track);
    ASSERT_TRUE(pipeline);
    ASSERT_EQ(pipeline->IsDoingRtcpMux(), !!(flags & PIPELINE_RTCP_MUX));
    // We cannot yet test send/recv with video.
    if (!(flags & PIPELINE_VIDEO)) {
      if (flags & PIPELINE_SEND) {
        ASSERT_TRUE_WAIT(pipeline->rtp_packets_sent() >= 40 &&
                         pipeline->rtcp_packets_received() >= 1,
                         kDefaultTimeout);
        ASSERT_GE(pipeline->rtp_packets_sent(), 40);
        ASSERT_GE(pipeline->rtcp_packets_received(), 1);
      } else {
        ASSERT_TRUE_WAIT(pipeline->rtp_packets_received() >= 40 &&
                         pipeline->rtcp_packets_sent() >= 1,
                         kDefaultTimeout);
        ASSERT_GE(pipeline->rtp_packets_received(), 40);
        ASSERT_GE(pipeline->rtcp_packets_sent(), 1);
      }
    }


    // Check feedback method for video
    if (flags & PIPELINE_VIDEO) {
        mozilla::MediaSessionConduit *conduit = pipeline->Conduit();
        ASSERT_TRUE(conduit);
        ASSERT_EQ(conduit->type(), mozilla::MediaSessionConduit::VIDEO);
        mozilla::VideoSessionConduit *video_conduit =
          static_cast<mozilla::VideoSessionConduit*>(conduit);
        ASSERT_EQ(!!(flags & PIPELINE_RTCP_NACK),
                  video_conduit->UsingNackBasic());
        ASSERT_EQ(frameRequestMethod, video_conduit->FrameRequestMethod());
    }
  }

public:
  mozilla::RefPtr<sipcc::PeerConnectionImpl> pc;
  nsRefPtr<TestObserver> pObserver;
  char* offer_;
  char* answer_;
  nsRefPtr<DOMMediaStream> domMediaStream_;
  sipcc::IceConfiguration cfg_;
  const std::string name;

private:
  void SDPSanityCheck(std::string sdp, uint32_t flags, bool offer)
  {
    ASSERT_TRUE(pObserver->state == TestObserver::stateSuccess);
    ASSERT_NE(sdp.find("v=0"), std::string::npos);
    ASSERT_NE(sdp.find("c=IN IP4"), std::string::npos);
    ASSERT_NE(sdp.find("a=fingerprint:sha-256"), std::string::npos);

    std::cout << name << ": SDPSanityCheck flags for "
              << (offer ? "offer" : "answer")
              << " = " << std::hex << std::showbase
              << flags << std::dec

              << ((flags & SHOULD_SEND_AUDIO)?" SHOULD_SEND_AUDIO":"")
              << ((flags & SHOULD_RECV_AUDIO)?" SHOULD_RECV_AUDIO":"")
              << ((flags & SHOULD_INACTIVE_AUDIO)?" SHOULD_INACTIVE_AUDIO":"")
              << ((flags & SHOULD_REJECT_AUDIO)?" SHOULD_REJECT_AUDIO":"")
              << ((flags & SHOULD_OMIT_AUDIO)?" SHOULD_OMIT_AUDIO":"")
              << ((flags & DONT_CHECK_AUDIO)?" DONT_CHECK_AUDIO":"")

              << ((flags & SHOULD_SEND_VIDEO)?" SHOULD_SEND_VIDEO":"")
              << ((flags & SHOULD_RECV_VIDEO)?" SHOULD_RECV_VIDEO":"")
              << ((flags & SHOULD_INACTIVE_VIDEO)?" SHOULD_INACTIVE_VIDEO":"")
              << ((flags & SHOULD_REJECT_VIDEO)?" SHOULD_REJECT_VIDEO":"")
              << ((flags & SHOULD_OMIT_VIDEO)?" SHOULD_OMIT_VIDEO":"")
              << ((flags & DONT_CHECK_VIDEO)?" DONT_CHECK_VIDEO":"")

              << ((flags & SHOULD_INCLUDE_DATA)?" SHOULD_INCLUDE_DATA":"")
              << ((flags & DONT_CHECK_DATA)?" DONT_CHECK_DATA":"")
              << std::endl;

    switch(flags & AUDIO_FLAGS) {
      case 0:
            ASSERT_EQ(sdp.find("a=rtpmap:109 opus/48000"), std::string::npos);
        break;
      case SHOULD_CHECK_AUDIO:
            ASSERT_NE(sdp.find("a=rtpmap:109 opus/48000"), std::string::npos);
            if (offer) {
              ASSERT_NE(sdp.find("a=rtpmap:0 PCMU/8000"), std::string::npos);
            }
        break;
      case SHOULD_SEND_AUDIO:
            ASSERT_NE(sdp.find("a=rtpmap:109 opus/48000"), std::string::npos);
            ASSERT_NE(sdp.find(" 0-15\r\na=sendonly"), std::string::npos);
            if (offer) {
              ASSERT_NE(sdp.find("a=rtpmap:0 PCMU/8000"), std::string::npos);
            }
        break;
      case SHOULD_RECV_AUDIO:
            ASSERT_NE(sdp.find("a=rtpmap:109 opus/48000"), std::string::npos);
            ASSERT_NE(sdp.find(" 0-15\r\na=recvonly"), std::string::npos);
            if (offer) {
              ASSERT_NE(sdp.find("a=rtpmap:0 PCMU/8000"), std::string::npos);
            }
        break;
      case SHOULD_SENDRECV_AUDIO:
            ASSERT_NE(sdp.find("a=rtpmap:109 opus/48000"), std::string::npos);
            ASSERT_NE(sdp.find(" 0-15\r\na=sendrecv"), std::string::npos);
            if (offer) {
              ASSERT_NE(sdp.find("a=rtpmap:0 PCMU/8000"), std::string::npos);
            }
        break;
      case SHOULD_INACTIVE_AUDIO:
            ASSERT_NE(sdp.find("a=rtpmap:109 opus/48000"), std::string::npos);
            ASSERT_NE(sdp.find(" 0-15\r\na=inactive"), std::string::npos);
        break;
      case SHOULD_REJECT_AUDIO:
            ASSERT_EQ(sdp.find("a=rtpmap:109 opus/48000"), std::string::npos);
            ASSERT_NE(sdp.find("m=audio 0 "), std::string::npos);
        break;
      case SHOULD_OMIT_AUDIO:
            ASSERT_EQ(sdp.find("m=audio"), std::string::npos);
        break;
      case DONT_CHECK_AUDIO:
        break;
      default:
            ASSERT_FALSE("Missing case in switch statement");
    }

    switch(flags & VIDEO_FLAGS) {
      case 0:
            ASSERT_EQ(sdp.find("a=rtpmap:120 VP8/90000"), std::string::npos);
        break;
      case SHOULD_CHECK_VIDEO:
            ASSERT_NE(sdp.find("a=rtpmap:120 VP8/90000"), std::string::npos);
        break;
      case SHOULD_SEND_VIDEO:
            ASSERT_NE(sdp.find("a=rtpmap:120 VP8/90000\r\na=sendonly"),
                  std::string::npos);
        break;
      case SHOULD_RECV_VIDEO:
            ASSERT_NE(sdp.find("a=rtpmap:120 VP8/90000\r\na=recvonly"),
                  std::string::npos);
        break;
      case SHOULD_SENDRECV_VIDEO:
            ASSERT_NE(sdp.find("a=rtpmap:120 VP8/90000\r\na=sendrecv"),
                  std::string::npos);
        break;
      case SHOULD_INACTIVE_VIDEO:
            ASSERT_NE(sdp.find("a=rtpmap:120 VP8/90000\r\na=inactive"),
                      std::string::npos);
        break;
      case SHOULD_REJECT_VIDEO:
            ASSERT_NE(sdp.find("m=video 0 "), std::string::npos);
        break;
      case SHOULD_OMIT_VIDEO:
            ASSERT_EQ(sdp.find("m=video"), std::string::npos);
        break;
      case DONT_CHECK_VIDEO:
        break;
      default:
            ASSERT_FALSE("Missing case in switch statement");
    }

    if (flags & SHOULD_INCLUDE_DATA) {
      ASSERT_NE(sdp.find("m=application"), std::string::npos);
    } else if (!(flags & DONT_CHECK_DATA)) {
      ASSERT_EQ(sdp.find("m=application"), std::string::npos);
    }
  }
};

class SignalingEnvironment : public ::testing::Environment {
 public:
  void TearDown() {
    // Signaling is shut down in XPCOM shutdown
  }
};

class SignalingAgentTest : public ::testing::Test {
 public:
  static void SetUpTestCase() {
    ASSERT_TRUE(SetupGlobalThread());
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

  bool CreateAgent(const std::string stun_addr, uint16_t stun_port,
                   bool wait_for_gather = true) {
    ScopedDeletePtr<SignalingAgent> agent(
        new SignalingAgent("agent", stun_addr, stun_port));

    agent->Init(gThread);

    if (wait_for_gather) {
      if (!agent->WaitForGatherAllowFail())
        return false;
    }

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


class SignalingTest : public ::testing::Test {
public:
  SignalingTest()
      : init_(false),
        a1_(nullptr),
        a2_(nullptr),
        wait_for_gather_(true),
        stun_addr_(g_stun_server_address),
        stun_port_(g_stun_server_port) {}

  SignalingTest(const std::string& stun_addr, uint16_t stun_port)
      : a1_(nullptr),
        a2_(nullptr),
        wait_for_gather_(true),
        stun_addr_(stun_addr),
        stun_port_(stun_port) {}

  static void SetUpTestCase() {
    ASSERT_TRUE(SetupGlobalThread());
  }

  void EnsureInit() {

    if (init_)
      return;

    a1_ = new SignalingAgent(callerName, stun_addr_, stun_port_);
    a2_ = new SignalingAgent(calleeName, stun_addr_, stun_port_);

    a1_->Init(gThread);
    a2_->Init(gThread);

    if (wait_for_gather_) {
      WaitForGather();
    }
  }

  void WaitForGather() {
    a1_->WaitForGather();
    a2_->WaitForGather();
  }

  static void TearDownTestCase() {
    gThread = nullptr;
  }

  void CreateOffer(sipcc::MediaConstraints& constraints,
                   uint32_t offerFlags, uint32_t sdpCheck) {
    EnsureInit();
    a1_->CreateOffer(constraints, offerFlags, sdpCheck);
  }

  void CreateSetOffer(sipcc::MediaConstraints& constraints, uint32_t sdpCheck) {
    EnsureInit();
    a1_->CreateOffer(constraints, OFFER_AV, sdpCheck);
    a1_->SetLocal(TestObserver::OFFER, a1_->offer());
  }

  void OfferAnswer(sipcc::MediaConstraints& aconstraints,
                   sipcc::MediaConstraints& bconstraints,
                   uint32_t offerAnswerFlags,
                   bool finishAfterAnswer, uint32_t offerSdpCheck,
                   uint32_t answerSdpCheck) {
    EnsureInit();
    a1_->CreateOffer(aconstraints, offerAnswerFlags, offerSdpCheck);
    a1_->SetLocal(TestObserver::OFFER, a1_->offer());
    a2_->SetRemote(TestObserver::OFFER, a1_->offer());
    a2_->CreateAnswer(bconstraints, a1_->offer(),
                     offerAnswerFlags, answerSdpCheck);
    if(true == finishAfterAnswer) {
        a2_->SetLocal(TestObserver::ANSWER, a2_->answer());
        a1_->SetRemote(TestObserver::ANSWER, a2_->answer());

        ASSERT_TRUE_WAIT(a1_->IceCompleted() == true, kDefaultTimeout);
        ASSERT_TRUE_WAIT(a2_->IceCompleted() == true, kDefaultTimeout);
    }
  }

  void OfferModifiedAnswer(sipcc::MediaConstraints& aconstraints,
                           sipcc::MediaConstraints& bconstraints,
                           uint32_t offerSdpCheck, uint32_t answerSdpCheck) {
    EnsureInit();
    a1_->CreateOffer(aconstraints, OFFER_AUDIO, offerSdpCheck);
    a1_->SetLocal(TestObserver::OFFER, a1_->offer());
    a2_->SetRemote(TestObserver::OFFER, a1_->offer());
    a2_->CreateAnswer(bconstraints, a1_->offer(), OFFER_AUDIO | ANSWER_AUDIO,
                     answerSdpCheck);
    a2_->SetLocal(TestObserver::ANSWER, a2_->answer());
    ParsedSDP sdpWrapper(a2_->answer());
    sdpWrapper.ReplaceLine("m=audio", "m=audio 65375 RTP/SAVPF 109 8 101\r\n");
    sdpWrapper.AddLine("a=rtpmap:8 PCMA/8000\r\n");
    std::cout << "Modified SDP " << std::endl
              << indent(sdpWrapper.getSdp()) << std::endl;
    a1_->SetRemote(TestObserver::ANSWER, sdpWrapper.getSdp());
    ASSERT_TRUE_WAIT(a1_->IceCompleted() == true, kDefaultTimeout);
    ASSERT_TRUE_WAIT(a2_->IceCompleted() == true, kDefaultTimeout);
  }

  void OfferAnswerTrickle(sipcc::MediaConstraints& aconstraints,
                          sipcc::MediaConstraints& bconstraints,
                          uint32_t offerSdpCheck, uint32_t answerSdpCheck) {
    EnsureInit();
    a1_->CreateOffer(aconstraints, OFFER_AV, offerSdpCheck);
    a1_->SetLocal(TestObserver::OFFER, a1_->offer());
    ParsedSDP a1_offer(a1_->offer());
    a2_->SetRemote(TestObserver::OFFER, a1_offer.sdp_without_ice_);
    a2_->CreateAnswer(bconstraints, a1_offer.sdp_without_ice_,
                     OFFER_AV|ANSWER_AV, answerSdpCheck);
    a2_->SetLocal(TestObserver::ANSWER, a2_->answer());
    ParsedSDP a2_answer(a2_->answer());
    a1_->SetRemote(TestObserver::ANSWER, a2_answer.sdp_without_ice_);
    // Now set the trickle ICE candidates
    a1_->DoTrickleIce(a2_answer);
    a2_->DoTrickleIce(a1_offer);
    ASSERT_TRUE_WAIT(a1_->IceCompleted() == true, kDefaultTimeout);
    ASSERT_TRUE_WAIT(a2_->IceCompleted() == true, kDefaultTimeout);
  }


  void OfferAnswerTrickleChrome(sipcc::MediaConstraints& aconstraints,
                          sipcc::MediaConstraints& bconstraints,
                          uint32_t offerSdpCheck, uint32_t answerSdpCheck) {
    EnsureInit();
    a1_->CreateOffer(aconstraints, OFFER_AV, offerSdpCheck);
    a1_->SetLocal(TestObserver::OFFER, a1_->offer());
    ParsedSDP a1_offer(a1_->offer());
    a2_->SetRemote(TestObserver::OFFER, a1_offer.sdp_without_ice_);
    a2_->CreateAnswer(bconstraints, a1_offer.sdp_without_ice_,
                     OFFER_AV|ANSWER_AV, answerSdpCheck);
    a2_->SetLocal(TestObserver::ANSWER, a2_->answer());
    ParsedSDP a2_answer(a2_->answer());
    a1_->SetRemote(TestObserver::ANSWER, a2_answer.sdp_without_ice_);
    // Now set the trickle ICE candidates
    a1_->DoTrickleIceChrome(a2_answer);
    a2_->DoTrickleIceChrome(a1_offer);
    ASSERT_TRUE_WAIT(a1_->IceCompleted() == true, kDefaultTimeout);
    ASSERT_TRUE_WAIT(a2_->IceCompleted() == true, kDefaultTimeout);
  }

  void CreateOfferRemoveStream(sipcc::MediaConstraints& constraints,
                               uint32_t hints, uint32_t sdpCheck) {
    EnsureInit();
    sipcc::MediaConstraints aconstraints;
    aconstraints.setBooleanConstraint("OfferToReceiveAudio", true, false);
    aconstraints.setBooleanConstraint("OfferToReceiveVideo", true, false);
    a1_->CreateOffer(aconstraints, OFFER_AV, SHOULD_SENDRECV_AV );
    a1_->CreateOfferRemoveStream(constraints, hints, sdpCheck);
  }

  void CreateOfferAudioOnly(sipcc::MediaConstraints& constraints,
                            uint32_t sdpCheck) {
    EnsureInit();
    a1_->CreateOffer(constraints, OFFER_AUDIO, sdpCheck);
  }

  void CreateOfferAddCandidate(sipcc::MediaConstraints& constraints,
                               const char * candidate, const char * mid,
                               unsigned short level, uint32_t sdpCheck) {
    EnsureInit();
    a1_->CreateOffer(constraints, OFFER_AV, sdpCheck);
    a1_->AddIceCandidate(candidate, mid, level, true);
  }

  void AddIceCandidateEarly(const char * candidate, const char * mid,
                            unsigned short level) {
    EnsureInit();
    a1_->AddIceCandidate(candidate, mid, level, false);
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

  void TestRtcpFb(const std::set<std::string>& feedback,
                  uint32_t rtcpFbFlags,
                  VideoSessionConduit::FrameRequestType frameRequestMethod) {
    EnsureInit();
    sipcc::MediaConstraints constraints;

    a1_->CreateOffer(constraints, OFFER_AV, SHOULD_SENDRECV_AV);
    a1_->SetLocal(TestObserver::OFFER, a1_->offer());

    ParsedSDP sdpWrapper(a1_->offer());

    // Strip out any existing rtcp-fb lines
    sdpWrapper.DeleteAllLines("a=rtcp-fb:120");

    // Add rtcp-fb lines for the desired feedback types
    // We know that the video section is generated second (last),
    // so appending these to the end of the SDP has the desired effect.
    std::set<std::string>::const_iterator it;
    for (it = feedback.begin(); it != feedback.end(); ++it) {
      sdpWrapper.AddLine(std::string("a=rtcp-fb:120 ") + (*it) + "\r\n");
    }

    std::cout << "Modified SDP " << std::endl
              << indent(sdpWrapper.getSdp()) << std::endl;

    // Double-check that the offered SDP matches what we expect
    CheckRtcpFbSdp(sdpWrapper.getSdp(), feedback);

    a2_->SetRemote(TestObserver::OFFER, sdpWrapper.getSdp());
    a2_->CreateAnswer(constraints, sdpWrapper.getSdp(), OFFER_AV | ANSWER_AV);

    CheckRtcpFbSdp(a2_->answer(), feedback);

    a2_->SetLocal(TestObserver::ANSWER, a2_->answer());
    a1_->SetRemote(TestObserver::ANSWER, a2_->answer());

    ASSERT_TRUE_WAIT(a1_->IceCompleted() == true, kDefaultTimeout);
    ASSERT_TRUE_WAIT(a2_->IceCompleted() == true, kDefaultTimeout);

    a1_->CloseSendStreams();
    a1_->CloseReceiveStreams();
    a2_->CloseSendStreams();
    a2_->CloseReceiveStreams();

    // Check caller video settings for remote pipeline
    a1_->CheckMediaPipeline(0, 2, (fRtcpMux ? PIPELINE_RTCP_MUX : 0) |
      PIPELINE_SEND | PIPELINE_VIDEO | rtcpFbFlags, frameRequestMethod);

    // Check callee video settings for remote pipeline
    a2_->CheckMediaPipeline(0, 2, (fRtcpMux ? PIPELINE_RTCP_MUX : 0) |
      PIPELINE_VIDEO | rtcpFbFlags, frameRequestMethod);
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

 protected:
  bool init_;
  ScopedDeletePtr<SignalingAgent> a1_;  // Canonically "caller"
  ScopedDeletePtr<SignalingAgent> a2_;  // Canonically "callee"
  bool wait_for_gather_;
  std::string stun_addr_;
  uint16_t stun_port_;
};

class FsFrPrefClearer {
  public:
    FsFrPrefClearer(nsCOMPtr<nsIPrefBranch> prefs): mPrefs(prefs) {}
    ~FsFrPrefClearer() {
      mPrefs->ClearUserPref("media.navigator.video.max_fs");
      mPrefs->ClearUserPref("media.navigator.video.max_fr");
    }
  private:
    nsCOMPtr<nsIPrefBranch> mPrefs;
};

TEST_F(SignalingTest, JustInit)
{
}

TEST_F(SignalingTest, CreateSetOffer)
{
  sipcc::MediaConstraints constraints;
  CreateSetOffer(constraints, SHOULD_SENDRECV_AV);
}

TEST_F(SignalingTest, CreateOfferAudioVideoConstraintUndefined)
{
  sipcc::MediaConstraints constraints;
  CreateOffer(constraints, OFFER_AV, SHOULD_SENDRECV_AV);
}

TEST_F(SignalingTest, CreateOfferNoVideoStreamRecvVideo)
{
  sipcc::MediaConstraints constraints;
  constraints.setBooleanConstraint("OfferToReceiveAudio", true, false);
  constraints.setBooleanConstraint("OfferToReceiveVideo", true, false);
  CreateOffer(constraints, OFFER_AUDIO,
              SHOULD_SENDRECV_AUDIO | SHOULD_RECV_VIDEO);
}

TEST_F(SignalingTest, CreateOfferNoAudioStreamRecvAudio)
{
  sipcc::MediaConstraints constraints;
  constraints.setBooleanConstraint("OfferToReceiveAudio", true, false);
  constraints.setBooleanConstraint("OfferToReceiveVideo", true, false);
  CreateOffer(constraints, OFFER_VIDEO,
              SHOULD_RECV_AUDIO | SHOULD_SENDRECV_VIDEO);
}

TEST_F(SignalingTest, CreateOfferNoVideoStream)
{
  sipcc::MediaConstraints constraints;
  constraints.setBooleanConstraint("OfferToReceiveAudio", true, false);
  constraints.setBooleanConstraint("OfferToReceiveVideo", false, false);
  CreateOffer(constraints, OFFER_AUDIO,
              SHOULD_SENDRECV_AUDIO | SHOULD_OMIT_VIDEO);
}

TEST_F(SignalingTest, CreateOfferNoAudioStream)
{
  sipcc::MediaConstraints constraints;
  constraints.setBooleanConstraint("OfferToReceiveAudio", false, false);
  constraints.setBooleanConstraint("OfferToReceiveVideo", true, false);
  CreateOffer(constraints, OFFER_VIDEO,
              SHOULD_OMIT_AUDIO | SHOULD_SENDRECV_VIDEO);
}

TEST_F(SignalingTest, CreateOfferDontReceiveAudio)
{
  sipcc::MediaConstraints constraints;
  constraints.setBooleanConstraint("OfferToReceiveAudio", false, false);
  constraints.setBooleanConstraint("OfferToReceiveVideo", true, false);
  CreateOffer(constraints, OFFER_AV,
              SHOULD_SEND_AUDIO | SHOULD_SENDRECV_VIDEO);
}

TEST_F(SignalingTest, CreateOfferDontReceiveVideo)
{
  sipcc::MediaConstraints constraints;
  constraints.setBooleanConstraint("OfferToReceiveAudio", true, false);
  constraints.setBooleanConstraint("OfferToReceiveVideo", false, false);
  CreateOffer(constraints, OFFER_AV,
              SHOULD_SENDRECV_AUDIO | SHOULD_SEND_VIDEO);
}

// XXX Disabled pending resolution of Bug 840728
TEST_F(SignalingTest, DISABLED_CreateOfferRemoveAudioStream)
{
  sipcc::MediaConstraints constraints;
  constraints.setBooleanConstraint("OfferToReceiveAudio", true, false);
  constraints.setBooleanConstraint("OfferToReceiveVideo", true, false);
  CreateOfferRemoveStream(constraints, DOMMediaStream::HINT_CONTENTS_AUDIO,
              SHOULD_RECV_AUDIO | SHOULD_SENDRECV_VIDEO);
}

// XXX Disabled pending resolution of Bug 840728
TEST_F(SignalingTest, DISABLED_CreateOfferDontReceiveAudioRemoveAudioStream)
{
  sipcc::MediaConstraints constraints;
  constraints.setBooleanConstraint("OfferToReceiveAudio", false, false);
  constraints.setBooleanConstraint("OfferToReceiveVideo", true, false);
  CreateOfferRemoveStream(constraints, DOMMediaStream::HINT_CONTENTS_AUDIO,
              SHOULD_SENDRECV_VIDEO);
}

// XXX Disabled pending resolution of Bug 840728
TEST_F(SignalingTest, DISABLED_CreateOfferDontReceiveVideoRemoveVideoStream)
{
  sipcc::MediaConstraints constraints;
  constraints.setBooleanConstraint("OfferToReceiveAudio", true, false);
  constraints.setBooleanConstraint("OfferToReceiveVideo", false, false);
  CreateOfferRemoveStream(constraints, DOMMediaStream::HINT_CONTENTS_VIDEO,
              SHOULD_SENDRECV_AUDIO);
}

TEST_F(SignalingTest, OfferAnswerNothingDisabled)
{
  sipcc::MediaConstraints constraints;
  OfferAnswer(constraints, constraints, OFFER_AV | ANSWER_AV, false,
              SHOULD_SENDRECV_AV, SHOULD_SENDRECV_AV);
}

TEST_F(SignalingTest, OfferAnswerDontReceiveAudioOnOffer)
{
  sipcc::MediaConstraints offerconstraints;
  offerconstraints.setBooleanConstraint("OfferToReceiveAudio", false, false);
  offerconstraints.setBooleanConstraint("OfferToReceiveVideo", true, false);
  sipcc::MediaConstraints answerconstraints;
  answerconstraints.setBooleanConstraint("OfferToReceiveAudio", true, false);
  answerconstraints.setBooleanConstraint("OfferToReceiveVideo", true, false);
  OfferAnswer(offerconstraints, answerconstraints, OFFER_AV | ANSWER_AV,
              false, SHOULD_SEND_AUDIO | SHOULD_SENDRECV_VIDEO,
              SHOULD_RECV_AUDIO | SHOULD_SENDRECV_VIDEO);
}

TEST_F(SignalingTest, OfferAnswerDontReceiveVideoOnOffer)
{
  sipcc::MediaConstraints offerconstraints;
  offerconstraints.setBooleanConstraint("OfferToReceiveAudio", true, false);
  offerconstraints.setBooleanConstraint("OfferToReceiveVideo", false, false);
  sipcc::MediaConstraints answerconstraints;
  answerconstraints.setBooleanConstraint("OfferToReceiveAudio", true, false);
  answerconstraints.setBooleanConstraint("OfferToReceiveVideo", true, false);
  OfferAnswer(offerconstraints, answerconstraints, OFFER_AV | ANSWER_AV,
              false, SHOULD_SENDRECV_AUDIO | SHOULD_SEND_VIDEO,
              SHOULD_SENDRECV_AUDIO | SHOULD_RECV_VIDEO);
}

TEST_F(SignalingTest, OfferAnswerDontReceiveAudioOnAnswer)
{
  sipcc::MediaConstraints offerconstraints;
  offerconstraints.setBooleanConstraint("OfferToReceiveAudio", true, false);
  offerconstraints.setBooleanConstraint("OfferToReceiveVideo", true, false);
  sipcc::MediaConstraints answerconstraints;
  answerconstraints.setBooleanConstraint("OfferToReceiveAudio", false, false);
  answerconstraints.setBooleanConstraint("OfferToReceiveVideo", true, false);
  OfferAnswer(offerconstraints, answerconstraints, OFFER_AV | ANSWER_AV,
              false, SHOULD_SENDRECV_AV,
              SHOULD_SEND_AUDIO | SHOULD_SENDRECV_VIDEO);
}

TEST_F(SignalingTest, OfferAnswerDontReceiveVideoOnAnswer)
{
  sipcc::MediaConstraints offerconstraints;
  offerconstraints.setBooleanConstraint("OfferToReceiveAudio", true, false);
  offerconstraints.setBooleanConstraint("OfferToReceiveVideo", true, false);
  sipcc::MediaConstraints answerconstraints;
  answerconstraints.setBooleanConstraint("OfferToReceiveAudio", true, false);
  answerconstraints.setBooleanConstraint("OfferToReceiveVideo", false, false);
  OfferAnswer(offerconstraints, answerconstraints, OFFER_AV | ANSWER_AV,
              false, SHOULD_SENDRECV_AV,
              SHOULD_SENDRECV_AUDIO | SHOULD_SEND_VIDEO);
}

TEST_F(SignalingTest, OfferAnswerDontAddAudioStreamOnOfferRecvAudio)
{
  sipcc::MediaConstraints offerconstraints;
  offerconstraints.setBooleanConstraint("OfferToReceiveAudio", true, false);
  offerconstraints.setBooleanConstraint("OfferToReceiveVideo", true, false);
  sipcc::MediaConstraints answerconstraints;
  answerconstraints.setBooleanConstraint("OfferToReceiveAudio", true, false);
  answerconstraints.setBooleanConstraint("OfferToReceiveVideo", true, false);
  OfferAnswer(offerconstraints, answerconstraints, OFFER_VIDEO | ANSWER_AV,
              false, SHOULD_RECV_AUDIO | SHOULD_SENDRECV_VIDEO,
              SHOULD_SEND_AUDIO | SHOULD_SENDRECV_VIDEO);
}

TEST_F(SignalingTest, OfferAnswerDontAddAudioStreamOnOffer)
{
  sipcc::MediaConstraints offerconstraints;
  offerconstraints.setBooleanConstraint("OfferToReceiveAudio", false, false);
  offerconstraints.setBooleanConstraint("OfferToReceiveVideo", true, false);
  sipcc::MediaConstraints answerconstraints;
  answerconstraints.setBooleanConstraint("OfferToReceiveAudio", true, false);
  answerconstraints.setBooleanConstraint("OfferToReceiveVideo", true, false);
  OfferAnswer(offerconstraints, answerconstraints, OFFER_VIDEO | ANSWER_AV,
              false, SHOULD_OMIT_AUDIO | SHOULD_SENDRECV_VIDEO,
              SHOULD_OMIT_AUDIO | SHOULD_SENDRECV_VIDEO);
}

TEST_F(SignalingTest, OfferAnswerDontAddVideoStreamOnOfferRecvVideo)
{
  sipcc::MediaConstraints offerconstraints;
  offerconstraints.setBooleanConstraint("OfferToReceiveAudio", true, false);
  offerconstraints.setBooleanConstraint("OfferToReceiveVideo", true, false);
  sipcc::MediaConstraints answerconstraints;
  answerconstraints.setBooleanConstraint("OfferToReceiveAudio", true, false);
  answerconstraints.setBooleanConstraint("OfferToReceiveVideo", true, false);
  OfferAnswer(offerconstraints, answerconstraints, OFFER_AUDIO | ANSWER_AV,
              false, SHOULD_SENDRECV_AUDIO | SHOULD_RECV_VIDEO,
              SHOULD_SENDRECV_AUDIO | SHOULD_SEND_VIDEO);
}

TEST_F(SignalingTest, OfferAnswerDontAddVideoStreamOnOffer)
{
  sipcc::MediaConstraints offerconstraints;
  offerconstraints.setBooleanConstraint("OfferToReceiveAudio", true, false);
  offerconstraints.setBooleanConstraint("OfferToReceiveVideo", false, false);
  sipcc::MediaConstraints answerconstraints;
  answerconstraints.setBooleanConstraint("OfferToReceiveAudio", true, false);
  answerconstraints.setBooleanConstraint("OfferToReceiveVideo", true, false);
  OfferAnswer(offerconstraints, answerconstraints, OFFER_AUDIO | ANSWER_AV,
              false, SHOULD_SENDRECV_AUDIO | SHOULD_OMIT_VIDEO,
              SHOULD_SENDRECV_AUDIO | SHOULD_OMIT_VIDEO);
}

TEST_F(SignalingTest, OfferAnswerDontAddAudioStreamOnAnswer)
{
  sipcc::MediaConstraints offerconstraints;
  offerconstraints.setBooleanConstraint("OfferToReceiveAudio", true, false);
  offerconstraints.setBooleanConstraint("OfferToReceiveVideo", true, false);
  sipcc::MediaConstraints answerconstraints;
  answerconstraints.setBooleanConstraint("OfferToReceiveAudio", true, false);
  answerconstraints.setBooleanConstraint("OfferToReceiveVideo", true, false);
  OfferAnswer(offerconstraints, answerconstraints, OFFER_AV | ANSWER_VIDEO,
              false, SHOULD_SENDRECV_AV,
              SHOULD_RECV_AUDIO | SHOULD_SENDRECV_VIDEO);
}

TEST_F(SignalingTest, OfferAnswerDontAddVideoStreamOnAnswer)
{
  sipcc::MediaConstraints offerconstraints;
  offerconstraints.setBooleanConstraint("OfferToReceiveAudio", true, false);
  offerconstraints.setBooleanConstraint("OfferToReceiveVideo", true, false);
  sipcc::MediaConstraints answerconstraints;
  answerconstraints.setBooleanConstraint("OfferToReceiveAudio", true, false);
  answerconstraints.setBooleanConstraint("OfferToReceiveVideo", true, false);
  OfferAnswer(offerconstraints, answerconstraints, OFFER_AV | ANSWER_AUDIO,
              false, SHOULD_SENDRECV_AV,
              SHOULD_SENDRECV_AUDIO | SHOULD_RECV_VIDEO);
}

TEST_F(SignalingTest, OfferAnswerDontAddVideoStreamOnAnswerDontReceiveVideoOnAnswer)
{
  sipcc::MediaConstraints offerconstraints;
  offerconstraints.setBooleanConstraint("OfferToReceiveAudio", true, false);
  offerconstraints.setBooleanConstraint("OfferToReceiveVideo", true, false);
  sipcc::MediaConstraints answerconstraints;
  answerconstraints.setBooleanConstraint("OfferToReceiveAudio", true, false);
  answerconstraints.setBooleanConstraint("OfferToReceiveVideo", false, false);
  OfferAnswer(offerconstraints, answerconstraints, OFFER_AV | ANSWER_AUDIO,
              false, SHOULD_SENDRECV_AV, SHOULD_SENDRECV_AUDIO );
}

TEST_F(SignalingTest, OfferAnswerDontAddAudioStreamOnAnswerDontReceiveAudioOnAnswer)
{
  sipcc::MediaConstraints offerconstraints;
  offerconstraints.setBooleanConstraint("OfferToReceiveAudio", true, false);
  offerconstraints.setBooleanConstraint("OfferToReceiveVideo", true, false);
  sipcc::MediaConstraints answerconstraints;
  answerconstraints.setBooleanConstraint("OfferToReceiveAudio", false, false);
  answerconstraints.setBooleanConstraint("OfferToReceiveVideo", true, false);
  OfferAnswer(offerconstraints, answerconstraints, OFFER_AV | ANSWER_VIDEO,
              false, SHOULD_SENDRECV_AV,
              SHOULD_REJECT_AUDIO | SHOULD_SENDRECV_VIDEO);
}

TEST_F(SignalingTest, OfferAnswerDontAddAudioStreamOnOfferDontReceiveAudioOnOffer)
{
  sipcc::MediaConstraints offerconstraints;
  offerconstraints.setBooleanConstraint("OfferToReceiveAudio", false, false);
  offerconstraints.setBooleanConstraint("OfferToReceiveVideo", true, false);
  sipcc::MediaConstraints answerconstraints;
  answerconstraints.setBooleanConstraint("OfferToReceiveAudio", true, false);
  answerconstraints.setBooleanConstraint("OfferToReceiveVideo", true, false);
  OfferAnswer(offerconstraints, answerconstraints, OFFER_VIDEO | ANSWER_AV,
              false, SHOULD_SENDRECV_VIDEO, SHOULD_SENDRECV_VIDEO);
}

TEST_F(SignalingTest, OfferAnswerDontAddVideoStreamOnOfferDontReceiveVideoOnOffer)
{
  sipcc::MediaConstraints offerconstraints;
  offerconstraints.setBooleanConstraint("OfferToReceiveAudio", true, false);
  offerconstraints.setBooleanConstraint("OfferToReceiveVideo", false, false);
  sipcc::MediaConstraints answerconstraints;
  answerconstraints.setBooleanConstraint("OfferToReceiveAudio", true, false);
  answerconstraints.setBooleanConstraint("OfferToReceiveVideo", true, false);
  OfferAnswer(offerconstraints, answerconstraints, OFFER_AUDIO | ANSWER_AV,
              false, SHOULD_SENDRECV_AUDIO | SHOULD_OMIT_VIDEO,
              SHOULD_SENDRECV_AUDIO | SHOULD_OMIT_VIDEO);
}

TEST_F(SignalingTest, OfferAnswerDontReceiveAudioNoAudioStreamOnOfferDontReceiveVideoOnAnswer)
{
  sipcc::MediaConstraints offerconstraints;
  offerconstraints.setBooleanConstraint("OfferToReceiveAudio", false, false);
  offerconstraints.setBooleanConstraint("OfferToReceiveVideo", true, false);
  sipcc::MediaConstraints answerconstraints;
  answerconstraints.setBooleanConstraint("OfferToReceiveAudio", true, false);
  answerconstraints.setBooleanConstraint("OfferToReceiveVideo", false, false);
  OfferAnswer(offerconstraints, answerconstraints, OFFER_VIDEO | ANSWER_AV,
              false, SHOULD_SENDRECV_VIDEO, SHOULD_SEND_VIDEO);
}

TEST_F(SignalingTest, CreateOfferAddCandidate)
{
  sipcc::MediaConstraints constraints;
  CreateOfferAddCandidate(constraints, strSampleCandidate.c_str(),
                          strSampleMid.c_str(), nSamplelevel,
                          SHOULD_SENDRECV_AV);
}

TEST_F(SignalingTest, AddIceCandidateEarly)
{
  sipcc::MediaConstraints constraints;
  AddIceCandidateEarly(strSampleCandidate.c_str(),
                       strSampleMid.c_str(), nSamplelevel);
}

// XXX adam@nostrum.com -- This test seems questionable; we need to think
// through what actually needs to be tested here.
TEST_F(SignalingTest, DISABLED_OfferAnswerReNegotiateOfferAnswerDontReceiveVideoNoVideoStream)
{
  sipcc::MediaConstraints aconstraints;
  aconstraints.setBooleanConstraint("OfferToReceiveAudio", true, false);
  aconstraints.setBooleanConstraint("OfferToReceiveVideo", true, false);

  sipcc::MediaConstraints bconstraints;
  bconstraints.setBooleanConstraint("OfferToReceiveAudio", true, false);
  bconstraints.setBooleanConstraint("OfferToReceiveVideo", false, false);

  OfferAnswer(aconstraints, aconstraints, OFFER_AV | ANSWER_AV,
              false, SHOULD_SENDRECV_AV, SHOULD_SENDRECV_AV);
  OfferAnswer(bconstraints, bconstraints, OFFER_AUDIO | ANSWER_AV,
              false, SHOULD_SENDRECV_AUDIO | SHOULD_SEND_VIDEO,
              SHOULD_SENDRECV_AUDIO | SHOULD_INACTIVE_VIDEO);
}

TEST_F(SignalingTest, OfferAnswerDontAddAudioStreamOnAnswerNoConstraints)
{
  sipcc::MediaConstraints offerconstraints;
  offerconstraints.setBooleanConstraint("OfferToReceiveAudio", true, false);
  offerconstraints.setBooleanConstraint("OfferToReceiveVideo", true, false);
  sipcc::MediaConstraints answerconstraints;
  OfferAnswer(offerconstraints, answerconstraints, OFFER_AV | ANSWER_VIDEO,
              false, SHOULD_SENDRECV_AV,
              SHOULD_RECV_AUDIO | SHOULD_SENDRECV_VIDEO);
}

TEST_F(SignalingTest, OfferAnswerDontAddVideoStreamOnAnswerNoConstraints)
{
  sipcc::MediaConstraints offerconstraints;
  offerconstraints.setBooleanConstraint("OfferToReceiveAudio", true, false);
  offerconstraints.setBooleanConstraint("OfferToReceiveVideo", true, false);
  sipcc::MediaConstraints answerconstraints;
  OfferAnswer(offerconstraints, answerconstraints, OFFER_AV | ANSWER_AUDIO,
              false, SHOULD_SENDRECV_AV,
              SHOULD_SENDRECV_AUDIO | SHOULD_RECV_VIDEO);
}

TEST_F(SignalingTest, OfferAnswerDontAddAudioVideoStreamsOnAnswerNoConstraints)
{
  sipcc::MediaConstraints offerconstraints;
  offerconstraints.setBooleanConstraint("OfferToReceiveAudio", true, false);
  offerconstraints.setBooleanConstraint("OfferToReceiveVideo", true, false);
  sipcc::MediaConstraints answerconstraints;
  OfferAnswer(offerconstraints, answerconstraints, OFFER_AV | ANSWER_NONE,
              false, SHOULD_SENDRECV_AV,
              SHOULD_RECV_AUDIO | SHOULD_RECV_VIDEO);
}

TEST_F(SignalingTest, FullCall)
{
  sipcc::MediaConstraints constraints;
  OfferAnswer(constraints, constraints, OFFER_AV | ANSWER_AV,
              true, SHOULD_SENDRECV_AV, SHOULD_SENDRECV_AV);

  // Wait for some data to get written
  ASSERT_TRUE_WAIT(a1_->GetPacketsSent(0) >= 40 &&
                   a2_->GetPacketsReceived(0) >= 40, kDefaultTimeout * 2);

  a1_->CloseSendStreams();
  a2_->CloseReceiveStreams();
  // Check that we wrote a bunch of data
  ASSERT_GE(a1_->GetPacketsSent(0), 40);
  //ASSERT_GE(a2_->GetPacketsSent(0), 40);
  //ASSERT_GE(a1_->GetPacketsReceived(0), 40);
  ASSERT_GE(a2_->GetPacketsReceived(0), 40);

  // Check the low-level media pipeline
  // for RTP and RTCP flows
  // The first Local pipeline gets stored at 0
  a1_->CheckMediaPipeline(0, 0, fRtcpMux ?
    PIPELINE_LOCAL | PIPELINE_RTCP_MUX | PIPELINE_SEND :
    PIPELINE_LOCAL | PIPELINE_SEND);

  // The first Remote pipeline gets stored at 1
  a2_->CheckMediaPipeline(0, 1, (fRtcpMux ?  PIPELINE_RTCP_MUX : 0));
}

TEST_F(SignalingTest, FullCallAudioOnly)
{
  sipcc::MediaConstraints constraints;
  OfferAnswer(constraints, constraints, OFFER_AUDIO | ANSWER_AUDIO,
              true, SHOULD_SENDRECV_AUDIO, SHOULD_SENDRECV_AUDIO);

  // Wait for some data to get written
  ASSERT_TRUE_WAIT(a1_->GetPacketsSent(0) >= 40 &&
                   a2_->GetPacketsReceived(0) >= 40, kDefaultTimeout * 2);

  a1_->CloseSendStreams();
  a2_->CloseReceiveStreams();
  // Check that we wrote a bunch of data
  ASSERT_GE(a1_->GetPacketsSent(0), 40);
  //ASSERT_GE(a2_->GetPacketsSent(0), 40);
  //ASSERT_GE(a1_->GetPacketsReceived(0), 40);
  ASSERT_GE(a2_->GetPacketsReceived(0), 40);
}

TEST_F(SignalingTest, FullCallAnswererRejectsVideo)
{
  sipcc::MediaConstraints offerconstraints;
  sipcc::MediaConstraints answerconstraints;
  answerconstraints.setBooleanConstraint("OfferToReceiveAudio", true, false);
  answerconstraints.setBooleanConstraint("OfferToReceiveVideo", false, false);
  OfferAnswer(offerconstraints, answerconstraints, OFFER_AV | ANSWER_AUDIO,
              true, SHOULD_SENDRECV_AV, SHOULD_SENDRECV_AUDIO);

  // Wait for some data to get written
  ASSERT_TRUE_WAIT(a1_->GetPacketsSent(0) >= 40 &&
                   a2_->GetPacketsReceived(0) >= 40, kDefaultTimeout * 2);

  a1_->CloseSendStreams();
  a2_->CloseReceiveStreams();
  // Check that we wrote a bunch of data
  ASSERT_GE(a1_->GetPacketsSent(0), 40);
  //ASSERT_GE(a2_->GetPacketsSent(0), 40);
  //ASSERT_GE(a1_->GetPacketsReceived(0), 40);
  ASSERT_GE(a2_->GetPacketsReceived(0), 40);
}

TEST_F(SignalingTest, FullCallVideoOnly)
{
  sipcc::MediaConstraints constraints;
  OfferAnswer(constraints, constraints, OFFER_VIDEO | ANSWER_VIDEO,
              true, SHOULD_SENDRECV_VIDEO, SHOULD_SENDRECV_VIDEO);

  // If we could check for video packets, we would wait for some to be written
  // here. Since we can't, we don't.
  // ASSERT_TRUE_WAIT(a1_->GetPacketsSent(0) >= 40 &&
  //                 a2_->GetPacketsReceived(0) >= 40, kDefaultTimeout * 2);

  a1_->CloseSendStreams();
  a2_->CloseReceiveStreams();

  // FIXME -- Ideally we would check that packets were sent
  // and received; however, the test driver setup does not
  // currently support sending/receiving with Fake_VideoStreamSource.
  //
  // Check that we wrote a bunch of data
  // ASSERT_GE(a1_->GetPacketsSent(0), 40);
  //ASSERT_GE(a2_->GetPacketsSent(0), 40);
  //ASSERT_GE(a1_->GetPacketsReceived(0), 40);
  // ASSERT_GE(a2_->GetPacketsReceived(0), 40);
}

TEST_F(SignalingTest, OfferModifiedAnswer)
{
  sipcc::MediaConstraints constraints;
  OfferModifiedAnswer(constraints, constraints, SHOULD_SENDRECV_AUDIO,
                      SHOULD_SENDRECV_AUDIO);
  a1_->CloseSendStreams();
  a2_->CloseReceiveStreams();
}

TEST_F(SignalingTest, FullCallTrickle)
{
  sipcc::MediaConstraints constraints;
  OfferAnswerTrickle(constraints, constraints,
                     SHOULD_SENDRECV_AV, SHOULD_SENDRECV_AV);

  std::cerr << "ICE handshake completed" << std::endl;

  // Wait for some data to get written
  ASSERT_TRUE_WAIT(a1_->GetPacketsSent(0) >= 40 &&
                   a2_->GetPacketsReceived(0) >= 40, kDefaultTimeout * 2);

  a1_->CloseSendStreams();
  a2_->CloseReceiveStreams();
  ASSERT_GE(a1_->GetPacketsSent(0), 40);
  ASSERT_GE(a2_->GetPacketsReceived(0), 40);
}

// Offer answer with trickle but with chrome-style candidates
TEST_F(SignalingTest, FullCallTrickleChrome)
{
  sipcc::MediaConstraints constraints;
  OfferAnswerTrickleChrome(constraints, constraints,
                           SHOULD_SENDRECV_AV, SHOULD_SENDRECV_AV);

  std::cerr << "ICE handshake completed" << std::endl;

  // Wait for some data to get written
  ASSERT_TRUE_WAIT(a1_->GetPacketsSent(0) >= 40 &&
                   a2_->GetPacketsReceived(0) >= 40, kDefaultTimeout * 2);

  a1_->CloseSendStreams();
  a2_->CloseReceiveStreams();
  ASSERT_GE(a1_->GetPacketsSent(0), 40);
  ASSERT_GE(a2_->GetPacketsReceived(0), 40);
}

// This test comes from Bug 810220
TEST_F(SignalingTest, AudioOnlyG711Call)
{
  EnsureInit();

  sipcc::MediaConstraints constraints;
  const std::string& offer(strG711SdpOffer);

  std::cout << "Setting offer to:" << std::endl << indent(offer) << std::endl;
  a2_->SetRemote(TestObserver::OFFER, offer);

  std::cout << "Creating answer:" << std::endl;
  a2_->CreateAnswer(constraints, offer, OFFER_AUDIO | ANSWER_AUDIO,
                   DONT_CHECK_AUDIO | DONT_CHECK_VIDEO | DONT_CHECK_DATA);

  std::string answer = a2_->answer();

  // They didn't offer opus, so our answer shouldn't include it.
  ASSERT_EQ(answer.find(" opus/"), std::string::npos);

  // They also didn't offer video or application
  ASSERT_EQ(answer.find("video"), std::string::npos);
  ASSERT_EQ(answer.find("application"), std::string::npos);

  // We should answer with PCMU and telephone-event
  ASSERT_NE(answer.find(" PCMU/8000"), std::string::npos);
  ASSERT_NE(answer.find(" telephone-event/8000"), std::string::npos);

  // Double-check the directionality
  ASSERT_NE(answer.find("\r\na=sendrecv"), std::string::npos);

}

// This test comes from Bug814038
TEST_F(SignalingTest, ChromeOfferAnswer)
{
  EnsureInit();

  sipcc::MediaConstraints constraints;

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
  a2_->CreateAnswer(constraints, offer, OFFER_AUDIO | ANSWER_AUDIO);

  std::string answer = a2_->answer();
}


TEST_F(SignalingTest, FullChromeHandshake)
{
  EnsureInit();

  sipcc::MediaConstraints constraints;
  constraints.setBooleanConstraint("OfferToReceiveAudio", true, false);
  constraints.setBooleanConstraint("OfferToReceiveVideo", true, false);

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
  a2_->CreateAnswer(constraints, offer, OFFER_AUDIO | ANSWER_AUDIO);

  std::cout << "Setting answer" << std::endl;
  a2_->SetLocal(TestObserver::ANSWER, a2_->answer());

  std::string answer = a2_->answer();
  ASSERT_NE(answer.find("111 opus/"), std::string::npos);
}

// Disabled pending resolution of bug 818640.
TEST_F(SignalingTest, DISABLED_OfferAllDynamicTypes)
{
  EnsureInit();

  sipcc::MediaConstraints constraints;
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
      a2_->CreateAnswer(constraints, offer, OFFER_AUDIO | ANSWER_AUDIO);

      std::string answer = a2_->answer();

      ASSERT_NE(answer.find(ss.str() + " opus/"), std::string::npos);
  }

}

TEST_F(SignalingTest, OfferAnswerCheckDescriptions)
{
  sipcc::MediaConstraints constraints;
  OfferAnswer(constraints, constraints, OFFER_AV | ANSWER_AV, true,
              SHOULD_SENDRECV_AV, SHOULD_SENDRECV_AV);

  std::cout << "Caller's Local Description: " << std::endl
            << indent(a1_->getLocalDescription()) << std::endl << std::endl;

  std::cout << "Caller's Remote Description: " << std::endl
            << indent(a1_->getRemoteDescription()) << std::endl << std::endl;

  std::cout << "Callee's Local Description: " << std::endl
            << indent(a2_->getLocalDescription()) << std::endl << std::endl;

  std::cout << "Callee's Remote Description: " << std::endl
            << indent(a2_->getRemoteDescription()) << std::endl << std::endl;

  ASSERT_EQ(a1_->getLocalDescription(),a2_->getRemoteDescription());
  ASSERT_EQ(a2_->getLocalDescription(),a1_->getRemoteDescription());
}

TEST_F(SignalingTest, CheckTrickleSdpChange)
{
  sipcc::MediaConstraints constraints;
  OfferAnswerTrickle(constraints, constraints,
                     SHOULD_SENDRECV_AV, SHOULD_SENDRECV_AV);
  std::cerr << "ICE handshake completed" << std::endl;

  a1_->CloseSendStreams();
  a2_->CloseReceiveStreams();

  std::cout << "Caller's Local Description: " << std::endl
            << indent(a1_->getLocalDescription()) << std::endl << std::endl;

  std::cout << "Caller's Remote Description: " << std::endl
            << indent(a1_->getRemoteDescription()) << std::endl << std::endl;

  std::cout << "Callee's Local Description: " << std::endl
            << indent(a2_->getLocalDescription()) << std::endl << std::endl;

  std::cout << "Callee's Remote Description: " << std::endl
            << indent(a2_->getRemoteDescription()) << std::endl << std::endl;

  ASSERT_NE(a1_->getLocalDescription().find("\r\na=candidate"),
            std::string::npos);
  ASSERT_NE(a1_->getRemoteDescription().find("\r\na=candidate"),
            std::string::npos);
  ASSERT_NE(a2_->getLocalDescription().find("\r\na=candidate"),
            std::string::npos);
  ASSERT_NE(a2_->getRemoteDescription().find("\r\na=candidate"),
            std::string::npos);
  /* TODO (abr): These checks aren't quite right, since trickle ICE
   * can easily result in SDP that is semantically identical but
   * varies syntactically (in particularly, the ordering of attributes
   * withing an m-line section can be different). This needs to be updated
   * to be a semantic comparision between the SDP. Currently, these checks
   * will fail whenever we add any other attributes to the SDP, such as
   * RTCP MUX or RTCP feedback.
  ASSERT_EQ(a1_->getLocalDescription(),a2_->getRemoteDescription());
  ASSERT_EQ(a2_->getLocalDescription(),a1_->getRemoteDescription());
  */
}

TEST_F(SignalingTest, ipAddrAnyOffer)
{
  EnsureInit();

  sipcc::MediaConstraints constraints;
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
    "a=sendrecv\r\n";

    a2_->SetRemote(TestObserver::OFFER, offer);
    ASSERT_TRUE(a2_->pObserver->state == TestObserver::stateSuccess);
    a2_->CreateAnswer(constraints, offer, OFFER_AUDIO | ANSWER_AUDIO);
    ASSERT_TRUE(a2_->pObserver->state == TestObserver::stateSuccess);
    std::string answer = a2_->answer();
    ASSERT_NE(answer.find("a=sendrecv"), std::string::npos);
}

static void CreateSDPForBigOTests(std::string& offer, const char *number) {
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
    "a=sendrecv\r\n";
}

TEST_F(SignalingTest, BigOValues)
{
  EnsureInit();

  std::string offer;

  CreateSDPForBigOTests(offer, "12345678901234567");

  a2_->SetRemote(TestObserver::OFFER, offer);
  ASSERT_EQ(a2_->pObserver->state, TestObserver::stateSuccess);
}

TEST_F(SignalingTest, BigOValuesExtraChars)
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

TEST_F(SignalingTest, BigOValuesTooBig)
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

TEST_F(SignalingTest, SetLocalAnswerInStable)
{
  EnsureInit();

  sipcc::MediaConstraints constraints;
  CreateOffer(constraints, OFFER_AUDIO, SHOULD_SENDRECV_AUDIO);

  // The signaling state will remain "stable" because the
  // SetLocalDescription call fails.
  a1_->SetLocal(TestObserver::ANSWER, a1_->offer(), true,
                PCImplSignalingState::SignalingStable);
  ASSERT_EQ(a1_->pObserver->lastStatusCode,
            sipcc::PeerConnectionImpl::kInvalidState);
}

TEST_F(SignalingTest, SetRemoteAnswerInStable) {
  EnsureInit();

  // The signaling state will remain "stable" because the
  // SetRemoteDescription call fails.
  a1_->SetRemote(TestObserver::ANSWER, strSampleSdpAudioVideoNoIce, true,
                PCImplSignalingState::SignalingStable);
  ASSERT_EQ(a1_->pObserver->lastStatusCode,
            sipcc::PeerConnectionImpl::kInvalidState);
}

TEST_F(SignalingTest, SetLocalAnswerInHaveLocalOffer) {
  sipcc::MediaConstraints constraints;
  CreateOffer(constraints, OFFER_AUDIO, SHOULD_SENDRECV_AUDIO);
  a1_->SetLocal(TestObserver::OFFER, a1_->offer());
  ASSERT_EQ(a1_->pObserver->lastStatusCode,
            sipcc::PeerConnectionImpl::kNoError);

  // The signaling state will remain "have-local-offer" because the
  // SetLocalDescription call fails.
  a1_->SetLocal(TestObserver::ANSWER, a1_->offer(), true,
                PCImplSignalingState::SignalingHaveLocalOffer);
  ASSERT_EQ(a1_->pObserver->lastStatusCode,
            sipcc::PeerConnectionImpl::kInvalidState);
}

TEST_F(SignalingTest, SetRemoteOfferInHaveLocalOffer) {
  sipcc::MediaConstraints constraints;
  CreateOffer(constraints, OFFER_AUDIO, SHOULD_SENDRECV_AUDIO);
  a1_->SetLocal(TestObserver::OFFER, a1_->offer());
  ASSERT_EQ(a1_->pObserver->lastStatusCode,
            sipcc::PeerConnectionImpl::kNoError);

  // The signaling state will remain "have-local-offer" because the
  // SetRemoteDescription call fails.
  a1_->SetRemote(TestObserver::OFFER, a1_->offer(), true,
                 PCImplSignalingState::SignalingHaveLocalOffer);
  ASSERT_EQ(a1_->pObserver->lastStatusCode,
            sipcc::PeerConnectionImpl::kInvalidState);
}

TEST_F(SignalingTest, SetLocalOfferInHaveRemoteOffer) {
  sipcc::MediaConstraints constraints;
  CreateOffer(constraints, OFFER_AUDIO, SHOULD_SENDRECV_AUDIO);
  a2_->SetRemote(TestObserver::OFFER, a1_->offer());
  ASSERT_EQ(a2_->pObserver->lastStatusCode,
            sipcc::PeerConnectionImpl::kNoError);

  // The signaling state will remain "have-remote-offer" because the
  // SetLocalDescription call fails.
  a2_->SetLocal(TestObserver::OFFER, a1_->offer(), true,
                PCImplSignalingState::SignalingHaveRemoteOffer);
  ASSERT_EQ(a2_->pObserver->lastStatusCode,
            sipcc::PeerConnectionImpl::kInvalidState);
}

TEST_F(SignalingTest, SetRemoteAnswerInHaveRemoteOffer) {
  sipcc::MediaConstraints constraints;
  CreateOffer(constraints, OFFER_AUDIO, SHOULD_SENDRECV_AUDIO);
  a2_->SetRemote(TestObserver::OFFER, a1_->offer());
  ASSERT_EQ(a2_->pObserver->lastStatusCode,
            sipcc::PeerConnectionImpl::kNoError);

  // The signaling state will remain "have-remote-offer" because the
  // SetRemoteDescription call fails.
  a2_->SetRemote(TestObserver::ANSWER, a1_->offer(), true,
               PCImplSignalingState::SignalingHaveRemoteOffer);
  ASSERT_EQ(a2_->pObserver->lastStatusCode,
            sipcc::PeerConnectionImpl::kInvalidState);
}

// Disabled until the spec adds a failure callback to addStream
TEST_F(SignalingTest, DISABLED_AddStreamInHaveLocalOffer) {
  sipcc::MediaConstraints constraints;
  CreateOffer(constraints, OFFER_AUDIO, SHOULD_SENDRECV_AUDIO);
  a1_->SetLocal(TestObserver::OFFER, a1_->offer());
  ASSERT_EQ(a1_->pObserver->lastStatusCode,
            sipcc::PeerConnectionImpl::kNoError);
  a1_->AddStream();
  ASSERT_EQ(a1_->pObserver->lastStatusCode,
            sipcc::PeerConnectionImpl::kInvalidState);
}

// Disabled until the spec adds a failure callback to removeStream
TEST_F(SignalingTest, DISABLED_RemoveStreamInHaveLocalOffer) {
  sipcc::MediaConstraints constraints;
  CreateOffer(constraints, OFFER_AUDIO, SHOULD_SENDRECV_AUDIO);
  a1_->SetLocal(TestObserver::OFFER, a1_->offer());
  ASSERT_EQ(a1_->pObserver->lastStatusCode,
            sipcc::PeerConnectionImpl::kNoError);
  a1_->RemoveLastStreamAdded();
  ASSERT_EQ(a1_->pObserver->lastStatusCode,
            sipcc::PeerConnectionImpl::kInvalidState);
}

TEST_F(SignalingTest, AddCandidateInHaveLocalOffer) {
  sipcc::MediaConstraints constraints;
  CreateOffer(constraints, OFFER_AUDIO, SHOULD_SENDRECV_AUDIO);
  a1_->SetLocal(TestObserver::OFFER, a1_->offer());
  ASSERT_EQ(a1_->pObserver->lastStatusCode,
            sipcc::PeerConnectionImpl::kNoError);
  a1_->AddIceCandidate(strSampleCandidate.c_str(),
                      strSampleMid.c_str(), nSamplelevel, false);
  ASSERT_EQ(a1_->pObserver->lastStatusCode,
            sipcc::PeerConnectionImpl::kInvalidState);
}

TEST_F(SignalingAgentTest, CreateOffer) {
  CreateAgent();
  sipcc::MediaConstraints constraints;
  agent(0)->CreateOffer(constraints, OFFER_AUDIO, SHOULD_SENDRECV_AUDIO);
  PR_Sleep(20000);
}

TEST_F(SignalingAgentTest, CreateOfferTrickleTestServer) {
  TestStunServer::GetInstance()->SetActive(false);
  TestStunServer::GetInstance()->SetResponseAddr(
      kBogusSrflxAddress, kBogusSrflxPort);

  CreateAgent(
      TestStunServer::GetInstance()->addr(),
      TestStunServer::GetInstance()->port(),
      false);

  sipcc::MediaConstraints constraints;
  agent(0)->CreateOffer(constraints, OFFER_AUDIO, SHOULD_SENDRECV_AUDIO);

  // Verify that the bogus addr is not there.
  ASSERT_FALSE(agent(0)->OfferContains(kBogusSrflxAddress));

  // Now enable the STUN server.
  TestStunServer::GetInstance()->SetActive(true);
  agent(0)->WaitForGather();

  // There shouldn't be any candidates until SetLocal.
  ASSERT_EQ(0U, agent(0)->MatchingCandidates(kBogusSrflxAddress));

  // Verify that the candidates appear in the offer.
  size_t match;
  match = agent(0)->getLocalDescription().find(kBogusSrflxAddress);
  ASSERT_LT(0U, match);
}

TEST_F(SignalingAgentTest, CreateOfferSetLocalTrickleTestServer) {
  TestStunServer::GetInstance()->SetActive(false);
  TestStunServer::GetInstance()->SetResponseAddr(
      kBogusSrflxAddress, kBogusSrflxPort);

  CreateAgent(
      TestStunServer::GetInstance()->addr(),
      TestStunServer::GetInstance()->port(),
      false);

  sipcc::MediaConstraints constraints;
  agent(0)->CreateOffer(constraints, OFFER_AUDIO, SHOULD_SENDRECV_AUDIO);

  // Verify that the bogus addr is not there.
  ASSERT_FALSE(agent(0)->OfferContains(kBogusSrflxAddress));

  // Now enable the STUN server.
  TestStunServer::GetInstance()->SetActive(true);
  agent(0)->WaitForGather();

  // There shouldn't be any candidates until SetLocal.
  ASSERT_EQ(0U, agent(0)->MatchingCandidates(kBogusSrflxAddress));

  agent(0)->SetLocal(TestObserver::OFFER, agent(0)->offer());
  PR_Sleep(1000); // Give time for the message queues.

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
      TestStunServer::GetInstance()->port(),
      false);

  std::string offer(strG711SdpOffer);
  agent(0)->SetRemote(TestObserver::OFFER, offer, true,
                 PCImplSignalingState::SignalingHaveRemoteOffer);
  ASSERT_EQ(agent(0)->pObserver->lastStatusCode,
            sipcc::PeerConnectionImpl::kNoError);

  sipcc::MediaConstraints constraints;
  agent(0)->CreateAnswer(constraints, offer, ANSWER_AUDIO, DONT_CHECK_AUDIO);

  // Verify that the bogus addr is not there.
  ASSERT_FALSE(agent(0)->AnswerContains(kBogusSrflxAddress));

  // Now enable the STUN server.
  TestStunServer::GetInstance()->SetActive(true);
  agent(0)->WaitForGather();

  // There shouldn't be any candidates until SetLocal.
  ASSERT_EQ(0U, agent(0)->MatchingCandidates(kBogusSrflxAddress));

  agent(0)->SetLocal(TestObserver::ANSWER, agent(0)->answer());
  PR_Sleep(1000); // Give time for the message queues.

  // Verify that we got our candidates.
  ASSERT_LE(2U, agent(0)->MatchingCandidates(kBogusSrflxAddress));

  // Verify that the candidates appear in the answer.
  size_t match;
  match = agent(0)->getLocalDescription().find(kBogusSrflxAddress);
  ASSERT_LT(0U, match);
}



TEST_F(SignalingAgentTest, CreateUntilFailThenWait) {
  int i;

  for (i=0; ; i++) {
    if (!CreateAgent())
      break;
    std::cerr << "Created agent " << i << std::endl;
  }
  std::cerr << "Failed after creating " << i << " PCs " << std::endl;
  PR_Sleep(10000);  // Wait to see if we crash
}

// Test for bug 856433.
TEST_F(SignalingAgentTest, CreateNoInit) {
  CreateAgentNoInit();
}


/*
 * Test for Bug 843595
 */
TEST_F(SignalingTest, missingUfrag)
{
  EnsureInit();

  sipcc::MediaConstraints constraints;
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
  a1_->CreateOffer(constraints, OFFER_AV, SHOULD_SENDRECV_AV);
  a1_->SetLocal(TestObserver::OFFER, offer, true);
  // We now detect the missing ICE parameters at SetRemoteDescription
  a2_->SetRemote(TestObserver::OFFER, offer, true,
                 PCImplSignalingState::SignalingStable);
  ASSERT_TRUE(a2_->pObserver->state == TestObserver::stateError);
}

TEST_F(SignalingTest, AudioOnlyCalleeNoRtcpMux)
{
  EnsureInit();

  sipcc::MediaConstraints constraints;

  a1_->CreateOffer(constraints, OFFER_AUDIO, SHOULD_SENDRECV_AUDIO);
  a1_->SetLocal(TestObserver::OFFER, a1_->offer(), false);
  ParsedSDP sdpWrapper(a1_->offer());
  sdpWrapper.DeleteLine("a=rtcp-mux");
  std::cout << "Modified SDP " << std::endl
            << indent(sdpWrapper.getSdp()) << std::endl;
  a2_->SetRemote(TestObserver::OFFER, sdpWrapper.getSdp(), false);
  a2_->CreateAnswer(constraints, sdpWrapper.getSdp(),
    OFFER_AUDIO | ANSWER_AUDIO);
  a2_->SetLocal(TestObserver::ANSWER, a2_->answer(), false);
  a1_->SetRemote(TestObserver::ANSWER, a2_->answer(), false);

  // Answer should not have a=rtcp-mux
  ASSERT_EQ(a2_->getLocalDescription().find("\r\na=rtcp-mux"),
            std::string::npos);

  ASSERT_TRUE_WAIT(a1_->IceCompleted() == true, kDefaultTimeout);
  ASSERT_TRUE_WAIT(a2_->IceCompleted() == true, kDefaultTimeout);

  // Wait for some data to get written
  ASSERT_TRUE_WAIT(a1_->GetPacketsSent(0) >= 40 &&
                   a2_->GetPacketsReceived(0) >= 40, kDefaultTimeout * 2);

  a1_->CloseSendStreams();
  a2_->CloseReceiveStreams();

  ASSERT_GE(a1_->GetPacketsSent(0), 40);
  ASSERT_GE(a2_->GetPacketsReceived(0), 40);

  // Check the low-level media pipeline
  // for RTP and RTCP flows
  // The first Local pipeline gets stored at 0
  a1_->CheckMediaPipeline(0, 0, PIPELINE_LOCAL | PIPELINE_SEND);

  // The first Remote pipeline gets stored at 1
  a2_->CheckMediaPipeline(0, 1, 0);
}

TEST_F(SignalingTest, FullCallAudioNoMuxVideoMux)
{
  EnsureInit();

  sipcc::MediaConstraints constraints;

  a1_->CreateOffer(constraints, OFFER_AV, SHOULD_SENDRECV_AV);
  a1_->SetLocal(TestObserver::OFFER, a1_->offer(), false);
  ParsedSDP sdpWrapper(a1_->offer());
  sdpWrapper.DeleteLine("a=rtcp-mux");
  std::cout << "Modified SDP " << std::endl
            << indent(sdpWrapper.getSdp()) << std::endl;
  a2_->SetRemote(TestObserver::OFFER, sdpWrapper.getSdp(), false);
  a2_->CreateAnswer(constraints, sdpWrapper.getSdp(), OFFER_AV | ANSWER_AV);
  a2_->SetLocal(TestObserver::ANSWER, a2_->answer(), false);
  a1_->SetRemote(TestObserver::ANSWER, a2_->answer(), false);

  // Answer should have only one a=rtcp-mux line
  size_t match = a2_->getLocalDescription().find("\r\na=rtcp-mux");
  if (fRtcpMux) {
    ASSERT_NE(match, std::string::npos);
    match = a2_->getLocalDescription().find("\r\na=rtcp-mux", match + 1);
  }
  ASSERT_EQ(match, std::string::npos);

  ASSERT_TRUE_WAIT(a1_->IceCompleted() == true, kDefaultTimeout);
  ASSERT_TRUE_WAIT(a2_->IceCompleted() == true, kDefaultTimeout);

  // Wait for some data to get written
  ASSERT_TRUE_WAIT(a1_->GetPacketsSent(0) >= 40 &&
                   a2_->GetPacketsReceived(0) >= 40, kDefaultTimeout * 2);

  a1_->CloseSendStreams();
  a2_->CloseReceiveStreams();

  ASSERT_GE(a1_->GetPacketsSent(0), 40);
  ASSERT_GE(a2_->GetPacketsReceived(0), 40);

  // Check the low-level media pipeline
  // for RTP and RTCP flows
  // The first Local pipeline gets stored at 0
  a1_->CheckMediaPipeline(0, 0, PIPELINE_LOCAL | PIPELINE_SEND);

  // Now check video mux.
  a1_->CheckMediaPipeline(0, 1,
    PIPELINE_LOCAL | (fRtcpMux ? PIPELINE_RTCP_MUX : 0) | PIPELINE_SEND |
    PIPELINE_VIDEO);

  // The first Remote pipeline gets stored at 1
  a2_->CheckMediaPipeline(0, 1, 0);

  // Now check video mux.
  a2_->CheckMediaPipeline(0, 2, (fRtcpMux ?  PIPELINE_RTCP_MUX : 0) |
    PIPELINE_VIDEO | PIPELINE_RTCP_NACK, VideoSessionConduit::FrameRequestPli);
}

TEST_F(SignalingTest, RtcpFbInOffer)
{
  EnsureInit();
  sipcc::MediaConstraints constraints;
  a1_->CreateOffer(constraints, OFFER_AV, SHOULD_SENDRECV_AV);
  const char *expected[] = { "nack", "nack pli", "ccm fir" };
  CheckRtcpFbSdp(a1_->offer(), ARRAY_TO_SET(std::string, expected));
}

TEST_F(SignalingTest, RtcpFbInAnswer)
{
  const char *feedbackTypes[] = { "nack", "nack pli", "ccm fir" };
  TestRtcpFb(ARRAY_TO_SET(std::string, feedbackTypes),
             PIPELINE_RTCP_NACK,
             VideoSessionConduit::FrameRequestPli);
}

TEST_F(SignalingTest, RtcpFbNoNackBasic)
{
  const char *feedbackTypes[] = { "nack pli", "ccm fir" };
  TestRtcpFb(ARRAY_TO_SET(std::string, feedbackTypes),
             0,
             VideoSessionConduit::FrameRequestPli);
}

TEST_F(SignalingTest, RtcpFbNoNackPli)
{
  const char *feedbackTypes[] = { "nack", "ccm fir" };
  TestRtcpFb(ARRAY_TO_SET(std::string, feedbackTypes),
             PIPELINE_RTCP_NACK,
             VideoSessionConduit::FrameRequestFir);
}

TEST_F(SignalingTest, RtcpFbNoCcmFir)
{
  const char *feedbackTypes[] = { "nack", "nack pli" };
  TestRtcpFb(ARRAY_TO_SET(std::string, feedbackTypes),
             PIPELINE_RTCP_NACK,
             VideoSessionConduit::FrameRequestPli);
}

TEST_F(SignalingTest, RtcpFbNoNack)
{
  const char *feedbackTypes[] = { "ccm fir" };
  TestRtcpFb(ARRAY_TO_SET(std::string, feedbackTypes),
             0,
             VideoSessionConduit::FrameRequestFir);
}

TEST_F(SignalingTest, RtcpFbNoFrameRequest)
{
  const char *feedbackTypes[] = { "nack" };
  TestRtcpFb(ARRAY_TO_SET(std::string, feedbackTypes),
             PIPELINE_RTCP_NACK,
             VideoSessionConduit::FrameRequestNone);
}

TEST_F(SignalingTest, RtcpFbPliOnly)
{
  const char *feedbackTypes[] = { "nack pli" };
  TestRtcpFb(ARRAY_TO_SET(std::string, feedbackTypes),
             0,
             VideoSessionConduit::FrameRequestPli);
}

TEST_F(SignalingTest, RtcpFbNoFeedback)
{
  const char *feedbackTypes[] = { };
  TestRtcpFb(ARRAY_TO_SET(std::string, feedbackTypes),
             0,
             VideoSessionConduit::FrameRequestNone);
}

// In this test we will change the offer SDP's a=setup value
// from actpass to passive.  This will make the answer do active.
TEST_F(SignalingTest, AudioCallForceDtlsRoles)
{
  EnsureInit();

  sipcc::MediaConstraints constraints;
  size_t match;

  a1_->CreateOffer(constraints, OFFER_AUDIO, SHOULD_SENDRECV_AUDIO);

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

  a1_->SetLocal(TestObserver::OFFER, offer.c_str(), false);
  a2_->SetRemote(TestObserver::OFFER, offer.c_str(), false);
  a2_->CreateAnswer(constraints, offer.c_str(), OFFER_AUDIO | ANSWER_AUDIO);

  // Now the answer should contain a=setup:active
  std::string answer(a2_->answer());
  match = answer.find("\r\na=setup:active");
  ASSERT_NE(match, std::string::npos);

  // This should setup the DTLS with the same roles
  // as the regular tests above.
  a2_->SetLocal(TestObserver::ANSWER, a2_->answer(), false);
  a1_->SetRemote(TestObserver::ANSWER, a2_->answer(), false);

  ASSERT_TRUE_WAIT(a1_->IceCompleted() == true, kDefaultTimeout);
  ASSERT_TRUE_WAIT(a2_->IceCompleted() == true, kDefaultTimeout);

  // Wait for some data to get written
  ASSERT_TRUE_WAIT(a1_->GetPacketsSent(0) >= 40 &&
                   a2_->GetPacketsReceived(0) >= 40, kDefaultTimeout * 2);

  a1_->CloseSendStreams();
  a2_->CloseReceiveStreams();

  ASSERT_GE(a1_->GetPacketsSent(0), 40);
  ASSERT_GE(a2_->GetPacketsReceived(0), 40);
}

// In this test we will change the offer SDP's a=setup value
// from actpass to active.  This will make the answer do passive
TEST_F(SignalingTest, AudioCallReverseDtlsRoles)
{
  EnsureInit();

  sipcc::MediaConstraints constraints;
  size_t match;

  a1_->CreateOffer(constraints, OFFER_AUDIO, SHOULD_SENDRECV_AUDIO);

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

  a1_->SetLocal(TestObserver::OFFER, offer.c_str(), false);
  a2_->SetRemote(TestObserver::OFFER, offer.c_str(), false);
  a2_->CreateAnswer(constraints, offer.c_str(), OFFER_AUDIO | ANSWER_AUDIO);

  // Now the answer should contain a=setup:passive
  std::string answer(a2_->answer());
  match = answer.find("\r\na=setup:passive");
  ASSERT_NE(match, std::string::npos);

  // This should setup the DTLS with the opposite roles
  // than the regular tests above.
  a2_->SetLocal(TestObserver::ANSWER, a2_->answer(), false);
  a1_->SetRemote(TestObserver::ANSWER, a2_->answer(), false);

  ASSERT_TRUE_WAIT(a1_->IceCompleted() == true, kDefaultTimeout);
  ASSERT_TRUE_WAIT(a2_->IceCompleted() == true, kDefaultTimeout);

  // Wait for some data to get written
  ASSERT_TRUE_WAIT(a1_->GetPacketsSent(0) >= 40 &&
                   a2_->GetPacketsReceived(0) >= 40, kDefaultTimeout * 2);

  a1_->CloseSendStreams();
  a2_->CloseReceiveStreams();

  ASSERT_GE(a1_->GetPacketsSent(0), 40);
  ASSERT_GE(a2_->GetPacketsReceived(0), 40);
}

// In this test we will change the answer SDP's a=setup value
// from active to passive.  This will make both sides do
// active and should not connect.
TEST_F(SignalingTest, AudioCallMismatchDtlsRoles)
{
  EnsureInit();

  sipcc::MediaConstraints constraints;
  size_t match;

  a1_->CreateOffer(constraints, OFFER_AUDIO, SHOULD_SENDRECV_AUDIO);

  // By default the offer should give actpass
  std::string offer(a1_->offer());
  match = offer.find("\r\na=setup:actpass");
  ASSERT_NE(match, std::string::npos);
  a1_->SetLocal(TestObserver::OFFER, offer.c_str(), false);
  a2_->SetRemote(TestObserver::OFFER, offer.c_str(), false);
  a2_->CreateAnswer(constraints, offer.c_str(), OFFER_AUDIO | ANSWER_AUDIO);

  // Now the answer should contain a=setup:active
  std::string answer(a2_->answer());
  match = answer.find("\r\na=setup:active");
  ASSERT_NE(match, std::string::npos);

  // Now replace the active with passive so that the offerer will
  // also do active.
  answer.replace(match, strlen("\r\na=setup:active"),
    "\r\na=setup:passive");
  std::cout << "Modified SDP " << std::endl
            << indent(answer) << std::endl;

  // This should setup the DTLS with both sides playing active
  a2_->SetLocal(TestObserver::ANSWER, answer.c_str(), false);
  a1_->SetRemote(TestObserver::ANSWER, answer.c_str(), false);

  ASSERT_TRUE_WAIT(a1_->IceCompleted() == true, kDefaultTimeout);
  ASSERT_TRUE_WAIT(a2_->IceCompleted() == true, kDefaultTimeout);

  // Not using ASSERT_TRUE_WAIT here because we expect failure
  PR_Sleep(kDefaultTimeout * 2); // Wait for some data to get written

  a1_->CloseSendStreams();
  a2_->CloseReceiveStreams();

  ASSERT_GE(a1_->GetPacketsSent(0), 40);
  // In this case we should receive nothing.
  ASSERT_EQ(a2_->GetPacketsReceived(0), 0);
}

// In this test we will change the offer SDP's a=setup value
// from actpass to garbage.  It should ignore the garbage value
// and respond with setup:active
TEST_F(SignalingTest, AudioCallGarbageSetup)
{
  EnsureInit();

  sipcc::MediaConstraints constraints;
  size_t match;

  a1_->CreateOffer(constraints, OFFER_AUDIO, SHOULD_SENDRECV_AUDIO);

  // By default the offer should give actpass
  std::string offer(a1_->offer());
  match = offer.find("\r\na=setup:actpass");
  ASSERT_NE(match, std::string::npos);
  // Now replace the actpass with a garbage value
  offer.replace(match, strlen("\r\na=setup:actpass"),
    "\r\na=setup:G4rb4g3V4lu3");
  std::cout << "Modified SDP " << std::endl
            << indent(offer) << std::endl;

  a1_->SetLocal(TestObserver::OFFER, offer.c_str(), false);
  a2_->SetRemote(TestObserver::OFFER, offer.c_str(), false);
  a2_->CreateAnswer(constraints, offer.c_str(), OFFER_AUDIO | ANSWER_AUDIO);

  // Now the answer should contain a=setup:active
  std::string answer(a2_->answer());
  match = answer.find("\r\na=setup:active");
  ASSERT_NE(match, std::string::npos);

  // This should setup the DTLS with the same roles
  // as the regular tests above.
  a2_->SetLocal(TestObserver::ANSWER, a2_->answer(), false);
  a1_->SetRemote(TestObserver::ANSWER, a2_->answer(), false);

  ASSERT_TRUE_WAIT(a1_->IceCompleted() == true, kDefaultTimeout);
  ASSERT_TRUE_WAIT(a2_->IceCompleted() == true, kDefaultTimeout);

  // Wait for some data to get written
  ASSERT_TRUE_WAIT(a1_->GetPacketsSent(0) >= 40 &&
                   a2_->GetPacketsReceived(0) >= 40, kDefaultTimeout * 2);

  a1_->CloseSendStreams();
  a2_->CloseReceiveStreams();

  ASSERT_GE(a1_->GetPacketsSent(0), 40);
  ASSERT_GE(a2_->GetPacketsReceived(0), 40);
}

// In this test we will change the offer SDP to remove the
// a=setup line.  Answer should respond with a=setup:active.
TEST_F(SignalingTest, AudioCallOfferNoSetupOrConnection)
{
  EnsureInit();

  sipcc::MediaConstraints constraints;
  size_t match;

  a1_->CreateOffer(constraints, OFFER_AUDIO, SHOULD_SENDRECV_AUDIO);

  // By default the offer should give setup:actpass
  std::string offer(a1_->offer());
  match = offer.find("\r\na=setup:actpass");
  ASSERT_NE(match, std::string::npos);
  // Remove the a=setup line
  offer.replace(match, strlen("\r\na=setup:actpass"), "");
  std::cout << "Modified SDP " << std::endl
            << indent(offer) << std::endl;

  a1_->SetLocal(TestObserver::OFFER, offer.c_str(), false);
  a2_->SetRemote(TestObserver::OFFER, offer.c_str(), false);
  a2_->CreateAnswer(constraints, offer.c_str(), OFFER_AUDIO | ANSWER_AUDIO);

  // Now the answer should contain a=setup:active
  std::string answer(a2_->answer());
  match = answer.find("\r\na=setup:active");
  ASSERT_NE(match, std::string::npos);

  // This should setup the DTLS with the same roles
  // as the regular tests above.
  a2_->SetLocal(TestObserver::ANSWER, a2_->answer(), false);
  a1_->SetRemote(TestObserver::ANSWER, a2_->answer(), false);

  ASSERT_TRUE_WAIT(a1_->IceCompleted() == true, kDefaultTimeout);
  ASSERT_TRUE_WAIT(a2_->IceCompleted() == true, kDefaultTimeout);

  // Wait for some data to get written
  ASSERT_TRUE_WAIT(a1_->GetPacketsSent(0) >= 40 &&
                   a2_->GetPacketsReceived(0) >= 40, kDefaultTimeout * 2);

  a1_->CloseSendStreams();
  a2_->CloseReceiveStreams();

  ASSERT_GE(a1_->GetPacketsSent(0), 40);
  ASSERT_GE(a2_->GetPacketsReceived(0), 40);
}

// In this test we will change the answer SDP to remove the
// a=setup line.  ICE should still connect since active will
// be assumed.
TEST_F(SignalingTest, AudioCallAnswerNoSetupOrConnection)
{
  EnsureInit();

  sipcc::MediaConstraints constraints;
  size_t match;

  a1_->CreateOffer(constraints, OFFER_AUDIO, SHOULD_SENDRECV_AUDIO);

  // By default the offer should give setup:actpass
  std::string offer(a1_->offer());
  match = offer.find("\r\na=setup:actpass");
  ASSERT_NE(match, std::string::npos);

  a1_->SetLocal(TestObserver::OFFER, offer.c_str(), false);
  a2_->SetRemote(TestObserver::OFFER, offer.c_str(), false);
  a2_->CreateAnswer(constraints, offer.c_str(), OFFER_AUDIO | ANSWER_AUDIO);

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

  ASSERT_TRUE_WAIT(a1_->IceCompleted() == true, kDefaultTimeout);
  ASSERT_TRUE_WAIT(a2_->IceCompleted() == true, kDefaultTimeout);

  // Wait for some data to get written
  ASSERT_TRUE_WAIT(a1_->GetPacketsSent(0) >= 40 &&
                   a2_->GetPacketsReceived(0) >= 40, kDefaultTimeout * 2);

  a1_->CloseSendStreams();
  a2_->CloseReceiveStreams();

  ASSERT_GE(a1_->GetPacketsSent(0), 40);
  ASSERT_GE(a2_->GetPacketsReceived(0), 40);
}


TEST_F(SignalingTest, FullCallRealTrickle)
{
  wait_for_gather_ = false;

  sipcc::MediaConstraints constraints;
  OfferAnswer(constraints, constraints, OFFER_AV | ANSWER_AV,
              true, SHOULD_SENDRECV_AV, SHOULD_SENDRECV_AV);

  // Wait for some data to get written
  ASSERT_TRUE_WAIT(a1_->GetPacketsSent(0) >= 40 &&
                   a2_->GetPacketsReceived(0) >= 40, kDefaultTimeout * 2);

  a1_->CloseSendStreams();
  a2_->CloseReceiveStreams();
  ASSERT_GE(a1_->GetPacketsSent(0), 40);
  ASSERT_GE(a2_->GetPacketsReceived(0), 40);
}

TEST_F(SignalingTest, FullCallRealTrickleTestServer)
{
  wait_for_gather_ = false;
  SetTestStunServer();

  sipcc::MediaConstraints constraints;
  OfferAnswer(constraints, constraints, OFFER_AV | ANSWER_AV,
              true, SHOULD_SENDRECV_AV, SHOULD_SENDRECV_AV);

  TestStunServer::GetInstance()->SetActive(true);

  // Wait for some data to get written
  ASSERT_TRUE_WAIT(a1_->GetPacketsSent(0) >= 40 &&
                   a2_->GetPacketsReceived(0) >= 40, kDefaultTimeout * 2);

  a1_->CloseSendStreams();
  a2_->CloseReceiveStreams();
  ASSERT_GE(a1_->GetPacketsSent(0), 40);
  ASSERT_GE(a2_->GetPacketsReceived(0), 40);
}

TEST_F(SignalingTest, hugeSdp)
{
  EnsureInit();

  sipcc::MediaConstraints constraints;
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

  a1_->CreateOffer(constraints, OFFER_AV, SHOULD_SENDRECV_AV);
  a1_->SetLocal(TestObserver::OFFER, offer, true);

  a2_->SetRemote(TestObserver::OFFER, offer, true);
  ASSERT_GE(a2_->getRemoteDescription().length(), 4096U);
  a2_->CreateAnswer(constraints, offer, OFFER_AV);
}

// Test max_fs and max_fr prefs have proper impact on SDP offer
TEST_F(SignalingTest, MaxFsFrInOffer)
{
  EnsureInit();

  sipcc::MediaConstraints constraints;

  nsCOMPtr<nsIPrefBranch> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
  ASSERT_TRUE(prefs);
  FsFrPrefClearer prefClearer(prefs);

  prefs->SetIntPref("media.navigator.video.max_fs", 300);
  prefs->SetIntPref("media.navigator.video.max_fr", 30);

  a1_->CreateOffer(constraints, OFFER_AV, SHOULD_CHECK_AV);

  // Verify that SDP contains correct max-fs and max-fr
  CheckMaxFsFrSdp(a1_->offer(), 120, 300, 30);
}

// Test max_fs and max_fr prefs have proper impact on SDP answer
TEST_F(SignalingTest, MaxFsFrInAnswer)
{
  EnsureInit();

  sipcc::MediaConstraints constraints;

  nsCOMPtr<nsIPrefBranch> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
  ASSERT_TRUE(prefs);
  FsFrPrefClearer prefClearer(prefs);

  // We don't want max_fs and max_fr prefs impact SDP at this moment
  prefs->SetIntPref("media.navigator.video.max_fs", 0);
  prefs->SetIntPref("media.navigator.video.max_fr", 0);

  a1_->CreateOffer(constraints, OFFER_AV, SHOULD_CHECK_AV);

  // SDP should not contain max-fs and max-fr here
  CheckMaxFsFrSdp(a1_->offer(), 120, 0, 0);

  a2_->SetRemote(TestObserver::OFFER, a1_->offer());

  prefs->SetIntPref("media.navigator.video.max_fs", 600);
  prefs->SetIntPref("media.navigator.video.max_fr", 60);

  a2_->CreateAnswer(constraints, a1_->offer(), OFFER_AV | ANSWER_AV);

  // Verify that SDP contains correct max-fs and max-fr
  CheckMaxFsFrSdp(a2_->answer(), 120, 600, 60);
}

// Test SDP offer has proper impact on callee's codec configuration
TEST_F(SignalingTest, MaxFsFrCalleeCodec)
{
  EnsureInit();

  sipcc::MediaConstraints constraints;

  nsCOMPtr<nsIPrefBranch> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
  ASSERT_TRUE(prefs);
  FsFrPrefClearer prefClearer(prefs);

  // We don't want max_fs and max_fr prefs impact SDP at this moment
  prefs->SetIntPref("media.navigator.video.max_fs", 0);
  prefs->SetIntPref("media.navigator.video.max_fr", 0);

  a1_->CreateOffer(constraints, OFFER_AV, SHOULD_CHECK_AV);

  ParsedSDP sdpWrapper(a1_->offer());

  sdpWrapper.ReplaceLine("a=rtpmap:120",
    "a=rtpmap:120 VP8/90000\r\na=fmtp:120 max-fs=300;max-fr=30\r\n");

  std::cout << "Modified SDP " << std::endl
            << indent(sdpWrapper.getSdp()) << std::endl;

  // Double confirm that SDP offer contains correct max-fs and max-fr
  CheckMaxFsFrSdp(sdpWrapper.getSdp(), 120, 300, 30);

  a1_->SetLocal(TestObserver::OFFER, sdpWrapper.getSdp());
  a2_->SetRemote(TestObserver::OFFER, sdpWrapper.getSdp());

  a2_->CreateAnswer(constraints, sdpWrapper.getSdp(), OFFER_AV | ANSWER_AV);

  // SDP should not contain max-fs and max-fr here
  CheckMaxFsFrSdp(a2_->answer(), 120, 0, 0);

  a2_->SetLocal(TestObserver::ANSWER, a2_->answer());
  a1_->SetRemote(TestObserver::ANSWER, a2_->answer());

  ASSERT_TRUE_WAIT(a1_->IceCompleted() == true, kDefaultTimeout);
  ASSERT_TRUE_WAIT(a2_->IceCompleted() == true, kDefaultTimeout);

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
TEST_F(SignalingTest, MaxFsFrCallerCodec)
{
  EnsureInit();

  sipcc::MediaConstraints constraints;

  nsCOMPtr<nsIPrefBranch> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
  ASSERT_TRUE(prefs);
  FsFrPrefClearer prefClearer(prefs);

  // We don't want max_fs and max_fr prefs impact SDP at this moment
  prefs->SetIntPref("media.navigator.video.max_fs", 0);
  prefs->SetIntPref("media.navigator.video.max_fr", 0);

  a1_->CreateOffer(constraints, OFFER_AV, SHOULD_CHECK_AV);
  a1_->SetLocal(TestObserver::OFFER, a1_->offer());
  a2_->SetRemote(TestObserver::OFFER, a1_->offer());

  a2_->CreateAnswer(constraints, a1_->offer(), OFFER_AV | ANSWER_AV);

  ParsedSDP sdpWrapper(a2_->answer());

  sdpWrapper.ReplaceLine("a=rtpmap:120",
    "a=rtpmap:120 VP8/90000\r\na=fmtp:120 max-fs=600;max-fr=60\r\n");

  std::cout << "Modified SDP " << std::endl
            << indent(sdpWrapper.getSdp()) << std::endl;

  // Double confirm that SDP answer contains correct max-fs and max-fr
  CheckMaxFsFrSdp(sdpWrapper.getSdp(), 120, 600, 60);

  a2_->SetLocal(TestObserver::ANSWER, sdpWrapper.getSdp());
  a1_->SetRemote(TestObserver::ANSWER, sdpWrapper.getSdp());

  ASSERT_TRUE_WAIT(a1_->IceCompleted() == true, kDefaultTimeout);
  ASSERT_TRUE_WAIT(a2_->IceCompleted() == true, kDefaultTimeout);

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

  ::testing::InitGoogleTest(&argc, argv);

  for(int i=0; i<argc; i++) {
    if (!strcmp(argv[i],"-t")) {
      kDefaultTimeout = 20000;
    }
  }

  ::testing::TestEventListeners& listeners =
        ::testing::UnitTest::GetInstance()->listeners();
  // Adds a listener to the end.  Google Test takes the ownership.
  listeners.Append(new test::RingbufferDumper(test_utils));
  test_utils->sts_target()->Dispatch(
    WrapRunnableNM(&TestStunServer::GetInstance), NS_DISPATCH_SYNC);

  ::testing::AddGlobalTestEnvironment(new test::SignalingEnvironment);
  int result = RUN_ALL_TESTS();

  test_utils->sts_target()->Dispatch(
    WrapRunnableNM(&TestStunServer::ShutdownInstance), NS_DISPATCH_SYNC);

  // Because we don't initialize on the main thread, we can't register for
  // XPCOM shutdown callbacks (where the context is usually shut down) --
  // so we need to explictly destroy the context.
  sipcc::PeerConnectionCtx::Destroy();
  delete test_utils;

  return result;
}
