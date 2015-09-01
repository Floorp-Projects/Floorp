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

#include "mtransport_test_utils.h"

namespace mozilla {
static std::string kAEqualsCandidate("a=candidate:");
const static size_t kNumCandidatesPerComponent = 3;

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
  AddTracks(JsepSessionImpl& side)
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
  AddTracks(JsepSessionImpl& side, const std::string& mediatypes)
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
  AddTracks(JsepSessionImpl& side,
            const std::vector<SdpMediaSection::MediaType>& mediatypes)
  {
    FakeUuidGenerator uuid_gen;
    std::string stream_id;
    std::string track_id;

    ASSERT_TRUE(uuid_gen.Generate(&stream_id));

    AddTracksToStream(side, stream_id, mediatypes);
  }

  void
  AddTracksToStream(JsepSessionImpl& side,
                    const std::string stream_id,
                    const std::string& mediatypes)
  {
    AddTracksToStream(side, stream_id, BuildTypes(mediatypes));
  }

  void
  AddTracksToStream(JsepSessionImpl& side,
                    const std::string stream_id,
                    const std::vector<SdpMediaSection::MediaType>& mediatypes)

  {
    FakeUuidGenerator uuid_gen;
    std::string track_id;

    for (auto track = mediatypes.begin(); track != mediatypes.end(); ++track) {
      ASSERT_TRUE(uuid_gen.Generate(&track_id));

      RefPtr<JsepTrack> mst(new JsepTrack(*track, stream_id, track_id));
      side.AddTrack(mst);
    }
  }

  bool HasMediaStream(std::vector<RefPtr<JsepTrack>> tracks) const {
    for (auto i = tracks.begin(); i != tracks.end(); ++i) {
      if ((*i)->GetMediaType() != SdpMediaSection::kApplication) {
        return 1;
      }
    }
    return 0;
  }

  const std::string GetFirstLocalStreamId(JsepSessionImpl& side) const {
    auto tracks = side.GetLocalTracks();
    return (*tracks.begin())->GetStreamId();
  }

  std::vector<std::string>
  GetMediaStreamIds(std::vector<RefPtr<JsepTrack>> tracks) const {
    std::vector<std::string> ids;
    for (auto i = tracks.begin(); i != tracks.end(); ++i) {
      // data channels don't have msid's
      if ((*i)->GetMediaType() == SdpMediaSection::kApplication) {
        continue;
      }
      ids.push_back((*i)->GetStreamId());
    }
    return ids;
  }

  std::vector<std::string>
  GetLocalMediaStreamIds(JsepSessionImpl& side) const {
    return GetMediaStreamIds(side.GetLocalTracks());
  }

  std::vector<std::string>
  GetRemoteMediaStreamIds(JsepSessionImpl& side) const {
    return GetMediaStreamIds(side.GetRemoteTracks());
  }

  std::vector<std::string>
  sortUniqueStrVector(std::vector<std::string> in) const {
    std::sort(in.begin(), in.end());
    auto it = std::unique(in.begin(), in.end());
    in.resize( std::distance(in.begin(), it));
    return in;
  }

  std::vector<std::string>
  GetLocalUniqueStreamIds(JsepSessionImpl& side) const {
    return sortUniqueStrVector(GetLocalMediaStreamIds(side));
  }

  std::vector<std::string>
  GetRemoteUniqueStreamIds(JsepSessionImpl& side) const {
    return sortUniqueStrVector(GetRemoteMediaStreamIds(side));
  }

  RefPtr<JsepTrack> GetTrack(JsepSessionImpl& side,
                             SdpMediaSection::MediaType type,
                             size_t index) const {
    auto tracks = side.GetLocalTracks();

    for (auto i = tracks.begin(); i != tracks.end(); ++i) {
      if ((*i)->GetMediaType() != type) {
        continue;
      }

      if (index != 0) {
        --index;
        continue;
      }

      return *i;
    }

    return RefPtr<JsepTrack>(nullptr);
  }

  RefPtr<JsepTrack> GetTrackOff(size_t index,
                                SdpMediaSection::MediaType type) {
    return GetTrack(mSessionOff, type, index);
  }

  RefPtr<JsepTrack> GetTrackAns(size_t index,
                                SdpMediaSection::MediaType type) {
    return GetTrack(mSessionAns, type, index);
  }

  class ComparePairsByLevel {
    public:
      bool operator()(const JsepTrackPair& lhs,
                      const JsepTrackPair& rhs) const {
        return lhs.mLevel < rhs.mLevel;
      }
  };

  std::vector<JsepTrackPair> GetTrackPairsByLevel(JsepSessionImpl& side) const {
    auto pairs = side.GetNegotiatedTrackPairs();
    std::sort(pairs.begin(), pairs.end(), ComparePairsByLevel());
    return pairs;
  }

  bool Equals(const JsepTrackPair& p1,
              const JsepTrackPair& p2) const {
    if (p1.mLevel != p2.mLevel) {
      return false;
    }

    // We don't check things like mBundleLevel, since that can change without
    // any changes to the transport, which is what we're really interested in.

    if (p1.mSending.get() != p2.mSending.get()) {
      return false;
    }

    if (p1.mReceiving.get() != p2.mReceiving.get()) {
      return false;
    }

    if (p1.mRtpTransport.get() != p2.mRtpTransport.get()) {
      return false;
    }

    if (p1.mRtcpTransport.get() != p2.mRtcpTransport.get()) {
      return false;
    }

    return true;
  }

  size_t GetTrackCount(JsepSessionImpl& side,
                       SdpMediaSection::MediaType type) const {
    auto tracks = side.GetLocalTracks();
    size_t result = 0;
    for (auto i = tracks.begin(); i != tracks.end(); ++i) {
      if ((*i)->GetMediaType() == type) {
        ++result;
      }
    }
    return result;
  }

  UniquePtr<Sdp> GetParsedLocalDescription(const JsepSessionImpl& side) const {
    return Parse(side.GetLocalDescription());
  }

  SdpMediaSection* GetMsection(Sdp& sdp,
                               SdpMediaSection::MediaType type,
                               size_t index) const {
    for (size_t i = 0; i < sdp.GetMediaSectionCount(); ++i) {
      auto& msection = sdp.GetMediaSection(i);
      if (msection.GetMediaType() != type) {
        continue;
      }

      if (index) {
        --index;
        continue;
      }

      return &msection;
    }

    return nullptr;
  }

  void
  SetPayloadTypeNumber(JsepSession& session,
                       const std::string& codecName,
                       const std::string& payloadType)
  {
    for (auto* codec : session.Codecs()) {
      if (codec->mName == codecName) {
        codec->mDefaultPt = payloadType;
      }
    }
  }

