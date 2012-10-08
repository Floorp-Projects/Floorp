/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <iostream>
#include <map>
#include <algorithm>
#include <string>

using namespace std;

#include "base/basictypes.h"

#define GTEST_HAS_RTTI 0
#include "gtest/gtest.h"
#include "gtest_utils.h"

#include "nspr.h"
#include "nss.h"
#include "ssl.h"
#include "prthread.h"

#include "FakeMediaStreams.h"
#include "FakeMediaStreamsImpl.h"
#include "PeerConnectionImpl.h"
#include "runnable_utils.h"
#include "nsStaticComponents.h"
#include "nsIDOMRTCPeerConnection.h"

#include "mtransport_test_utils.h"
MtransportTestUtils test_utils;

static int kDefaultTimeout = 5000;


namespace test {


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

class TestObserver : public IPeerConnectionObserver
{
public:
  enum Action {
    OFFER,
    ANSWER
  };

  enum StateType {
    kReadyState,
    kIceState,
    kSdpState,
    kSipccState
  };

  enum ResponseState {
    stateNoResponse,
    stateSuccess,
    stateError
  };

  TestObserver(sipcc::PeerConnectionImpl *peerConnection) :
    state(stateNoResponse),
    onAddStreamCalled(false),
    pc(peerConnection) {
  }

  virtual ~TestObserver() {}

  std::vector<nsDOMMediaStream *> GetStreams() { return streams; }

  NS_DECL_ISUPPORTS
  NS_DECL_IPEERCONNECTIONOBSERVER

