/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <iostream>
#include <map>

#include "nspr.h"
#include "nss.h"
#include "ssl.h"

#include "mozilla/RefPtr.h"

#define GTEST_HAS_RTTI 0
#include "gtest/gtest.h"
#include "gtest_utils.h"

#include "FakeMediaStreams.h"
#include "FakeMediaStreamsImpl.h"

#include "signaling/src/sdp/SdpMediaSection.h"
#include "signaling/src/sdp/SipccSdpParser.h"
#include "signaling/src/jsep/JsepCodecDescription.h"
#include "signaling/src/jsep/JsepTrack.h"
#include "signaling/src/jsep/JsepSession.h"
#include "signaling/src/jsep/JsepSessionImpl.h"
#include "signaling/src/jsep/JsepTrack.h"

namespace mozilla {
static const char* kCandidates[] = {
  "0 1 UDP 9999 192.168.0.1 2000 typ host",
  "0 1 UDP 9999 192.168.0.1 2001 typ host",
  "0 1 UDP 9999 192.168.0.2 2002 typ srflx raddr 10.252.34.97 rport 53594",
  // Mix up order
  "0 1 UDP 9999 192.168.1.2 2012 typ srflx raddr 10.252.34.97 rport 53594",
  "0 1 UDP 9999 192.168.1.1 2010 typ host",
  "0 1 UDP 9999 192.168.1.1 2011 typ host"
};

static std::string kAEqualsCandidate("a=candidate:");

class JsepSessionTestBase : public ::testing::Test
{
};

class FakeUuidGenerator : public mozilla::JsepUuidGenerator
{
public:
  bool
  Generate(std::string* str)
  {
    std::ostringstream os;
    os << "FAKE_UUID_" << ++ctr;
    *str = os.str();

    return true;
  }

private:
  static uint64_t ctr;
};

uint64_t FakeUuidGenerator::ctr = 1000;

class JsepSessionTest : public JsepSessionTestBase,
                        public ::testing::WithParamInterface<std::string>
{
public:
  JsepSessionTest()
      : mSessionOff("Offerer", MakeUnique<FakeUuidGenerator>()),
        mSessionAns("Answerer", MakeUnique<FakeUuidGenerator>())
  {
    EXPECT_EQ(NS_OK, mSessionOff.Init());
    EXPECT_EQ(NS_OK, mSessionAns.Init());

    AddTransportData(&mSessionOff, &mOffererTransport);
    AddTransportData(&mSessionAns, &mAnswererTransport);
  }

protected:
  struct TransportData {
    std::string mIceUfrag;
    std::string mIcePwd;
    std::map<std::string, std::vector<uint8_t> > mFingerprints;
  };

  void
  AddDtlsFingerprint(const std::string& alg, JsepSessionImpl* session,
                     TransportData* tdata)
  {
    std::vector<uint8_t> fp;
    fp.assign((alg == "sha-1") ? 20 : 32,
              (session->GetName() == "Offerer") ? 0x4f : 0x41);
    session->AddDtlsFingerprint(alg, fp);
    tdata->mFingerprints[alg] = fp;
  }

  void
  AddTransportData(JsepSessionImpl* session, TransportData* tdata)
  {
    // Values here semi-borrowed from JSEP draft.
    tdata->mIceUfrag = session->GetName() + "-ufrag";
    tdata->mIcePwd = session->GetName() + "-1234567890";
    session->SetIceCredentials(tdata->mIceUfrag, tdata->mIcePwd);
    AddDtlsFingerprint("sha-1", session, tdata);
    AddDtlsFingerprint("sha-256", session, tdata);
  }

  std::string
  CreateOffer(const Maybe<JsepOfferOptions> options = Nothing())
  {
    JsepOfferOptions defaultOptions;
    const JsepOfferOptions& optionsRef = options ? *options : defaultOptions;
    std::string offer;
    nsresult rv = mSessionOff.CreateOffer(optionsRef, &offer);
    EXPECT_EQ(NS_OK, rv) << mSessionOff.GetLastError();

    std::cerr << "OFFER: " << offer << std::endl;

    ValidateTransport(mOffererTransport, offer);

    return offer;
  }

  void
  AddTracks(JsepSessionImpl* side)
  {
    // Add tracks.
    if (types.empty()) {
      types = BuildTypes(GetParam());
    }
    AddTracks(side, types);

    // Now that we have added streams, we expect audio, then video, then
    // application in the SDP, regardless of the order in which the streams were
    // added.
    std::sort(types.begin(), types.end());
  }

  void
  AddTracks(JsepSessionImpl* side, const std::string& mediatypes)
  {
    AddTracks(side, BuildTypes(mediatypes));
  }

  std::vector<SdpMediaSection::MediaType>
  BuildTypes(const std::string& mediatypes)
  {
    std::vector<SdpMediaSection::MediaType> result;
    size_t ptr = 0;

    for (;;) {
      size_t comma = mediatypes.find(',', ptr);
      std::string chunk = mediatypes.substr(ptr, comma - ptr);

      SdpMediaSection::MediaType type;
      if (chunk == "audio") {
        type = SdpMediaSection::kAudio;
      } else if (chunk == "video") {
        type = SdpMediaSection::kVideo;
      } else if (chunk == "datachannel") {
        type = SdpMediaSection::kApplication;
      } else {
        MOZ_CRASH();
      }
      result.push_back(type);

      if (comma == std::string::npos)
        break;
      ptr = comma + 1;
    }

    return result;
  }

  void
  AddTracks(JsepSessionImpl* side,
            const std::vector<SdpMediaSection::MediaType>& mediatypes)
  {
    FakeUuidGenerator uuid_gen;
    std::string stream_id;
    std::string track_id;

    ASSERT_TRUE(uuid_gen.Generate(&stream_id));

    for (auto track = mediatypes.begin(); track != mediatypes.end(); ++track) {
      ASSERT_TRUE(uuid_gen.Generate(&track_id));

      RefPtr<JsepTrack> mst(new JsepTrack(*track, stream_id, track_id));
      side->AddTrack(mst);
    }
  }

  void
  EnsureNegotiationFailure(SdpMediaSection::MediaType type,
                           const std::string& codecName)
  {
    for (auto i = mSessionOff.Codecs().begin(); i != mSessionOff.Codecs().end();
         ++i) {
      auto* codec = *i;
      if (codec->mType == type && codec->mName != codecName) {
        codec->mEnabled = false;
      }
    }

    for (auto i = mSessionAns.Codecs().begin(); i != mSessionAns.Codecs().end();
         ++i) {
      auto* codec = *i;
      if (codec->mType == type && codec->mName == codecName) {
        codec->mEnabled = false;
      }
    }
  }

  std::string
  CreateAnswer()
  {
    JsepAnswerOptions options;
    std::string answer;
    nsresult rv = mSessionAns.CreateAnswer(options, &answer);
    EXPECT_EQ(NS_OK, rv);

    std::cerr << "ANSWER: " << answer << std::endl;

    ValidateTransport(mAnswererTransport, answer);

    return answer;
  }

  static const uint32_t NO_CHECKS = 0;
  static const uint32_t CHECK_SUCCESS = 1;
  static const uint32_t CHECK_TRACKS = 1 << 2;
  static const uint32_t ALL_CHECKS = CHECK_SUCCESS | CHECK_TRACKS;

  void
  SetLocalOffer(const std::string& offer, uint32_t checkFlags = ALL_CHECKS)
  {
    nsresult rv = mSessionOff.SetLocalDescription(kJsepSdpOffer, offer);

    if (checkFlags & CHECK_SUCCESS) {
      ASSERT_EQ(NS_OK, rv);
    }

    if (checkFlags & CHECK_TRACKS) {
      // Check that the transports exist.
      ASSERT_EQ(types.size(), mSessionOff.GetTransportCount());
    }
  }

  void
  SetRemoteOffer(const std::string& offer, uint32_t checkFlags = ALL_CHECKS)
  {
    nsresult rv = mSessionAns.SetRemoteDescription(kJsepSdpOffer, offer);

    if (checkFlags & CHECK_SUCCESS) {
      ASSERT_EQ(NS_OK, rv);
    }

    if (checkFlags & CHECK_TRACKS) {
      // Now verify that the right stuff is in the tracks.
      ASSERT_EQ(types.size(), mSessionAns.GetRemoteTrackCount());
      for (size_t i = 0; i < types.size(); ++i) {
        RefPtr<JsepTrack> rtrack;
        ASSERT_EQ(NS_OK, mSessionAns.GetRemoteTrack(i, &rtrack));
        ASSERT_EQ(types[i], rtrack->GetMediaType());
      }
    }
  }

  void
  SetLocalAnswer(const std::string& answer, uint32_t checkFlags = ALL_CHECKS)
  {
    nsresult rv = mSessionAns.SetLocalDescription(kJsepSdpAnswer, answer);
    if (checkFlags & CHECK_SUCCESS) {
      ASSERT_EQ(NS_OK, rv);
    }

    if (checkFlags & CHECK_TRACKS) {
      // Verify that the right stuff is in the tracks.
      ASSERT_EQ(types.size(), mSessionAns.GetNegotiatedTrackPairCount());
      for (size_t i = 0; i < types.size(); ++i) {
        const JsepTrackPair* pair;
        ASSERT_EQ(NS_OK, mSessionAns.GetNegotiatedTrackPair(i, &pair));
        ASSERT_TRUE(pair->mSending);
        ASSERT_EQ(types[i], pair->mSending->GetMediaType());
        ASSERT_TRUE(pair->mReceiving);
        ASSERT_EQ(types[i], pair->mReceiving->GetMediaType());
      }
    }
    DumpTrackPairs(mSessionOff);
  }

  void
  SetRemoteAnswer(const std::string& answer, uint32_t checkFlags = ALL_CHECKS)
  {
    nsresult rv = mSessionOff.SetRemoteDescription(kJsepSdpAnswer, answer);
    if (checkFlags & CHECK_SUCCESS) {
      ASSERT_EQ(NS_OK, rv);
    }

    if (checkFlags & CHECK_TRACKS) {
      // Verify that the right stuff is in the tracks.
      ASSERT_EQ(types.size(), mSessionAns.GetNegotiatedTrackPairCount());
      for (size_t i = 0; i < types.size(); ++i) {
        const JsepTrackPair* pair;
        ASSERT_EQ(NS_OK, mSessionAns.GetNegotiatedTrackPair(i, &pair));
        ASSERT_TRUE(pair->mSending);
        ASSERT_EQ(types[i], pair->mSending->GetMediaType());
        ASSERT_TRUE(pair->mReceiving);
        ASSERT_EQ(types[i], pair->mReceiving->GetMediaType());
      }
    }
    DumpTrackPairs(mSessionAns);
  }

  void
  GatherCandidates(JsepSession& session)
  {
    session.AddLocalIceCandidate(kAEqualsCandidate + kCandidates[0], "", 0);
    session.AddLocalIceCandidate(kAEqualsCandidate + kCandidates[1], "", 0);
    session.AddLocalIceCandidate(kAEqualsCandidate + kCandidates[2], "", 0);
    session.EndOfLocalCandidates("192.168.0.2", 2002, 0);

    session.AddLocalIceCandidate(kAEqualsCandidate + kCandidates[3], "", 1);
    session.AddLocalIceCandidate(kAEqualsCandidate + kCandidates[4], "", 1);
    session.AddLocalIceCandidate(kAEqualsCandidate + kCandidates[5], "", 1);
    session.EndOfLocalCandidates("192.168.1.2", 2012, 1);

    std::cerr << "local SDP after candidates: "
              << session.GetLocalDescription();
  }

  void
  TrickleCandidates(JsepSession& session)
  {
    session.AddRemoteIceCandidate(kAEqualsCandidate + kCandidates[0], "", 0);
    session.AddRemoteIceCandidate(kAEqualsCandidate + kCandidates[1], "", 0);
    session.AddRemoteIceCandidate(kAEqualsCandidate + kCandidates[2], "", 0);

    session.AddRemoteIceCandidate(kAEqualsCandidate + kCandidates[3], "", 1);
    session.AddRemoteIceCandidate(kAEqualsCandidate + kCandidates[4], "", 1);
    session.AddRemoteIceCandidate(kAEqualsCandidate + kCandidates[5], "", 1);

    std::cerr << "remote SDP after candidates: "
              << session.GetRemoteDescription();
  }

  void
  GatherOffererCandidates()
  {
    GatherCandidates(mSessionOff);
  }

  void
  TrickleOffererCandidates()
  {
    TrickleCandidates(mSessionAns);
  }

  // For streaming parse errors
  std::string
  GetParseErrors(const SipccSdpParser& parser) const
  {
    std::stringstream output;
    for (auto e = parser.GetParseErrors().begin();
         e != parser.GetParseErrors().end();
         ++e) {
      output << e->first << ": " << e->second << std::endl;
    }
    return output.str();
  }

  void
  ValidateCandidates(JsepSession& session, bool local)
  {
    std::string sdp =
        local ? session.GetLocalDescription() : session.GetRemoteDescription();
    SipccSdpParser parser;
    UniquePtr<Sdp> parsed = parser.Parse(sdp);
    ASSERT_TRUE(parsed) << "Parse failed on " << std::endl << sdp << std::endl
                        << "Errors were: " << GetParseErrors(parser);
    ASSERT_LT(0U, parsed->GetMediaSectionCount());

    auto& msection_0 = parsed->GetMediaSection(0);

    // We should not be doing things like setting the c-line on remote SDP
    if (local) {
      ASSERT_EQ("192.168.0.2", msection_0.GetConnection().GetAddress());
      ASSERT_EQ(2002U, msection_0.GetPort());
      // TODO: Check end-of-candidates. Issue 200
    }

    auto& attrs_0 = msection_0.GetAttributeList();
    ASSERT_TRUE(attrs_0.HasAttribute(SdpAttribute::kCandidateAttribute));

    auto& candidates_0 = attrs_0.GetCandidate();
    ASSERT_EQ(3U, candidates_0.size());
    ASSERT_EQ(kCandidates[0], candidates_0[0]);
    ASSERT_EQ(kCandidates[1], candidates_0[1]);
    ASSERT_EQ(kCandidates[2], candidates_0[2]);

    if (parsed->GetMediaSectionCount() > 1) {
      auto& msection_1 = parsed->GetMediaSection(1);

      if (local) {
        ASSERT_EQ("192.168.1.2", msection_1.GetConnection().GetAddress());
        ASSERT_EQ(2012U, msection_1.GetPort());
        // TODO: Check end-of-candidates. Issue 200
      }

      auto& attrs_1 = msection_1.GetAttributeList();
      ASSERT_TRUE(attrs_1.HasAttribute(SdpAttribute::kCandidateAttribute));

      auto& candidates_1 = attrs_1.GetCandidate();
      ASSERT_EQ(3U, candidates_1.size());
      ASSERT_EQ(kCandidates[3], candidates_1[0]);
      ASSERT_EQ(kCandidates[4], candidates_1[1]);
      ASSERT_EQ(kCandidates[5], candidates_1[2]);
    }
  }

  void
  ValidateOffererCandidates()
  {
    ValidateCandidates(mSessionOff, true);
  }

  void
  ValidateAnswererCandidates()
  {
    ValidateCandidates(mSessionAns, false);
  }

  void
  DumpTrack(const JsepTrack& track)
  {
    std::cerr << "  type=" << track.GetMediaType() << std::endl;
    std::cerr << "  protocol=" << track.GetNegotiatedDetails()->GetProtocol()
              << std::endl;
    std::cerr << "  codecs=" << std::endl;
    size_t num_codecs = track.GetNegotiatedDetails()->GetCodecCount();
    for (size_t i = 0; i < num_codecs; ++i) {
      const JsepCodecDescription* codec;
      ASSERT_EQ(NS_OK, track.GetNegotiatedDetails()->GetCodec(i, &codec));
      std::cerr << "    " << codec->mName << std::endl;
    }
  }

  void
  DumpTrackPairs(const JsepSessionImpl& session)
  {
    size_t count = mSessionAns.GetNegotiatedTrackPairCount();
    for (size_t i = 0; i < count; ++i) {
      std::cerr << "Track pair " << i << std::endl;
      const JsepTrackPair* pair;
      ASSERT_EQ(NS_OK, mSessionAns.GetNegotiatedTrackPair(i, &pair));
      if (pair->mSending) {
        std::cerr << "Sending-->" << std::endl;
        DumpTrack(*pair->mSending);
      }
      if (pair->mReceiving) {
        std::cerr << "Receiving-->" << std::endl;
        DumpTrack(*pair->mReceiving);
      }
    }
  }

  JsepSessionImpl mSessionOff;
  JsepSessionImpl mSessionAns;
  std::vector<SdpMediaSection::MediaType> types;

private:
  void
  ValidateTransport(TransportData& source, const std::string& sdp_str)
  {
    SipccSdpParser parser;
    auto sdp = mozilla::Move(parser.Parse(sdp_str));
    ASSERT_TRUE(sdp) << "Should have valid SDP" << std::endl
                     << "Errors were: " << GetParseErrors(parser);
    size_t num_m_sections = sdp->GetMediaSectionCount();
    for (size_t i = 0; i < num_m_sections; ++i) {
      auto& msection = sdp->GetMediaSection(i);

      if (msection.GetMediaType() == SdpMediaSection::kApplication) {
        ASSERT_EQ(SdpMediaSection::kDtlsSctp, msection.GetProtocol());
      } else {
        ASSERT_EQ(SdpMediaSection::kRtpSavpf, msection.GetProtocol());
      }

      if (msection.GetPort() == 0) {
        ASSERT_EQ(SdpDirectionAttribute::kInactive,
                  msection.GetDirectionAttribute().mValue);
        // Maybe validate that no attributes are present except rtpmap and
        // inactive?
        continue;
      }
      const SdpAttributeList& attrs = msection.GetAttributeList();
      ASSERT_EQ(source.mIceUfrag, attrs.GetIceUfrag());
      ASSERT_EQ(source.mIcePwd, attrs.GetIcePwd());
      const SdpFingerprintAttributeList& fps = attrs.GetFingerprint();
      for (auto fp = fps.mFingerprints.begin(); fp != fps.mFingerprints.end();
           ++fp) {
        std::string alg_str = "None";

        if (fp->hashFunc == SdpFingerprintAttributeList::kSha1) {
          alg_str = "sha-1";
        } else if (fp->hashFunc == SdpFingerprintAttributeList::kSha256) {
          alg_str = "sha-256";
        }

        ASSERT_EQ(source.mFingerprints[alg_str], fp->fingerprint);
      }
      ASSERT_EQ(source.mFingerprints.size(), fps.mFingerprints.size());
    }
  }

  TransportData mOffererTransport;
  TransportData mAnswererTransport;
};

TEST_F(JsepSessionTestBase, CreateDestroy) {}

TEST_P(JsepSessionTest, CreateOffer)
{
  AddTracks(&mSessionOff);
  CreateOffer();
}

TEST_P(JsepSessionTest, CreateOfferSetLocal)
{
  AddTracks(&mSessionOff);
  std::string offer = CreateOffer();
  SetLocalOffer(offer);
}

TEST_P(JsepSessionTest, CreateOfferSetLocalSetRemote)
{
  AddTracks(&mSessionOff);
  std::string offer = CreateOffer();
  SetLocalOffer(offer);
  SetRemoteOffer(offer);
}

TEST_P(JsepSessionTest, CreateOfferSetLocalSetRemoteCreateAnswer)
{
  AddTracks(&mSessionOff);
  std::string offer = CreateOffer();
  SetLocalOffer(offer);
  SetRemoteOffer(offer);
  AddTracks(&mSessionAns);
  std::string answer = CreateAnswer();
}

TEST_P(JsepSessionTest, CreateOfferSetLocalSetRemoteCreateAnswerSetLocal)
{
  AddTracks(&mSessionOff);
  std::string offer = CreateOffer();
  SetLocalOffer(offer);
  SetRemoteOffer(offer);
  AddTracks(&mSessionAns);
  std::string answer = CreateAnswer();
  SetLocalAnswer(answer);
}

TEST_P(JsepSessionTest, FullCall)
{
  AddTracks(&mSessionOff);
  std::string offer = CreateOffer();
  SetLocalOffer(offer);
  SetRemoteOffer(offer);
  AddTracks(&mSessionAns);
  std::string answer = CreateAnswer();
  SetLocalAnswer(answer);
  SetRemoteAnswer(answer);
}

TEST_P(JsepSessionTest, FullCallWithCandidates)
{
  AddTracks(&mSessionOff);
  std::string offer = CreateOffer();
  SetLocalOffer(offer);
  GatherOffererCandidates();
  ValidateOffererCandidates();
  SetRemoteOffer(offer);
  TrickleOffererCandidates();
  ValidateAnswererCandidates();
  AddTracks(&mSessionAns);
  std::string answer = CreateAnswer();
  SetLocalAnswer(answer);
  SetRemoteAnswer(answer);
}

INSTANTIATE_TEST_CASE_P(Variants, JsepSessionTest,
                        ::testing::Values("audio",
                                          "video",
                                          "datachannel",
                                          "audio,video",
                                          "video,audio",
                                          "audio,datachannel",
                                          "video,datachannel",
                                          "video,audio,datachannel",
                                          "audio,video,datachannel",
                                          "datachannel,audio",
                                          "datachannel,video",
                                          "datachannel,audio,video",
                                          "datachannel,video,audio"));

// offerToReceiveXxx variants

TEST_F(JsepSessionTest, OfferAnswerRecvOnlyLines)
{
  JsepOfferOptions options;
  options.mOfferToReceiveAudio = Some(static_cast<size_t>(1U));
  options.mOfferToReceiveVideo = Some(static_cast<size_t>(2U));
  options.mDontOfferDataChannel = Some(true);
  std::string offer = CreateOffer(Some(options));

  SipccSdpParser parser;
  auto outputSdp = mozilla::Move(parser.Parse(offer));
  ASSERT_TRUE(outputSdp) << "Should have valid SDP" << std::endl
                         << "Errors were: " << GetParseErrors(parser);

  ASSERT_EQ(3U, outputSdp->GetMediaSectionCount());
  ASSERT_EQ(SdpMediaSection::kAudio,
            outputSdp->GetMediaSection(0).GetMediaType());
  ASSERT_EQ(SdpDirectionAttribute::kRecvonly,
            outputSdp->GetMediaSection(0).GetAttributeList().GetDirection());
  ASSERT_EQ(SdpMediaSection::kVideo,
            outputSdp->GetMediaSection(1).GetMediaType());
  ASSERT_EQ(SdpDirectionAttribute::kRecvonly,
            outputSdp->GetMediaSection(1).GetAttributeList().GetDirection());
  ASSERT_EQ(SdpMediaSection::kVideo,
            outputSdp->GetMediaSection(2).GetMediaType());
  ASSERT_EQ(SdpDirectionAttribute::kRecvonly,
            outputSdp->GetMediaSection(2).GetAttributeList().GetDirection());

  ASSERT_TRUE(outputSdp->GetMediaSection(0).GetAttributeList().HasAttribute(
      SdpAttribute::kRtcpMuxAttribute));
  ASSERT_TRUE(outputSdp->GetMediaSection(1).GetAttributeList().HasAttribute(
      SdpAttribute::kRtcpMuxAttribute));
  ASSERT_TRUE(outputSdp->GetMediaSection(2).GetAttributeList().HasAttribute(
      SdpAttribute::kRtcpMuxAttribute));

  SetLocalOffer(offer, CHECK_SUCCESS);

  AddTracks(&mSessionAns, "audio,video");
  SetRemoteOffer(offer, CHECK_SUCCESS);

  std::string answer = CreateAnswer();
  outputSdp = mozilla::Move(parser.Parse(answer));

  ASSERT_EQ(3U, outputSdp->GetMediaSectionCount());
  ASSERT_EQ(SdpMediaSection::kAudio,
            outputSdp->GetMediaSection(0).GetMediaType());
  ASSERT_EQ(SdpDirectionAttribute::kSendonly,
            outputSdp->GetMediaSection(0).GetAttributeList().GetDirection());
  ASSERT_EQ(SdpMediaSection::kVideo,
            outputSdp->GetMediaSection(1).GetMediaType());
  ASSERT_EQ(SdpDirectionAttribute::kSendonly,
            outputSdp->GetMediaSection(1).GetAttributeList().GetDirection());
  ASSERT_EQ(SdpMediaSection::kVideo,
            outputSdp->GetMediaSection(2).GetMediaType());
  ASSERT_EQ(SdpDirectionAttribute::kInactive,
            outputSdp->GetMediaSection(2).GetAttributeList().GetDirection());
}

TEST_F(JsepSessionTest, OfferAnswerSendOnlyLines)
{
  AddTracks(&mSessionOff, "audio,video,video");

  JsepOfferOptions options;
  options.mOfferToReceiveAudio = Some(static_cast<size_t>(0U));
  options.mOfferToReceiveVideo = Some(static_cast<size_t>(1U));
  options.mDontOfferDataChannel = Some(true);
  std::string offer = CreateOffer(Some(options));

  SipccSdpParser parser;
  auto outputSdp = mozilla::Move(parser.Parse(offer));
  ASSERT_TRUE(outputSdp) << "Should have valid SDP" << std::endl
                         << "Errors were: " << GetParseErrors(parser);

  ASSERT_EQ(3U, outputSdp->GetMediaSectionCount());
  ASSERT_EQ(SdpMediaSection::kAudio,
            outputSdp->GetMediaSection(0).GetMediaType());
  ASSERT_EQ(SdpDirectionAttribute::kSendonly,
            outputSdp->GetMediaSection(0).GetAttributeList().GetDirection());
  ASSERT_EQ(SdpMediaSection::kVideo,
            outputSdp->GetMediaSection(1).GetMediaType());
  ASSERT_EQ(SdpDirectionAttribute::kSendrecv,
            outputSdp->GetMediaSection(1).GetAttributeList().GetDirection());
  ASSERT_EQ(SdpMediaSection::kVideo,
            outputSdp->GetMediaSection(2).GetMediaType());
  ASSERT_EQ(SdpDirectionAttribute::kSendonly,
            outputSdp->GetMediaSection(2).GetAttributeList().GetDirection());

  ASSERT_TRUE(outputSdp->GetMediaSection(0).GetAttributeList().HasAttribute(
      SdpAttribute::kRtcpMuxAttribute));
  ASSERT_TRUE(outputSdp->GetMediaSection(1).GetAttributeList().HasAttribute(
      SdpAttribute::kRtcpMuxAttribute));
  ASSERT_TRUE(outputSdp->GetMediaSection(2).GetAttributeList().HasAttribute(
      SdpAttribute::kRtcpMuxAttribute));

  SetLocalOffer(offer, CHECK_SUCCESS);

  AddTracks(&mSessionAns, "audio,video");
  SetRemoteOffer(offer, CHECK_SUCCESS);

  std::string answer = CreateAnswer();
  outputSdp = mozilla::Move(parser.Parse(answer));

  ASSERT_EQ(3U, outputSdp->GetMediaSectionCount());
  ASSERT_EQ(SdpMediaSection::kAudio,
            outputSdp->GetMediaSection(0).GetMediaType());
  ASSERT_EQ(SdpDirectionAttribute::kRecvonly,
            outputSdp->GetMediaSection(0).GetAttributeList().GetDirection());
  ASSERT_EQ(SdpMediaSection::kVideo,
            outputSdp->GetMediaSection(1).GetMediaType());
  ASSERT_EQ(SdpDirectionAttribute::kSendrecv,
            outputSdp->GetMediaSection(1).GetAttributeList().GetDirection());
  ASSERT_EQ(SdpMediaSection::kVideo,
            outputSdp->GetMediaSection(2).GetMediaType());
  ASSERT_EQ(SdpDirectionAttribute::kRecvonly,
            outputSdp->GetMediaSection(2).GetAttributeList().GetDirection());
}

TEST_F(JsepSessionTest, CreateOfferNoDatachannelDefault)
{
  RefPtr<JsepTrack> msta(
      new JsepTrack(SdpMediaSection::kAudio, "offerer_stream", "a1"));
  mSessionOff.AddTrack(msta);

  RefPtr<JsepTrack> mstv1(
      new JsepTrack(SdpMediaSection::kVideo, "offerer_stream", "v1"));
  mSessionOff.AddTrack(mstv1);

  std::string offer = CreateOffer();

  SipccSdpParser parser;
  auto outputSdp = mozilla::Move(parser.Parse(offer));
  ASSERT_TRUE(outputSdp) << "Should have valid SDP" << std::endl
                         << "Errors were: " << GetParseErrors(parser);

  ASSERT_EQ(2U, outputSdp->GetMediaSectionCount());
  ASSERT_EQ(SdpMediaSection::kAudio,
            outputSdp->GetMediaSection(0).GetMediaType());
  ASSERT_EQ(SdpMediaSection::kVideo,
            outputSdp->GetMediaSection(1).GetMediaType());
}

TEST_F(JsepSessionTest, ValidateOfferedCodecParams)
{
  types.push_back(SdpMediaSection::kAudio);
  types.push_back(SdpMediaSection::kVideo);

  RefPtr<JsepTrack> msta(
      new JsepTrack(SdpMediaSection::kAudio, "offerer_stream", "a1"));
  mSessionOff.AddTrack(msta);
  RefPtr<JsepTrack> mstv1(
      new JsepTrack(SdpMediaSection::kVideo, "offerer_stream", "v2"));
  mSessionOff.AddTrack(mstv1);

  std::string offer = CreateOffer();

  SipccSdpParser parser;
  auto outputSdp = mozilla::Move(parser.Parse(offer));
  ASSERT_TRUE(outputSdp) << "Should have valid SDP" << std::endl
                         << "Errors were: " << GetParseErrors(parser);

  ASSERT_EQ(2U, outputSdp->GetMediaSectionCount());
  auto& video_section = outputSdp->GetMediaSection(1);
  ASSERT_EQ(SdpMediaSection::kVideo, video_section.GetMediaType());
  auto& video_attrs = video_section.GetAttributeList();
  ASSERT_EQ(SdpDirectionAttribute::kSendrecv, video_attrs.GetDirection());

  ASSERT_EQ(3U, video_section.GetFormats().size());
  ASSERT_EQ("120", video_section.GetFormats()[0]);
  ASSERT_EQ("126", video_section.GetFormats()[1]);
  ASSERT_EQ("97", video_section.GetFormats()[2]);

  // Validate rtpmap
  ASSERT_TRUE(video_attrs.HasAttribute(SdpAttribute::kRtpmapAttribute));
  auto& rtpmaps = video_attrs.GetRtpmap();
  ASSERT_TRUE(rtpmaps.HasEntry("120"));
  ASSERT_TRUE(rtpmaps.HasEntry("126"));
  ASSERT_TRUE(rtpmaps.HasEntry("97"));

  auto& vp8_entry = rtpmaps.GetEntry("120");
  auto& h264_1_entry = rtpmaps.GetEntry("126");
  auto& h264_0_entry = rtpmaps.GetEntry("97");

  ASSERT_EQ("VP8", vp8_entry.name);
  ASSERT_EQ("H264", h264_1_entry.name);
  ASSERT_EQ("H264", h264_0_entry.name);

  // Validate fmtps
  ASSERT_TRUE(video_attrs.HasAttribute(SdpAttribute::kFmtpAttribute));
  auto& fmtps = video_attrs.GetFmtp().mFmtps;

  ASSERT_EQ(3U, fmtps.size());

  // VP8
  ASSERT_EQ("120", fmtps[0].format);
  ASSERT_TRUE(fmtps[0].parameters);
  ASSERT_EQ(SdpRtpmapAttributeList::kVP8, fmtps[0].parameters->codec_type);

  auto& parsed_vp8_params =
      *static_cast<const SdpFmtpAttributeList::VP8Parameters*>(
          fmtps[0].parameters.get());

  ASSERT_EQ((uint32_t)12288, parsed_vp8_params.max_fs);
  ASSERT_EQ((uint32_t)60, parsed_vp8_params.max_fr);

  // H264 packetization mode 1
  ASSERT_EQ("126", fmtps[1].format);
  ASSERT_TRUE(fmtps[1].parameters);
  ASSERT_EQ(SdpRtpmapAttributeList::kH264, fmtps[1].parameters->codec_type);

  auto& parsed_h264_1_params =
      *static_cast<const SdpFmtpAttributeList::H264Parameters*>(
          fmtps[1].parameters.get());

  ASSERT_EQ((uint32_t)0x42e00d, parsed_h264_1_params.profile_level_id);
  ASSERT_TRUE(parsed_h264_1_params.level_asymmetry_allowed);
  ASSERT_EQ(1U, parsed_h264_1_params.packetization_mode);

  // H264 packetization mode 0
  ASSERT_EQ("97", fmtps[2].format);
  ASSERT_TRUE(fmtps[2].parameters);
  ASSERT_EQ(SdpRtpmapAttributeList::kH264, fmtps[2].parameters->codec_type);

  auto& parsed_h264_0_params =
      *static_cast<const SdpFmtpAttributeList::H264Parameters*>(
          fmtps[2].parameters.get());

  ASSERT_EQ((uint32_t)0x42e00d, parsed_h264_0_params.profile_level_id);
  ASSERT_TRUE(parsed_h264_0_params.level_asymmetry_allowed);
  ASSERT_EQ(0U, parsed_h264_0_params.packetization_mode);
}

TEST_F(JsepSessionTest, ValidateAnsweredCodecParams)
{

  for (auto i = mSessionAns.Codecs().begin(); i != mSessionAns.Codecs().end();
       ++i) {
    auto* codec = *i;
    if (codec->mName == "H264") {
      JsepVideoCodecDescription* h264 =
          static_cast<JsepVideoCodecDescription*>(codec);
      h264->mProfileLevelId = 0x42a00d;
      // Switch up the pts
      if (h264->mDefaultPt == "126") {
        h264->mDefaultPt = "97";
      } else {
        h264->mDefaultPt = "126";
      }
    }
  }

  types.push_back(SdpMediaSection::kAudio);
  types.push_back(SdpMediaSection::kVideo);

  RefPtr<JsepTrack> msta(
      new JsepTrack(SdpMediaSection::kAudio, "offerer_stream", "a1"));
  mSessionOff.AddTrack(msta);
  RefPtr<JsepTrack> mstv1(
      new JsepTrack(SdpMediaSection::kVideo, "offerer_stream", "v1"));
  mSessionOff.AddTrack(mstv1);

  std::string offer = CreateOffer();
  SetLocalOffer(offer);
  SetRemoteOffer(offer);

  RefPtr<JsepTrack> msta_ans(
      new JsepTrack(SdpMediaSection::kAudio, "answerer_stream", "a1"));
  mSessionAns.AddTrack(msta);
  RefPtr<JsepTrack> mstv1_ans(
      new JsepTrack(SdpMediaSection::kVideo, "answerer_stream", "v1"));
  mSessionAns.AddTrack(mstv1);

  std::string answer = CreateAnswer();

  SipccSdpParser parser;
  auto outputSdp = mozilla::Move(parser.Parse(answer));
  ASSERT_TRUE(outputSdp) << "Should have valid SDP" << std::endl
                         << "Errors were: " << GetParseErrors(parser);

  ASSERT_EQ(2U, outputSdp->GetMediaSectionCount());
  auto& video_section = outputSdp->GetMediaSection(1);
  ASSERT_EQ(SdpMediaSection::kVideo, video_section.GetMediaType());
  auto& video_attrs = video_section.GetAttributeList();
  ASSERT_EQ(SdpDirectionAttribute::kSendrecv, video_attrs.GetDirection());

  // TODO(bug 1099351): Once fixed, this stuff will need to be updated.
  ASSERT_EQ(1U, video_section.GetFormats().size());
  // ASSERT_EQ(3U, video_section.GetFormats().size());
  ASSERT_EQ("120", video_section.GetFormats()[0]);
  // ASSERT_EQ("126", video_section.GetFormats()[1]);
  // ASSERT_EQ("97", video_section.GetFormats()[2]);

  // Validate rtpmap
  ASSERT_TRUE(video_attrs.HasAttribute(SdpAttribute::kRtpmapAttribute));
  auto& rtpmaps = video_attrs.GetRtpmap();
  ASSERT_TRUE(rtpmaps.HasEntry("120"));
  // ASSERT_TRUE(rtpmaps.HasEntry("126"));
  // ASSERT_TRUE(rtpmaps.HasEntry("97"));

  auto& vp8_entry = rtpmaps.GetEntry("120");
  // auto& h264_1_entry = rtpmaps.GetEntry("126");
  // auto& h264_0_entry = rtpmaps.GetEntry("97");

  ASSERT_EQ("VP8", vp8_entry.name);
  // ASSERT_EQ("H264", h264_1_entry.name);
  // ASSERT_EQ("H264", h264_0_entry.name);

  // Validate fmtps
  ASSERT_TRUE(video_attrs.HasAttribute(SdpAttribute::kFmtpAttribute));
  auto& fmtps = video_attrs.GetFmtp().mFmtps;

  ASSERT_EQ(1U, fmtps.size());
  // ASSERT_EQ(3U, fmtps.size());

  // VP8
  ASSERT_EQ("120", fmtps[0].format);
  ASSERT_TRUE(fmtps[0].parameters);
  ASSERT_EQ(SdpRtpmapAttributeList::kVP8, fmtps[0].parameters->codec_type);

  auto& parsed_vp8_params =
      *static_cast<const SdpFmtpAttributeList::VP8Parameters*>(
          fmtps[0].parameters.get());

  ASSERT_EQ((uint32_t)12288, parsed_vp8_params.max_fs);
  ASSERT_EQ((uint32_t)60, parsed_vp8_params.max_fr);


  SetLocalAnswer(answer);
  SetRemoteAnswer(answer);

  ASSERT_EQ(2U, mSessionOff.GetNegotiatedTrackPairCount());
  const JsepTrackPair* offerVideoPair;
  ASSERT_EQ(NS_OK, mSessionOff.GetNegotiatedTrackPair(1, &offerVideoPair));
  ASSERT_TRUE(offerVideoPair->mSending);
  ASSERT_TRUE(offerVideoPair->mReceiving);
  ASSERT_TRUE(offerVideoPair->mSending->GetNegotiatedDetails());
  ASSERT_TRUE(offerVideoPair->mReceiving->GetNegotiatedDetails());
  ASSERT_EQ(1U,
      offerVideoPair->mSending->GetNegotiatedDetails()->GetCodecCount());
  ASSERT_EQ(1U,
      offerVideoPair->mReceiving->GetNegotiatedDetails()->GetCodecCount());
  const JsepCodecDescription* offerRecvCodec;
  ASSERT_EQ(NS_OK,
      offerVideoPair->mReceiving->GetNegotiatedDetails()->GetCodec(
        0,
        &offerRecvCodec));
  const JsepCodecDescription* offerSendCodec;
  ASSERT_EQ(NS_OK,
      offerVideoPair->mSending->GetNegotiatedDetails()->GetCodec(
        0,
        &offerSendCodec));

  ASSERT_EQ(2U, mSessionAns.GetNegotiatedTrackPairCount());
  const JsepTrackPair* answerVideoPair;
  ASSERT_EQ(NS_OK, mSessionAns.GetNegotiatedTrackPair(1, &answerVideoPair));
  ASSERT_TRUE(answerVideoPair->mSending);
  ASSERT_TRUE(answerVideoPair->mReceiving);
  ASSERT_TRUE(answerVideoPair->mSending->GetNegotiatedDetails());
  ASSERT_TRUE(answerVideoPair->mReceiving->GetNegotiatedDetails());
  ASSERT_EQ(1U,
      answerVideoPair->mSending->GetNegotiatedDetails()->GetCodecCount());
  ASSERT_EQ(1U,
      answerVideoPair->mReceiving->GetNegotiatedDetails()->GetCodecCount());
  const JsepCodecDescription* answerRecvCodec;
  ASSERT_EQ(NS_OK,
      answerVideoPair->mReceiving->GetNegotiatedDetails()->GetCodec(
        0,
        &answerRecvCodec));
  const JsepCodecDescription* answerSendCodec;
  ASSERT_EQ(NS_OK,
      answerVideoPair->mSending->GetNegotiatedDetails()->GetCodec(
        0,
        &answerSendCodec));

#if 0
  // H264 packetization mode 1
  ASSERT_EQ("126", fmtps[1].format);
  ASSERT_TRUE(fmtps[1].parameters);
  ASSERT_EQ(SdpRtpmapAttributeList::kH264, fmtps[1].parameters->codec_type);

  auto& parsed_h264_1_params =
    *static_cast<const SdpFmtpAttributeList::H264Parameters*>(
        fmtps[1].parameters.get());

  ASSERT_EQ((uint32_t)0x42a00d, parsed_h264_1_params.profile_level_id);
  ASSERT_TRUE(parsed_h264_1_params.level_asymmetry_allowed);
  ASSERT_EQ(1U, parsed_h264_1_params.packetization_mode);

  // H264 packetization mode 0
  ASSERT_EQ("97", fmtps[2].format);
  ASSERT_TRUE(fmtps[2].parameters);
  ASSERT_EQ(SdpRtpmapAttributeList::kH264, fmtps[2].parameters->codec_type);

  auto& parsed_h264_0_params =
    *static_cast<const SdpFmtpAttributeList::H264Parameters*>(
        fmtps[2].parameters.get());

  ASSERT_EQ((uint32_t)0x42a00d, parsed_h264_0_params.profile_level_id);
  ASSERT_TRUE(parsed_h264_0_params.level_asymmetry_allowed);
  ASSERT_EQ(0U, parsed_h264_0_params.packetization_mode);
#endif
}

TEST_P(JsepSessionTest, TestRejectMline)
{
  AddTracks(&mSessionOff);
  AddTracks(&mSessionAns);

  switch (types.front()) {
    case SdpMediaSection::kAudio:
      // Sabotage audio
      EnsureNegotiationFailure(types.front(), "opus");
      break;
    case SdpMediaSection::kVideo:
      // Sabotage video
      EnsureNegotiationFailure(types.front(), "H264");
      break;
    case SdpMediaSection::kApplication:
      // Sabotage datachannel
      EnsureNegotiationFailure(types.front(), "webrtc-datachannel");
      break;
    default:
      ASSERT_TRUE(false) << "Unknown media type";
  }

  std::string offer = CreateOffer();
  mSessionOff.SetLocalDescription(kJsepSdpOffer, offer);
  mSessionAns.SetRemoteDescription(kJsepSdpOffer, offer);

  std::string answer = CreateAnswer();

  SipccSdpParser parser;
  auto outputSdp = mozilla::Move(parser.Parse(answer));
  ASSERT_TRUE(outputSdp) << "Should have valid SDP" << std::endl
                         << "Errors were: " << GetParseErrors(parser);

  ASSERT_NE(0U, outputSdp->GetMediaSectionCount());
  SdpMediaSection* failed_section = nullptr;

  for (size_t i = 0; i < outputSdp->GetMediaSectionCount(); ++i) {
    if (outputSdp->GetMediaSection(i).GetMediaType() == types.front()) {
      failed_section = &outputSdp->GetMediaSection(i);
    }
  }

  ASSERT_TRUE(failed_section) << "Failed type was entirely absent from SDP";
  auto& failed_attrs = failed_section->GetAttributeList();
  ASSERT_EQ(SdpDirectionAttribute::kInactive, failed_attrs.GetDirection());
  ASSERT_EQ(0U, failed_section->GetPort());

  mSessionAns.SetLocalDescription(kJsepSdpAnswer, answer);
  mSessionOff.SetRemoteDescription(kJsepSdpAnswer, answer);

  ASSERT_EQ(types.size() - 1, mSessionOff.GetNegotiatedTrackPairCount());
  ASSERT_EQ(types.size() - 1, mSessionAns.GetNegotiatedTrackPairCount());

  ASSERT_EQ(types.size(), mSessionOff.GetTransportCount());
  ASSERT_EQ(types.size(), mSessionOff.GetLocalTrackCount());
  ASSERT_EQ(types.size() - 1, mSessionOff.GetRemoteTrackCount());

  ASSERT_EQ(types.size(), mSessionAns.GetTransportCount());
  ASSERT_EQ(types.size(), mSessionAns.GetLocalTrackCount());
  ASSERT_EQ(types.size(), mSessionAns.GetRemoteTrackCount());
}

TEST_F(JsepSessionTest, CreateOfferNoMlines)
{
  JsepOfferOptions options;
  std::string offer;
  nsresult rv = mSessionOff.CreateOffer(options, &offer);
  ASSERT_NE(NS_OK, rv);
  ASSERT_NE("", mSessionOff.GetLastError());
}

TEST_F(JsepSessionTest, TestIceLite)
{
  AddTracks(&mSessionOff, "audio");
  AddTracks(&mSessionAns, "audio");
  std::string offer = CreateOffer();
  SetLocalOffer(offer, CHECK_SUCCESS);

  SipccSdpParser parser;
  UniquePtr<Sdp> parsedOffer = parser.Parse(offer);
  parsedOffer->GetAttributeList().SetAttribute(
      new SdpFlagAttribute(SdpAttribute::kIceLiteAttribute));

  std::ostringstream os;
  parsedOffer->Serialize(os);
  SetRemoteOffer(os.str(), CHECK_SUCCESS);

  ASSERT_TRUE(mSessionAns.RemoteIsIceLite());
  ASSERT_FALSE(mSessionOff.RemoteIsIceLite());
}

TEST_F(JsepSessionTest, TestIceOptions)
{
  AddTracks(&mSessionOff, "audio");
  AddTracks(&mSessionAns, "audio");
  std::string offer = CreateOffer();
  SetLocalOffer(offer, CHECK_SUCCESS);
  SetRemoteOffer(offer, CHECK_SUCCESS);
  std::string answer = CreateAnswer();
  SetLocalAnswer(answer, CHECK_SUCCESS);
  SetRemoteAnswer(answer, CHECK_SUCCESS);

  ASSERT_EQ(1U, mSessionOff.GetIceOptions().size());
  ASSERT_EQ("trickle", mSessionOff.GetIceOptions()[0]);

  ASSERT_EQ(1U, mSessionAns.GetIceOptions().size());
  ASSERT_EQ("trickle", mSessionAns.GetIceOptions()[0]);
}

TEST_F(JsepSessionTest, TestExtmap)
{
  AddTracks(&mSessionOff, "audio");
  AddTracks(&mSessionAns, "audio");
  // ssrc-audio-level will be extmap 1 for both
  mSessionOff.AddAudioRtpExtension("foo"); // Default mapping of 2
  mSessionOff.AddAudioRtpExtension("bar"); // Default mapping of 3
  mSessionAns.AddAudioRtpExtension("bar"); // Default mapping of 2
  std::string offer = CreateOffer();
  SetLocalOffer(offer, CHECK_SUCCESS);
  SetRemoteOffer(offer, CHECK_SUCCESS);
  std::string answer = CreateAnswer();
  SetLocalAnswer(answer, CHECK_SUCCESS);
  SetRemoteAnswer(answer, CHECK_SUCCESS);

  SipccSdpParser parser;
  UniquePtr<Sdp> parsedOffer = parser.Parse(offer);
  ASSERT_EQ(1U, parsedOffer->GetMediaSectionCount());

  auto& offerMediaAttrs = parsedOffer->GetMediaSection(0).GetAttributeList();
  ASSERT_TRUE(offerMediaAttrs.HasAttribute(SdpAttribute::kExtmapAttribute));
  auto& offerExtmap = offerMediaAttrs.GetExtmap().mExtmaps;
  ASSERT_EQ(3U, offerExtmap.size());
  ASSERT_EQ("urn:ietf:params:rtp-hdrext:ssrc-audio-level",
      offerExtmap[0].extensionname);
  ASSERT_EQ(1U, offerExtmap[0].entry);
  ASSERT_EQ("foo", offerExtmap[1].extensionname);
  ASSERT_EQ(2U, offerExtmap[1].entry);
  ASSERT_EQ("bar", offerExtmap[2].extensionname);
  ASSERT_EQ(3U, offerExtmap[2].entry);

  UniquePtr<Sdp> parsedAnswer = parser.Parse(answer);
  ASSERT_EQ(1U, parsedAnswer->GetMediaSectionCount());

  auto& answerMediaAttrs = parsedAnswer->GetMediaSection(0).GetAttributeList();
  ASSERT_TRUE(answerMediaAttrs.HasAttribute(SdpAttribute::kExtmapAttribute));
  auto& answerExtmap = answerMediaAttrs.GetExtmap().mExtmaps;
  ASSERT_EQ(2U, answerExtmap.size());
  ASSERT_EQ("urn:ietf:params:rtp-hdrext:ssrc-audio-level",
      answerExtmap[0].extensionname);
  ASSERT_EQ(1U, answerExtmap[0].entry);
  // We ensure that the entry for "bar" matches what was in the offer
  ASSERT_EQ("bar", answerExtmap[1].extensionname);
  ASSERT_EQ(3U, answerExtmap[1].entry);
}

TEST_F(JsepSessionTest, TestRtcpFbStar)
{
  AddTracks(&mSessionOff, "video");
  AddTracks(&mSessionAns, "video");

  std::string offer = CreateOffer();

  SipccSdpParser parser;
  UniquePtr<Sdp> parsedOffer = parser.Parse(offer);
  auto* rtcpfbs = new SdpRtcpFbAttributeList;
  rtcpfbs->PushEntry("*", SdpRtcpFbAttributeList::kNack);
  parsedOffer->GetMediaSection(0).GetAttributeList().SetAttribute(rtcpfbs);
  offer = parsedOffer->ToString();

  SetLocalOffer(offer, CHECK_SUCCESS);
  SetRemoteOffer(offer, CHECK_SUCCESS);
  std::string answer = CreateAnswer();
  SetLocalAnswer(answer, CHECK_SUCCESS);
  SetRemoteAnswer(answer, CHECK_SUCCESS);

  ASSERT_EQ(1U, mSessionAns.GetRemoteTrackCount());
  RefPtr<JsepTrack> track;
  ASSERT_EQ(NS_OK, mSessionAns.GetRemoteTrack(0, &track));
  ASSERT_TRUE(track->GetNegotiatedDetails());
  auto* details = track->GetNegotiatedDetails();
  for (size_t i = 0; i < details->GetCodecCount(); ++i) {
    const JsepCodecDescription* codec;
    ASSERT_EQ(NS_OK, details->GetCodec(i, &codec));
    const JsepVideoCodecDescription* videoCodec =
      static_cast<const JsepVideoCodecDescription*>(codec);
    ASSERT_EQ(1U, videoCodec->mNackFbTypes.size());
    ASSERT_EQ("", videoCodec->mNackFbTypes[0]);
  }
}

} // namespace mozilla

int
main(int argc, char** argv)
{
  NSS_NoDB_Init(nullptr);
  NSS_SetDomesticPolicy();

  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