  void
  SetCodecEnabled(JsepSession& session,
                  const std::string& codecName,
                  bool enabled)
  {
    for (auto* codec : session.Codecs()) {
      if (codec->mName == codecName) {
        codec->mEnabled = enabled;
      }
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

  void OfferAnswer(uint32_t checkFlags = ALL_CHECKS,
                   const Maybe<JsepOfferOptions> options = Nothing()) {
    std::string offer = CreateOffer(options);
    SetLocalOffer(offer, checkFlags);
    SetRemoteOffer(offer, checkFlags);

    std::string answer = CreateAnswer();
    SetLocalAnswer(answer, checkFlags);
    SetRemoteAnswer(answer, checkFlags);
  }

  void
  SetLocalOffer(const std::string& offer, uint32_t checkFlags = ALL_CHECKS)
  {
    nsresult rv = mSessionOff.SetLocalDescription(kJsepSdpOffer, offer);

    if (checkFlags & CHECK_SUCCESS) {
      ASSERT_EQ(NS_OK, rv);
    }

    if (checkFlags & CHECK_TRACKS) {
      // Check that the transports exist.
      ASSERT_EQ(types.size(), mSessionOff.GetTransports().size());
      auto tracks = mSessionOff.GetLocalTracks();
      for (size_t i = 0; i < types.size(); ++i) {
        ASSERT_NE("", tracks[i]->GetStreamId());
        ASSERT_NE("", tracks[i]->GetTrackId());
        if (tracks[i]->GetMediaType() != SdpMediaSection::kApplication) {
          std::string msidAttr("a=msid:");
          msidAttr += tracks[i]->GetStreamId();
          msidAttr += " ";
          msidAttr += tracks[i]->GetTrackId();
          ASSERT_NE(std::string::npos, offer.find(msidAttr))
            << "Did not find " << msidAttr << " in offer";
        }
      }
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
      auto tracks = mSessionAns.GetRemoteTracks();
      // Now verify that the right stuff is in the tracks.
      ASSERT_EQ(types.size(), tracks.size());
      for (size_t i = 0; i < tracks.size(); ++i) {
        ASSERT_EQ(types[i], tracks[i]->GetMediaType());
        ASSERT_NE("", tracks[i]->GetStreamId());
        ASSERT_NE("", tracks[i]->GetTrackId());
        if (tracks[i]->GetMediaType() != SdpMediaSection::kApplication) {
          std::string msidAttr("a=msid:");
          msidAttr += tracks[i]->GetStreamId();
          msidAttr += " ";
          msidAttr += tracks[i]->GetTrackId();
          ASSERT_NE(std::string::npos, offer.find(msidAttr))
            << "Did not find " << msidAttr << " in offer";
        }
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
      auto pairs = mSessionAns.GetNegotiatedTrackPairs();
      ASSERT_EQ(types.size(), pairs.size());
      for (size_t i = 0; i < types.size(); ++i) {
        ASSERT_TRUE(pairs[i].mSending);
        ASSERT_EQ(types[i], pairs[i].mSending->GetMediaType());
        ASSERT_TRUE(pairs[i].mReceiving);
        ASSERT_EQ(types[i], pairs[i].mReceiving->GetMediaType());
        ASSERT_NE("", pairs[i].mSending->GetStreamId());
        ASSERT_NE("", pairs[i].mSending->GetTrackId());
        // These might have been in the SDP, or might have been randomly
        // chosen by JsepSessionImpl
        ASSERT_NE("", pairs[i].mReceiving->GetStreamId());
        ASSERT_NE("", pairs[i].mReceiving->GetTrackId());

        if (pairs[i].mReceiving->GetMediaType() != SdpMediaSection::kApplication) {
          std::string msidAttr("a=msid:");
          msidAttr += pairs[i].mSending->GetStreamId();
          msidAttr += " ";
          msidAttr += pairs[i].mSending->GetTrackId();
          ASSERT_NE(std::string::npos, answer.find(msidAttr))
            << "Did not find " << msidAttr << " in offer";
        }
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
      auto pairs = mSessionOff.GetNegotiatedTrackPairs();
      ASSERT_EQ(types.size(), pairs.size());
      for (size_t i = 0; i < types.size(); ++i) {
        ASSERT_TRUE(pairs[i].mSending);
        ASSERT_EQ(types[i], pairs[i].mSending->GetMediaType());
        ASSERT_TRUE(pairs[i].mReceiving);
        ASSERT_EQ(types[i], pairs[i].mReceiving->GetMediaType());
        ASSERT_NE("", pairs[i].mSending->GetStreamId());
        ASSERT_NE("", pairs[i].mSending->GetTrackId());
        // These might have been in the SDP, or might have been randomly
        // chosen by JsepSessionImpl
        ASSERT_NE("", pairs[i].mReceiving->GetStreamId());
        ASSERT_NE("", pairs[i].mReceiving->GetTrackId());

        if (pairs[i].mReceiving->GetMediaType() != SdpMediaSection::kApplication) {
          std::string msidAttr("a=msid:");
          msidAttr += pairs[i].mReceiving->GetStreamId();
          msidAttr += " ";
          msidAttr += pairs[i].mReceiving->GetTrackId();
          ASSERT_NE(std::string::npos, answer.find(msidAttr))
            << "Did not find " << msidAttr << " in offer";
        }
      }
    }
    DumpTrackPairs(mSessionAns);
  }

  typedef enum {
    RTP = 1,
    RTCP = 2
  } ComponentType;

  class CandidateSet {
    public:
      CandidateSet() {}

      void Gather(JsepSession& session,
                  const std::vector<SdpMediaSection::MediaType>& types,
                  ComponentType maxComponent = RTCP)
      {
        for (size_t level = 0; level < types.size(); ++level) {
          Gather(session, level, RTP);
          if (types[level] != SdpMediaSection::kApplication &&
              maxComponent == RTCP) {
            Gather(session, level, RTCP);
          }
        }
        FinishGathering(session);
      }

      void Gather(JsepSession& session, size_t level, ComponentType component)
      {
        static uint16_t port = 1000;
        std::vector<std::string> candidates;
        for (size_t i = 0; i < kNumCandidatesPerComponent; ++i) {
          ++port;
          std::ostringstream candidate;
          candidate << "0 " << static_cast<uint16_t>(component)
                    << " UDP 9999 192.168.0.1 " << port << " typ host";
          std::string mid;
          bool skipped;
          session.AddLocalIceCandidate(kAEqualsCandidate + candidate.str(),
                                       level, &mid, &skipped);
          if (!skipped) {
            // TODO (bug 1095793): Need to add mid to mCandidatesToTrickle
            mCandidatesToTrickle.push_back(
                std::pair<uint16_t, std::string>(
                  level, kAEqualsCandidate + candidate.str()));
            candidates.push_back(candidate.str());
          }
        }

        // Stomp existing candidates
        mCandidates[level][component] = candidates;

        // Stomp existing defaults
        mDefaultCandidates[level][component] =
          std::make_pair("192.168.0.1", port);
      }

      void FinishGathering(JsepSession& session) const
      {
        // Copy so we can be terse and use []
        for (auto levelAndCandidates : mDefaultCandidates) {
          ASSERT_EQ(1U, levelAndCandidates.second.count(RTP));
          session.EndOfLocalCandidates(
              levelAndCandidates.second[RTP].first,
              levelAndCandidates.second[RTP].second,
              // Will be empty string if not present, which is how we indicate
              // that there is no default for RTCP
              levelAndCandidates.second[RTCP].first,
              levelAndCandidates.second[RTCP].second,
              levelAndCandidates.first);
        }
      }

      void Trickle(JsepSession& session)
      {
        for (const auto& levelAndCandidate : mCandidatesToTrickle) {
          session.AddRemoteIceCandidate(levelAndCandidate.second,
                                        "",
                                        levelAndCandidate.first);
        }
        mCandidatesToTrickle.clear();
      }

      void CheckRtpCandidates(bool expectRtpCandidates,
                              const SdpMediaSection& msection,
                              size_t transportLevel,
                              const std::string& context) const
      {
        auto& attrs = msection.GetAttributeList();

        ASSERT_EQ(expectRtpCandidates,
                  attrs.HasAttribute(SdpAttribute::kCandidateAttribute))
          << context << " (level " << msection.GetLevel() << ")";

        if (expectRtpCandidates) {
          // Copy so we can be terse and use []
          auto expectedCandidates = mCandidates;
          ASSERT_LE(kNumCandidatesPerComponent,
                    expectedCandidates[transportLevel][RTP].size());

          auto& candidates = attrs.GetCandidate();
          ASSERT_LE(kNumCandidatesPerComponent, candidates.size())
            << context << " (level " << msection.GetLevel() << ")";
          for (size_t i = 0; i < kNumCandidatesPerComponent; ++i) {
            ASSERT_EQ(expectedCandidates[transportLevel][RTP][i], candidates[i])
              << context << " (level " << msection.GetLevel() << ")";
          }
        }
      }

      void CheckRtcpCandidates(bool expectRtcpCandidates,
                               const SdpMediaSection& msection,
                               size_t transportLevel,
                               const std::string& context) const
      {
        auto& attrs = msection.GetAttributeList();

        if (expectRtcpCandidates) {
          // Copy so we can be terse and use []
          auto expectedCandidates = mCandidates;
          ASSERT_LE(kNumCandidatesPerComponent,
                    expectedCandidates[transportLevel][RTCP].size());

          ASSERT_TRUE(attrs.HasAttribute(SdpAttribute::kCandidateAttribute))
            << context << " (level " << msection.GetLevel() << ")";
          auto& candidates = attrs.GetCandidate();
          ASSERT_EQ(kNumCandidatesPerComponent * 2, candidates.size())
            << context << " (level " << msection.GetLevel() << ")";
          for (size_t i = 0; i < kNumCandidatesPerComponent; ++i) {
            ASSERT_EQ(expectedCandidates[transportLevel][RTCP][i],
                      candidates[i + kNumCandidatesPerComponent])
              << context << " (level " << msection.GetLevel() << ")";
          }
        }
      }

      void CheckDefaultRtpCandidate(bool expectDefault,
                                    const SdpMediaSection& msection,
                                    size_t transportLevel,
                                    const std::string& context) const
      {
        if (expectDefault) {
          // Copy so we can be terse and use []
          auto defaultCandidates = mDefaultCandidates;
          ASSERT_EQ(defaultCandidates[transportLevel][RTP].first,
                    msection.GetConnection().GetAddress())
            << context << " (level " << msection.GetLevel() << ")";
          ASSERT_EQ(defaultCandidates[transportLevel][RTP].second,
                    msection.GetPort())
            << context << " (level " << msection.GetLevel() << ")";
        } else {
          ASSERT_EQ("0.0.0.0", msection.GetConnection().GetAddress())
            << context << " (level " << msection.GetLevel() << ")";
          ASSERT_EQ(9U, msection.GetPort())
            << context << " (level " << msection.GetLevel() << ")";
        }
      }

      void CheckDefaultRtcpCandidate(bool expectDefault,
                                     const SdpMediaSection& msection,
                                     size_t transportLevel,
                                     const std::string& context) const
      {
        if (expectDefault) {
          // Copy so we can be terse and use []
          auto defaultCandidates = mDefaultCandidates;
          ASSERT_TRUE(msection.GetAttributeList().HasAttribute(
                SdpAttribute::kRtcpAttribute))
            << context << " (level " << msection.GetLevel() << ")";
          auto& rtcpAttr = msection.GetAttributeList().GetRtcp();
          ASSERT_EQ(defaultCandidates[transportLevel][RTCP].second,
                    rtcpAttr.mPort)
            << context << " (level " << msection.GetLevel() << ")";
          ASSERT_EQ(sdp::kInternet, rtcpAttr.mNetType)
            << context << " (level " << msection.GetLevel() << ")";
          ASSERT_EQ(sdp::kIPv4, rtcpAttr.mAddrType)
            << context << " (level " << msection.GetLevel() << ")";
          ASSERT_EQ(defaultCandidates[transportLevel][RTCP].first,
                    rtcpAttr.mAddress)
            << context << " (level " << msection.GetLevel() << ")";
        } else {
          ASSERT_FALSE(msection.GetAttributeList().HasAttribute(
                SdpAttribute::kRtcpAttribute))
            << context << " (level " << msection.GetLevel() << ")";
        }
      }

    private:
      typedef size_t Level;
      typedef std::string Candidate;
      typedef std::string Address;
      typedef uint16_t Port;
      // Default candidates are put into the m-line, c-line, and rtcp
      // attribute for endpoints that don't support ICE.
      std::map<Level,
               std::map<ComponentType,
                        std::pair<Address, Port>>> mDefaultCandidates;
      std::map<Level,
               std::map<ComponentType,
                        std::vector<Candidate>>> mCandidates;
      // Level/candidate pairs that need to be trickled
      std::vector<std::pair<Level, Candidate>> mCandidatesToTrickle;
  };

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

  void CheckEndOfCandidates(bool expectEoc,
                            const SdpMediaSection& msection,
                            const std::string& context)
  {
    if (expectEoc) {
      ASSERT_TRUE(msection.GetAttributeList().HasAttribute(
            SdpAttribute::kEndOfCandidatesAttribute))
        << context << " (level " << msection.GetLevel() << ")";
    } else {
      ASSERT_FALSE(msection.GetAttributeList().HasAttribute(
            SdpAttribute::kEndOfCandidatesAttribute))
        << context << " (level " << msection.GetLevel() << ")";
    }
  }

  void CheckPairs(const JsepSession& session, const std::string& context)
  {
    auto pairs = session.GetNegotiatedTrackPairs();

    for (JsepTrackPair& pair : pairs) {
      if (types.size() == 1) {
        ASSERT_FALSE(pair.mBundleLevel.isSome()) << context;
      } else {
        ASSERT_TRUE(pair.mBundleLevel.isSome()) << context;
        ASSERT_EQ(0U, *pair.mBundleLevel) << context;
      }
    }
  }

  void
  DisableMsid(std::string* sdp) const {
    size_t pos = sdp->find("a=msid-semantic");
    ASSERT_NE(std::string::npos, pos);
    (*sdp)[pos + 2] = 'X'; // garble, a=Xsid-semantic
  }

  void
  DisableBundle(std::string* sdp) const {
    size_t pos = sdp->find("a=group:BUNDLE");
    ASSERT_NE(std::string::npos, pos);
    (*sdp)[pos + 11] = 'G'; // garble, a=group:BUNGLE
  }

  void
  DisableMsection(std::string* sdp, size_t level) const {
    UniquePtr<Sdp> parsed(Parse(*sdp));
    ASSERT_TRUE(parsed.get());
    ASSERT_LT(level, parsed->GetMediaSectionCount());
    parsed->GetMediaSection(level).SetPort(0);

    auto& attrs = parsed->GetMediaSection(level).GetAttributeList();

    ASSERT_TRUE(attrs.HasAttribute(SdpAttribute::kMidAttribute));
    std::string mid = attrs.GetMid();

    attrs.Clear();

    ASSERT_TRUE(
        parsed->GetAttributeList().HasAttribute(SdpAttribute::kGroupAttribute));

    SdpGroupAttributeList* newGroupAttr(new SdpGroupAttributeList(
          parsed->GetAttributeList().GetGroup()));
    newGroupAttr->RemoveMid(mid);
    parsed->GetAttributeList().SetAttribute(newGroupAttr);
    (*sdp) = parsed->ToString();
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
    auto pairs = mSessionAns.GetNegotiatedTrackPairs();
    for (auto i = pairs.begin(); i != pairs.end(); ++i) {
      std::cerr << "Track pair " << i->mLevel << std::endl;
      if (i->mSending) {
        std::cerr << "Sending-->" << std::endl;
        DumpTrack(*i->mSending);
      }
      if (i->mReceiving) {
        std::cerr << "Receiving-->" << std::endl;
        DumpTrack(*i->mReceiving);
      }
    }
  }

  UniquePtr<Sdp>
  Parse(const std::string& sdp) const
  {
    SipccSdpParser parser;
    UniquePtr<Sdp> parsed = parser.Parse(sdp);
    EXPECT_TRUE(parsed.get()) << "Should have valid SDP" << std::endl
                              << "Errors were: " << GetParseErrors(parser);
    return parsed;
  }

  JsepSessionImpl mSessionOff;
  CandidateSet mOffCandidates;
  JsepSessionImpl mSessionAns;
  CandidateSet mAnsCandidates;
  std::vector<SdpMediaSection::MediaType> types;
  std::vector<std::pair<std::string, uint16_t>> mGatheredCandidates;

private:
  void
  ValidateTransport(TransportData& source, const std::string& sdp_str)
  {
    UniquePtr<Sdp> sdp(Parse(sdp_str));
    ASSERT_TRUE(!!sdp);
    size_t num_m_sections = sdp->GetMediaSectionCount();
    for (size_t i = 0; i < num_m_sections; ++i) {
      auto& msection = sdp->GetMediaSection(i);

      if (msection.GetMediaType() == SdpMediaSection::kApplication) {
        ASSERT_EQ(SdpMediaSection::kDtlsSctp, msection.GetProtocol());
      } else {
        ASSERT_EQ(SdpMediaSection::kUdpTlsRtpSavpf, msection.GetProtocol());
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
  AddTracks(mSessionOff);
  CreateOffer();
}

TEST_P(JsepSessionTest, CreateOfferSetLocal)
{
  AddTracks(mSessionOff);
  std::string offer = CreateOffer();
  SetLocalOffer(offer);
}

TEST_P(JsepSessionTest, CreateOfferSetLocalSetRemote)
{
  AddTracks(mSessionOff);
  std::string offer = CreateOffer();
  SetLocalOffer(offer);
  SetRemoteOffer(offer);
}

TEST_P(JsepSessionTest, CreateOfferSetLocalSetRemoteCreateAnswer)
{
  AddTracks(mSessionOff);
  std::string offer = CreateOffer();
  SetLocalOffer(offer);
  SetRemoteOffer(offer);
  AddTracks(mSessionAns);
  std::string answer = CreateAnswer();
}

TEST_P(JsepSessionTest, CreateOfferSetLocalSetRemoteCreateAnswerSetLocal)
{
  AddTracks(mSessionOff);
  std::string offer = CreateOffer();
  SetLocalOffer(offer);
  SetRemoteOffer(offer);
  AddTracks(mSessionAns);
  std::string answer = CreateAnswer();
  SetLocalAnswer(answer);
}

TEST_P(JsepSessionTest, FullCall)
{
  AddTracks(mSessionOff);
  std::string offer = CreateOffer();
  SetLocalOffer(offer);
  SetRemoteOffer(offer);
  AddTracks(mSessionAns);
  std::string answer = CreateAnswer();
  SetLocalAnswer(answer);
  SetRemoteAnswer(answer);
}

TEST_P(JsepSessionTest, RenegotiationNoChange)
{
  AddTracks(mSessionOff);
  std::string offer = CreateOffer();
  SetLocalOffer(offer);
  SetRemoteOffer(offer);

  auto added = mSessionAns.GetRemoteTracksAdded();
  auto removed = mSessionAns.GetRemoteTracksRemoved();
  ASSERT_EQ(types.size(), added.size());
  ASSERT_EQ(0U, removed.size());

  AddTracks(mSessionAns);
  std::string answer = CreateAnswer();
  SetLocalAnswer(answer);
  SetRemoteAnswer(answer);

  added = mSessionOff.GetRemoteTracksAdded();
  removed = mSessionOff.GetRemoteTracksRemoved();
  ASSERT_EQ(types.size(), added.size());
  ASSERT_EQ(0U, removed.size());

  auto offererPairs = GetTrackPairsByLevel(mSessionOff);
  auto answererPairs = GetTrackPairsByLevel(mSessionAns);

  std::string reoffer = CreateOffer();
  SetLocalOffer(reoffer);
  SetRemoteOffer(reoffer);

  added = mSessionAns.GetRemoteTracksAdded();
  removed = mSessionAns.GetRemoteTracksRemoved();
  ASSERT_EQ(0U, added.size());
  ASSERT_EQ(0U, removed.size());

  std::string reanswer = CreateAnswer();
  SetLocalAnswer(reanswer);
  SetRemoteAnswer(reanswer);

  added = mSessionOff.GetRemoteTracksAdded();
  removed = mSessionOff.GetRemoteTracksRemoved();
  ASSERT_EQ(0U, added.size());
  ASSERT_EQ(0U, removed.size());

  auto newOffererPairs = GetTrackPairsByLevel(mSessionOff);
  auto newAnswererPairs = GetTrackPairsByLevel(mSessionAns);

  ASSERT_EQ(offererPairs.size(), newOffererPairs.size());
  for (size_t i = 0; i < offererPairs.size(); ++i) {
    ASSERT_TRUE(Equals(offererPairs[i], newOffererPairs[i]));
  }

  ASSERT_EQ(answererPairs.size(), newAnswererPairs.size());
  for (size_t i = 0; i < answererPairs.size(); ++i) {
    ASSERT_TRUE(Equals(answererPairs[i], newAnswererPairs[i]));
  }
}

TEST_P(JsepSessionTest, RenegotiationOffererAddsTrack)
{
  AddTracks(mSessionOff);
  AddTracks(mSessionAns);

  OfferAnswer();

  auto offererPairs = GetTrackPairsByLevel(mSessionOff);
  auto answererPairs = GetTrackPairsByLevel(mSessionAns);

  std::vector<SdpMediaSection::MediaType> extraTypes;
  extraTypes.push_back(SdpMediaSection::kAudio);
  extraTypes.push_back(SdpMediaSection::kVideo);
  AddTracks(mSessionOff, extraTypes);
  types.insert(types.end(), extraTypes.begin(), extraTypes.end());

  OfferAnswer(CHECK_SUCCESS);

  auto added = mSessionAns.GetRemoteTracksAdded();
  auto removed = mSessionAns.GetRemoteTracksRemoved();
  ASSERT_EQ(2U, added.size());
  ASSERT_EQ(0U, removed.size());
  ASSERT_EQ(SdpMediaSection::kAudio, added[0]->GetMediaType());
  ASSERT_EQ(SdpMediaSection::kVideo, added[1]->GetMediaType());

  added = mSessionOff.GetRemoteTracksAdded();
  removed = mSessionOff.GetRemoteTracksRemoved();
  ASSERT_EQ(0U, added.size());
  ASSERT_EQ(0U, removed.size());

  auto newOffererPairs = GetTrackPairsByLevel(mSessionOff);
  auto newAnswererPairs = GetTrackPairsByLevel(mSessionAns);

  ASSERT_EQ(offererPairs.size() + 2, newOffererPairs.size());
  for (size_t i = 0; i < offererPairs.size(); ++i) {
    ASSERT_TRUE(Equals(offererPairs[i], newOffererPairs[i]));
  }

  ASSERT_EQ(answererPairs.size() + 2, newAnswererPairs.size());
  for (size_t i = 0; i < answererPairs.size(); ++i) {
    ASSERT_TRUE(Equals(answererPairs[i], newAnswererPairs[i]));
  }
}

TEST_P(JsepSessionTest, RenegotiationAnswererAddsTrack)
{
  AddTracks(mSessionOff);
  AddTracks(mSessionAns);

  OfferAnswer();

  auto offererPairs = GetTrackPairsByLevel(mSessionOff);
  auto answererPairs = GetTrackPairsByLevel(mSessionAns);

  std::vector<SdpMediaSection::MediaType> extraTypes;
  extraTypes.push_back(SdpMediaSection::kAudio);
  extraTypes.push_back(SdpMediaSection::kVideo);
  AddTracks(mSessionAns, extraTypes);
  types.insert(types.end(), extraTypes.begin(), extraTypes.end());

  // We need to add a recvonly m-section to the offer for this to work
  JsepOfferOptions options;
  options.mOfferToReceiveAudio =
    Some(GetTrackCount(mSessionOff, SdpMediaSection::kAudio) + 1);
  options.mOfferToReceiveVideo =
    Some(GetTrackCount(mSessionOff, SdpMediaSection::kVideo) + 1);

  std::string offer = CreateOffer(Some(options));
  SetLocalOffer(offer, CHECK_SUCCESS);
  SetRemoteOffer(offer, CHECK_SUCCESS);

  std::string answer = CreateAnswer();
  SetLocalAnswer(answer, CHECK_SUCCESS);
  SetRemoteAnswer(answer, CHECK_SUCCESS);

  auto added = mSessionAns.GetRemoteTracksAdded();
  auto removed = mSessionAns.GetRemoteTracksRemoved();
  ASSERT_EQ(0U, added.size());
  ASSERT_EQ(0U, removed.size());

  added = mSessionOff.GetRemoteTracksAdded();
  removed = mSessionOff.GetRemoteTracksRemoved();
  ASSERT_EQ(2U, added.size());
  ASSERT_EQ(0U, removed.size());
  ASSERT_EQ(SdpMediaSection::kAudio, added[0]->GetMediaType());
  ASSERT_EQ(SdpMediaSection::kVideo, added[1]->GetMediaType());

  auto newOffererPairs = GetTrackPairsByLevel(mSessionOff);
  auto newAnswererPairs = GetTrackPairsByLevel(mSessionAns);

  ASSERT_EQ(offererPairs.size() + 2, newOffererPairs.size());
  for (size_t i = 0; i < offererPairs.size(); ++i) {
    ASSERT_TRUE(Equals(offererPairs[i], newOffererPairs[i]));
  }

  ASSERT_EQ(answererPairs.size() + 2, newAnswererPairs.size());
  for (size_t i = 0; i < answererPairs.size(); ++i) {
    ASSERT_TRUE(Equals(answererPairs[i], newAnswererPairs[i]));
  }
}

TEST_P(JsepSessionTest, RenegotiationBothAddTrack)
{
  AddTracks(mSessionOff);
  AddTracks(mSessionAns);

  OfferAnswer();

  auto offererPairs = GetTrackPairsByLevel(mSessionOff);
  auto answererPairs = GetTrackPairsByLevel(mSessionAns);

  std::vector<SdpMediaSection::MediaType> extraTypes;
  extraTypes.push_back(SdpMediaSection::kAudio);
  extraTypes.push_back(SdpMediaSection::kVideo);
  AddTracks(mSessionAns, extraTypes);
  AddTracks(mSessionOff, extraTypes);
  types.insert(types.end(), extraTypes.begin(), extraTypes.end());

  OfferAnswer(CHECK_SUCCESS);

  auto added = mSessionAns.GetRemoteTracksAdded();
  auto removed = mSessionAns.GetRemoteTracksRemoved();
  ASSERT_EQ(2U, added.size());
  ASSERT_EQ(0U, removed.size());
  ASSERT_EQ(SdpMediaSection::kAudio, added[0]->GetMediaType());
  ASSERT_EQ(SdpMediaSection::kVideo, added[1]->GetMediaType());

  added = mSessionOff.GetRemoteTracksAdded();
  removed = mSessionOff.GetRemoteTracksRemoved();
  ASSERT_EQ(2U, added.size());
  ASSERT_EQ(0U, removed.size());
  ASSERT_EQ(SdpMediaSection::kAudio, added[0]->GetMediaType());
  ASSERT_EQ(SdpMediaSection::kVideo, added[1]->GetMediaType());

  auto newOffererPairs = GetTrackPairsByLevel(mSessionOff);
  auto newAnswererPairs = GetTrackPairsByLevel(mSessionAns);

  ASSERT_EQ(offererPairs.size() + 2, newOffererPairs.size());
  for (size_t i = 0; i < offererPairs.size(); ++i) {
    ASSERT_TRUE(Equals(offererPairs[i], newOffererPairs[i]));
  }

  ASSERT_EQ(answererPairs.size() + 2, newAnswererPairs.size());
  for (size_t i = 0; i < answererPairs.size(); ++i) {
    ASSERT_TRUE(Equals(answererPairs[i], newAnswererPairs[i]));
  }
}

TEST_P(JsepSessionTest, RenegotiationBothAddTracksToExistingStream)
{
  AddTracks(mSessionOff);
  AddTracks(mSessionAns);
  if (GetParam() == "datachannel") {
    return;
  }

  OfferAnswer();

  auto oHasStream = HasMediaStream(mSessionOff.GetLocalTracks());
  auto aHasStream = HasMediaStream(mSessionAns.GetLocalTracks());
  ASSERT_EQ(oHasStream, GetLocalUniqueStreamIds(mSessionOff).size());
  ASSERT_EQ(aHasStream, GetLocalUniqueStreamIds(mSessionAns).size());
  ASSERT_EQ(aHasStream, GetRemoteUniqueStreamIds(mSessionOff).size());
  ASSERT_EQ(oHasStream, GetRemoteUniqueStreamIds(mSessionAns).size());

  auto firstOffId = GetFirstLocalStreamId(mSessionOff);
  auto firstAnsId = GetFirstLocalStreamId(mSessionAns);

  auto offererPairs = GetTrackPairsByLevel(mSessionOff);
  auto answererPairs = GetTrackPairsByLevel(mSessionAns);

  std::vector<SdpMediaSection::MediaType> extraTypes;
  extraTypes.push_back(SdpMediaSection::kAudio);
  extraTypes.push_back(SdpMediaSection::kVideo);
  AddTracksToStream(mSessionOff, firstOffId, extraTypes);
  AddTracksToStream(mSessionAns, firstAnsId, extraTypes);
  types.insert(types.end(), extraTypes.begin(), extraTypes.end());

  OfferAnswer(CHECK_SUCCESS);

  oHasStream = HasMediaStream(mSessionOff.GetLocalTracks());
  aHasStream = HasMediaStream(mSessionAns.GetLocalTracks());

  ASSERT_EQ(oHasStream, GetLocalUniqueStreamIds(mSessionOff).size());
  ASSERT_EQ(aHasStream, GetLocalUniqueStreamIds(mSessionAns).size());
  ASSERT_EQ(aHasStream, GetRemoteUniqueStreamIds(mSessionOff).size());
  ASSERT_EQ(oHasStream, GetRemoteUniqueStreamIds(mSessionAns).size());
  if (oHasStream) {
    ASSERT_STREQ(firstOffId.c_str(),
                 GetFirstLocalStreamId(mSessionOff).c_str());
  }
  if (aHasStream) {
    ASSERT_STREQ(firstAnsId.c_str(),
                 GetFirstLocalStreamId(mSessionAns).c_str());
  }
}

TEST_P(JsepSessionTest, RenegotiationOffererRemovesTrack)
{
  AddTracks(mSessionOff);
  AddTracks(mSessionAns);
  if (types.front() == SdpMediaSection::kApplication) {
    return;
  }

  OfferAnswer();

  auto offererPairs = GetTrackPairsByLevel(mSessionOff);
  auto answererPairs = GetTrackPairsByLevel(mSessionAns);

  RefPtr<JsepTrack> removedTrack = GetTrackOff(0, types.front());
  ASSERT_TRUE(removedTrack);
  ASSERT_EQ(NS_OK, mSessionOff.RemoveTrack(removedTrack->GetStreamId(),
                                           removedTrack->GetTrackId()));

  OfferAnswer(CHECK_SUCCESS);

  auto added = mSessionAns.GetRemoteTracksAdded();
  auto removed = mSessionAns.GetRemoteTracksRemoved();
  ASSERT_EQ(0U, added.size());
  ASSERT_EQ(1U, removed.size());

  ASSERT_EQ(removedTrack->GetMediaType(), removed[0]->GetMediaType());
  ASSERT_EQ(removedTrack->GetStreamId(), removed[0]->GetStreamId());
  ASSERT_EQ(removedTrack->GetTrackId(), removed[0]->GetTrackId());

  added = mSessionOff.GetRemoteTracksAdded();
  removed = mSessionOff.GetRemoteTracksRemoved();
  ASSERT_EQ(0U, added.size());
  ASSERT_EQ(0U, removed.size());

  // First m-section should be recvonly
  auto offer = GetParsedLocalDescription(mSessionOff);
  auto* msection = GetMsection(*offer, types.front(), 0);
  ASSERT_TRUE(msection);
  ASSERT_TRUE(msection->IsReceiving());
  ASSERT_FALSE(msection->IsSending());

  // First audio m-section should be sendonly
  auto answer = GetParsedLocalDescription(mSessionAns);
  msection = GetMsection(*answer, types.front(), 0);
  ASSERT_TRUE(msection);
  ASSERT_FALSE(msection->IsReceiving());
  ASSERT_TRUE(msection->IsSending());

  auto newOffererPairs = GetTrackPairsByLevel(mSessionOff);
  auto newAnswererPairs = GetTrackPairsByLevel(mSessionAns);

  // Will be the same size since we still have a track on one side.
  ASSERT_EQ(offererPairs.size(), newOffererPairs.size());

  // This should be the only difference.
  ASSERT_TRUE(offererPairs[0].mSending);
  ASSERT_FALSE(newOffererPairs[0].mSending);

  // Remove this difference, let loop below take care of the rest
  offererPairs[0].mSending = nullptr;
  for (size_t i = 0; i < offererPairs.size(); ++i) {
    ASSERT_TRUE(Equals(offererPairs[i], newOffererPairs[i]));
  }

  // Will be the same size since we still have a track on one side.
  ASSERT_EQ(answererPairs.size(), newAnswererPairs.size());

  // This should be the only difference.
  ASSERT_TRUE(answererPairs[0].mReceiving);
  ASSERT_FALSE(newAnswererPairs[0].mReceiving);

  // Remove this difference, let loop below take care of the rest
  answererPairs[0].mReceiving = nullptr;
  for (size_t i = 0; i < answererPairs.size(); ++i) {
    ASSERT_TRUE(Equals(answererPairs[i], newAnswererPairs[i]));
  }
}

TEST_P(JsepSessionTest, RenegotiationAnswererRemovesTrack)
{
  AddTracks(mSessionOff);
  AddTracks(mSessionAns);
  if (types.front() == SdpMediaSection::kApplication) {
    return;
  }

  OfferAnswer();

  auto offererPairs = GetTrackPairsByLevel(mSessionOff);
  auto answererPairs = GetTrackPairsByLevel(mSessionAns);

  RefPtr<JsepTrack> removedTrack = GetTrackAns(0, types.front());
  ASSERT_TRUE(removedTrack);
  ASSERT_EQ(NS_OK, mSessionAns.RemoveTrack(removedTrack->GetStreamId(),
                                           removedTrack->GetTrackId()));

  OfferAnswer(CHECK_SUCCESS);

  auto added = mSessionAns.GetRemoteTracksAdded();
  auto removed = mSessionAns.GetRemoteTracksRemoved();
  ASSERT_EQ(0U, added.size());
  ASSERT_EQ(0U, removed.size());

  added = mSessionOff.GetRemoteTracksAdded();
  removed = mSessionOff.GetRemoteTracksRemoved();
  ASSERT_EQ(0U, added.size());
  ASSERT_EQ(1U, removed.size());

  ASSERT_EQ(removedTrack->GetMediaType(), removed[0]->GetMediaType());
  ASSERT_EQ(removedTrack->GetStreamId(), removed[0]->GetStreamId());
  ASSERT_EQ(removedTrack->GetTrackId(), removed[0]->GetTrackId());

  // First m-section should be sendrecv
  auto offer = GetParsedLocalDescription(mSessionOff);
  auto* msection = GetMsection(*offer, types.front(), 0);
  ASSERT_TRUE(msection);
  ASSERT_TRUE(msection->IsReceiving());
  ASSERT_TRUE(msection->IsSending());

  // First audio m-section should be recvonly
  auto answer = GetParsedLocalDescription(mSessionAns);
  msection = GetMsection(*answer, types.front(), 0);
  ASSERT_TRUE(msection);
  ASSERT_TRUE(msection->IsReceiving());
  ASSERT_FALSE(msection->IsSending());

  auto newOffererPairs = GetTrackPairsByLevel(mSessionOff);
  auto newAnswererPairs = GetTrackPairsByLevel(mSessionAns);

  // Will be the same size since we still have a track on one side.
  ASSERT_EQ(offererPairs.size(), newOffererPairs.size());

  // This should be the only difference.
  ASSERT_TRUE(offererPairs[0].mReceiving);
  ASSERT_FALSE(newOffererPairs[0].mReceiving);

  // Remove this difference, let loop below take care of the rest
  offererPairs[0].mReceiving = nullptr;
  for (size_t i = 0; i < offererPairs.size(); ++i) {
    ASSERT_TRUE(Equals(offererPairs[i], newOffererPairs[i]));
  }

  // Will be the same size since we still have a track on one side.
  ASSERT_EQ(answererPairs.size(), newAnswererPairs.size());

  // This should be the only difference.
  ASSERT_TRUE(answererPairs[0].mSending);
  ASSERT_FALSE(newAnswererPairs[0].mSending);

  // Remove this difference, let loop below take care of the rest
  answererPairs[0].mSending = nullptr;
  for (size_t i = 0; i < answererPairs.size(); ++i) {
    ASSERT_TRUE(Equals(answererPairs[i], newAnswererPairs[i]));
  }
}

TEST_P(JsepSessionTest, RenegotiationBothRemoveTrack)
{
  AddTracks(mSessionOff);
  AddTracks(mSessionAns);
  if (types.front() == SdpMediaSection::kApplication) {
    return;
  }

  OfferAnswer();

  auto offererPairs = GetTrackPairsByLevel(mSessionOff);
  auto answererPairs = GetTrackPairsByLevel(mSessionAns);

  RefPtr<JsepTrack> removedTrackAnswer = GetTrackAns(0, types.front());
  ASSERT_TRUE(removedTrackAnswer);
  ASSERT_EQ(NS_OK, mSessionAns.RemoveTrack(removedTrackAnswer->GetStreamId(),
                                           removedTrackAnswer->GetTrackId()));

  RefPtr<JsepTrack> removedTrackOffer = GetTrackOff(0, types.front());
  ASSERT_TRUE(removedTrackOffer);
  ASSERT_EQ(NS_OK, mSessionOff.RemoveTrack(removedTrackOffer->GetStreamId(),
                                           removedTrackOffer->GetTrackId()));

  OfferAnswer(CHECK_SUCCESS);

  auto added = mSessionAns.GetRemoteTracksAdded();
  auto removed = mSessionAns.GetRemoteTracksRemoved();
  ASSERT_EQ(0U, added.size());
  ASSERT_EQ(1U, removed.size());

  ASSERT_EQ(removedTrackOffer->GetMediaType(), removed[0]->GetMediaType());
  ASSERT_EQ(removedTrackOffer->GetStreamId(), removed[0]->GetStreamId());
  ASSERT_EQ(removedTrackOffer->GetTrackId(), removed[0]->GetTrackId());

  added = mSessionOff.GetRemoteTracksAdded();
  removed = mSessionOff.GetRemoteTracksRemoved();
  ASSERT_EQ(0U, added.size());
  ASSERT_EQ(1U, removed.size());

  ASSERT_EQ(removedTrackAnswer->GetMediaType(), removed[0]->GetMediaType());
  ASSERT_EQ(removedTrackAnswer->GetStreamId(), removed[0]->GetStreamId());
  ASSERT_EQ(removedTrackAnswer->GetTrackId(), removed[0]->GetTrackId());

  // First m-section should be recvonly
  auto offer = GetParsedLocalDescription(mSessionOff);
  auto* msection = GetMsection(*offer, types.front(), 0);
  ASSERT_TRUE(msection);
  ASSERT_TRUE(msection->IsReceiving());
  ASSERT_FALSE(msection->IsSending());

  // First m-section should be inactive, and rejected
  auto answer = GetParsedLocalDescription(mSessionAns);
  msection = GetMsection(*answer, types.front(), 0);
  ASSERT_TRUE(msection);
  ASSERT_FALSE(msection->IsReceiving());
  ASSERT_FALSE(msection->IsSending());
  ASSERT_FALSE(msection->GetPort());

  auto newOffererPairs = GetTrackPairsByLevel(mSessionOff);
  auto newAnswererPairs = GetTrackPairsByLevel(mSessionAns);

  ASSERT_EQ(offererPairs.size(), newOffererPairs.size() + 1);

  for (size_t i = 0; i < newOffererPairs.size(); ++i) {
    JsepTrackPair oldPair(offererPairs[i + 1]);
    JsepTrackPair newPair(newOffererPairs[i]);
    ASSERT_EQ(oldPair.mLevel, newPair.mLevel);
    ASSERT_EQ(oldPair.mSending.get(), newPair.mSending.get());
    ASSERT_EQ(oldPair.mReceiving.get(), newPair.mReceiving.get());
    ASSERT_TRUE(oldPair.mBundleLevel.isSome());
    ASSERT_TRUE(newPair.mBundleLevel.isSome());
    ASSERT_EQ(0U, *oldPair.mBundleLevel);
    ASSERT_EQ(1U, *newPair.mBundleLevel);
  }

  ASSERT_EQ(answererPairs.size(), newAnswererPairs.size() + 1);

  for (size_t i = 0; i < newAnswererPairs.size(); ++i) {
    JsepTrackPair oldPair(answererPairs[i + 1]);
    JsepTrackPair newPair(newAnswererPairs[i]);
    ASSERT_EQ(oldPair.mLevel, newPair.mLevel);
    ASSERT_EQ(oldPair.mSending.get(), newPair.mSending.get());
    ASSERT_EQ(oldPair.mReceiving.get(), newPair.mReceiving.get());
    ASSERT_TRUE(oldPair.mBundleLevel.isSome());
    ASSERT_TRUE(newPair.mBundleLevel.isSome());
    ASSERT_EQ(0U, *oldPair.mBundleLevel);
    ASSERT_EQ(1U, *newPair.mBundleLevel);
  }
}

TEST_P(JsepSessionTest, RenegotiationBothRemoveThenAddTrack)
{
  AddTracks(mSessionOff);
  AddTracks(mSessionAns);
  if (types.front() == SdpMediaSection::kApplication) {
    return;
  }

  SdpMediaSection::MediaType removedType = types.front();

  OfferAnswer();

  RefPtr<JsepTrack> removedTrackAnswer = GetTrackAns(0, removedType);
  ASSERT_TRUE(removedTrackAnswer);
  ASSERT_EQ(NS_OK, mSessionAns.RemoveTrack(removedTrackAnswer->GetStreamId(),
                                           removedTrackAnswer->GetTrackId()));

  RefPtr<JsepTrack> removedTrackOffer = GetTrackOff(0, removedType);
  ASSERT_TRUE(removedTrackOffer);
  ASSERT_EQ(NS_OK, mSessionOff.RemoveTrack(removedTrackOffer->GetStreamId(),
                                           removedTrackOffer->GetTrackId()));

  OfferAnswer(CHECK_SUCCESS);

  auto offererPairs = GetTrackPairsByLevel(mSessionOff);
  auto answererPairs = GetTrackPairsByLevel(mSessionAns);

  std::vector<SdpMediaSection::MediaType> extraTypes;
  extraTypes.push_back(removedType);
  AddTracks(mSessionAns, extraTypes);
  AddTracks(mSessionOff, extraTypes);
  types.insert(types.end(), extraTypes.begin(), extraTypes.end());

  OfferAnswer(CHECK_SUCCESS);

  auto added = mSessionAns.GetRemoteTracksAdded();
  auto removed = mSessionAns.GetRemoteTracksRemoved();
  ASSERT_EQ(1U, added.size());
  ASSERT_EQ(0U, removed.size());
  ASSERT_EQ(removedType, added[0]->GetMediaType());

  added = mSessionOff.GetRemoteTracksAdded();
  removed = mSessionOff.GetRemoteTracksRemoved();
  ASSERT_EQ(1U, added.size());
  ASSERT_EQ(0U, removed.size());
  ASSERT_EQ(removedType, added[0]->GetMediaType());

  auto newOffererPairs = GetTrackPairsByLevel(mSessionOff);
  auto newAnswererPairs = GetTrackPairsByLevel(mSessionAns);

  ASSERT_EQ(offererPairs.size() + 1, newOffererPairs.size());
  ASSERT_EQ(answererPairs.size() + 1, newAnswererPairs.size());

  // Ensure that the m-section was re-used; no gaps
  for (size_t i = 0; i < newOffererPairs.size(); ++i) {
    ASSERT_EQ(i, newOffererPairs[i].mLevel);
  }
  for (size_t i = 0; i < newAnswererPairs.size(); ++i) {
    ASSERT_EQ(i, newAnswererPairs[i].mLevel);
  }
}

TEST_P(JsepSessionTest, RenegotiationBothRemoveTrackDifferentMsection)
{
  AddTracks(mSessionOff);
  AddTracks(mSessionAns);
  if (types.front() == SdpMediaSection::kApplication) {
    return;
  }

  if (types.size() < 2 || types[0] != types[1]) {
    // For simplicity, just run in cases where we have two of the same type
    return;
  }

  OfferAnswer();

  auto offererPairs = GetTrackPairsByLevel(mSessionOff);
  auto answererPairs = GetTrackPairsByLevel(mSessionAns);

  RefPtr<JsepTrack> removedTrackAnswer = GetTrackAns(0, types.front());
  ASSERT_TRUE(removedTrackAnswer);
  ASSERT_EQ(NS_OK, mSessionAns.RemoveTrack(removedTrackAnswer->GetStreamId(),
                                           removedTrackAnswer->GetTrackId()));

  // Second instance of the same type
  RefPtr<JsepTrack> removedTrackOffer = GetTrackOff(1, types.front());
  ASSERT_TRUE(removedTrackOffer);
  ASSERT_EQ(NS_OK, mSessionOff.RemoveTrack(removedTrackOffer->GetStreamId(),
                                           removedTrackOffer->GetTrackId()));

  OfferAnswer(CHECK_SUCCESS);

  auto added = mSessionAns.GetRemoteTracksAdded();
  auto removed = mSessionAns.GetRemoteTracksRemoved();
  ASSERT_EQ(0U, added.size());
  ASSERT_EQ(1U, removed.size());

  ASSERT_EQ(removedTrackOffer->GetMediaType(), removed[0]->GetMediaType());
  ASSERT_EQ(removedTrackOffer->GetStreamId(), removed[0]->GetStreamId());
  ASSERT_EQ(removedTrackOffer->GetTrackId(), removed[0]->GetTrackId());

  added = mSessionOff.GetRemoteTracksAdded();
  removed = mSessionOff.GetRemoteTracksRemoved();
  ASSERT_EQ(0U, added.size());
  ASSERT_EQ(1U, removed.size());

  ASSERT_EQ(removedTrackAnswer->GetMediaType(), removed[0]->GetMediaType());
  ASSERT_EQ(removedTrackAnswer->GetStreamId(), removed[0]->GetStreamId());
  ASSERT_EQ(removedTrackAnswer->GetTrackId(), removed[0]->GetTrackId());

  // Second m-section should be recvonly
  auto offer = GetParsedLocalDescription(mSessionOff);
  auto* msection = GetMsection(*offer, types.front(), 1);
  ASSERT_TRUE(msection);
  ASSERT_TRUE(msection->IsReceiving());
  ASSERT_FALSE(msection->IsSending());

  // First m-section should be recvonly
  auto answer = GetParsedLocalDescription(mSessionAns);
  msection = GetMsection(*answer, types.front(), 0);
  ASSERT_TRUE(msection);
  ASSERT_TRUE(msection->IsReceiving());
  ASSERT_FALSE(msection->IsSending());

  auto newOffererPairs = GetTrackPairsByLevel(mSessionOff);
  auto newAnswererPairs = GetTrackPairsByLevel(mSessionAns);

  ASSERT_EQ(offererPairs.size(), newOffererPairs.size());

  // This should be the only difference.
  ASSERT_TRUE(offererPairs[0].mReceiving);
  ASSERT_FALSE(newOffererPairs[0].mReceiving);

  // Remove this difference, let loop below take care of the rest
  offererPairs[0].mReceiving = nullptr;

  // This should be the only difference.
  ASSERT_TRUE(offererPairs[1].mSending);
  ASSERT_FALSE(newOffererPairs[1].mSending);

  // Remove this difference, let loop below take care of the rest
  offererPairs[1].mSending = nullptr;

  for (size_t i = 0; i < newOffererPairs.size(); ++i) {
    ASSERT_TRUE(Equals(offererPairs[i], newOffererPairs[i]));
  }

  ASSERT_EQ(answererPairs.size(), newAnswererPairs.size());

  // This should be the only difference.
  ASSERT_TRUE(answererPairs[0].mSending);
  ASSERT_FALSE(newAnswererPairs[0].mSending);

  // Remove this difference, let loop below take care of the rest
  answererPairs[0].mSending = nullptr;

  // This should be the only difference.
  ASSERT_TRUE(answererPairs[1].mReceiving);
  ASSERT_FALSE(newAnswererPairs[1].mReceiving);

  // Remove this difference, let loop below take care of the rest
  answererPairs[1].mReceiving = nullptr;

  for (size_t i = 0; i < newAnswererPairs.size(); ++i) {
    ASSERT_TRUE(Equals(answererPairs[i], newAnswererPairs[i]));
  }
}

TEST_P(JsepSessionTest, RenegotiationOffererReplacesTrack)
{
  AddTracks(mSessionOff);
  AddTracks(mSessionAns);

  if (types.front() == SdpMediaSection::kApplication) {
    return;
  }

  OfferAnswer();

  auto offererPairs = GetTrackPairsByLevel(mSessionOff);
  auto answererPairs = GetTrackPairsByLevel(mSessionAns);

  RefPtr<JsepTrack> removedTrack = GetTrackOff(0, types.front());
  ASSERT_TRUE(removedTrack);
  ASSERT_EQ(NS_OK, mSessionOff.RemoveTrack(removedTrack->GetStreamId(),
                                           removedTrack->GetTrackId()));
  RefPtr<JsepTrack> addedTrack(
      new JsepTrack(types.front(), "newstream", "newtrack"));
  ASSERT_EQ(NS_OK, mSessionOff.AddTrack(addedTrack));

  OfferAnswer(CHECK_SUCCESS);

  auto added = mSessionAns.GetRemoteTracksAdded();
  auto removed = mSessionAns.GetRemoteTracksRemoved();
  ASSERT_EQ(1U, added.size());
  ASSERT_EQ(1U, removed.size());

  ASSERT_EQ(removedTrack->GetMediaType(), removed[0]->GetMediaType());
  ASSERT_EQ(removedTrack->GetStreamId(), removed[0]->GetStreamId());
  ASSERT_EQ(removedTrack->GetTrackId(), removed[0]->GetTrackId());

  ASSERT_EQ(addedTrack->GetMediaType(), added[0]->GetMediaType());
  ASSERT_EQ(addedTrack->GetStreamId(), added[0]->GetStreamId());
  ASSERT_EQ(addedTrack->GetTrackId(), added[0]->GetTrackId());

  added = mSessionOff.GetRemoteTracksAdded();
  removed = mSessionOff.GetRemoteTracksRemoved();
  ASSERT_EQ(0U, added.size());
  ASSERT_EQ(0U, removed.size());

  // First audio m-section should be sendrecv
  auto offer = GetParsedLocalDescription(mSessionOff);
  auto* msection = GetMsection(*offer, types.front(), 0);
  ASSERT_TRUE(msection);
  ASSERT_TRUE(msection->IsReceiving());
  ASSERT_TRUE(msection->IsSending());

  // First audio m-section should be sendrecv
  auto answer = GetParsedLocalDescription(mSessionAns);
  msection = GetMsection(*answer, types.front(), 0);
  ASSERT_TRUE(msection);
  ASSERT_TRUE(msection->IsReceiving());
  ASSERT_TRUE(msection->IsSending());

  auto newOffererPairs = GetTrackPairsByLevel(mSessionOff);
  auto newAnswererPairs = GetTrackPairsByLevel(mSessionAns);

  ASSERT_EQ(offererPairs.size(), newOffererPairs.size());

  ASSERT_NE(offererPairs[0].mSending->GetStreamId(),
            newOffererPairs[0].mSending->GetStreamId());
  ASSERT_NE(offererPairs[0].mSending->GetTrackId(),
            newOffererPairs[0].mSending->GetTrackId());

  // Skip first pair
  for (size_t i = 1; i < offererPairs.size(); ++i) {
    ASSERT_TRUE(Equals(offererPairs[i], newOffererPairs[i]));
  }

  ASSERT_EQ(answererPairs.size(), newAnswererPairs.size());

  ASSERT_NE(answererPairs[0].mReceiving->GetStreamId(),
            newAnswererPairs[0].mReceiving->GetStreamId());
  ASSERT_NE(answererPairs[0].mReceiving->GetTrackId(),
            newAnswererPairs[0].mReceiving->GetTrackId());

  // Skip first pair
  for (size_t i = 1; i < newAnswererPairs.size(); ++i) {
    ASSERT_TRUE(Equals(answererPairs[i], newAnswererPairs[i]));
  }
}

// Tests whether auto-assigned remote msids (ie; what happens when the other
// side doesn't use msid attributes) are stable across renegotiation.
TEST_P(JsepSessionTest, RenegotiationAutoAssignedMsidIsStable)
{
  AddTracks(mSessionOff);
  std::string offer = CreateOffer();
  SetLocalOffer(offer);
  SetRemoteOffer(offer);
  AddTracks(mSessionAns);
  std::string answer = CreateAnswer();
  SetLocalAnswer(answer);

  DisableMsid(&answer);

  SetRemoteAnswer(answer, CHECK_SUCCESS);

  auto offererPairs = GetTrackPairsByLevel(mSessionOff);

  // Make sure that DisableMsid actually worked, since it is kinda hacky
  auto answererPairs = GetTrackPairsByLevel(mSessionAns);
  ASSERT_EQ(offererPairs.size(), answererPairs.size());
  for (size_t i = 0; i < offererPairs.size(); ++i) {
    ASSERT_TRUE(offererPairs[i].mReceiving);
    ASSERT_TRUE(answererPairs[i].mSending);
    // These should not match since we've monkeyed with the msid
    ASSERT_NE(offererPairs[i].mReceiving->GetStreamId(),
              answererPairs[i].mSending->GetStreamId());
    ASSERT_NE(offererPairs[i].mReceiving->GetTrackId(),
              answererPairs[i].mSending->GetTrackId());
  }

  offer = CreateOffer();
  SetLocalOffer(offer);
  SetRemoteOffer(offer);
  AddTracks(mSessionAns);
  answer = CreateAnswer();
  SetLocalAnswer(answer);

  DisableMsid(&answer);

  SetRemoteAnswer(answer, CHECK_SUCCESS);

  auto newOffererPairs = mSessionOff.GetNegotiatedTrackPairs();

  ASSERT_EQ(offererPairs.size(), newOffererPairs.size());
  for (size_t i = 0; i < offererPairs.size(); ++i) {
    ASSERT_TRUE(Equals(offererPairs[i], newOffererPairs[i]));
  }
}

// Tests behavior when the answerer does not use msid in the initial exchange,
// but does on renegotiation.
TEST_P(JsepSessionTest, RenegotiationAnswererEnablesMsid)
{
  AddTracks(mSessionOff);
  std::string offer = CreateOffer();
  SetLocalOffer(offer);
  SetRemoteOffer(offer);
  AddTracks(mSessionAns);
  std::string answer = CreateAnswer();
  SetLocalAnswer(answer);

  DisableMsid(&answer);

  SetRemoteAnswer(answer, CHECK_SUCCESS);

  auto offererPairs = GetTrackPairsByLevel(mSessionOff);

  offer = CreateOffer();
  SetLocalOffer(offer);
  SetRemoteOffer(offer);
  AddTracks(mSessionAns);
  answer = CreateAnswer();
  SetLocalAnswer(answer);
  SetRemoteAnswer(answer, CHECK_SUCCESS);

  auto newOffererPairs = mSessionOff.GetNegotiatedTrackPairs();

  ASSERT_EQ(offererPairs.size(), newOffererPairs.size());
  for (size_t i = 0; i < offererPairs.size(); ++i) {
    ASSERT_EQ(offererPairs[i].mReceiving->GetMediaType(),
              newOffererPairs[i].mReceiving->GetMediaType());

    ASSERT_EQ(offererPairs[i].mSending, newOffererPairs[i].mSending);
    ASSERT_EQ(offererPairs[i].mRtpTransport, newOffererPairs[i].mRtpTransport);
    ASSERT_EQ(offererPairs[i].mRtcpTransport, newOffererPairs[i].mRtcpTransport);

    if (offererPairs[i].mReceiving->GetMediaType() ==
        SdpMediaSection::kApplication) {
      ASSERT_EQ(offererPairs[i].mReceiving, newOffererPairs[i].mReceiving);
    } else {
      // This should be the only difference
      ASSERT_NE(offererPairs[i].mReceiving, newOffererPairs[i].mReceiving);
    }
  }
}

TEST_P(JsepSessionTest, RenegotiationAnswererDisablesMsid)
{
  AddTracks(mSessionOff);
  std::string offer = CreateOffer();
  SetLocalOffer(offer);
  SetRemoteOffer(offer);
  AddTracks(mSessionAns);
  std::string answer = CreateAnswer();
  SetLocalAnswer(answer);
  SetRemoteAnswer(answer, CHECK_SUCCESS);

  auto offererPairs = GetTrackPairsByLevel(mSessionOff);

  offer = CreateOffer();
  SetLocalOffer(offer);
  SetRemoteOffer(offer);
  AddTracks(mSessionAns);
  answer = CreateAnswer();
  SetLocalAnswer(answer);

  DisableMsid(&answer);

  SetRemoteAnswer(answer, CHECK_SUCCESS);

  auto newOffererPairs = mSessionOff.GetNegotiatedTrackPairs();

  ASSERT_EQ(offererPairs.size(), newOffererPairs.size());
  for (size_t i = 0; i < offererPairs.size(); ++i) {
    ASSERT_EQ(offererPairs[i].mReceiving->GetMediaType(),
              newOffererPairs[i].mReceiving->GetMediaType());

    ASSERT_EQ(offererPairs[i].mSending, newOffererPairs[i].mSending);
    ASSERT_EQ(offererPairs[i].mRtpTransport, newOffererPairs[i].mRtpTransport);
    ASSERT_EQ(offererPairs[i].mRtcpTransport, newOffererPairs[i].mRtcpTransport);

    if (offererPairs[i].mReceiving->GetMediaType() ==
        SdpMediaSection::kApplication) {
      ASSERT_EQ(offererPairs[i].mReceiving, newOffererPairs[i].mReceiving);
    } else {
      // This should be the only difference
      ASSERT_NE(offererPairs[i].mReceiving, newOffererPairs[i].mReceiving);
    }
  }
}

// Tests behavior when offerer does not use bundle on the initial offer/answer,
// but does on renegotiation.
TEST_P(JsepSessionTest, RenegotiationOffererEnablesBundle)
{
  AddTracks(mSessionOff);
  AddTracks(mSessionAns);

  if (types.size() < 2) {
    // No bundle will happen here.
    return;
  }

  std::string offer = CreateOffer();

  DisableBundle(&offer);

  SetLocalOffer(offer);
  SetRemoteOffer(offer);
  std::string answer = CreateAnswer();
  SetLocalAnswer(answer);
  SetRemoteAnswer(answer);

  auto offererPairs = GetTrackPairsByLevel(mSessionOff);
  auto answererPairs = GetTrackPairsByLevel(mSessionAns);

  OfferAnswer();

  auto newOffererPairs = GetTrackPairsByLevel(mSessionOff);
  auto newAnswererPairs = GetTrackPairsByLevel(mSessionAns);

  ASSERT_EQ(newOffererPairs.size(), newAnswererPairs.size());
  ASSERT_EQ(offererPairs.size(), newOffererPairs.size());
  ASSERT_EQ(answererPairs.size(), newAnswererPairs.size());

  for (size_t i = 0; i < newOffererPairs.size(); ++i) {
    // No bundle initially
    ASSERT_FALSE(offererPairs[i].mBundleLevel.isSome());
    ASSERT_FALSE(answererPairs[i].mBundleLevel.isSome());
    if (i != 0) {
      ASSERT_NE(offererPairs[0].mRtpTransport.get(),
                offererPairs[i].mRtpTransport.get());
      if (offererPairs[0].mRtcpTransport) {
        ASSERT_NE(offererPairs[0].mRtcpTransport.get(),
                  offererPairs[i].mRtcpTransport.get());
      }
      ASSERT_NE(answererPairs[0].mRtpTransport.get(),
                answererPairs[i].mRtpTransport.get());
      if (answererPairs[0].mRtcpTransport) {
        ASSERT_NE(answererPairs[0].mRtcpTransport.get(),
                  answererPairs[i].mRtcpTransport.get());
      }
    }

    // Verify that bundle worked after renegotiation
    ASSERT_TRUE(newOffererPairs[i].mBundleLevel.isSome());
    ASSERT_TRUE(newAnswererPairs[i].mBundleLevel.isSome());
    ASSERT_EQ(newOffererPairs[0].mRtpTransport.get(),
              newOffererPairs[i].mRtpTransport.get());
    ASSERT_EQ(newOffererPairs[0].mRtcpTransport.get(),
              newOffererPairs[i].mRtcpTransport.get());
    ASSERT_EQ(newAnswererPairs[0].mRtpTransport.get(),
              newAnswererPairs[i].mRtpTransport.get());
    ASSERT_EQ(newAnswererPairs[0].mRtcpTransport.get(),
              newAnswererPairs[i].mRtcpTransport.get());
  }
}

TEST_P(JsepSessionTest, RenegotiationOffererDisablesBundleTransport)
{
  AddTracks(mSessionOff);
  AddTracks(mSessionAns);

  if (types.size() < 2) {
    return;
  }

  OfferAnswer();

  auto offererPairs = GetTrackPairsByLevel(mSessionOff);
  auto answererPairs = GetTrackPairsByLevel(mSessionAns);

  std::string reoffer = CreateOffer();

  DisableMsection(&reoffer, 0);

  SetLocalOffer(reoffer, CHECK_SUCCESS);
  SetRemoteOffer(reoffer, CHECK_SUCCESS);
  std::string reanswer = CreateAnswer();
  SetLocalAnswer(reanswer, CHECK_SUCCESS);
  SetRemoteAnswer(reanswer, CHECK_SUCCESS);

  auto newOffererPairs = GetTrackPairsByLevel(mSessionOff);
  auto newAnswererPairs = GetTrackPairsByLevel(mSessionAns);

  ASSERT_EQ(newOffererPairs.size(), newAnswererPairs.size());
  ASSERT_EQ(offererPairs.size(), newOffererPairs.size() + 1);
  ASSERT_EQ(answererPairs.size(), newAnswererPairs.size() + 1);

  for (size_t i = 0; i < newOffererPairs.size(); ++i) {
    ASSERT_TRUE(newOffererPairs[i].mBundleLevel.isSome());
    ASSERT_TRUE(newAnswererPairs[i].mBundleLevel.isSome());
    ASSERT_EQ(1U, *newOffererPairs[i].mBundleLevel);
    ASSERT_EQ(1U, *newAnswererPairs[i].mBundleLevel);
    ASSERT_EQ(newOffererPairs[0].mRtpTransport.get(),
              newOffererPairs[i].mRtpTransport.get());
    ASSERT_EQ(newOffererPairs[0].mRtcpTransport.get(),
              newOffererPairs[i].mRtcpTransport.get());
    ASSERT_EQ(newAnswererPairs[0].mRtpTransport.get(),
              newAnswererPairs[i].mRtpTransport.get());
    ASSERT_EQ(newAnswererPairs[0].mRtcpTransport.get(),
              newAnswererPairs[i].mRtcpTransport.get());
  }

  ASSERT_NE(newOffererPairs[0].mRtpTransport.get(),
            offererPairs[0].mRtpTransport.get());
  ASSERT_NE(newAnswererPairs[0].mRtpTransport.get(),
            answererPairs[0].mRtpTransport.get());

  ASSERT_LE(1U, mSessionOff.GetTransports().size());
  ASSERT_LE(1U, mSessionAns.GetTransports().size());

  ASSERT_EQ(0U, mSessionOff.GetTransports()[0]->mComponents);
  ASSERT_EQ(0U, mSessionAns.GetTransports()[0]->mComponents);
}

TEST_P(JsepSessionTest, RenegotiationAnswererDisablesBundleTransport)
{
  AddTracks(mSessionOff);
  AddTracks(mSessionAns);

  if (types.size() < 2) {
    return;
  }

  OfferAnswer();

  auto offererPairs = GetTrackPairsByLevel(mSessionOff);
  auto answererPairs = GetTrackPairsByLevel(mSessionAns);

  std::string reoffer = CreateOffer();
  SetLocalOffer(reoffer, CHECK_SUCCESS);
  SetRemoteOffer(reoffer, CHECK_SUCCESS);
  std::string reanswer = CreateAnswer();

  DisableMsection(&reanswer, 0);

  SetLocalAnswer(reanswer, CHECK_SUCCESS);
  SetRemoteAnswer(reanswer, CHECK_SUCCESS);

  auto newOffererPairs = GetTrackPairsByLevel(mSessionOff);
  auto newAnswererPairs = GetTrackPairsByLevel(mSessionAns);

  ASSERT_EQ(newOffererPairs.size(), newAnswererPairs.size());
  ASSERT_EQ(offererPairs.size(), newOffererPairs.size() + 1);
  ASSERT_EQ(answererPairs.size(), newAnswererPairs.size() + 1);

  for (size_t i = 0; i < newOffererPairs.size(); ++i) {
    ASSERT_TRUE(newOffererPairs[i].mBundleLevel.isSome());
    ASSERT_TRUE(newAnswererPairs[i].mBundleLevel.isSome());
    ASSERT_EQ(1U, *newOffererPairs[i].mBundleLevel);
    ASSERT_EQ(1U, *newAnswererPairs[i].mBundleLevel);
    ASSERT_EQ(newOffererPairs[0].mRtpTransport.get(),
              newOffererPairs[i].mRtpTransport.get());
    ASSERT_EQ(newOffererPairs[0].mRtcpTransport.get(),
              newOffererPairs[i].mRtcpTransport.get());
    ASSERT_EQ(newAnswererPairs[0].mRtpTransport.get(),
              newAnswererPairs[i].mRtpTransport.get());
    ASSERT_EQ(newAnswererPairs[0].mRtcpTransport.get(),
              newAnswererPairs[i].mRtcpTransport.get());
  }

  ASSERT_NE(newOffererPairs[0].mRtpTransport.get(),
            offererPairs[0].mRtpTransport.get());
  ASSERT_NE(newAnswererPairs[0].mRtpTransport.get(),
            answererPairs[0].mRtpTransport.get());
}

TEST_P(JsepSessionTest, FullCallWithCandidates)
{
  AddTracks(mSessionOff);
  std::string offer = CreateOffer();
  SetLocalOffer(offer);
  mOffCandidates.Gather(mSessionOff, types);

  UniquePtr<Sdp> localOffer(Parse(mSessionOff.GetLocalDescription()));
  for (size_t i = 0; i < localOffer->GetMediaSectionCount(); ++i) {
    mOffCandidates.CheckRtpCandidates(
        true, localOffer->GetMediaSection(i), i,
        "Local offer after gathering should have RTP candidates.");
    mOffCandidates.CheckDefaultRtpCandidate(
        true, localOffer->GetMediaSection(i), i,
        "Local offer after gathering should have a default RTP candidate.");
    mOffCandidates.CheckRtcpCandidates(
        types[i] != SdpMediaSection::kApplication,
        localOffer->GetMediaSection(i), i,
        "Local offer after gathering should have RTCP candidates "
        "(unless m=application)");
    mOffCandidates.CheckDefaultRtcpCandidate(
        types[i] != SdpMediaSection::kApplication,
        localOffer->GetMediaSection(i), i,
        "Local offer after gathering should have a default RTCP candidate "
        "(unless m=application)");
    CheckEndOfCandidates(true, localOffer->GetMediaSection(i),
        "Local offer after gathering should have an end-of-candidates.");
  }

  SetRemoteOffer(offer);
  mOffCandidates.Trickle(mSessionAns);

  UniquePtr<Sdp> remoteOffer(Parse(mSessionAns.GetRemoteDescription()));
  for (size_t i = 0; i < remoteOffer->GetMediaSectionCount(); ++i) {
    mOffCandidates.CheckRtpCandidates(
        true, remoteOffer->GetMediaSection(i), i,
        "Remote offer after trickle should have RTP candidates.");
    mOffCandidates.CheckDefaultRtpCandidate(
        false, remoteOffer->GetMediaSection(i), i,
        "Initial remote offer should not have a default RTP candidate.");
    mOffCandidates.CheckRtcpCandidates(
        types[i] != SdpMediaSection::kApplication,
        remoteOffer->GetMediaSection(i), i,
        "Remote offer after trickle should have RTCP candidates "
        "(unless m=application)");
    mOffCandidates.CheckDefaultRtcpCandidate(
        false, remoteOffer->GetMediaSection(i), i,
        "Initial remote offer should not have a default RTCP candidate.");
    CheckEndOfCandidates(false, remoteOffer->GetMediaSection(i),
        "Initial remote offer should not have an end-of-candidates.");
  }

  AddTracks(mSessionAns);
  std::string answer = CreateAnswer();
  SetLocalAnswer(answer);
  // This will gather candidates that mSessionAns knows it doesn't need.
  // They should not be present in the SDP.
  mAnsCandidates.Gather(mSessionAns, types);

  UniquePtr<Sdp> localAnswer(Parse(mSessionAns.GetLocalDescription()));
  for (size_t i = 0; i < localAnswer->GetMediaSectionCount(); ++i) {
    mAnsCandidates.CheckRtpCandidates(
        i == 0, localAnswer->GetMediaSection(i), i,
        "Local answer after gathering should have RTP candidates on level 0.");
    mAnsCandidates.CheckDefaultRtpCandidate(
        true, localAnswer->GetMediaSection(i), 0,
        "Local answer after gathering should have a default RTP candidate "
        "on all levels that matches transport level 0.");
    mAnsCandidates.CheckRtcpCandidates(
        false, localAnswer->GetMediaSection(i), i,
        "Local answer after gathering should not have RTCP candidates "
        "(because we're answering with rtcp-mux)");
    mAnsCandidates.CheckDefaultRtcpCandidate(
        false, localAnswer->GetMediaSection(i), i,
        "Local answer after gathering should not have a default RTCP candidate "
        "(because we're answering with rtcp-mux)");
    CheckEndOfCandidates(i == 0, localAnswer->GetMediaSection(i),
        "Local answer after gathering should have an end-of-candidates only for"
        " level 0.");
  }

  SetRemoteAnswer(answer);
  mAnsCandidates.Trickle(mSessionOff);

  UniquePtr<Sdp> remoteAnswer(Parse(mSessionOff.GetRemoteDescription()));
  for (size_t i = 0; i < remoteAnswer->GetMediaSectionCount(); ++i) {
    mAnsCandidates.CheckRtpCandidates(
        i == 0, remoteAnswer->GetMediaSection(i), i,
        "Remote answer after trickle should have RTP candidates on level 0.");
    mAnsCandidates.CheckDefaultRtpCandidate(
        false, remoteAnswer->GetMediaSection(i), i,
        "Remote answer after trickle should not have a default RTP candidate.");
    mAnsCandidates.CheckRtcpCandidates(
        false, remoteAnswer->GetMediaSection(i), i,
        "Remote answer after trickle should not have RTCP candidates "
        "(because we're answering with rtcp-mux)");
    mAnsCandidates.CheckDefaultRtcpCandidate(
        false, remoteAnswer->GetMediaSection(i), i,
        "Remote answer after trickle should not have a default RTCP "
        "candidate.");
    CheckEndOfCandidates(false, remoteAnswer->GetMediaSection(i),
        "Remote answer after trickle should not have an end-of-candidates.");
  }
}

TEST_P(JsepSessionTest, RenegotiationWithCandidates)
{
  AddTracks(mSessionOff);
  std::string offer = CreateOffer();
  SetLocalOffer(offer);
  mOffCandidates.Gather(mSessionOff, types);
  SetRemoteOffer(offer);
  mOffCandidates.Trickle(mSessionAns);
  AddTracks(mSessionAns);
  std::string answer = CreateAnswer();
  SetLocalAnswer(answer);
  mAnsCandidates.Gather(mSessionAns, types);
  SetRemoteAnswer(answer);
  mAnsCandidates.Trickle(mSessionOff);

  offer = CreateOffer();
  SetLocalOffer(offer);

  UniquePtr<Sdp> parsedOffer(Parse(offer));
  for (size_t i = 0; i < parsedOffer->GetMediaSectionCount(); ++i) {
    mOffCandidates.CheckRtpCandidates(
        i == 0, parsedOffer->GetMediaSection(i), i,
        "Local reoffer before gathering should have RTP candidates on level 0"
        " only.");
    mOffCandidates.CheckDefaultRtpCandidate(
        i == 0, parsedOffer->GetMediaSection(i), 0,
        "Local reoffer before gathering should have a default RTP candidate "
        "on level 0 only.");
    mOffCandidates.CheckRtcpCandidates(
        false, parsedOffer->GetMediaSection(i), i,
        "Local reoffer before gathering should not have RTCP candidates.");
    mOffCandidates.CheckDefaultRtcpCandidate(
        false, parsedOffer->GetMediaSection(i), i,
        "Local reoffer before gathering should not have a default RTCP "
        "candidate.");
    CheckEndOfCandidates(false, parsedOffer->GetMediaSection(i),
        "Local reoffer before gathering should not have an end-of-candidates.");
  }

  // mSessionAns should generate a reoffer that is similar
  std::string otherOffer;
  JsepOfferOptions defaultOptions;
  nsresult rv = mSessionAns.CreateOffer(defaultOptions, &otherOffer);
  ASSERT_EQ(NS_OK, rv);
  parsedOffer = Parse(otherOffer);
  for (size_t i = 0; i < parsedOffer->GetMediaSectionCount(); ++i) {
    mAnsCandidates.CheckRtpCandidates(
        i == 0, parsedOffer->GetMediaSection(i), i,
        "Local reoffer before gathering should have RTP candidates on level 0"
        " only. (previous answerer)");
    mAnsCandidates.CheckDefaultRtpCandidate(
        i == 0, parsedOffer->GetMediaSection(i), 0,
        "Local reoffer before gathering should have a default RTP candidate "
        "on level 0 only. (previous answerer)");
    mAnsCandidates.CheckRtcpCandidates(
        false, parsedOffer->GetMediaSection(i), i,
        "Local reoffer before gathering should not have RTCP candidates."
        " (previous answerer)");
    mAnsCandidates.CheckDefaultRtcpCandidate(
        false, parsedOffer->GetMediaSection(i), i,
        "Local reoffer before gathering should not have a default RTCP "
        "candidate. (previous answerer)");
    CheckEndOfCandidates(false, parsedOffer->GetMediaSection(i),
        "Local reoffer before gathering should not have an end-of-candidates. "
        "(previous answerer)");
  }

  // Ok, let's continue with the renegotiation
  SetRemoteOffer(offer);

  // PeerConnection will not re-gather for RTP, but it will for RTCP in case
  // the answerer decides to turn off rtcp-mux.
  if (types[0] != SdpMediaSection::kApplication) {
    mOffCandidates.Gather(mSessionOff, 0, RTCP);
  }

  // Since the remaining levels were bundled, PeerConnection will re-gather for
  // both RTP and RTCP, in case the answerer rejects bundle.
  for (size_t level = 1; level < types.size(); ++level) {
    mOffCandidates.Gather(mSessionOff, level, RTP);
    if (types[level] != SdpMediaSection::kApplication) {
      mOffCandidates.Gather(mSessionOff, level, RTCP);
    }
  }
  mOffCandidates.FinishGathering(mSessionOff);

  mOffCandidates.Trickle(mSessionAns);

  UniquePtr<Sdp> localOffer(Parse(mSessionOff.GetLocalDescription()));
  for (size_t i = 0; i < localOffer->GetMediaSectionCount(); ++i) {
    mOffCandidates.CheckRtpCandidates(
        true, localOffer->GetMediaSection(i), i,
        "Local reoffer after gathering should have RTP candidates.");
    mOffCandidates.CheckDefaultRtpCandidate(
        true, localOffer->GetMediaSection(i), i,
        "Local reoffer after gathering should have a default RTP candidate.");
    mOffCandidates.CheckRtcpCandidates(
        types[i] != SdpMediaSection::kApplication,
        localOffer->GetMediaSection(i), i,
        "Local reoffer after gathering should have RTCP candidates "
        "(unless m=application)");
    mOffCandidates.CheckDefaultRtcpCandidate(
        types[i] != SdpMediaSection::kApplication,
        localOffer->GetMediaSection(i), i,
        "Local reoffer after gathering should have a default RTCP candidate "
        "(unless m=application)");
    CheckEndOfCandidates(true, localOffer->GetMediaSection(i),
        "Local reoffer after gathering should have an end-of-candidates.");
  }

  UniquePtr<Sdp> remoteOffer(Parse(mSessionAns.GetRemoteDescription()));
  for (size_t i = 0; i < remoteOffer->GetMediaSectionCount(); ++i) {
    mOffCandidates.CheckRtpCandidates(
        true, remoteOffer->GetMediaSection(i), i,
        "Remote reoffer after trickle should have RTP candidates.");
    mOffCandidates.CheckDefaultRtpCandidate(
        i == 0, remoteOffer->GetMediaSection(i), i,
        "Remote reoffer should have a default RTP candidate on level 0 "
        "(because it was gathered last offer/answer).");
    mOffCandidates.CheckRtcpCandidates(
        types[i] != SdpMediaSection::kApplication,
        remoteOffer->GetMediaSection(i), i,
        "Remote reoffer after trickle should have RTCP candidates.");
    mOffCandidates.CheckDefaultRtcpCandidate(
        false, remoteOffer->GetMediaSection(i), i,
        "Remote reoffer should not have a default RTCP candidate.");
    CheckEndOfCandidates(false, remoteOffer->GetMediaSection(i),
        "Remote reoffer should not have an end-of-candidates.");
  }

  answer = CreateAnswer();
  SetLocalAnswer(answer);
  SetRemoteAnswer(answer);
  // No candidates should be gathered at the answerer, but default candidates
  // should be set.
  mAnsCandidates.FinishGathering(mSessionAns);

  UniquePtr<Sdp> localAnswer(Parse(mSessionAns.GetLocalDescription()));
  for (size_t i = 0; i < localAnswer->GetMediaSectionCount(); ++i) {
    mAnsCandidates.CheckRtpCandidates(
        i == 0, localAnswer->GetMediaSection(i), i,
        "Local reanswer after gathering should have RTP candidates on level "
        "0.");
    mAnsCandidates.CheckDefaultRtpCandidate(
        true, localAnswer->GetMediaSection(i), 0,
        "Local reanswer after gathering should have a default RTP candidate "
        "on all levels that matches transport level 0.");
    mAnsCandidates.CheckRtcpCandidates(
        false, localAnswer->GetMediaSection(i), i,
        "Local reanswer after gathering should not have RTCP candidates "
        "(because we're reanswering with rtcp-mux)");
    mAnsCandidates.CheckDefaultRtcpCandidate(
        false, localAnswer->GetMediaSection(i), i,
        "Local reanswer after gathering should not have a default RTCP "
        "candidate (because we're reanswering with rtcp-mux)");
    CheckEndOfCandidates(i == 0, localAnswer->GetMediaSection(i),
        "Local reanswer after gathering should have an end-of-candidates only "
        "for level 0.");
  }

  UniquePtr<Sdp> remoteAnswer(Parse(mSessionOff.GetRemoteDescription()));
  for (size_t i = 0; i < localAnswer->GetMediaSectionCount(); ++i) {
    mAnsCandidates.CheckRtpCandidates(
        i == 0, remoteAnswer->GetMediaSection(i), i,
        "Remote reanswer after trickle should have RTP candidates on level 0.");
    mAnsCandidates.CheckDefaultRtpCandidate(
        i == 0, remoteAnswer->GetMediaSection(i), i,
        "Remote reanswer should have a default RTP candidate on level 0 "
        "(because it was gathered last offer/answer).");
    mAnsCandidates.CheckRtcpCandidates(
        false, remoteAnswer->GetMediaSection(i), i,
        "Remote reanswer after trickle should not have RTCP candidates "
        "(because we're reanswering with rtcp-mux)");
    mAnsCandidates.CheckDefaultRtcpCandidate(
        false, remoteAnswer->GetMediaSection(i), i,
        "Remote reanswer after trickle should not have a default RTCP "
        "candidate.");
    CheckEndOfCandidates(false, remoteAnswer->GetMediaSection(i),
        "Remote reanswer after trickle should not have an end-of-candidates.");
  }
}


INSTANTIATE_TEST_CASE_P(
    Variants,
    JsepSessionTest,
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
                      "datachannel,video,audio",
                      "audio,datachannel,video",
                      "video,datachannel,audio",
                      "audio,audio",
                      "video,video",
                      "audio,audio,video",
                      "audio,video,video",
                      "audio,audio,video,video",
                      "audio,audio,video,video,datachannel"));

// offerToReceiveXxx variants

TEST_F(JsepSessionTest, OfferAnswerRecvOnlyLines)
{
  JsepOfferOptions options;
  options.mOfferToReceiveAudio = Some(static_cast<size_t>(1U));
  options.mOfferToReceiveVideo = Some(static_cast<size_t>(2U));
  options.mDontOfferDataChannel = Some(true);
  std::string offer = CreateOffer(Some(options));

  UniquePtr<Sdp> parsedOffer(Parse(offer));
  ASSERT_TRUE(!!parsedOffer);

  ASSERT_EQ(3U, parsedOffer->GetMediaSectionCount());
  ASSERT_EQ(SdpMediaSection::kAudio,
            parsedOffer->GetMediaSection(0).GetMediaType());
  ASSERT_EQ(SdpDirectionAttribute::kRecvonly,
            parsedOffer->GetMediaSection(0).GetAttributeList().GetDirection());
  ASSERT_TRUE(parsedOffer->GetMediaSection(0).GetAttributeList().HasAttribute(
        SdpAttribute::kSsrcAttribute));

  ASSERT_EQ(SdpMediaSection::kVideo,
            parsedOffer->GetMediaSection(1).GetMediaType());
  ASSERT_EQ(SdpDirectionAttribute::kRecvonly,
            parsedOffer->GetMediaSection(1).GetAttributeList().GetDirection());
  ASSERT_TRUE(parsedOffer->GetMediaSection(1).GetAttributeList().HasAttribute(
        SdpAttribute::kSsrcAttribute));

  ASSERT_EQ(SdpMediaSection::kVideo,
            parsedOffer->GetMediaSection(2).GetMediaType());
  ASSERT_EQ(SdpDirectionAttribute::kRecvonly,
            parsedOffer->GetMediaSection(2).GetAttributeList().GetDirection());
  ASSERT_TRUE(parsedOffer->GetMediaSection(2).GetAttributeList().HasAttribute(
        SdpAttribute::kSsrcAttribute));

  ASSERT_TRUE(parsedOffer->GetMediaSection(0).GetAttributeList().HasAttribute(
      SdpAttribute::kRtcpMuxAttribute));
  ASSERT_TRUE(parsedOffer->GetMediaSection(1).GetAttributeList().HasAttribute(
      SdpAttribute::kRtcpMuxAttribute));
  ASSERT_TRUE(parsedOffer->GetMediaSection(2).GetAttributeList().HasAttribute(
      SdpAttribute::kRtcpMuxAttribute));

  SetLocalOffer(offer, CHECK_SUCCESS);

  AddTracks(mSessionAns, "audio,video");
  SetRemoteOffer(offer, CHECK_SUCCESS);

  std::string answer = CreateAnswer();
  UniquePtr<Sdp> parsedAnswer(Parse(answer));

  ASSERT_EQ(3U, parsedAnswer->GetMediaSectionCount());
  ASSERT_EQ(SdpMediaSection::kAudio,
            parsedAnswer->GetMediaSection(0).GetMediaType());
  ASSERT_EQ(SdpDirectionAttribute::kSendonly,
            parsedAnswer->GetMediaSection(0).GetAttributeList().GetDirection());
  ASSERT_EQ(SdpMediaSection::kVideo,
            parsedAnswer->GetMediaSection(1).GetMediaType());
  ASSERT_EQ(SdpDirectionAttribute::kSendonly,
            parsedAnswer->GetMediaSection(1).GetAttributeList().GetDirection());
  ASSERT_EQ(SdpMediaSection::kVideo,
            parsedAnswer->GetMediaSection(2).GetMediaType());
  ASSERT_EQ(SdpDirectionAttribute::kInactive,
            parsedAnswer->GetMediaSection(2).GetAttributeList().GetDirection());

  SetLocalAnswer(answer, CHECK_SUCCESS);
  SetRemoteAnswer(answer, CHECK_SUCCESS);

  std::vector<JsepTrackPair> trackPairs(mSessionOff.GetNegotiatedTrackPairs());
  ASSERT_EQ(2U, trackPairs.size());
  for (auto pair : trackPairs) {
    auto ssrcs = parsedOffer->GetMediaSection(pair.mLevel).GetAttributeList()
                 .GetSsrc().mSsrcs;
    ASSERT_EQ(1U, ssrcs.size());
    ASSERT_EQ(pair.mRecvonlySsrc, ssrcs.front().ssrc);
  }
}

TEST_F(JsepSessionTest, OfferAnswerSendOnlyLines)
{
  AddTracks(mSessionOff, "audio,video,video");

  JsepOfferOptions options;
  options.mOfferToReceiveAudio = Some(static_cast<size_t>(0U));
  options.mOfferToReceiveVideo = Some(static_cast<size_t>(1U));
  options.mDontOfferDataChannel = Some(true);
  std::string offer = CreateOffer(Some(options));

  UniquePtr<Sdp> outputSdp(Parse(offer));
  ASSERT_TRUE(!!outputSdp);

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

  AddTracks(mSessionAns, "audio,video");
  SetRemoteOffer(offer, CHECK_SUCCESS);

  std::string answer = CreateAnswer();
  outputSdp = Parse(answer);

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

TEST_F(JsepSessionTest, OfferToReceiveAudioNotUsed)
{
  JsepOfferOptions options;
  options.mOfferToReceiveAudio = Some<size_t>(1);

  OfferAnswer(CHECK_SUCCESS, Some(options));

  UniquePtr<Sdp> offer(Parse(mSessionOff.GetLocalDescription()));
  ASSERT_TRUE(offer.get());
  ASSERT_EQ(1U, offer->GetMediaSectionCount());
  ASSERT_EQ(SdpMediaSection::kAudio,
            offer->GetMediaSection(0).GetMediaType());
  ASSERT_EQ(SdpDirectionAttribute::kRecvonly,
            offer->GetMediaSection(0).GetAttributeList().GetDirection());

  UniquePtr<Sdp> answer(Parse(mSessionAns.GetLocalDescription()));
  ASSERT_TRUE(answer.get());
  ASSERT_EQ(1U, answer->GetMediaSectionCount());
  ASSERT_EQ(SdpMediaSection::kAudio,
            answer->GetMediaSection(0).GetMediaType());
  ASSERT_EQ(SdpDirectionAttribute::kInactive,
            answer->GetMediaSection(0).GetAttributeList().GetDirection());
}

TEST_F(JsepSessionTest, OfferToReceiveVideoNotUsed)
{
  JsepOfferOptions options;
  options.mOfferToReceiveVideo = Some<size_t>(1);

  OfferAnswer(CHECK_SUCCESS, Some(options));

  UniquePtr<Sdp> offer(Parse(mSessionOff.GetLocalDescription()));
  ASSERT_TRUE(offer.get());
  ASSERT_EQ(1U, offer->GetMediaSectionCount());
  ASSERT_EQ(SdpMediaSection::kVideo,
            offer->GetMediaSection(0).GetMediaType());
  ASSERT_EQ(SdpDirectionAttribute::kRecvonly,
            offer->GetMediaSection(0).GetAttributeList().GetDirection());

  UniquePtr<Sdp> answer(Parse(mSessionAns.GetLocalDescription()));
  ASSERT_TRUE(answer.get());
  ASSERT_EQ(1U, answer->GetMediaSectionCount());
  ASSERT_EQ(SdpMediaSection::kVideo,
            answer->GetMediaSection(0).GetMediaType());
  ASSERT_EQ(SdpDirectionAttribute::kInactive,
            answer->GetMediaSection(0).GetAttributeList().GetDirection());
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

  UniquePtr<Sdp> outputSdp(Parse(offer));
  ASSERT_TRUE(!!outputSdp);

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

  UniquePtr<Sdp> outputSdp(Parse(offer));
  ASSERT_TRUE(!!outputSdp);

  ASSERT_EQ(2U, outputSdp->GetMediaSectionCount());
  auto& video_section = outputSdp->GetMediaSection(1);
  ASSERT_EQ(SdpMediaSection::kVideo, video_section.GetMediaType());
  auto& video_attrs = video_section.GetAttributeList();
  ASSERT_EQ(SdpDirectionAttribute::kSendrecv, video_attrs.GetDirection());

  ASSERT_EQ(4U, video_section.GetFormats().size());
  ASSERT_EQ("120", video_section.GetFormats()[0]);
  ASSERT_EQ("121", video_section.GetFormats()[1]);
  ASSERT_EQ("126", video_section.GetFormats()[2]);
  ASSERT_EQ("97", video_section.GetFormats()[3]);

  // Validate rtpmap
  ASSERT_TRUE(video_attrs.HasAttribute(SdpAttribute::kRtpmapAttribute));
  auto& rtpmaps = video_attrs.GetRtpmap();
  ASSERT_TRUE(rtpmaps.HasEntry("120"));
  ASSERT_TRUE(rtpmaps.HasEntry("121"));
  ASSERT_TRUE(rtpmaps.HasEntry("126"));
  ASSERT_TRUE(rtpmaps.HasEntry("97"));

  auto& vp8_entry = rtpmaps.GetEntry("120");
  auto& vp9_entry = rtpmaps.GetEntry("121");
  auto& h264_1_entry = rtpmaps.GetEntry("126");
  auto& h264_0_entry = rtpmaps.GetEntry("97");

  ASSERT_EQ("VP8", vp8_entry.name);
  ASSERT_EQ("VP9", vp9_entry.name);
  ASSERT_EQ("H264", h264_1_entry.name);
  ASSERT_EQ("H264", h264_0_entry.name);

  // Validate fmtps
  ASSERT_TRUE(video_attrs.HasAttribute(SdpAttribute::kFmtpAttribute));
  auto& fmtps = video_attrs.GetFmtp().mFmtps;

  ASSERT_EQ(4U, fmtps.size());

  // VP8
  ASSERT_EQ("120", fmtps[0].format);
  ASSERT_TRUE(!!fmtps[0].parameters);
  ASSERT_EQ(SdpRtpmapAttributeList::kVP8, fmtps[0].parameters->codec_type);

  auto& parsed_vp8_params =
      *static_cast<const SdpFmtpAttributeList::VP8Parameters*>(
          fmtps[0].parameters.get());

  ASSERT_EQ((uint32_t)12288, parsed_vp8_params.max_fs);
  ASSERT_EQ((uint32_t)60, parsed_vp8_params.max_fr);

  // VP9
  ASSERT_EQ("121", fmtps[1].format);
  ASSERT_TRUE(!!fmtps[1].parameters);
  ASSERT_EQ(SdpRtpmapAttributeList::kVP9, fmtps[1].parameters->codec_type);

  auto& parsed_vp9_params =
      *static_cast<const SdpFmtpAttributeList::VP8Parameters*>(
          fmtps[1].parameters.get());

  ASSERT_EQ((uint32_t)12288, parsed_vp9_params.max_fs);
  ASSERT_EQ((uint32_t)60, parsed_vp9_params.max_fr);

  // H264 packetization mode 1
  ASSERT_EQ("126", fmtps[2].format);
  ASSERT_TRUE(!!fmtps[2].parameters);
  ASSERT_EQ(SdpRtpmapAttributeList::kH264, fmtps[2].parameters->codec_type);

  auto& parsed_h264_1_params =
      *static_cast<const SdpFmtpAttributeList::H264Parameters*>(
          fmtps[2].parameters.get());

  ASSERT_EQ((uint32_t)0x42e00d, parsed_h264_1_params.profile_level_id);
  ASSERT_TRUE(parsed_h264_1_params.level_asymmetry_allowed);
  ASSERT_EQ(1U, parsed_h264_1_params.packetization_mode);

  // H264 packetization mode 0
  ASSERT_EQ("97", fmtps[3].format);
  ASSERT_TRUE(!!fmtps[3].parameters);
  ASSERT_EQ(SdpRtpmapAttributeList::kH264, fmtps[3].parameters->codec_type);

  auto& parsed_h264_0_params =
      *static_cast<const SdpFmtpAttributeList::H264Parameters*>(
          fmtps[3].parameters.get());

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

  UniquePtr<Sdp> outputSdp(Parse(answer));
  ASSERT_TRUE(!!outputSdp);

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
  ASSERT_TRUE(!!fmtps[0].parameters);
  ASSERT_EQ(SdpRtpmapAttributeList::kVP8, fmtps[0].parameters->codec_type);

  auto& parsed_vp8_params =
      *static_cast<const SdpFmtpAttributeList::VP8Parameters*>(
          fmtps[0].parameters.get());

  ASSERT_EQ((uint32_t)12288, parsed_vp8_params.max_fs);
  ASSERT_EQ((uint32_t)60, parsed_vp8_params.max_fr);


  SetLocalAnswer(answer);
  SetRemoteAnswer(answer);

  auto offerPairs = mSessionOff.GetNegotiatedTrackPairs();
  ASSERT_EQ(2U, offerPairs.size());
  ASSERT_TRUE(offerPairs[1].mSending);
  ASSERT_TRUE(offerPairs[1].mReceiving);
  ASSERT_TRUE(offerPairs[1].mSending->GetNegotiatedDetails());
  ASSERT_TRUE(offerPairs[1].mReceiving->GetNegotiatedDetails());
  ASSERT_EQ(1U,
      offerPairs[1].mSending->GetNegotiatedDetails()->GetCodecCount());
  ASSERT_EQ(1U,
      offerPairs[1].mReceiving->GetNegotiatedDetails()->GetCodecCount());
  const JsepCodecDescription* offerRecvCodec;
  ASSERT_EQ(NS_OK,
      offerPairs[1].mReceiving->GetNegotiatedDetails()->GetCodec(
        0,
        &offerRecvCodec));
  const JsepCodecDescription* offerSendCodec;
  ASSERT_EQ(NS_OK,
      offerPairs[1].mSending->GetNegotiatedDetails()->GetCodec(
        0,
        &offerSendCodec));

  auto answerPairs = mSessionAns.GetNegotiatedTrackPairs();
  ASSERT_EQ(2U, answerPairs.size());
  ASSERT_TRUE(answerPairs[1].mSending);
  ASSERT_TRUE(answerPairs[1].mReceiving);
  ASSERT_TRUE(answerPairs[1].mSending->GetNegotiatedDetails());
  ASSERT_TRUE(answerPairs[1].mReceiving->GetNegotiatedDetails());
  ASSERT_EQ(1U,
      answerPairs[1].mSending->GetNegotiatedDetails()->GetCodecCount());
  ASSERT_EQ(1U,
      answerPairs[1].mReceiving->GetNegotiatedDetails()->GetCodecCount());
  const JsepCodecDescription* answerRecvCodec;
  ASSERT_EQ(NS_OK,
      answerPairs[1].mReceiving->GetNegotiatedDetails()->GetCodec(
        0,
        &answerRecvCodec));
  const JsepCodecDescription* answerSendCodec;
  ASSERT_EQ(NS_OK,
      answerPairs[1].mSending->GetNegotiatedDetails()->GetCodec(
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

static void
Replace(const std::string& toReplace,
        const std::string& with,
        std::string* in)
{
  size_t pos = in->find(toReplace);
  ASSERT_NE(std::string::npos, pos);
  in->replace(pos, toReplace.size(), with);
}

static void ReplaceAll(const std::string& toReplace,
                       const std::string& with,
                       std::string* in)
{
  while (in->find(toReplace) != std::string::npos) {
    Replace(toReplace, with, in);
  }
}

typedef enum
{
  kSending,
  kReceiving
} Direction;

static void
GetCodec(JsepSession& session,
         size_t pairIndex,
         Direction direction,
         size_t codecIndex,
         const JsepCodecDescription** codecOut)
{
  *codecOut = nullptr;
  ASSERT_LT(pairIndex, session.GetNegotiatedTrackPairs().size());
  JsepTrackPair pair(session.GetNegotiatedTrackPairs().front());
  RefPtr<JsepTrack> track(
      (direction == kSending) ? pair.mSending : pair.mReceiving);
  ASSERT_TRUE(track);
  ASSERT_TRUE(track->GetNegotiatedDetails());
  ASSERT_LT(codecIndex, track->GetNegotiatedDetails()->GetCodecCount());
  ASSERT_EQ(NS_OK,
            track->GetNegotiatedDetails()->GetCodec(codecIndex, codecOut));
}

static void
ForceH264(JsepSession& session, uint32_t profileLevelId)
{
  for (JsepCodecDescription* codec : session.Codecs()) {
    if (codec->mName == "H264") {
      JsepVideoCodecDescription* h264 =
          static_cast<JsepVideoCodecDescription*>(codec);
      h264->mProfileLevelId = profileLevelId;
    } else {
      codec->mEnabled = false;
    }
  }
}

TEST_F(JsepSessionTest, TestH264Negotiation)
{
  ForceH264(mSessionOff, 0x42e00b);
  ForceH264(mSessionAns, 0x42e00d);

  AddTracks(mSessionOff, "video");
  AddTracks(mSessionAns, "video");

  std::string offer(CreateOffer());
  SetLocalOffer(offer, CHECK_SUCCESS);

  SetRemoteOffer(offer, CHECK_SUCCESS);
  std::string answer(CreateAnswer());

  SetRemoteAnswer(answer, CHECK_SUCCESS);
  SetLocalAnswer(answer, CHECK_SUCCESS);

  const JsepCodecDescription* offererSendCodec;
  GetCodec(mSessionOff, 0, kSending, 0, &offererSendCodec);
  ASSERT_TRUE(offererSendCodec);
  ASSERT_EQ("H264", offererSendCodec->mName);
  const JsepVideoCodecDescription* offererVideoSendCodec(
      static_cast<const JsepVideoCodecDescription*>(offererSendCodec));
  ASSERT_EQ((uint32_t)0x42e00d, offererVideoSendCodec->mProfileLevelId);

  const JsepCodecDescription* offererRecvCodec;
  GetCodec(mSessionOff, 0, kReceiving, 0, &offererRecvCodec);
  ASSERT_EQ("H264", offererRecvCodec->mName);
  const JsepVideoCodecDescription* offererVideoRecvCodec(
      static_cast<const JsepVideoCodecDescription*>(offererRecvCodec));
  ASSERT_EQ((uint32_t)0x42e00b, offererVideoRecvCodec->mProfileLevelId);

  const JsepCodecDescription* answererSendCodec;
  GetCodec(mSessionAns, 0, kSending, 0, &answererSendCodec);
  ASSERT_TRUE(answererSendCodec);
  ASSERT_EQ("H264", answererSendCodec->mName);
  const JsepVideoCodecDescription* answererVideoSendCodec(
      static_cast<const JsepVideoCodecDescription*>(answererSendCodec));
  ASSERT_EQ((uint32_t)0x42e00b, answererVideoSendCodec->mProfileLevelId);

  const JsepCodecDescription* answererRecvCodec;
  GetCodec(mSessionAns, 0, kReceiving, 0, &answererRecvCodec);
  ASSERT_EQ("H264", answererRecvCodec->mName);
  const JsepVideoCodecDescription* answererVideoRecvCodec(
      static_cast<const JsepVideoCodecDescription*>(answererRecvCodec));
  ASSERT_EQ((uint32_t)0x42e00d, answererVideoRecvCodec->mProfileLevelId);
}

TEST_F(JsepSessionTest, TestH264NegotiationFails)
{
  ForceH264(mSessionOff, 0x42000b);
  ForceH264(mSessionAns, 0x42e00d);

  AddTracks(mSessionOff, "video");
  AddTracks(mSessionAns, "video");

  std::string offer(CreateOffer());
  SetLocalOffer(offer, CHECK_SUCCESS);

  SetRemoteOffer(offer, CHECK_SUCCESS);
  std::string answer(CreateAnswer());

  SetRemoteAnswer(answer, CHECK_SUCCESS);
  SetLocalAnswer(answer, CHECK_SUCCESS);

  ASSERT_EQ(0U, mSessionOff.GetNegotiatedTrackPairs().size());
  ASSERT_EQ(0U, mSessionAns.GetNegotiatedTrackPairs().size());
}

TEST_F(JsepSessionTest, TestH264NegotiationOffererDefault)
{
  ForceH264(mSessionOff, 0x42000d);
  ForceH264(mSessionAns, 0x42000d);

  AddTracks(mSessionOff, "video");
  AddTracks(mSessionAns, "video");

  std::string offer(CreateOffer());
  SetLocalOffer(offer, CHECK_SUCCESS);

  Replace("profile-level-id=42000d",
          "some-unknown-param=0",
          &offer);

  SetRemoteOffer(offer, CHECK_SUCCESS);
  std::string answer(CreateAnswer());

  SetRemoteAnswer(answer, CHECK_SUCCESS);
  SetLocalAnswer(answer, CHECK_SUCCESS);

  const JsepCodecDescription* answererSendCodec;
  GetCodec(mSessionAns, 0, kSending, 0, &answererSendCodec);
  ASSERT_TRUE(answererSendCodec);
  ASSERT_EQ("H264", answererSendCodec->mName);
  const JsepVideoCodecDescription* answererVideoSendCodec(
      static_cast<const JsepVideoCodecDescription*>(answererSendCodec));
  ASSERT_EQ((uint32_t)0x420010, answererVideoSendCodec->mProfileLevelId);
}

TEST_F(JsepSessionTest, TestH264NegotiationOffererNoFmtp)
{
  ForceH264(mSessionOff, 0x42000d);
  ForceH264(mSessionAns, 0x42001e);

  AddTracks(mSessionOff, "video");
  AddTracks(mSessionAns, "video");

  std::string offer(CreateOffer());
  SetLocalOffer(offer, CHECK_SUCCESS);

  Replace("a=fmtp", "a=oops", &offer);

  SetRemoteOffer(offer, CHECK_SUCCESS);
  std::string answer(CreateAnswer());

  SetRemoteAnswer(answer, CHECK_SUCCESS);
  SetLocalAnswer(answer, CHECK_SUCCESS);

  const JsepCodecDescription* answererSendCodec;
  GetCodec(mSessionAns, 0, kSending, 0, &answererSendCodec);
  ASSERT_TRUE(answererSendCodec);
  ASSERT_EQ("H264", answererSendCodec->mName);
  const JsepVideoCodecDescription* answererVideoSendCodec(
      static_cast<const JsepVideoCodecDescription*>(answererSendCodec));
  ASSERT_EQ((uint32_t)0x420010, answererVideoSendCodec->mProfileLevelId);

  const JsepCodecDescription* answererRecvCodec;
  GetCodec(mSessionAns, 0, kReceiving, 0, &answererRecvCodec);
  ASSERT_EQ("H264", answererRecvCodec->mName);
  const JsepVideoCodecDescription* answererVideoRecvCodec(
      static_cast<const JsepVideoCodecDescription*>(answererRecvCodec));
  ASSERT_EQ((uint32_t)0x420010, answererVideoRecvCodec->mProfileLevelId);
}

TEST_F(JsepSessionTest, TestH264LevelAsymmetryDisallowedByOffererWithLowLevel)
{
  ForceH264(mSessionOff, 0x42e00b);
  ForceH264(mSessionAns, 0x42e00d);

  AddTracks(mSessionOff, "video");
  AddTracks(mSessionAns, "video");

  std::string offer(CreateOffer());
  SetLocalOffer(offer, CHECK_SUCCESS);

  Replace("level-asymmetry-allowed=1",
          "level-asymmetry-allowed=0",
          &offer);

  SetRemoteOffer(offer, CHECK_SUCCESS);
  std::string answer(CreateAnswer());

  SetRemoteAnswer(answer, CHECK_SUCCESS);
  SetLocalAnswer(answer, CHECK_SUCCESS);

  // Offerer doesn't know about the shenanigans we've pulled here, so will
  // behave normally, and we test the normal behavior elsewhere.

  const JsepCodecDescription* answererSendCodec;
  GetCodec(mSessionAns, 0, kSending, 0, &answererSendCodec);
  ASSERT_TRUE(answererSendCodec);
  ASSERT_EQ("H264", answererSendCodec->mName);
  const JsepVideoCodecDescription* answererVideoSendCodec(
      static_cast<const JsepVideoCodecDescription*>(answererSendCodec));
  ASSERT_EQ((uint32_t)0x42e00b, answererVideoSendCodec->mProfileLevelId);

  const JsepCodecDescription* answererRecvCodec;
  GetCodec(mSessionAns, 0, kReceiving, 0, &answererRecvCodec);
  ASSERT_EQ("H264", answererRecvCodec->mName);
  const JsepVideoCodecDescription* answererVideoRecvCodec(
      static_cast<const JsepVideoCodecDescription*>(answererRecvCodec));
  ASSERT_EQ((uint32_t)0x42e00b, answererVideoRecvCodec->mProfileLevelId);
}

TEST_F(JsepSessionTest, TestH264LevelAsymmetryDisallowedByOffererWithHighLevel)
{
  ForceH264(mSessionOff, 0x42e00d);
  ForceH264(mSessionAns, 0x42e00b);

  AddTracks(mSessionOff, "video");
  AddTracks(mSessionAns, "video");

  std::string offer(CreateOffer());
  SetLocalOffer(offer, CHECK_SUCCESS);

  Replace("level-asymmetry-allowed=1",
          "level-asymmetry-allowed=0",
          &offer);

  SetRemoteOffer(offer, CHECK_SUCCESS);
  std::string answer(CreateAnswer());

  SetRemoteAnswer(answer, CHECK_SUCCESS);
  SetLocalAnswer(answer, CHECK_SUCCESS);

  // Offerer doesn't know about the shenanigans we've pulled here, so will
  // behave normally, and we test the normal behavior elsewhere.

  const JsepCodecDescription* answererSendCodec;
  GetCodec(mSessionAns, 0, kSending, 0, &answererSendCodec);
  ASSERT_TRUE(answererSendCodec);
  ASSERT_EQ("H264", answererSendCodec->mName);
  const JsepVideoCodecDescription* answererVideoSendCodec(
      static_cast<const JsepVideoCodecDescription*>(answererSendCodec));
  ASSERT_EQ((uint32_t)0x42e00b, answererVideoSendCodec->mProfileLevelId);

  const JsepCodecDescription* answererRecvCodec;
  GetCodec(mSessionAns, 0, kReceiving, 0, &answererRecvCodec);
  ASSERT_EQ("H264", answererRecvCodec->mName);
  const JsepVideoCodecDescription* answererVideoRecvCodec(
      static_cast<const JsepVideoCodecDescription*>(answererRecvCodec));
  ASSERT_EQ((uint32_t)0x42e00b, answererVideoRecvCodec->mProfileLevelId);
}

TEST_F(JsepSessionTest, TestH264LevelAsymmetryDisallowedByAnswererWithLowLevel)
{
  ForceH264(mSessionOff, 0x42e00d);
  ForceH264(mSessionAns, 0x42e00b);

  AddTracks(mSessionOff, "video");
  AddTracks(mSessionAns, "video");

  std::string offer(CreateOffer());
  SetLocalOffer(offer, CHECK_SUCCESS);
  SetRemoteOffer(offer, CHECK_SUCCESS);
  std::string answer(CreateAnswer());

  Replace("level-asymmetry-allowed=1",
          "level-asymmetry-allowed=0",
          &answer);

  SetRemoteAnswer(answer, CHECK_SUCCESS);
  SetLocalAnswer(answer, CHECK_SUCCESS);

  const JsepCodecDescription* offererSendCodec;
  GetCodec(mSessionOff, 0, kSending, 0, &offererSendCodec);
  ASSERT_TRUE(offererSendCodec);
  ASSERT_EQ("H264", offererSendCodec->mName);
  const JsepVideoCodecDescription* offererVideoSendCodec(
      static_cast<const JsepVideoCodecDescription*>(offererSendCodec));
  ASSERT_EQ((uint32_t)0x42e00b, offererVideoSendCodec->mProfileLevelId);

  const JsepCodecDescription* offererRecvCodec;
  GetCodec(mSessionOff, 0, kReceiving, 0, &offererRecvCodec);
  ASSERT_EQ("H264", offererRecvCodec->mName);
  const JsepVideoCodecDescription* offererVideoRecvCodec(
      static_cast<const JsepVideoCodecDescription*>(offererRecvCodec));
  ASSERT_EQ((uint32_t)0x42e00b, offererVideoRecvCodec->mProfileLevelId);

  // Answerer doesn't know we've pulled these shenanigans, it should act as if
  // it did not set level-asymmetry-required, and we already check that
  // elsewhere
}

TEST_F(JsepSessionTest, TestH264LevelAsymmetryDisallowedByAnswererWithHighLevel)
{
  ForceH264(mSessionOff, 0x42e00b);
  ForceH264(mSessionAns, 0x42e00d);

  AddTracks(mSessionOff, "video");
  AddTracks(mSessionAns, "video");

  std::string offer(CreateOffer());
  SetLocalOffer(offer, CHECK_SUCCESS);
  SetRemoteOffer(offer, CHECK_SUCCESS);
  std::string answer(CreateAnswer());

  Replace("level-asymmetry-allowed=1",
          "level-asymmetry-allowed=0",
          &answer);

  SetRemoteAnswer(answer, CHECK_SUCCESS);
  SetLocalAnswer(answer, CHECK_SUCCESS);

  const JsepCodecDescription* offererSendCodec;
  GetCodec(mSessionOff, 0, kSending, 0, &offererSendCodec);
  ASSERT_TRUE(offererSendCodec);
  ASSERT_EQ("H264", offererSendCodec->mName);
  const JsepVideoCodecDescription* offererVideoSendCodec(
      static_cast<const JsepVideoCodecDescription*>(offererSendCodec));
  ASSERT_EQ((uint32_t)0x42e00b, offererVideoSendCodec->mProfileLevelId);

  const JsepCodecDescription* offererRecvCodec;
  GetCodec(mSessionOff, 0, kReceiving, 0, &offererRecvCodec);
  ASSERT_EQ("H264", offererRecvCodec->mName);
  const JsepVideoCodecDescription* offererVideoRecvCodec(
      static_cast<const JsepVideoCodecDescription*>(offererRecvCodec));
  ASSERT_EQ((uint32_t)0x42e00b, offererVideoRecvCodec->mProfileLevelId);

  // Answerer doesn't know we've pulled these shenanigans, it should act as if
  // it did not set level-asymmetry-required, and we already check that
  // elsewhere
}

TEST_P(JsepSessionTest, TestRejectMline)
{
  AddTracks(mSessionOff);
  AddTracks(mSessionAns);

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

  UniquePtr<Sdp> outputSdp(Parse(answer));
  ASSERT_TRUE(!!outputSdp);

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

  size_t numRejected = std::count(types.begin(), types.end(), types.front());
  size_t numAccepted = types.size() - numRejected;

  ASSERT_EQ(numAccepted, mSessionOff.GetNegotiatedTrackPairs().size());
  ASSERT_EQ(numAccepted, mSessionAns.GetNegotiatedTrackPairs().size());

  ASSERT_EQ(types.size(), mSessionOff.GetTransports().size());
  ASSERT_EQ(types.size(), mSessionOff.GetLocalTracks().size());
  ASSERT_EQ(numAccepted, mSessionOff.GetRemoteTracks().size());

  ASSERT_EQ(types.size(), mSessionAns.GetTransports().size());
  ASSERT_EQ(types.size(), mSessionAns.GetLocalTracks().size());
  ASSERT_EQ(types.size(), mSessionAns.GetRemoteTracks().size());
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
  AddTracks(mSessionOff, "audio");
  AddTracks(mSessionAns, "audio");
  std::string offer = CreateOffer();
  SetLocalOffer(offer, CHECK_SUCCESS);

  UniquePtr<Sdp> parsedOffer(Parse(offer));
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
  AddTracks(mSessionOff, "audio");
  AddTracks(mSessionAns, "audio");
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
  AddTracks(mSessionOff, "audio");
  AddTracks(mSessionAns, "audio");
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

  UniquePtr<Sdp> parsedOffer(Parse(offer));
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

  UniquePtr<Sdp> parsedAnswer(Parse(answer));
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
  AddTracks(mSessionOff, "video");
  AddTracks(mSessionAns, "video");

  std::string offer = CreateOffer();

  UniquePtr<Sdp> parsedOffer(Parse(offer));
  auto* rtcpfbs = new SdpRtcpFbAttributeList;
  rtcpfbs->PushEntry("*", SdpRtcpFbAttributeList::kNack);
  parsedOffer->GetMediaSection(0).GetAttributeList().SetAttribute(rtcpfbs);
  offer = parsedOffer->ToString();

  SetLocalOffer(offer, CHECK_SUCCESS);
  SetRemoteOffer(offer, CHECK_SUCCESS);
  std::string answer = CreateAnswer();
  SetLocalAnswer(answer, CHECK_SUCCESS);
  SetRemoteAnswer(answer, CHECK_SUCCESS);

  ASSERT_EQ(1U, mSessionAns.GetRemoteTracks().size());
  RefPtr<JsepTrack> track = mSessionAns.GetRemoteTracks()[0];
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

TEST_F(JsepSessionTest, TestUniquePayloadTypes)
{
  // The audio payload types will all appear more than once, but the video
  // payload types will be unique.
  AddTracks(mSessionOff, "audio,audio,video");
  AddTracks(mSessionAns, "audio,audio,video");

  std::string offer = CreateOffer();
  SetLocalOffer(offer, CHECK_SUCCESS);
  SetRemoteOffer(offer, CHECK_SUCCESS);
  std::string answer = CreateAnswer();
  SetLocalAnswer(answer, CHECK_SUCCESS);
  SetRemoteAnswer(answer, CHECK_SUCCESS);

  auto offerPairs = mSessionOff.GetNegotiatedTrackPairs();
  auto answerPairs = mSessionAns.GetNegotiatedTrackPairs();
  ASSERT_EQ(3U, offerPairs.size());
  ASSERT_EQ(3U, answerPairs.size());

  ASSERT_TRUE(offerPairs[0].mReceiving);
  ASSERT_TRUE(offerPairs[0].mReceiving->GetNegotiatedDetails());
  ASSERT_EQ(0U,
      offerPairs[0].mReceiving->GetNegotiatedDetails()->
      GetUniquePayloadTypes().size());

  ASSERT_TRUE(offerPairs[1].mReceiving);
  ASSERT_TRUE(offerPairs[1].mReceiving->GetNegotiatedDetails());
  ASSERT_EQ(0U,
      offerPairs[1].mReceiving->GetNegotiatedDetails()->
      GetUniquePayloadTypes().size());

  ASSERT_TRUE(offerPairs[2].mReceiving);
  ASSERT_TRUE(offerPairs[2].mReceiving->GetNegotiatedDetails());
  ASSERT_NE(0U,
      offerPairs[2].mReceiving->GetNegotiatedDetails()->
      GetUniquePayloadTypes().size());

  ASSERT_TRUE(answerPairs[0].mReceiving);
  ASSERT_TRUE(answerPairs[0].mReceiving->GetNegotiatedDetails());
  ASSERT_EQ(0U,
      answerPairs[0].mReceiving->GetNegotiatedDetails()->
      GetUniquePayloadTypes().size());

  ASSERT_TRUE(answerPairs[1].mReceiving);
  ASSERT_TRUE(answerPairs[1].mReceiving->GetNegotiatedDetails());
  ASSERT_EQ(0U,
      answerPairs[1].mReceiving->GetNegotiatedDetails()->
      GetUniquePayloadTypes().size());

  ASSERT_TRUE(answerPairs[2].mReceiving);
  ASSERT_TRUE(answerPairs[2].mReceiving->GetNegotiatedDetails());
  ASSERT_NE(0U,
      answerPairs[2].mReceiving->GetNegotiatedDetails()->
      GetUniquePayloadTypes().size());
}

TEST_F(JsepSessionTest, UnknownFingerprintAlgorithm)
{
  types.push_back(SdpMediaSection::kAudio);
  AddTracks(mSessionOff, "audio");
  AddTracks(mSessionAns, "audio");

  std::string offer(CreateOffer());
  SetLocalOffer(offer);
  ReplaceAll("fingerprint:sha", "fingerprint:foo", &offer);
  nsresult rv = mSessionAns.SetRemoteDescription(kJsepSdpOffer, offer);
  ASSERT_NE(NS_OK, rv);
  ASSERT_NE("", mSessionAns.GetLastError());
}

TEST(H264ProfileLevelIdTest, TestLevelComparisons)
{
  ASSERT_LT(JsepVideoCodecDescription::GetSaneH264Level(0x421D0B), // 1b
            JsepVideoCodecDescription::GetSaneH264Level(0x420D0B)); // 1.1
  ASSERT_LT(JsepVideoCodecDescription::GetSaneH264Level(0x420D0A), // 1.0
            JsepVideoCodecDescription::GetSaneH264Level(0x421D0B)); // 1b
  ASSERT_LT(JsepVideoCodecDescription::GetSaneH264Level(0x420D0A), // 1.0
            JsepVideoCodecDescription::GetSaneH264Level(0x420D0B)); // 1.1

  ASSERT_LT(JsepVideoCodecDescription::GetSaneH264Level(0x640009), // 1b
            JsepVideoCodecDescription::GetSaneH264Level(0x64000B)); // 1.1
  ASSERT_LT(JsepVideoCodecDescription::GetSaneH264Level(0x64000A), // 1.0
            JsepVideoCodecDescription::GetSaneH264Level(0x640009)); // 1b
  ASSERT_LT(JsepVideoCodecDescription::GetSaneH264Level(0x64000A), // 1.0
            JsepVideoCodecDescription::GetSaneH264Level(0x64000B)); // 1.1
}

TEST(H264ProfileLevelIdTest, TestLevelSetting)
{
  uint32_t profileLevelId = 0x420D0A;
  JsepVideoCodecDescription::SetSaneH264Level(
      JsepVideoCodecDescription::GetSaneH264Level(0x42100B),
      &profileLevelId);
  ASSERT_EQ((uint32_t)0x421D0B, profileLevelId);

  JsepVideoCodecDescription::SetSaneH264Level(
      JsepVideoCodecDescription::GetSaneH264Level(0x42000A),
      &profileLevelId);
  ASSERT_EQ((uint32_t)0x420D0A, profileLevelId);

  profileLevelId = 0x6E100A;
  JsepVideoCodecDescription::SetSaneH264Level(
      JsepVideoCodecDescription::GetSaneH264Level(0x640009),
      &profileLevelId);
  ASSERT_EQ((uint32_t)0x6E1009, profileLevelId);

  JsepVideoCodecDescription::SetSaneH264Level(
      JsepVideoCodecDescription::GetSaneH264Level(0x64000B),
      &profileLevelId);
  ASSERT_EQ((uint32_t)0x6E100B, profileLevelId);
}

TEST_F(JsepSessionTest, StronglyPreferredCodec)
{
  for (JsepCodecDescription* codec : mSessionAns.Codecs()) {
    if (codec->mName == "H264") {
      codec->mStronglyPreferred = true;
    }
  }

  types.push_back(SdpMediaSection::kVideo);
  AddTracks(mSessionOff, "video");
  AddTracks(mSessionAns, "video");

  OfferAnswer();

  const JsepCodecDescription* codec;
  GetCodec(mSessionAns, 0, kSending, 0, &codec);
  ASSERT_TRUE(codec);
  ASSERT_EQ("H264", codec->mName);
  GetCodec(mSessionAns, 0, kReceiving, 0, &codec);
  ASSERT_TRUE(codec);
  ASSERT_EQ("H264", codec->mName);
}

TEST_F(JsepSessionTest, LowDynamicPayloadType)
{
  SetPayloadTypeNumber(mSessionOff, "opus", "12");
  types.push_back(SdpMediaSection::kAudio);
  AddTracks(mSessionOff, "audio");
  AddTracks(mSessionAns, "audio");

  OfferAnswer();
  const JsepCodecDescription* codec;
  GetCodec(mSessionAns, 0, kSending, 0, &codec);
  ASSERT_TRUE(codec);
  ASSERT_EQ("opus", codec->mName);
  ASSERT_EQ("12", codec->mDefaultPt);
  GetCodec(mSessionAns, 0, kReceiving, 0, &codec);
  ASSERT_TRUE(codec);
  ASSERT_EQ("opus", codec->mName);
  ASSERT_EQ("12", codec->mDefaultPt);
}

TEST_F(JsepSessionTest, PayloadTypeClash)
{
  // Disable this so mSessionOff doesn't have a duplicate
  SetCodecEnabled(mSessionOff, "PCMU", false);
  SetPayloadTypeNumber(mSessionOff, "opus", "0");
  SetPayloadTypeNumber(mSessionAns, "PCMU", "0");
  types.push_back(SdpMediaSection::kAudio);
  AddTracks(mSessionOff, "audio");
  AddTracks(mSessionAns, "audio");

  OfferAnswer();
  const JsepCodecDescription* codec;
  GetCodec(mSessionAns, 0, kSending, 0, &codec);
  ASSERT_TRUE(codec);
  ASSERT_EQ("opus", codec->mName);
  ASSERT_EQ("0", codec->mDefaultPt);
  GetCodec(mSessionAns, 0, kReceiving, 0, &codec);
  ASSERT_TRUE(codec);
  ASSERT_EQ("opus", codec->mName);
  ASSERT_EQ("0", codec->mDefaultPt);

  // Now, make sure that mSessionAns does not put a=rtpmap:0 PCMU in a reoffer,
  // since pt 0 is taken for opus (the answerer still supports PCMU, and will
  // reoffer it, but it should choose a new payload type for it)
  JsepOfferOptions options;
  std::string reoffer;
  nsresult rv = mSessionAns.CreateOffer(options, &reoffer);
  ASSERT_EQ(NS_OK, rv);
  ASSERT_EQ(std::string::npos, reoffer.find("a=rtpmap:0 PCMU")) << reoffer;
}

TEST_P(JsepSessionTest, TestGlareRollback)
{
  AddTracks(mSessionOff);
  AddTracks(mSessionAns);
  JsepOfferOptions options;

  std::string offer;
  ASSERT_EQ(NS_OK, mSessionAns.CreateOffer(options, &offer));
  ASSERT_EQ(NS_OK,
            mSessionAns.SetLocalDescription(kJsepSdpOffer, offer));
  ASSERT_EQ(kJsepStateHaveLocalOffer, mSessionAns.GetState());

  ASSERT_EQ(NS_OK, mSessionOff.CreateOffer(options, &offer));
  ASSERT_EQ(NS_OK,
            mSessionOff.SetLocalDescription(kJsepSdpOffer, offer));
  ASSERT_EQ(kJsepStateHaveLocalOffer, mSessionOff.GetState());

  ASSERT_EQ(NS_ERROR_UNEXPECTED,
            mSessionAns.SetRemoteDescription(kJsepSdpOffer, offer));
  ASSERT_EQ(NS_OK,
            mSessionAns.SetLocalDescription(kJsepSdpRollback, ""));
  ASSERT_EQ(kJsepStateStable, mSessionAns.GetState());

  SetRemoteOffer(offer);

  std::string answer = CreateAnswer();
  SetLocalAnswer(answer);
  SetRemoteAnswer(answer);
}

TEST_P(JsepSessionTest, TestRejectOfferRollback)
{
  AddTracks(mSessionOff);
  AddTracks(mSessionAns);

  std::string offer = CreateOffer();
  SetLocalOffer(offer);
  SetRemoteOffer(offer);

  ASSERT_EQ(NS_OK,
            mSessionAns.SetRemoteDescription(kJsepSdpRollback, ""));
  ASSERT_EQ(kJsepStateStable, mSessionAns.GetState());
  ASSERT_EQ(types.size(), mSessionAns.GetRemoteTracksRemoved().size());

  ASSERT_EQ(NS_OK,
            mSessionOff.SetLocalDescription(kJsepSdpRollback, ""));
  ASSERT_EQ(kJsepStateStable, mSessionOff.GetState());

  OfferAnswer();
}

TEST_P(JsepSessionTest, TestInvalidRollback)
{
  AddTracks(mSessionOff);
  AddTracks(mSessionAns);

  ASSERT_EQ(NS_ERROR_UNEXPECTED,
            mSessionOff.SetLocalDescription(kJsepSdpRollback, ""));
  ASSERT_EQ(NS_ERROR_UNEXPECTED,
            mSessionOff.SetRemoteDescription(kJsepSdpRollback, ""));

  std::string offer = CreateOffer();
  ASSERT_EQ(NS_ERROR_UNEXPECTED,
            mSessionOff.SetLocalDescription(kJsepSdpRollback, ""));
  ASSERT_EQ(NS_ERROR_UNEXPECTED,
            mSessionOff.SetRemoteDescription(kJsepSdpRollback, ""));

  SetLocalOffer(offer);
  ASSERT_EQ(NS_ERROR_UNEXPECTED,
            mSessionOff.SetRemoteDescription(kJsepSdpRollback, ""));

  SetRemoteOffer(offer);
  ASSERT_EQ(NS_ERROR_UNEXPECTED,
            mSessionAns.SetLocalDescription(kJsepSdpRollback, ""));

  std::string answer = CreateAnswer();
  ASSERT_EQ(NS_ERROR_UNEXPECTED,
            mSessionAns.SetLocalDescription(kJsepSdpRollback, ""));

  SetLocalAnswer(answer);
  ASSERT_EQ(NS_ERROR_UNEXPECTED,
            mSessionAns.SetLocalDescription(kJsepSdpRollback, ""));
  ASSERT_EQ(NS_ERROR_UNEXPECTED,
            mSessionAns.SetRemoteDescription(kJsepSdpRollback, ""));

  SetRemoteAnswer(answer);
  ASSERT_EQ(NS_ERROR_UNEXPECTED,
            mSessionOff.SetLocalDescription(kJsepSdpRollback, ""));
  ASSERT_EQ(NS_ERROR_UNEXPECTED,
            mSessionOff.SetRemoteDescription(kJsepSdpRollback, ""));
}

size_t GetActiveTransportCount(const JsepSession& session)
{
  auto transports = session.GetTransports();
  size_t activeTransportCount = 0;
  for (RefPtr<JsepTransport>& transport : transports) {
    activeTransportCount += transport->mComponents;
  }
  return activeTransportCount;
}

TEST_P(JsepSessionTest, TestBalancedBundle)
{
  AddTracks(mSessionOff);
  AddTracks(mSessionAns);

  mSessionOff.SetBundlePolicy(kBundleBalanced);

  std::string offer = CreateOffer();
  SipccSdpParser parser;
  UniquePtr<Sdp> parsedOffer = parser.Parse(offer);
  ASSERT_TRUE(parsedOffer.get());

  std::map<SdpMediaSection::MediaType, SdpMediaSection*> firstByType;

  for (size_t i = 0; i < parsedOffer->GetMediaSectionCount(); ++i) {
    SdpMediaSection& msection(parsedOffer->GetMediaSection(i));
    bool firstOfType = !firstByType.count(msection.GetMediaType());
    if (firstOfType) {
      firstByType[msection.GetMediaType()] = &msection;
    }
    ASSERT_EQ(!firstOfType,
              msection.GetAttributeList().HasAttribute(
                SdpAttribute::kBundleOnlyAttribute));
  }

  SetLocalOffer(offer);
  SetRemoteOffer(offer);
  std::string answer = CreateAnswer();
  SetLocalAnswer(answer);
  SetRemoteAnswer(answer);

  CheckPairs(mSessionOff, "Offerer pairs");
  CheckPairs(mSessionAns, "Answerer pairs");
  EXPECT_EQ(1U, GetActiveTransportCount(mSessionOff));
  EXPECT_EQ(1U, GetActiveTransportCount(mSessionAns));
}

TEST_P(JsepSessionTest, TestMaxBundle)
{
  AddTracks(mSessionOff);
  AddTracks(mSessionAns);

  mSessionOff.SetBundlePolicy(kBundleMaxBundle);
  OfferAnswer();

  std::string offer = mSessionOff.GetLocalDescription();
  SipccSdpParser parser;
  UniquePtr<Sdp> parsedOffer = parser.Parse(offer);
  ASSERT_TRUE(parsedOffer.get());

  ASSERT_FALSE(
      parsedOffer->GetMediaSection(0).GetAttributeList().HasAttribute(
        SdpAttribute::kBundleOnlyAttribute));
  for (size_t i = 1; i < parsedOffer->GetMediaSectionCount(); ++i) {
    ASSERT_TRUE(
        parsedOffer->GetMediaSection(i).GetAttributeList().HasAttribute(
          SdpAttribute::kBundleOnlyAttribute));
  }


  CheckPairs(mSessionOff, "Offerer pairs");
  CheckPairs(mSessionAns, "Answerer pairs");
  EXPECT_EQ(1U, GetActiveTransportCount(mSessionOff));
  EXPECT_EQ(1U, GetActiveTransportCount(mSessionAns));
}

TEST_F(JsepSessionTest, TestNonDefaultProtocol)
{
  AddTracks(mSessionOff, "audio,video,datachannel");
  AddTracks(mSessionAns, "audio,video,datachannel");

  std::string offer;
  ASSERT_EQ(NS_OK, mSessionOff.CreateOffer(JsepOfferOptions(), &offer));
  offer.replace(offer.find("UDP/TLS/RTP/SAVPF"),
                strlen("UDP/TLS/RTP/SAVPF"),
                "RTP/SAVPF");
  offer.replace(offer.find("UDP/TLS/RTP/SAVPF"),
                strlen("UDP/TLS/RTP/SAVPF"),
                "RTP/SAVPF");
  mSessionOff.SetLocalDescription(kJsepSdpOffer, offer);
  mSessionAns.SetRemoteDescription(kJsepSdpOffer, offer);

  std::string answer;
  mSessionAns.CreateAnswer(JsepAnswerOptions(), &answer);
  UniquePtr<Sdp> parsedAnswer = Parse(answer);
  ASSERT_EQ(3U, parsedAnswer->GetMediaSectionCount());
  ASSERT_EQ(SdpMediaSection::kRtpSavpf,
            parsedAnswer->GetMediaSection(0).GetProtocol());
  ASSERT_EQ(SdpMediaSection::kRtpSavpf,
            parsedAnswer->GetMediaSection(1).GetProtocol());

  mSessionAns.SetLocalDescription(kJsepSdpAnswer, answer);
  mSessionOff.SetRemoteDescription(kJsepSdpAnswer, answer);

  // Make sure reoffer uses the same protocol as before
  mSessionOff.CreateOffer(JsepOfferOptions(), &offer);
  UniquePtr<Sdp> parsedOffer = Parse(offer);
  ASSERT_EQ(3U, parsedOffer->GetMediaSectionCount());
  ASSERT_EQ(SdpMediaSection::kRtpSavpf,
            parsedOffer->GetMediaSection(0).GetProtocol());
  ASSERT_EQ(SdpMediaSection::kRtpSavpf,
            parsedOffer->GetMediaSection(1).GetProtocol());

  // Make sure reoffer from other side uses the same protocol as before
  mSessionAns.CreateOffer(JsepOfferOptions(), &offer);
  parsedOffer = Parse(offer);
  ASSERT_EQ(3U, parsedOffer->GetMediaSectionCount());
  ASSERT_EQ(SdpMediaSection::kRtpSavpf,
            parsedOffer->GetMediaSection(0).GetProtocol());
  ASSERT_EQ(SdpMediaSection::kRtpSavpf,
            parsedOffer->GetMediaSection(1).GetProtocol());
}

} // namespace mozilla

int
main(int argc, char** argv)
{
  // Prevents some log spew
  ScopedXPCOM xpcom("jsep_session_unittest");

  NSS_NoDB_Init(nullptr);
  NSS_SetDomesticPolicy();

  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