  ResponseState state;
  char *lastString;
  uint32_t lastStatusCode;
  uint32_t lastStateType;
  bool onAddStreamCalled;

private:
  sipcc::PeerConnectionImpl *pc;
  std::vector<nsDOMMediaStream *> streams;
};

NS_IMPL_THREADSAFE_ISUPPORTS1(TestObserver, IPeerConnectionObserver)

NS_IMETHODIMP
TestObserver::OnCreateOfferSuccess(const char* offer)
{
  state = stateSuccess;
  cout << "onCreateOfferSuccess = " << offer << endl;
  lastString = strdup(offer);
  return NS_OK;
}

NS_IMETHODIMP
TestObserver::OnCreateOfferError(uint32_t code)
{
  state = stateError;
  cout << "onCreateOfferError" << endl;
  lastStatusCode = code;
  return NS_OK;
}

NS_IMETHODIMP
TestObserver::OnCreateAnswerSuccess(const char* answer)
{
  state = stateSuccess;
  cout << "onCreateAnswerSuccess = " << answer << endl;
  lastString = strdup(answer);
  return NS_OK;
}

NS_IMETHODIMP
TestObserver::OnCreateAnswerError(uint32_t code)
{
  state = stateError;
  lastStatusCode = code;
  return NS_OK;
}

NS_IMETHODIMP
TestObserver::OnSetLocalDescriptionSuccess(uint32_t code)
{
  state = stateSuccess;
  lastStatusCode = code;
  return NS_OK;
}

NS_IMETHODIMP
TestObserver::OnSetRemoteDescriptionSuccess(uint32_t code)
{
  state = stateSuccess;
  lastStatusCode = code;
  return NS_OK;
}

NS_IMETHODIMP
TestObserver::OnSetLocalDescriptionError(uint32_t code)
{
  state = stateError;
  lastStatusCode = code;
  return NS_OK;
}

NS_IMETHODIMP
TestObserver::OnSetRemoteDescriptionError(uint32_t code)
{
  state = stateError;
  lastStatusCode = code;
  return NS_OK;
}

NS_IMETHODIMP
TestObserver::NotifyConnection()
{
  return NS_OK;
}

NS_IMETHODIMP
TestObserver::NotifyClosedConnection()
{
  return NS_OK;
}

NS_IMETHODIMP
TestObserver::NotifyDataChannel(nsIDOMDataChannel *channel)
{
  return NS_OK;
}

NS_IMETHODIMP
TestObserver::OnStateChange(uint32_t state_type)
{
  nsresult rv;
  uint32_t gotstate;

  switch (state_type)
  {
  case kReadyState:
    rv = pc->GetReadyState(&gotstate);
    NS_ENSURE_SUCCESS(rv, rv);
    cout << "Ready State: " << gotstate << endl;
    break;
  case kIceState:
    rv = pc->GetIceState(&gotstate);
    NS_ENSURE_SUCCESS(rv, rv);
    cout << "ICE State: " << gotstate << endl;
    break;
  case kSdpState:
    cout << "SDP State: " << endl;
    // NS_ENSURE_SUCCESS(rv, rv);
    break;
  case kSipccState:
    rv = pc->GetSipccState(&gotstate);
    NS_ENSURE_SUCCESS(rv, rv);
    cout << "SIPCC State: " << gotstate << endl;
    break;
  default:
    // Unknown State
    break;
  }

  state = stateSuccess;
  lastStateType = state_type;
  return NS_OK;
}


NS_IMETHODIMP
TestObserver::OnAddStream(nsIDOMMediaStream *stream, const char *type)
{
  PR_ASSERT(stream);

  nsDOMMediaStream *ms = static_cast<nsDOMMediaStream *>(stream);

  cout << "OnAddStream called hints=" << ms->GetHintContents() << endl;
  state = stateSuccess;
  onAddStreamCalled = true;

  // We know that the media stream is secretly a Fake_SourceMediaStream,
  // so now we can start it pulling from us
  Fake_SourceMediaStream *fs = static_cast<Fake_SourceMediaStream *>(ms->GetStream());

  nsresult ret;
  test_utils.sts_target()->Dispatch(
    WrapRunnableRet(fs, &Fake_SourceMediaStream::Start, &ret),
    NS_DISPATCH_SYNC);

  streams.push_back(ms);

  return NS_OK;
}

NS_IMETHODIMP
TestObserver::OnRemoveStream()
{
  state = stateSuccess;
  return NS_OK;
}

NS_IMETHODIMP
TestObserver::OnAddTrack()
{
  state = stateSuccess;
  return NS_OK;
}

NS_IMETHODIMP
TestObserver::OnRemoveTrack()
{
  state = stateSuccess;
  return NS_OK;
}

NS_IMETHODIMP
TestObserver::FoundIceCandidate(const char* strCandidate)
{
  return NS_OK;
}

class ParsedSDP {
 public:
  //Line number with the corresponding SDP line.
  typedef pair<int, string> SdpLine;

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

  
  void ReplaceLine(std::string objType, std::string content)
  {
    std::multimap<std::string, SdpLine>::iterator it;
    it = sdp_map_.find(objType);
    if(it != sdp_map_.end()) {
      SdpLine sdp_line_pair = (*it).second;
      int line_no = sdp_line_pair.first;
      sdp_map_.erase(it);
      std::string value = content.substr(objType.length());
      sdp_map_.insert(std::pair<std::string, SdpLine>(objType, make_pair(line_no,value)));
    }
  }
  
  void AddLine(std::string content) 
  {
    size_t whiteSpace = content.find(' ');
    std::string key;
    std::string value;
    if(whiteSpace == string::npos) {
      key = content.substr(0,  content.size() - 2);
      value = "";
    } else {
      key = content.substr(0, whiteSpace);
      value = content.substr(whiteSpace+1);
    }
    sdp_map_.insert(std::pair<std::string, SdpLine>(key, make_pair(num_lines,value)));
    num_lines++;
  }

  //Parse SDP as string into map that looks like:
  // key: sdp content till first space
  // value : <line_number, sdp content after the first space>
  void Parse()
  {
    size_t prev = 0;
    size_t found = 0;
    num_lines = 0;
    for(;;) {
      found = sdp_.find('\n', found + 1);
      if (found == string::npos)
        break;
      std::string line = sdp_.substr(prev, (found - prev) + 1);
      size_t whiteSpace = line.find(' ');
      std::string key;
      std::string value;
      if(whiteSpace == string::npos) {
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
      SdpLine sdp_line_pair = make_pair(num_lines,value);
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

    //generate our final sdp in string format
    std::string sdp;
    for(int i=0; i < sdp_lines.size(); i++)
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
  SignalingAgent() {
    Init();
  }
  ~SignalingAgent() {
    Close();
  }

  void Init()
  {
    size_t found = 2;
    ASSERT_TRUE(found > 0);

    pc = sipcc::PeerConnectionImpl::CreatePeerConnection();
    ASSERT_TRUE(pc);

    pObserver = new TestObserver(pc);
    ASSERT_TRUE(pObserver);

    ASSERT_EQ(pc->Initialize(pObserver, nullptr, nullptr), NS_OK);

    ASSERT_TRUE_WAIT(sipcc_state() == sipcc::PeerConnectionImpl::kStarted,
                     kDefaultTimeout);
    ASSERT_TRUE_WAIT(ice_state() == sipcc::PeerConnectionImpl::kIceWaiting, 5000);
    cout << "Init Complete" << endl;
  }

  uint32_t sipcc_state()
  {
    uint32_t res;

    pc->GetSipccState(&res);
    return res;
  }

  uint32_t ice_state()
  {
    uint32_t res;

    pc->GetIceState(&res);
    return res;
  }

  void Close()
  {
    cout << "Close" << endl;
    pc->Close();
    // Shutdown is synchronous evidently.
    // ASSERT_TRUE(pObserver->WaitForObserverCall());
    // ASSERT_EQ(pc->sipcc_state(), sipcc::PeerConnectionInterface::kIdle);
  }

  char* offer() const { return offer_; }
  char* answer() const { return answer_; }

  void CreateOffer(const char* hints, bool audio, bool video) {

    // Create a media stream as if it came from GUM
    Fake_AudioStreamSource *audio_stream = 
      new Fake_AudioStreamSource();

    nsresult ret;
    test_utils.sts_target()->Dispatch(
      WrapRunnableRet(audio_stream, &Fake_MediaStream::Start, &ret),
        NS_DISPATCH_SYNC);

    ASSERT_TRUE(NS_SUCCEEDED(ret));

    // store in object to be used by RemoveStream
    nsRefPtr<nsDOMMediaStream> domMediaStream = new nsDOMMediaStream(audio_stream);
    domMediaStream_ = domMediaStream;

    uint32_t aHintContents = 0;

    if (audio) {
      aHintContents |= nsDOMMediaStream::HINT_CONTENTS_AUDIO;
    }
    if (video) {
      aHintContents |= nsDOMMediaStream::HINT_CONTENTS_VIDEO;
    }

    PR_ASSERT(aHintContents);

    domMediaStream->SetHintContents(aHintContents);

    pc->AddStream(domMediaStream);
    domMediaStream_ = domMediaStream;

    // Now call CreateOffer as JS would
    pObserver->state = TestObserver::stateNoResponse;
    ASSERT_EQ(pc->CreateOffer(hints), NS_OK);
    ASSERT_TRUE_WAIT(pObserver->state == TestObserver::stateSuccess, kDefaultTimeout);
    SDPSanityCheck(pObserver->lastString, audio, video, true);
    offer_ = pObserver->lastString;
  }

  void CreateOfferExpectError(const char* hints) {
    ASSERT_EQ(pc->CreateOffer(hints), NS_OK);
    ASSERT_TRUE_WAIT(pObserver->state == TestObserver::stateError, kDefaultTimeout);
  }

  void CreateAnswer(const char* hints, std::string offer) {
    // Create a media stream as if it came from GUM
    nsRefPtr<nsDOMMediaStream> domMediaStream = new nsDOMMediaStream();

    // Pretend GUM got both audio and video.
    domMediaStream->SetHintContents(nsDOMMediaStream::HINT_CONTENTS_AUDIO | nsDOMMediaStream::HINT_CONTENTS_VIDEO);

    pc->AddStream(domMediaStream);

    pObserver->state = TestObserver::stateNoResponse;
    ASSERT_EQ(pc->CreateAnswer(hints, offer.c_str()), NS_OK);
    ASSERT_TRUE_WAIT(pObserver->state == TestObserver::stateSuccess, kDefaultTimeout);
    SDPSanityCheck(pObserver->lastString, true, true, false);
    answer_ = pObserver->lastString;
  }

  void CreateOfferRemoveStream(const char* hints, bool audio, bool video) {

    uint32_t aHintContents = 0;

    if (!audio) {
      aHintContents |= nsDOMMediaStream::HINT_CONTENTS_VIDEO;
    }
    if (!video) {
      aHintContents |= nsDOMMediaStream::HINT_CONTENTS_AUDIO;
    }

    domMediaStream_->SetHintContents(aHintContents);

    // When complete RemoveStream will remove and entire stream and its tracks
    // not just disable a track as this is currently doing
    pc->RemoveStream(domMediaStream_);

    // Now call CreateOffer as JS would
    pObserver->state = TestObserver::stateNoResponse;
    ASSERT_EQ(pc->CreateOffer(hints), NS_OK);
    ASSERT_TRUE_WAIT(pObserver->state == TestObserver::stateSuccess, kDefaultTimeout);
    SDPSanityCheck(pObserver->lastString, video, audio, true);
    offer_ = pObserver->lastString;
  }

  void SetRemote(TestObserver::Action action, std::string remote) {
    pObserver->state = TestObserver::stateNoResponse;
    ASSERT_EQ(pc->SetRemoteDescription(action, remote.c_str()), NS_OK);
    ASSERT_TRUE_WAIT(pObserver->state == TestObserver::stateSuccess, kDefaultTimeout);
  }

  void SetLocal(TestObserver::Action action, std::string local) {
    pObserver->state = TestObserver::stateNoResponse;
    ASSERT_EQ(pc->SetLocalDescription(action, local.c_str()), NS_OK);
    ASSERT_TRUE_WAIT(pObserver->state == TestObserver::stateSuccess, kDefaultTimeout);
  }

  void DoTrickleIce(ParsedSDP &sdp) {
    for (std::multimap<int, std::string>::iterator it = sdp.ice_candidates_.begin();
         it != sdp.ice_candidates_.end(); ++it) {
      if ((*it).first != 0) {
        std::cerr << "Adding trickle ICE candidate " << (*it).second << std::endl;
        
        ASSERT_TRUE(NS_SUCCEEDED(pc->AddIceCandidate((*it).second.c_str(), "", (*it).first)));
      }
    }
  }
  

  bool IceCompleted() {
    uint32_t state;
    pc->GetIceState(&state);
    return state == sipcc::PeerConnectionImpl::kIceConnected;
  }

  void AddIceCandidate(const char* candidate, const char* mid, unsigned short level) {
    pc->AddIceCandidate(candidate, mid, level);
  }

#if 0
  void CreateOfferSetLocal(const char* hints) {
      CreateOffer(hints);

      pObserver->state = TestObserver::stateNoResponse;
      ASSERT_EQ(pc->SetLocalDescription(sipcc::OFFER, pObserver->lastString), NS_OK);
      ASSERT_TRUE(pObserver->WaitForObserverCall());
      ASSERT_EQ(pObserver->state, TestObserver::stateSuccess);
      ASSERT_EQ(pc->SetRemoteDescription(sipcc::OFFER, strSampleSdpAudioVideoNoIce), NS_OK);
      ASSERT_TRUE(pObserver->WaitForObserverCall());
      ASSERT_EQ(pObserver->state, TestObserver::stateSuccess);
    }

  void CreateAnswer(const char* hints, )
    {
      std::string offer = strSampleSdpAudioVideoNoIce;
      std::string strHints(hints);

      ASSERT_EQ(pc->CreateAnswer(strHints, offer), NS_OK);
      ASSERT_TRUE(pObserver->WaitForObserverCall());
      ASSERT_EQ(pObserver->state, TestObserver::stateSuccess);
      SDPSanityCheck(pObserver->lastString, true, true, false);
    }
#endif

  int GetPacketsReceived(int stream) {
    std::vector<nsDOMMediaStream *> streams = pObserver->GetStreams();

    if ((int) streams.size() <= stream) {
      return 0;
    }

    return streams[stream]->GetStream()->AsSourceStream()->GetSegmentsAdded();
  }

  int GetPacketsSent(int stream) {
    return static_cast<Fake_MediaStreamBase *>(
        domMediaStream_->GetStream())->GetSegmentsAdded();
  }


public:
  mozilla::RefPtr<sipcc::PeerConnectionImpl> pc;
  nsRefPtr<TestObserver> pObserver;
  char* offer_;
  char* answer_;
  nsRefPtr<nsDOMMediaStream> domMediaStream_;


private:
  void SDPSanityCheck(std::string sdp, bool shouldHaveAudio, bool shouldHaveVideo, bool offer)
  {
    ASSERT_NE(sdp.find("v=0"), std::string::npos);
    ASSERT_NE(sdp.find("c=IN IP4"), std::string::npos);
    ASSERT_NE(sdp.find("a=fingerprint:sha-256"), std::string::npos);

    if (shouldHaveAudio)
    {
    	if (offer)
    		ASSERT_NE(sdp.find("a=rtpmap:0 PCMU/8000"), std::string::npos);


    	// after negotiation we are left with one codec
    	ASSERT_NE(sdp.find("a=rtpmap:109 opus/48000"), std::string::npos);
    }

    if (shouldHaveVideo)
    {
      ASSERT_NE(sdp.find("a=rtpmap:120 VP8/90000"), std::string::npos);
    }
  }
};

class SignalingEnvironment : public ::testing::Environment {
 public:
  void TearDown() {
    sipcc::PeerConnectionImpl::Shutdown();
  }
};

class SignalingTest : public ::testing::Test {
public:
  void CreateOffer(const char* hints) {
    a1_.CreateOffer(hints, true, true);
  }

  void CreateSetOffer(const char* hints) {
    a1_.CreateOffer(hints, true, true);
    a1_.SetLocal(TestObserver::OFFER, a1_.offer());
  }
  
  void OfferAnswer(const char* ahints, const char* bhints) {
    a1_.CreateOffer(ahints, true, true);
    a1_.SetLocal(TestObserver::OFFER, a1_.offer());
    a2_.SetRemote(TestObserver::OFFER, a1_.offer());
    a2_.CreateAnswer(bhints, a1_.offer());
    a2_.SetLocal(TestObserver::ANSWER, a2_.answer());
    a1_.SetRemote(TestObserver::ANSWER, a2_.answer());
    ASSERT_TRUE_WAIT(a1_.IceCompleted() == true, kDefaultTimeout);
    ASSERT_TRUE_WAIT(a2_.IceCompleted() == true, kDefaultTimeout);
  }

  void OfferModifiedAnswer(const char* ahints, const char* bhints) {
    a1_.CreateOffer(ahints, true, true);
    a1_.SetLocal(TestObserver::OFFER, a1_.offer());
    a2_.SetRemote(TestObserver::OFFER, a1_.offer());
    a2_.CreateAnswer(bhints, a1_.offer());
    a2_.SetLocal(TestObserver::ANSWER, a2_.answer());
    ParsedSDP sdpWrapper(a2_.answer());
    sdpWrapper.ReplaceLine("m=audio", "m=audio 65375 RTP/SAVPF 109 8 101\r\n");
    sdpWrapper.AddLine("a=rtpmap:8 PCMA/8000\r\n");
    cout << "Modified SDP " << sdpWrapper.getSdp() << endl;
    a1_.SetRemote(TestObserver::ANSWER, sdpWrapper.getSdp());
    ASSERT_TRUE_WAIT(a1_.IceCompleted() == true, kDefaultTimeout);
    ASSERT_TRUE_WAIT(a2_.IceCompleted() == true, kDefaultTimeout);
  }

  void OfferAnswerTrickle(const char* ahints, const char* bhints) {
    a1_.CreateOffer(ahints, true, true);
    a1_.SetLocal(TestObserver::OFFER, a1_.offer());
    ParsedSDP a1_offer(a1_.offer());
    a2_.SetRemote(TestObserver::OFFER, a1_offer.sdp_without_ice_);
    a2_.CreateAnswer(bhints, a1_offer.sdp_without_ice_);
    a2_.SetLocal(TestObserver::ANSWER, a2_.answer());
    ParsedSDP a2_answer(a2_.answer());
    a1_.SetRemote(TestObserver::ANSWER, a2_answer.sdp_without_ice_);
    // Now set the trickle ICE candidates
    a1_.DoTrickleIce(a2_answer);
    a2_.DoTrickleIce(a1_offer);
    ASSERT_TRUE_WAIT(a1_.IceCompleted() == true, kDefaultTimeout);
    ASSERT_TRUE_WAIT(a2_.IceCompleted() == true, kDefaultTimeout);
  }

  void CreateOfferVideoOnly(const char* hints) {
    a1_.CreateOffer(hints, false, true);
  }

  void CreateOfferAudioOnly(const char * hints) {
    a1_.CreateOffer(hints, true, false);
  }

  void CreateOfferRemoveStream(const char * hints) {
	a1_.CreateOffer(hints, true, true);
    a1_.CreateOfferRemoveStream(hints, false, true);
  }

  void CreateOfferAddCandidate(const char * hints, const char * candidate, const char * mid, unsigned short level) {
    a1_.CreateOffer(hints, true, true);
    a1_.AddIceCandidate(candidate, mid, level);
  }

 protected:
  SignalingAgent a1_;  // Canonically "caller"
  SignalingAgent a2_;  // Canonically "callee"
};


TEST_F(SignalingTest, JustInit)
{
}

TEST_F(SignalingTest, CreateOfferNoHints)
{
  CreateOffer("");
}

TEST_F(SignalingTest, CreateSetOffer)
{
  CreateSetOffer("");
}

TEST_F(SignalingTest, CreateOfferVideoOnly)
{
  CreateOfferVideoOnly("");
}

TEST_F(SignalingTest, CreateOfferAudioOnly)
{
  CreateOfferAudioOnly("");
}

TEST_F(SignalingTest, CreateOfferRemoveStream)
{
	CreateOfferRemoveStream("");
}

TEST_F(SignalingTest, CreateOfferAddCandidate)
{
	CreateOfferAddCandidate("", strSampleCandidate.c_str(), strSampleMid.c_str(), nSamplelevel);
}

TEST_F(SignalingTest, OfferAnswer)
{
  OfferAnswer("", "");
  PR_Sleep(kDefaultTimeout * 2); // Wait for completion
}

TEST_F(SignalingTest, OfferModifiedAnswer)
{
  OfferModifiedAnswer("", "");
  PR_Sleep(kDefaultTimeout * 2); // Wait for completion
}

TEST_F(SignalingTest, FullCall)
{
  OfferAnswer("", "");
  PR_Sleep(kDefaultTimeout * 2); // Wait for some data to get written

  // Check that we wrote a bunch of data
  ASSERT_GE(a1_.GetPacketsSent(0), 40);
  //ASSERT_GE(a2_.GetPacketsSent(0), 40);
  //ASSERT_GE(a1_.GetPacketsReceived(0), 40);
  ASSERT_GE(a2_.GetPacketsReceived(0), 40);
}

TEST_F(SignalingTest, FullCallTrickle)
{
  OfferAnswerTrickle("", "");
  PR_Sleep(kDefaultTimeout * 2); // Wait for some data to get written

  ASSERT_GE(a1_.GetPacketsSent(0), 40);
  ASSERT_GE(a2_.GetPacketsReceived(0), 40);
}

//TEST_F(SignalingTest, CreateOfferHints)
//{
//  CreateOffer("audio,video");
//}

//TEST_F(SignalingTest, CreateOfferBadHints)
//{
//  CreateOfferExpectError("9.uoeuhaoensthuaeugc.pdu8g");
//}

//TEST_F(SignalingTest, CreateOfferSetLocal)
//{
//  CreateOfferSetLocal("");
//}

//TEST_F(SignalingTest, CreateAnswerNoHints)
//{
//  CreateAnswer("");
//}

} // End namespace test.

int main(int argc, char **argv)
{
  test_utils.InitServices();
  NSS_NoDB_Init(NULL);
  NSS_SetDomesticPolicy();

  ::testing::InitGoogleTest(&argc, argv);

  for(int i=0; i<argc; i++) {
    if (!strcmp(argv[i],"-t")) {
      kDefaultTimeout = 20000;
    }

  }

  ::testing::AddGlobalTestEnvironment(new test::SignalingEnvironment);
  int result = RUN_ALL_TESTS();
  return result;
}
