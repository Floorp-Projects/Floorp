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

#include "mozilla/Preferences.h"
#include "mozilla/RefPtr.h"
#include "mozilla/Tuple.h"

#define GTEST_HAS_RTTI 0
#include "gtest/gtest.h"

#include "signaling/src/sdp/SdpMediaSection.h"
#include "signaling/src/sdp/SipccSdpParser.h"
#include "signaling/src/jsep/JsepCodecDescription.h"
#include "signaling/src/jsep/JsepTrack.h"
#include "signaling/src/jsep/JsepSession.h"
#include "signaling/src/jsep/JsepSessionImpl.h"
#include "signaling/src/jsep/JsepTrack.h"

namespace mozilla {
static std::string kAEqualsCandidate("a=candidate:");
const static size_t kNumCandidatesPerComponent = 3;

class JsepSessionTestBase : public ::testing::Test {
 public:
  static void SetUpTestCase() {
    NSS_NoDB_Init(nullptr);
    NSS_SetDomesticPolicy();
  }
};

class FakeUuidGenerator : public mozilla::JsepUuidGenerator {
 public:
  bool Generate(std::string* str) {
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
                        public ::testing::WithParamInterface<std::string> {
 public:
  JsepSessionTest() : mSdpHelper(&mLastError) {
    Preferences::SetCString("media.peerconnection.sdp.parser", "legacy");
    Preferences::SetCString("media.peerconnection.sdp.alternate_parse_mode",
                            "never");
    Preferences::SetBool("media.peerconnection.video.use_rtx", true);
    Preferences::SetBool("media.navigator.video.use_transport_cc", true);

    mSessionOff =
        MakeUnique<JsepSessionImpl>("Offerer", MakeUnique<FakeUuidGenerator>());
    mSessionAns = MakeUnique<JsepSessionImpl>("Answerer",
                                              MakeUnique<FakeUuidGenerator>());

    EXPECT_EQ(NS_OK, mSessionOff->Init());
    EXPECT_EQ(NS_OK, mSessionAns->Init());

    mOffererTransport = MakeUnique<TransportData>();
    mAnswererTransport = MakeUnique<TransportData>();

    AddTransportData(*mSessionOff, *mOffererTransport);
    AddTransportData(*mSessionAns, *mAnswererTransport);

    mOffCandidates = MakeUnique<CandidateSet>();
    mAnsCandidates = MakeUnique<CandidateSet>();
  }

 protected:
  struct TransportData {
    std::map<std::string, std::vector<uint8_t>> mFingerprints;
  };

  void AddDtlsFingerprint(const std::string& alg, JsepSessionImpl& session,
                          TransportData& tdata) {
    std::vector<uint8_t> fp;
    fp.assign((alg == "sha-1") ? 20 : 32,
              (session.GetName() == "Offerer") ? 0x4f : 0x41);
    session.AddDtlsFingerprint(alg, fp);
    tdata.mFingerprints[alg] = fp;
  }

  void AddTransportData(JsepSessionImpl& session, TransportData& tdata) {
    AddDtlsFingerprint("sha-1", session, tdata);
    AddDtlsFingerprint("sha-256", session, tdata);
  }

  void CheckTransceiverInvariants(
      const std::map<size_t, RefPtr<JsepTransceiver>>& oldTransceivers,
      const std::map<size_t, RefPtr<JsepTransceiver>>& newTransceivers) {
    ASSERT_LE(oldTransceivers.size(), newTransceivers.size());
    std::set<size_t> levels;

    for (const auto& [id, newTransceiver] : newTransceivers) {
      (void)id;  // Lame, but no better way to do this right now.
      if (newTransceiver->HasLevel()) {
        ASSERT_FALSE(levels.count(newTransceiver->GetLevel()))
        << "Two new transceivers are mapped to level "
        << newTransceiver->GetLevel();
        levels.insert(newTransceiver->GetLevel());
      }
    }

    auto last = levels.rbegin();
    if (last != levels.rend()) {
      ASSERT_LE(*last, levels.size())
          << "Max level observed in transceivers was " << *last
          << ", but there are only " << levels.size()
          << " levels in the "
             "transceivers.";
    }

    for (const auto& [id, oldTransceiver] : oldTransceivers) {
      (void)id;  // Lame, but no better way to do this right now.
      if (oldTransceiver->HasLevel()) {
        ASSERT_TRUE(levels.count(oldTransceiver->GetLevel()))
        << "Level " << oldTransceiver->GetLevel()
        << " had a transceiver in the old, but not the new (or, "
           "perhaps this level had more than one transceiver in the "
           "old)";
        levels.erase(oldTransceiver->GetLevel());
      }
    }
  }

  std::map<size_t, RefPtr<JsepTransceiver>> DeepCopy(
      const std::map<size_t, RefPtr<JsepTransceiver>>& transceivers) {
    std::map<size_t, RefPtr<JsepTransceiver>> copy;
    for (const auto& [id, transceiver] : transceivers) {
      copy[id] = new JsepTransceiver(*transceiver);
    }
    return copy;
  }

  std::string CreateOffer(const Maybe<JsepOfferOptions>& options = Nothing()) {
    std::map<size_t, RefPtr<JsepTransceiver>> transceiversBefore =
        DeepCopy(mSessionOff->GetTransceivers());
    JsepOfferOptions defaultOptions;
    const JsepOfferOptions& optionsRef = options ? *options : defaultOptions;
    std::string offer;
    JsepSession::Result result = mSessionOff->CreateOffer(optionsRef, &offer);
    EXPECT_FALSE(result.mError.isSome()) << mSessionOff->GetLastError();

    std::cerr << "OFFER: " << offer << std::endl;

    ValidateTransport(*mOffererTransport, offer);

    if (transceiversBefore.size() != mSessionOff->GetTransceivers().size()) {
      EXPECT_TRUE(false) << "CreateOffer changed number of transceivers!";
      return offer;
    }

    CheckTransceiverInvariants(transceiversBefore,
                               mSessionOff->GetTransceivers());

    for (size_t i = 0; i < transceiversBefore.size(); ++i) {
      RefPtr<JsepTransceiver>& oldTransceiver = transceiversBefore[i];
      RefPtr<JsepTransceiver>& newTransceiver =
          mSessionOff->GetTransceivers()[i];
      EXPECT_EQ(oldTransceiver->IsStopped(), newTransceiver->IsStopped());

      if (oldTransceiver->IsStopped()) {
        if (!newTransceiver->HasLevel()) {
          // Tolerate unmapping of stopped transceivers by removing this
          // difference.
          oldTransceiver->ClearLevel();
        }
      } else if (!oldTransceiver->HasLevel()) {
        EXPECT_TRUE(newTransceiver->HasLevel());
        // Tolerate new mappings.
        oldTransceiver->SetLevel(newTransceiver->GetLevel());
      }

      EXPECT_TRUE(Equals(*oldTransceiver, *newTransceiver));
    }

    return offer;
  }

  typedef enum { NO_ADDTRACK_MAGIC, ADDTRACK_MAGIC } AddTrackMagic;

  void AddTracks(JsepSessionImpl& side, AddTrackMagic magic = ADDTRACK_MAGIC) {
    // Add tracks.
    if (types.empty()) {
      types = BuildTypes(GetParam());
    }
    AddTracks(side, types, magic);

    // Now, we move datachannel to the end
    auto it =
        std::find(types.begin(), types.end(), SdpMediaSection::kApplication);
    if (it != types.end()) {
      types.erase(it);
      types.push_back(SdpMediaSection::kApplication);
    }
  }

  void AddTracks(JsepSessionImpl& side, const std::string& mediatypes,
                 AddTrackMagic magic = ADDTRACK_MAGIC) {
    AddTracks(side, BuildTypes(mediatypes), magic);
  }

  JsepTrack RemoveTrack(JsepSession& side, size_t index) {
    if (side.GetTransceivers().size() <= index) {
      EXPECT_TRUE(false) << "Index " << index << " out of bounds!";
      return JsepTrack(SdpMediaSection::kAudio, sdp::kSend);
    }

    RefPtr<JsepTransceiver>& transceiver(side.GetTransceivers()[index]);
    JsepTrack& track = transceiver->mSendTrack;
    EXPECT_FALSE(track.GetStreamIds().empty()) << "No track at index " << index;

    JsepTrack original(track);
    track.ClearStreamIds();
    transceiver->mJsDirection &= SdpDirectionAttribute::Direction::kRecvonly;
    return original;
  }

  void SetDirection(JsepSession& side, size_t index,
                    SdpDirectionAttribute::Direction direction) {
    ASSERT_LT(index, side.GetTransceivers().size())
        << "Index " << index << " out of bounds!";

    side.GetTransceivers()[index]->mJsDirection = direction;
  }

  std::vector<SdpMediaSection::MediaType> BuildTypes(
      const std::string& mediatypes) {
    std::vector<SdpMediaSection::MediaType> result;
    size_t ptr = 0;

    for (;;) {
      size_t comma = mediatypes.find(',', ptr);
      std::string chunk = mediatypes.substr(ptr, comma - ptr);

      if (chunk == "audio") {
        result.push_back(SdpMediaSection::kAudio);
      } else if (chunk == "video") {
        result.push_back(SdpMediaSection::kVideo);
      } else if (chunk == "datachannel") {
        result.push_back(SdpMediaSection::kApplication);
      } else {
        MOZ_CRASH();
      }

      if (comma == std::string::npos) break;
      ptr = comma + 1;
    }

    return result;
  }

  void AddTracks(JsepSessionImpl& side,
                 const std::vector<SdpMediaSection::MediaType>& mediatypes,
                 AddTrackMagic magic = ADDTRACK_MAGIC) {
    FakeUuidGenerator uuid_gen;
    std::string stream_id;
    std::string track_id;

    ASSERT_TRUE(uuid_gen.Generate(&stream_id));

    AddTracksToStream(side, stream_id, mediatypes, magic);
  }

  void AddTracksToStream(JsepSessionImpl& side, const std::string stream_id,
                         const std::string& mediatypes,
                         AddTrackMagic magic = ADDTRACK_MAGIC) {
    AddTracksToStream(side, stream_id, BuildTypes(mediatypes), magic);
  }

  // A bit of a hack. JsepSessionImpl populates the track-id automatically, just
  // in case, because the w3c spec requires msid to be set even when there's no
  // send track.
  bool IsNull(const JsepTrack& track) const {
    return track.GetStreamIds().empty() &&
           (track.GetMediaType() != SdpMediaSection::MediaType::kApplication);
  }

  void AddTracksToStream(
      JsepSessionImpl& side, const std::string stream_id,
      const std::vector<SdpMediaSection::MediaType>& mediatypes,
      AddTrackMagic magic = ADDTRACK_MAGIC)

  {
    FakeUuidGenerator uuid_gen;
    std::string track_id;

    for (auto type : mediatypes) {
      ASSERT_TRUE(uuid_gen.Generate(&track_id));

      RefPtr<JsepTransceiver> suitableTransceiver;
      size_t i;
      if (magic == ADDTRACK_MAGIC) {
        for (auto& [id, transceiver] : side.GetTransceivers()) {
          if (transceiver->mSendTrack.GetMediaType() != type) {
            continue;
          }

          if (IsNull(transceiver->mSendTrack) ||
              transceiver->GetMediaType() == SdpMediaSection::kApplication) {
            suitableTransceiver = transceiver;
            i = id;
            break;
          }
        }
      }

      if (!suitableTransceiver) {
        suitableTransceiver = new JsepTransceiver(type);
        side.AddTransceiver(suitableTransceiver);
        i = side.GetTransceivers().rbegin()->first;
      }

      std::cerr << "Updating send track for transceiver " << i << std::endl;
      if (magic == ADDTRACK_MAGIC) {
        suitableTransceiver->SetAddTrackMagic();
      }
      suitableTransceiver->mJsDirection |=
          SdpDirectionAttribute::Direction::kSendonly;
      suitableTransceiver->mSendTrack.UpdateStreamIds(
          std::vector<std::string>(1, stream_id));
    }
  }

  bool HasMediaStream(const std::vector<JsepTrack>& tracks) const {
    for (const auto& track : tracks) {
      if (track.GetMediaType() != SdpMediaSection::kApplication) {
        return true;
      }
    }
    return false;
  }

  const std::string GetFirstLocalStreamId(JsepSessionImpl& side) const {
    auto tracks = GetLocalTracks(side);
    return tracks.begin()->GetStreamIds()[0];
  }

  std::vector<JsepTrack> GetLocalTracks(const JsepSession& session) const {
    std::vector<JsepTrack> result;
    for (const auto& [id, transceiver] : session.GetTransceivers()) {
      (void)id;  // Lame, but no better way to do this right now.
      if (!IsNull(transceiver->mSendTrack)) {
        result.push_back(transceiver->mSendTrack);
      }
    }
    return result;
  }

  std::vector<JsepTrack> GetRemoteTracks(const JsepSession& session) const {
    std::vector<JsepTrack> result;
    for (const auto& [id, transceiver] : session.GetTransceivers()) {
      (void)id;  // Lame, but no better way to do this right now.
      if (!IsNull(transceiver->mRecvTrack)) {
        result.push_back(transceiver->mRecvTrack);
      }
    }
    return result;
  }

  JsepTransceiver* GetDatachannelTransceiver(JsepSession& side) {
    for (const auto& [id, transceiver] : side.GetTransceivers()) {
      (void)id;  // Lame, but no better way to do this right now.
      if (transceiver->mSendTrack.GetMediaType() ==
          SdpMediaSection::MediaType::kApplication) {
        return transceiver.get();
      }
    }

    return nullptr;
  }

  JsepTransceiver* GetNegotiatedTransceiver(JsepSession& side, size_t index) {
    for (const auto& [id, transceiver] : side.GetTransceivers()) {
      (void)id;  // Lame, but no better way to do this right now.
      if (transceiver->mSendTrack.GetNegotiatedDetails() ||
          transceiver->mRecvTrack.GetNegotiatedDetails()) {
        if (index) {
          --index;
          continue;
        }

        return transceiver.get();
      }
    }

    return nullptr;
  }

  JsepTransceiver* GetTransceiverByLevel(
      const std::map<size_t, RefPtr<JsepTransceiver>>& transceivers,
      size_t level) {
    for (const auto& [id, transceiver] : transceivers) {
      (void)id;  // Lame, but no better way to do this right now.
      if (transceiver->HasLevel() && transceiver->GetLevel() == level) {
        return transceiver.get();
      }
    }

    return nullptr;
  }

  JsepTransceiver* GetTransceiverByLevel(JsepSession& side, size_t level) {
    return GetTransceiverByLevel(side.GetTransceivers(), level);
  }

  std::vector<std::string> GetMediaStreamIds(
      const std::vector<JsepTrack>& tracks) const {
    std::vector<std::string> ids;
    for (const auto& track : tracks) {
      // data channels don't have msid's
      if (track.GetMediaType() == SdpMediaSection::kApplication) {
        continue;
      }
      ids.insert(ids.end(), track.GetStreamIds().begin(),
                 track.GetStreamIds().end());
    }
    return ids;
  }

  std::vector<std::string> GetLocalMediaStreamIds(JsepSessionImpl& side) const {
    return GetMediaStreamIds(GetLocalTracks(side));
  }

  std::vector<std::string> GetRemoteMediaStreamIds(
      JsepSessionImpl& side) const {
    return GetMediaStreamIds(GetRemoteTracks(side));
  }

  std::vector<std::string> sortUniqueStrVector(
      std::vector<std::string> in) const {
    std::sort(in.begin(), in.end());
    auto it = std::unique(in.begin(), in.end());
    in.resize(std::distance(in.begin(), it));
    return in;
  }

  std::vector<std::string> GetLocalUniqueStreamIds(
      JsepSessionImpl& side) const {
    return sortUniqueStrVector(GetLocalMediaStreamIds(side));
  }

  std::vector<std::string> GetRemoteUniqueStreamIds(
      JsepSessionImpl& side) const {
    return sortUniqueStrVector(GetRemoteMediaStreamIds(side));
  }

  JsepTrack GetTrack(JsepSessionImpl& side, SdpMediaSection::MediaType type,
                     size_t index) const {
    for (const auto& [id, transceiver] : side.GetTransceivers()) {
      (void)id;  // Lame, but no better way to do this right now.
      if (IsNull(transceiver->mSendTrack) ||
          transceiver->mSendTrack.GetMediaType() != type) {
        continue;
      }

      if (index != 0) {
        --index;
        continue;
      }

      return transceiver->mSendTrack;
    }

    return JsepTrack(type, sdp::kSend);
  }

  JsepTrack GetTrackOff(size_t index, SdpMediaSection::MediaType type) {
    return GetTrack(*mSessionOff, type, index);
  }

  JsepTrack GetTrackAns(size_t index, SdpMediaSection::MediaType type) {
    return GetTrack(*mSessionAns, type, index);
  }

  bool Equals(const SdpFingerprintAttributeList::Fingerprint& f1,
              const SdpFingerprintAttributeList::Fingerprint& f2) const {
    if (f1.hashFunc != f2.hashFunc) {
      return false;
    }

    if (f1.fingerprint != f2.fingerprint) {
      return false;
    }

    return true;
  }

  bool Equals(const SdpFingerprintAttributeList& f1,
              const SdpFingerprintAttributeList& f2) const {
    if (f1.mFingerprints.size() != f2.mFingerprints.size()) {
      return false;
    }

    for (size_t i = 0; i < f1.mFingerprints.size(); ++i) {
      if (!Equals(f1.mFingerprints[i], f2.mFingerprints[i])) {
        return false;
      }
    }

    return true;
  }

  bool Equals(const UniquePtr<JsepDtlsTransport>& t1,
              const UniquePtr<JsepDtlsTransport>& t2) const {
    if (!t1 && !t2) {
      return true;
    }

    if (!t1 || !t2) {
      return false;
    }

    if (!Equals(t1->GetFingerprints(), t2->GetFingerprints())) {
      return false;
    }

    if (t1->GetRole() != t2->GetRole()) {
      return false;
    }

    return true;
  }

  bool Equals(const UniquePtr<JsepIceTransport>& t1,
              const UniquePtr<JsepIceTransport>& t2) const {
    if (!t1 && !t2) {
      return true;
    }

    if (!t1 || !t2) {
      return false;
    }

    if (t1->GetUfrag() != t2->GetUfrag()) {
      return false;
    }

    if (t1->GetPassword() != t2->GetPassword()) {
      return false;
    }

    return true;
  }

  bool Equals(const JsepTransport& t1, const JsepTransport& t2) const {
    if (t1.mTransportId != t2.mTransportId) {
      std::cerr << "Transport id differs: " << t1.mTransportId << " vs "
                << t2.mTransportId << std::endl;
      return false;
    }

    if (t1.mComponents != t2.mComponents) {
      std::cerr << "Component count differs" << std::endl;
      return false;
    }

    if (!Equals(t1.mIce, t2.mIce)) {
      std::cerr << "ICE differs" << std::endl;
      return false;
    }

    return true;
  }

  bool Equals(const JsepTrack& t1, const JsepTrack& t2) const {
    if (t1.GetMediaType() != t2.GetMediaType()) {
      return false;
    }

    if (t1.GetDirection() != t2.GetDirection()) {
      return false;
    }

    if (t1.GetStreamIds() != t2.GetStreamIds()) {
      return false;
    }

    if (t1.GetActive() != t2.GetActive()) {
      return false;
    }

    if (t1.GetCNAME() != t2.GetCNAME()) {
      return false;
    }

    if (t1.GetSsrcs() != t2.GetSsrcs()) {
      return false;
    }

    return true;
  }

  bool Equals(const JsepTransceiver& p1, const JsepTransceiver& p2) const {
    if (p1.HasLevel() != p2.HasLevel()) {
      std::cerr << "One transceiver has a level, the other doesn't"
                << std::endl;
      return false;
    }

    if (p1.HasLevel() && (p1.GetLevel() != p2.GetLevel())) {
      std::cerr << "Level differs: " << p1.GetLevel() << " vs " << p2.GetLevel()
                << std::endl;
      return false;
    }

    // We don't check things like BundleLevel(), since that can change without
    // any changes to the transport, which is what we're really interested in.

    if (p1.IsStopped() != p2.IsStopped()) {
      std::cerr << "One transceiver is stopped, the other is not" << std::endl;
      return false;
    }

    if (p1.IsAssociated() != p2.IsAssociated()) {
      std::cerr << "One transceiver has a mid, the other doesn't" << std::endl;
      return false;
    }

    if (p1.IsAssociated() && (p1.GetMid() != p2.GetMid())) {
      std::cerr << "mid differs: " << p1.GetMid() << " vs " << p2.GetMid()
                << std::endl;
      return false;
    }

    if (!Equals(p1.mSendTrack, p2.mSendTrack)) {
      std::cerr << "Send track differs" << std::endl;
      return false;
    }

    if (!Equals(p1.mRecvTrack, p2.mRecvTrack)) {
      std::cerr << "Receive track differs" << std::endl;
      return false;
    }

    if (!Equals(p1.mTransport, p2.mTransport)) {
      std::cerr << "Transport differs" << std::endl;
      return false;
    }

    return true;
  }

  bool Equals(const std::map<size_t, RefPtr<JsepTransceiver>>& t1,
              const std::map<size_t, RefPtr<JsepTransceiver>>& t2) const {
    if (t1.size() != t2.size()) {
      std::cerr << "Size differs: t1.size = " << t1.size()
                << ", t2.size = " << t2.size() << std::endl;
      return false;
    }

    for (const auto& [id, transceiver] : t1) {
      if (!t2.count(id)) {
        return false;
      }
      if (!Equals(*transceiver, *t2.at(id))) {
        return false;
      }
    }

    return true;
  }

  size_t GetTrackCount(JsepSessionImpl& side,
                       SdpMediaSection::MediaType type) const {
    size_t result = 0;
    for (const auto& track : GetLocalTracks(side)) {
      if (track.GetMediaType() == type) {
        ++result;
      }
    }
    return result;
  }

  UniquePtr<Sdp> GetParsedLocalDescription(const JsepSessionImpl& side) const {
    return Parse(side.GetLocalDescription(kJsepDescriptionCurrent));
  }

  SdpMediaSection* GetMsection(Sdp& sdp, SdpMediaSection::MediaType type,
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

  void SetPayloadTypeNumber(JsepSession& session, const std::string& codecName,
                            const std::string& payloadType) {
    for (auto& codec : session.Codecs()) {
      if (codec->mName == codecName) {
        codec->mDefaultPt = payloadType;
      }
    }
  }

  void SetCodecEnabled(JsepSession& session, const std::string& codecName,
                       bool enabled) {
    for (auto& codec : session.Codecs()) {
      if (codec->mName == codecName) {
        codec->mEnabled = enabled;
      }
    }
  }

  void EnsureNegotiationFailure(SdpMediaSection::MediaType type,
                                const std::string& codecName) {
    for (auto& codec : mSessionOff->Codecs()) {
      if (codec->mType == type && codec->mName != codecName) {
        codec->mEnabled = false;
      }
    }

    for (auto& codec : mSessionAns->Codecs()) {
      if (codec->mType == type && codec->mName == codecName) {
        codec->mEnabled = false;
      }
    }
  }

  std::string CreateAnswer() {
    std::map<size_t, RefPtr<JsepTransceiver>> transceiversBefore =
        DeepCopy(mSessionAns->GetTransceivers());

    JsepAnswerOptions options;
    std::string answer;

    JsepSession::Result result = mSessionAns->CreateAnswer(options, &answer);
    EXPECT_FALSE(result.mError.isSome());

    std::cerr << "ANSWER: " << answer << std::endl;

    ValidateTransport(*mAnswererTransport, answer);
    CheckTransceiverInvariants(transceiversBefore,
                               mSessionAns->GetTransceivers());

    return answer;
  }

  static const uint32_t NO_CHECKS = 0;
  static const uint32_t CHECK_SUCCESS = 1;
  static const uint32_t CHECK_TRACKS = 1 << 2;
  static const uint32_t ALL_CHECKS = CHECK_SUCCESS | CHECK_TRACKS;

  void OfferAnswer(uint32_t checkFlags = ALL_CHECKS,
                   const Maybe<JsepOfferOptions>& options = Nothing()) {
    std::string offer = CreateOffer(options);
    SetLocalOffer(offer, checkFlags);
    SetRemoteOffer(offer, checkFlags);

    std::string answer = CreateAnswer();
    SetLocalAnswer(answer, checkFlags);
    SetRemoteAnswer(answer, checkFlags);
  }

  void SetLocalOffer(const std::string& offer,
                     uint32_t checkFlags = ALL_CHECKS) {
    std::map<size_t, RefPtr<JsepTransceiver>> transceiversBefore =
        DeepCopy(mSessionOff->GetTransceivers());

    JsepSession::Result result =
        mSessionOff->SetLocalDescription(kJsepSdpOffer, offer);

    CheckTransceiverInvariants(transceiversBefore,
                               mSessionOff->GetTransceivers());

    if (checkFlags & CHECK_SUCCESS) {
      ASSERT_FALSE(result.mError.isSome());
    }

    if (checkFlags & CHECK_TRACKS) {
      // This assumes no recvonly or inactive transceivers.
      ASSERT_EQ(types.size(), mSessionOff->GetTransceivers().size());
      for (const auto& [id, transceiver] : mSessionOff->GetTransceivers()) {
        (void)id;  // Lame, but no better way to do this right now.
        if (!transceiver->HasLevel()) {
          continue;
        }
        const auto& track(transceiver->mSendTrack);
        size_t level = transceiver->GetLevel();
        ASSERT_FALSE(IsNull(track));
        ASSERT_EQ(types[level], track.GetMediaType());
        if (track.GetMediaType() != SdpMediaSection::kApplication) {
          std::string msidAttr("a=msid:");
          msidAttr += track.GetStreamIds()[0];
          ASSERT_NE(std::string::npos, offer.find(msidAttr))
              << "Did not find " << msidAttr << " in offer";
        }
      }
      if (types.size() == 1 && types[0] == SdpMediaSection::kApplication) {
        ASSERT_EQ(std::string::npos, offer.find("a=ssrc"))
            << "Data channel should not contain SSRC";
      }
    }
  }

  void SetRemoteOffer(const std::string& offer,
                      uint32_t checkFlags = ALL_CHECKS) {
    std::map<size_t, RefPtr<JsepTransceiver>> transceiversBefore =
        DeepCopy(mSessionAns->GetTransceivers());

    JsepSession::Result result =
        mSessionAns->SetRemoteDescription(kJsepSdpOffer, offer);

    CheckTransceiverInvariants(transceiversBefore,
                               mSessionAns->GetTransceivers());

    if (checkFlags & CHECK_SUCCESS) {
      ASSERT_FALSE(result.mError.isSome());
    }

    if (checkFlags & CHECK_TRACKS) {
      // This assumes no recvonly or inactive transceivers.
      ASSERT_EQ(types.size(), mSessionAns->GetTransceivers().size());
      for (const auto& [id, transceiver] : mSessionAns->GetTransceivers()) {
        (void)id;  // Lame, but no better way to do this right now.
        if (!transceiver->HasLevel()) {
          continue;
        }
        const auto& track(transceiver->mRecvTrack);
        size_t level = transceiver->GetLevel();
        ASSERT_FALSE(IsNull(track));
        ASSERT_EQ(types[level], track.GetMediaType());
        if (track.GetMediaType() != SdpMediaSection::kApplication) {
          std::string msidAttr("a=msid:");
          msidAttr += track.GetStreamIds()[0];
          // Track id will not match, and will eventually not be present at all
          ASSERT_NE(std::string::npos, offer.find(msidAttr))
              << "Did not find " << msidAttr << " in offer";
        }
      }
    }
  }

  void SetLocalAnswer(const std::string& answer,
                      uint32_t checkFlags = ALL_CHECKS) {
    std::map<size_t, RefPtr<JsepTransceiver>> transceiversBefore =
        DeepCopy(mSessionAns->GetTransceivers());

    JsepSession::Result result =
        mSessionAns->SetLocalDescription(kJsepSdpAnswer, answer);
    if (checkFlags & CHECK_SUCCESS) {
      ASSERT_FALSE(result.mError.isSome());
    }

    CheckTransceiverInvariants(transceiversBefore,
                               mSessionAns->GetTransceivers());

    if (checkFlags & CHECK_TRACKS) {
      // Verify that the right stuff is in the tracks.
      ASSERT_EQ(types.size(), mSessionAns->GetTransceivers().size());
      for (const auto& [id, transceiver] : mSessionAns->GetTransceivers()) {
        (void)id;  // Lame, but no better way to do this right now.
        if (!transceiver->HasLevel()) {
          continue;
        }
        const auto& sendTrack(transceiver->mSendTrack);
        const auto& recvTrack(transceiver->mRecvTrack);
        size_t level = transceiver->GetLevel();
        ASSERT_FALSE(IsNull(sendTrack));
        ASSERT_EQ(types[level], sendTrack.GetMediaType());
        // These might have been in the SDP, or might have been randomly
        // chosen by JsepSessionImpl
        ASSERT_FALSE(IsNull(recvTrack));
        ASSERT_EQ(types[level], recvTrack.GetMediaType());

        if (recvTrack.GetMediaType() != SdpMediaSection::kApplication) {
          std::string msidAttr("a=msid:");
          msidAttr += sendTrack.GetStreamIds()[0];
          ASSERT_NE(std::string::npos, answer.find(msidAttr))
              << "Did not find " << msidAttr << " in answer";
        }
      }
      if (types.size() == 1 && types[0] == SdpMediaSection::kApplication) {
        ASSERT_EQ(std::string::npos, answer.find("a=ssrc"))
            << "Data channel should not contain SSRC";
      }
    }
    std::cerr << "Answerer transceivers:" << std::endl;
    DumpTransceivers(*mSessionAns);
  }

  void SetRemoteAnswer(const std::string& answer,
                       uint32_t checkFlags = ALL_CHECKS) {
    std::map<size_t, RefPtr<JsepTransceiver>> transceiversBefore =
        DeepCopy(mSessionOff->GetTransceivers());

    JsepSession::Result result =
        mSessionOff->SetRemoteDescription(kJsepSdpAnswer, answer);
    if (checkFlags & CHECK_SUCCESS) {
      ASSERT_FALSE(result.mError.isSome());
    }

    CheckTransceiverInvariants(transceiversBefore,
                               mSessionOff->GetTransceivers());

    if (checkFlags & CHECK_TRACKS) {
      // Verify that the right stuff is in the tracks.
      ASSERT_EQ(types.size(), mSessionOff->GetTransceivers().size());
      for (const auto& [id, transceiver] : mSessionOff->GetTransceivers()) {
        (void)id;  // Lame, but no better way to do this right now.
        if (!transceiver->HasLevel()) {
          continue;
        }
        const auto& sendTrack(transceiver->mSendTrack);
        const auto& recvTrack(transceiver->mRecvTrack);
        size_t level = transceiver->GetLevel();
        ASSERT_FALSE(IsNull(sendTrack));
        ASSERT_EQ(types[level], sendTrack.GetMediaType());
        // These might have been in the SDP, or might have been randomly
        // chosen by JsepSessionImpl
        ASSERT_FALSE(IsNull(recvTrack));
        ASSERT_EQ(types[level], recvTrack.GetMediaType());

        if (recvTrack.GetMediaType() != SdpMediaSection::kApplication) {
          std::string msidAttr("a=msid:");
          msidAttr += recvTrack.GetStreamIds()[0];
          // Track id will not match, and will eventually not be present at all
          ASSERT_NE(std::string::npos, answer.find(msidAttr))
              << "Did not find " << msidAttr << " in answer";
        }
      }
    }
    std::cerr << "Offerer transceivers:" << std::endl;
    DumpTransceivers(*mSessionOff);
  }

  std::string GetTransportId(const JsepSession& session, size_t level) {
    for (const auto& [id, transceiver] : session.GetTransceivers()) {
      (void)id;  // Lame, but no better way to do this right now.
      if (transceiver->HasLevel() && transceiver->GetLevel() == level) {
        return transceiver->mTransport.mTransportId;
      }
    }
    return std::string();
  }

  typedef enum { RTP = 1, RTCP = 2 } ComponentType;

  class CandidateSet {
   public:
    CandidateSet() {}

    void Gather(JsepSession& session, ComponentType maxComponent = RTCP) {
      for (const auto& [id, transceiver] : session.GetTransceivers()) {
        (void)id;  // Lame, but no better way to do this right now.
        if (transceiver->HasOwnTransport()) {
          Gather(session, transceiver->mTransport.mTransportId, RTP);
          if (transceiver->mTransport.mComponents > 1) {
            Gather(session, transceiver->mTransport.mTransportId, RTCP);
          }
        }
      }
      FinishGathering(session);
    }

    void Gather(JsepSession& session, const std::string& transportId,
                ComponentType component) {
      static uint16_t port = 1000;
      std::vector<std::string> candidates;

      for (size_t i = 0; i < kNumCandidatesPerComponent; ++i) {
        ++port;
        std::ostringstream candidate;
        candidate << "0 " << static_cast<uint16_t>(component)
                  << " UDP 9999 192.168.0.1 " << port << " typ host";
        std::string mid;
        uint16_t level = 0;
        bool skipped;
        session.AddLocalIceCandidate(kAEqualsCandidate + candidate.str(),
                                     transportId, "", &level, &mid, &skipped);
        if (!skipped) {
          mCandidatesToTrickle.push_back(Tuple<Level, Mid, Candidate>(
              level, mid, kAEqualsCandidate + candidate.str()));
          candidates.push_back(candidate.str());
        }
      }

      // Stomp existing candidates
      mCandidates[transportId][component] = candidates;

      // Stomp existing defaults
      mDefaultCandidates[transportId][component] =
          std::make_pair("192.168.0.1", port);
      session.UpdateDefaultCandidate(
          mDefaultCandidates[transportId][RTP].first,
          mDefaultCandidates[transportId][RTP].second,
          // Will be empty string if not present, which is how we indicate
          // that there is no default for RTCP
          mDefaultCandidates[transportId][RTCP].first,
          mDefaultCandidates[transportId][RTCP].second, transportId);
    }

    void FinishGathering(JsepSession& session) const {
      // Copy so we can be terse and use []
      for (auto idAndCandidates : mDefaultCandidates) {
        ASSERT_EQ(1U, idAndCandidates.second.count(RTP));
        // do a final UpdateDefaultCandidate here in case candidates were
        // cleared during renegotiation.
        session.UpdateDefaultCandidate(
            idAndCandidates.second[RTP].first,
            idAndCandidates.second[RTP].second,
            // Will be empty string if not present, which is how we indicate
            // that there is no default for RTCP
            idAndCandidates.second[RTCP].first,
            idAndCandidates.second[RTCP].second, idAndCandidates.first);
        std::string mid;
        uint16_t level = 0;
        bool skipped;
        session.AddLocalIceCandidate("", idAndCandidates.first, "", &level,
                                     &mid, &skipped);
      }
    }

    void Trickle(JsepSession& session) {
      std::string transportId;
      for (const auto& levelMidAndCandidate : mCandidatesToTrickle) {
        Level level;
        Mid mid;
        Candidate candidate;
        Tie(level, mid, candidate) = levelMidAndCandidate;
        std::cerr << "trickling candidate: " << candidate << " level: " << level
                  << " mid: " << mid << std::endl;
        Maybe<unsigned long> lev = Some(level);
        session.AddRemoteIceCandidate(candidate, mid, lev, "", &transportId);
      }
      session.AddRemoteIceCandidate("", "", Maybe<uint16_t>(), "",
                                    &transportId);
      mCandidatesToTrickle.clear();
    }

    void CheckRtpCandidates(bool expectRtpCandidates,
                            const SdpMediaSection& msection,
                            const std::string& transportId,
                            const std::string& context) const {
      auto& attrs = msection.GetAttributeList();

      ASSERT_EQ(expectRtpCandidates,
                attrs.HasAttribute(SdpAttribute::kCandidateAttribute))
          << context << " (level " << msection.GetLevel() << ")";

      if (expectRtpCandidates) {
        // Copy so we can be terse and use []
        auto expectedCandidates = mCandidates;
        ASSERT_LE(kNumCandidatesPerComponent,
                  expectedCandidates[transportId][RTP].size());

        auto& candidates = attrs.GetCandidate();
        ASSERT_LE(kNumCandidatesPerComponent, candidates.size())
            << context << " (level " << msection.GetLevel() << ")";
        for (size_t i = 0; i < kNumCandidatesPerComponent; ++i) {
          ASSERT_EQ(expectedCandidates[transportId][RTP][i], candidates[i])
              << context << " (level " << msection.GetLevel() << ")";
        }
      }
    }

    void CheckRtcpCandidates(bool expectRtcpCandidates,
                             const SdpMediaSection& msection,
                             const std::string& transportId,
                             const std::string& context) const {
      auto& attrs = msection.GetAttributeList();

      if (expectRtcpCandidates) {
        // Copy so we can be terse and use []
        auto expectedCandidates = mCandidates;
        ASSERT_LE(kNumCandidatesPerComponent,
                  expectedCandidates[transportId][RTCP].size());

        ASSERT_TRUE(attrs.HasAttribute(SdpAttribute::kCandidateAttribute))
        << context << " (level " << msection.GetLevel() << ")";
        auto& candidates = attrs.GetCandidate();
        ASSERT_EQ(kNumCandidatesPerComponent * 2, candidates.size())
            << context << " (level " << msection.GetLevel() << ")";
        for (size_t i = 0; i < kNumCandidatesPerComponent; ++i) {
          ASSERT_EQ(expectedCandidates[transportId][RTCP][i],
                    candidates[i + kNumCandidatesPerComponent])
              << context << " (level " << msection.GetLevel() << ")";
        }
      }
    }

    void CheckDefaultRtpCandidate(bool expectDefault,
                                  const SdpMediaSection& msection,
                                  const std::string& transportId,
                                  const std::string& context) const {
      Address expectedAddress = "0.0.0.0";
      Port expectedPort = 9U;

      if (expectDefault) {
        // Copy so we can be terse and use []
        auto defaultCandidates = mDefaultCandidates;
        expectedAddress = defaultCandidates[transportId][RTP].first;
        expectedPort = defaultCandidates[transportId][RTP].second;
      }

      // if bundle-only attribute is present, expect port 0
      const SdpAttributeList& attrs = msection.GetAttributeList();
      if (attrs.HasAttribute(SdpAttribute::kBundleOnlyAttribute)) {
        expectedPort = 0U;
      }

      ASSERT_EQ(expectedAddress, msection.GetConnection().GetAddress())
          << context << " (level " << msection.GetLevel() << ")";
      ASSERT_EQ(expectedPort, msection.GetPort())
          << context << " (level " << msection.GetLevel() << ")";
    }

    void CheckDefaultRtcpCandidate(bool expectDefault,
                                   const SdpMediaSection& msection,
                                   const std::string& transportId,
                                   const std::string& context) const {
      if (expectDefault) {
        // Copy so we can be terse and use []
        auto defaultCandidates = mDefaultCandidates;
        ASSERT_TRUE(msection.GetAttributeList().HasAttribute(
            SdpAttribute::kRtcpAttribute))
        << context << " (level " << msection.GetLevel() << ")";
        auto& rtcpAttr = msection.GetAttributeList().GetRtcp();
        ASSERT_EQ(defaultCandidates[transportId][RTCP].second, rtcpAttr.mPort)
            << context << " (level " << msection.GetLevel() << ")";
        ASSERT_EQ(sdp::kInternet, rtcpAttr.mNetType)
            << context << " (level " << msection.GetLevel() << ")";
        ASSERT_EQ(sdp::kIPv4, rtcpAttr.mAddrType)
            << context << " (level " << msection.GetLevel() << ")";
        ASSERT_EQ(defaultCandidates[transportId][RTCP].first, rtcpAttr.mAddress)
            << context << " (level " << msection.GetLevel() << ")";
      } else {
        ASSERT_FALSE(msection.GetAttributeList().HasAttribute(
            SdpAttribute::kRtcpAttribute))
        << context << " (level " << msection.GetLevel() << ")";
      }
    }

   private:
    typedef size_t Level;
    typedef std::string TransportId;
    typedef std::string Mid;
    typedef std::string Candidate;
    typedef std::string Address;
    typedef uint16_t Port;
    // Default candidates are put into the m-line, c-line, and rtcp
    // attribute for endpoints that don't support ICE.
    std::map<TransportId, std::map<ComponentType, std::pair<Address, Port>>>
        mDefaultCandidates;
    std::map<TransportId, std::map<ComponentType, std::vector<Candidate>>>
        mCandidates;
    // Level/mid/candidate tuples that need to be trickled
    std::vector<Tuple<Level, Mid, Candidate>> mCandidatesToTrickle;
  };

  // For streaming parse errors
  std::string GetParseErrors(
      const UniquePtr<SdpParser::Results>& results) const {
    std::stringstream output;
    auto errors = std::move(results->Errors());
    for (auto error : errors) {
      output << error.first << ": " << error.second << std::endl;
    }
    return output.str();
  }

  void CheckEndOfCandidates(bool expectEoc, const SdpMediaSection& msection,
                            const std::string& context) {
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

  void CheckTransceiversAreBundled(const JsepSession& session,
                                   const std::string& context) {
    for (const auto& [id, transceiver] : session.GetTransceivers()) {
      (void)id;  // Lame, but no better way to do this right now.
      ASSERT_TRUE(transceiver->HasBundleLevel())
      << context;
      ASSERT_EQ(0U, transceiver->BundleLevel()) << context;
    }
  }

  void DisableMsid(std::string* sdp) const {
    while (true) {
      size_t pos = sdp->find("a=msid");
      if (pos == std::string::npos) {
        break;
      }
      (*sdp)[pos + 2] = 'X';  // garble, a=Xsid
    }
  }

  void DisableBundle(std::string* sdp) const {
    size_t pos = sdp->find("a=group:BUNDLE");
    ASSERT_NE(std::string::npos, pos);
    (*sdp)[pos + 11] = 'G';  // garble, a=group:BUNGLE
  }

  void DisableMsection(std::string* sdp, size_t level) const {
    UniquePtr<Sdp> parsed(Parse(*sdp));
    ASSERT_TRUE(parsed.get());
    ASSERT_LT(level, parsed->GetMediaSectionCount());
    SdpHelper::DisableMsection(parsed.get(), &parsed->GetMediaSection(level));
    (*sdp) = parsed->ToString();
  }

  void CopyTransportAttributes(std::string* sdp, size_t src_level,
                               size_t dst_level) {
    UniquePtr<Sdp> parsed(Parse(*sdp));
    ASSERT_TRUE(parsed.get());
    ASSERT_LT(src_level, parsed->GetMediaSectionCount());
    ASSERT_LT(dst_level, parsed->GetMediaSectionCount());
    nsresult rv =
        mSdpHelper.CopyTransportParams(2, parsed->GetMediaSection(src_level),
                                       &parsed->GetMediaSection(dst_level));
    ASSERT_EQ(NS_OK, rv);
    (*sdp) = parsed->ToString();
  }

  void ReplaceInSdp(std::string* sdp, const char* searchStr,
                    const char* replaceStr) const {
    if (searchStr[0] == '\0') return;
    size_t pos = 0;
    while ((pos = sdp->find(searchStr, pos)) != std::string::npos) {
      sdp->replace(pos, strlen(searchStr), replaceStr);
      pos += strlen(replaceStr);
    }
  }

  void ValidateDisabledMSection(const SdpMediaSection* msection) {
    ASSERT_EQ(1U, msection->GetFormats().size());

    auto& attrs = msection->GetAttributeList();
    ASSERT_TRUE(attrs.HasAttribute(SdpAttribute::kMidAttribute));
    ASSERT_TRUE(attrs.HasAttribute(SdpAttribute::kDirectionAttribute));
    ASSERT_FALSE(attrs.HasAttribute(SdpAttribute::kBundleOnlyAttribute));
    ASSERT_EQ(SdpDirectionAttribute::kInactive,
              msection->GetDirectionAttribute().mValue);
    ASSERT_EQ(3U, attrs.Count());
    if (msection->GetMediaType() == SdpMediaSection::kAudio) {
      ASSERT_EQ("0", msection->GetFormats()[0]);
      const SdpRtpmapAttributeList::Rtpmap* rtpmap(msection->FindRtpmap("0"));
      ASSERT_TRUE(rtpmap);
      ASSERT_EQ("0", rtpmap->pt);
      ASSERT_EQ("PCMU", rtpmap->name);
    } else if (msection->GetMediaType() == SdpMediaSection::kVideo) {
      ASSERT_EQ("120", msection->GetFormats()[0]);
      const SdpRtpmapAttributeList::Rtpmap* rtpmap(msection->FindRtpmap("120"));
      ASSERT_TRUE(rtpmap);
      ASSERT_EQ("120", rtpmap->pt);
      ASSERT_EQ("VP8", rtpmap->name);
    } else if (msection->GetMediaType() == SdpMediaSection::kApplication) {
      if (msection->GetProtocol() == SdpMediaSection::kUdpDtlsSctp ||
          msection->GetProtocol() == SdpMediaSection::kTcpDtlsSctp) {
        // draft 21 format
        ASSERT_EQ("webrtc-datachannel", msection->GetFormats()[0]);
        ASSERT_FALSE(msection->GetSctpmap());
        ASSERT_EQ(0U, msection->GetSctpPort());
      } else {
        // old draft 05 format
        ASSERT_EQ("0", msection->GetFormats()[0]);
        const SdpSctpmapAttributeList::Sctpmap* sctpmap(msection->GetSctpmap());
        ASSERT_TRUE(sctpmap);
        ASSERT_EQ("0", sctpmap->pt);
        ASSERT_EQ("rejected", sctpmap->name);
        ASSERT_EQ(0U, sctpmap->streams);
      }
    } else {
      // Not that we would have any test which tests this...
      ASSERT_EQ("19", msection->GetFormats()[0]);
      const SdpRtpmapAttributeList::Rtpmap* rtpmap(msection->FindRtpmap("19"));
      ASSERT_TRUE(rtpmap);
      ASSERT_EQ("19", rtpmap->pt);
      ASSERT_EQ("reserved", rtpmap->name);
    }

    ASSERT_FALSE(msection->GetAttributeList().HasAttribute(
        SdpAttribute::kMsidAttribute));
  }

  void ValidateSetupAttribute(const JsepSessionImpl& side,
                              const SdpSetupAttribute::Role expectedRole) {
    auto sdp = GetParsedLocalDescription(side);
    for (size_t i = 0; sdp && i < sdp->GetMediaSectionCount(); ++i) {
      if (sdp->GetMediaSection(i).GetAttributeList().HasAttribute(
              SdpAttribute::kSetupAttribute)) {
        auto role = sdp->GetMediaSection(i).GetAttributeList().GetSetup().mRole;
        ASSERT_EQ(expectedRole, role);
      }
    }
  }

  void DumpTrack(const JsepTrack& track) {
    const JsepTrackNegotiatedDetails* details = track.GetNegotiatedDetails();
    std::cerr << "  type=" << track.GetMediaType() << std::endl;
    if (!details) {
      std::cerr << "  not negotiated" << std::endl;
      return;
    }
    std::cerr << "  encodings=" << std::endl;
    for (size_t i = 0; i < details->GetEncodingCount(); ++i) {
      const JsepTrackEncoding& encoding = details->GetEncoding(i);
      std::cerr << "    id=" << encoding.mRid << std::endl;
      for (const auto& codec : encoding.GetCodecs()) {
        std::cerr << "      " << codec->mName << " enabled("
                  << (codec->mEnabled ? "yes" : "no") << ")";
        if (track.GetMediaType() == SdpMediaSection::kAudio) {
          const JsepAudioCodecDescription* audioCodec =
              static_cast<const JsepAudioCodecDescription*>(codec.get());
          std::cerr << " dtmf(" << (audioCodec->mDtmfEnabled ? "yes" : "no")
                    << ")";
        }
        if (track.GetMediaType() == SdpMediaSection::kVideo) {
          const JsepVideoCodecDescription* videoCodec =
              static_cast<const JsepVideoCodecDescription*>(codec.get());
          std::cerr << " rtx("
                    << (videoCodec->mRtxEnabled ? videoCodec->mRtxPayloadType
                                                : "no")
                    << ")";
        }
        std::cerr << std::endl;
      }
    }
  }

  void DumpTransport(const JsepTransport& transport) {
    std::cerr << "  id=" << transport.mTransportId << std::endl;
    std::cerr << "  components=" << transport.mComponents << std::endl;
  }

  void DumpTransceivers(const JsepSessionImpl& session) {
    for (const auto& [id, transceiver] : session.GetTransceivers()) {
      (void)id;  // Lame, but no better way to do this right now.
      std::cerr << "Transceiver ";
      if (transceiver->HasLevel()) {
        std::cerr << transceiver->GetLevel() << std::endl;
      } else {
        std::cerr << "<NO LEVEL>" << std::endl;
      }
      if (transceiver->HasBundleLevel()) {
        std::cerr << "(bundle level is " << transceiver->BundleLevel() << ")"
                  << std::endl;
      }
      if (!IsNull(transceiver->mSendTrack)) {
        std::cerr << "Sending-->" << std::endl;
        DumpTrack(transceiver->mSendTrack);
      }
      if (!IsNull(transceiver->mRecvTrack)) {
        std::cerr << "Receiving-->" << std::endl;
        DumpTrack(transceiver->mRecvTrack);
      }
      std::cerr << "Transport-->" << std::endl;
      DumpTransport(transceiver->mTransport);
    }
  }

  UniquePtr<Sdp> Parse(const std::string& sdp) const {
    SipccSdpParser parser;
    auto results = parser.Parse(sdp);
    UniquePtr<Sdp> parsed = std::move(results->Sdp());
    EXPECT_TRUE(parsed.get()) << "Should have valid SDP" << std::endl
                              << "Errors were: " << GetParseErrors(results);
    return parsed;
  }

  void SwapOfferAnswerRoles() {
    mSessionOff.swap(mSessionAns);
    mOffCandidates.swap(mAnsCandidates);
    mOffererTransport.swap(mAnswererTransport);
  }

  UniquePtr<JsepSessionImpl> mSessionOff;
  UniquePtr<CandidateSet> mOffCandidates;
  UniquePtr<JsepSessionImpl> mSessionAns;
  UniquePtr<CandidateSet> mAnsCandidates;

  std::vector<SdpMediaSection::MediaType> types;
  std::vector<std::pair<std::string, uint16_t>> mGatheredCandidates;

 private:
  void ValidateTransport(TransportData& source, const std::string& sdp_str) {
    UniquePtr<Sdp> sdp(Parse(sdp_str));
    ASSERT_TRUE(!!sdp);
    size_t num_m_sections = sdp->GetMediaSectionCount();
    for (size_t i = 0; i < num_m_sections; ++i) {
      auto& msection = sdp->GetMediaSection(i);

      if (msection.GetMediaType() == SdpMediaSection::kApplication) {
        if (!(msection.GetProtocol() == SdpMediaSection::kUdpDtlsSctp ||
              msection.GetProtocol() == SdpMediaSection::kTcpDtlsSctp)) {
          // old draft 05 format
          ASSERT_EQ(SdpMediaSection::kDtlsSctp, msection.GetProtocol());
        }
      } else {
        ASSERT_EQ(SdpMediaSection::kUdpTlsRtpSavpf, msection.GetProtocol());
      }

      const SdpAttributeList& attrs = msection.GetAttributeList();
      bool bundle_only = attrs.HasAttribute(SdpAttribute::kBundleOnlyAttribute);

      // port 0 only means disabled when the bundle-only attribute is missing
      if (!bundle_only && msection.GetPort() == 0) {
        ValidateDisabledMSection(&msection);
        continue;
      }
      if (!mSdpHelper.IsBundleSlave(*sdp, i)) {
        const SdpAttributeList& attrs = msection.GetAttributeList();

        ASSERT_FALSE(attrs.GetIceUfrag().empty());
        ASSERT_FALSE(attrs.GetIcePwd().empty());
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
  }

  std::string mLastError;
  SdpHelper mSdpHelper;

  UniquePtr<TransportData> mOffererTransport;
  UniquePtr<TransportData> mAnswererTransport;
};

TEST_F(JsepSessionTestBase, CreateDestroy) {}

TEST_P(JsepSessionTest, CreateOffer) {
  AddTracks(*mSessionOff);
  CreateOffer();
}

TEST_P(JsepSessionTest, CreateOfferSetLocal) {
  AddTracks(*mSessionOff);
  std::string offer = CreateOffer();
  SetLocalOffer(offer);
}

TEST_P(JsepSessionTest, CreateOfferSetLocalSetRemote) {
  AddTracks(*mSessionOff);
  std::string offer = CreateOffer();
  SetLocalOffer(offer);
  SetRemoteOffer(offer);
}

TEST_P(JsepSessionTest, CreateOfferSetLocalSetRemoteCreateAnswer) {
  AddTracks(*mSessionOff);
  std::string offer = CreateOffer();
  SetLocalOffer(offer);
  SetRemoteOffer(offer);
  AddTracks(*mSessionAns);
  std::string answer = CreateAnswer();
}

TEST_P(JsepSessionTest, CreateOfferSetLocalSetRemoteCreateAnswerSetLocal) {
  AddTracks(*mSessionOff);
  std::string offer = CreateOffer();
  SetLocalOffer(offer);
  SetRemoteOffer(offer);
  AddTracks(*mSessionAns);
  std::string answer = CreateAnswer();
  SetLocalAnswer(answer);
}

TEST_P(JsepSessionTest, FullCall) {
  AddTracks(*mSessionOff);
  std::string offer = CreateOffer();
  SetLocalOffer(offer);
  SetRemoteOffer(offer);
  AddTracks(*mSessionAns);
  std::string answer = CreateAnswer();
  SetLocalAnswer(answer);
  SetRemoteAnswer(answer);
}

TEST_P(JsepSessionTest, GetDescriptions) {
  AddTracks(*mSessionOff);
  std::string offer = CreateOffer();
  SetLocalOffer(offer);
  std::string desc = mSessionOff->GetLocalDescription(kJsepDescriptionCurrent);
  ASSERT_EQ(0U, desc.size());
  desc = mSessionOff->GetLocalDescription(kJsepDescriptionPending);
  ASSERT_NE(0U, desc.size());
  desc = mSessionOff->GetLocalDescription(kJsepDescriptionPendingOrCurrent);
  ASSERT_NE(0U, desc.size());
  desc = mSessionOff->GetRemoteDescription(kJsepDescriptionPendingOrCurrent);
  ASSERT_EQ(0U, desc.size());
  desc = mSessionAns->GetLocalDescription(kJsepDescriptionPendingOrCurrent);
  ASSERT_EQ(0U, desc.size());
  desc = mSessionAns->GetRemoteDescription(kJsepDescriptionPendingOrCurrent);
  ASSERT_EQ(0U, desc.size());

  SetRemoteOffer(offer);
  desc = mSessionAns->GetRemoteDescription(kJsepDescriptionCurrent);
  ASSERT_EQ(0U, desc.size());
  desc = mSessionAns->GetRemoteDescription(kJsepDescriptionPending);
  ASSERT_NE(0U, desc.size());
  desc = mSessionAns->GetRemoteDescription(kJsepDescriptionPendingOrCurrent);
  ASSERT_NE(0U, desc.size());
  desc = mSessionAns->GetLocalDescription(kJsepDescriptionPendingOrCurrent);
  ASSERT_EQ(0U, desc.size());
  desc = mSessionOff->GetLocalDescription(kJsepDescriptionPendingOrCurrent);
  ASSERT_NE(0U, desc.size());
  desc = mSessionOff->GetRemoteDescription(kJsepDescriptionPendingOrCurrent);
  ASSERT_EQ(0U, desc.size());

  AddTracks(*mSessionAns);
  std::string answer = CreateAnswer();
  SetLocalAnswer(answer);
  desc = mSessionAns->GetLocalDescription(kJsepDescriptionCurrent);
  ASSERT_NE(0U, desc.size());
  desc = mSessionAns->GetLocalDescription(kJsepDescriptionPending);
  ASSERT_EQ(0U, desc.size());
  desc = mSessionAns->GetLocalDescription(kJsepDescriptionPendingOrCurrent);
  ASSERT_NE(0U, desc.size());
  desc = mSessionAns->GetRemoteDescription(kJsepDescriptionCurrent);
  ASSERT_NE(0U, desc.size());
  desc = mSessionAns->GetRemoteDescription(kJsepDescriptionPending);
  ASSERT_EQ(0U, desc.size());
  desc = mSessionAns->GetRemoteDescription(kJsepDescriptionPendingOrCurrent);
  ASSERT_NE(0U, desc.size());
  desc = mSessionOff->GetLocalDescription(kJsepDescriptionPendingOrCurrent);
  ASSERT_NE(0U, desc.size());
  desc = mSessionOff->GetRemoteDescription(kJsepDescriptionPendingOrCurrent);
  ASSERT_EQ(0U, desc.size());

  SetRemoteAnswer(answer);
  desc = mSessionOff->GetLocalDescription(kJsepDescriptionCurrent);
  ASSERT_NE(0U, desc.size());
  desc = mSessionOff->GetLocalDescription(kJsepDescriptionPending);
  ASSERT_EQ(0U, desc.size());
  desc = mSessionOff->GetLocalDescription(kJsepDescriptionPendingOrCurrent);
  ASSERT_NE(0U, desc.size());
  desc = mSessionOff->GetRemoteDescription(kJsepDescriptionCurrent);
  ASSERT_NE(0U, desc.size());
  desc = mSessionOff->GetRemoteDescription(kJsepDescriptionPending);
  ASSERT_EQ(0U, desc.size());
  desc = mSessionOff->GetRemoteDescription(kJsepDescriptionPendingOrCurrent);
  ASSERT_NE(0U, desc.size());
  desc = mSessionAns->GetLocalDescription(kJsepDescriptionPendingOrCurrent);
  ASSERT_NE(0U, desc.size());
  desc = mSessionAns->GetRemoteDescription(kJsepDescriptionPendingOrCurrent);
  ASSERT_NE(0U, desc.size());
}

TEST_P(JsepSessionTest, RenegotiationNoChange) {
  AddTracks(*mSessionOff);
  std::string offer = CreateOffer();
  SetLocalOffer(offer);
  SetRemoteOffer(offer);

  AddTracks(*mSessionAns);
  std::string answer = CreateAnswer();
  SetLocalAnswer(answer);
  SetRemoteAnswer(answer);

  ValidateSetupAttribute(*mSessionOff, SdpSetupAttribute::kActpass);
  ValidateSetupAttribute(*mSessionAns, SdpSetupAttribute::kActive);

  std::map<size_t, RefPtr<JsepTransceiver>> origOffererTransceivers =
      DeepCopy(mSessionOff->GetTransceivers());
  std::map<size_t, RefPtr<JsepTransceiver>> origAnswererTransceivers =
      DeepCopy(mSessionAns->GetTransceivers());

  std::string reoffer = CreateOffer();
  SetLocalOffer(reoffer);
  SetRemoteOffer(reoffer);

  std::string reanswer = CreateAnswer();
  SetLocalAnswer(reanswer);
  SetRemoteAnswer(reanswer);

  ValidateSetupAttribute(*mSessionOff, SdpSetupAttribute::kActpass);
  ValidateSetupAttribute(*mSessionAns, SdpSetupAttribute::kActive);

  auto newOffererTransceivers = mSessionOff->GetTransceivers();
  auto newAnswererTransceivers = mSessionAns->GetTransceivers();

  ASSERT_TRUE(Equals(origOffererTransceivers, newOffererTransceivers));
  ASSERT_TRUE(Equals(origAnswererTransceivers, newAnswererTransceivers));
}

// Disabled: See Bug 1329028
TEST_P(JsepSessionTest, DISABLED_RenegotiationSwappedRolesNoChange) {
  AddTracks(*mSessionOff);
  std::string offer = CreateOffer();
  SetLocalOffer(offer);
  SetRemoteOffer(offer);

  AddTracks(*mSessionAns);
  std::string answer = CreateAnswer();
  SetLocalAnswer(answer);
  SetRemoteAnswer(answer);

  ValidateSetupAttribute(*mSessionOff, SdpSetupAttribute::kActpass);
  ValidateSetupAttribute(*mSessionAns, SdpSetupAttribute::kActive);

  auto offererTransceivers = DeepCopy(mSessionOff->GetTransceivers());
  auto answererTransceivers = DeepCopy(mSessionAns->GetTransceivers());

  SwapOfferAnswerRoles();

  std::string reoffer = CreateOffer();
  SetLocalOffer(reoffer);
  SetRemoteOffer(reoffer);

  std::string reanswer = CreateAnswer();
  SetLocalAnswer(reanswer);
  SetRemoteAnswer(reanswer);

  ValidateSetupAttribute(*mSessionOff, SdpSetupAttribute::kActpass);
  ValidateSetupAttribute(*mSessionAns, SdpSetupAttribute::kPassive);

  auto newOffererTransceivers = mSessionOff->GetTransceivers();
  auto newAnswererTransceivers = mSessionAns->GetTransceivers();

  ASSERT_TRUE(Equals(offererTransceivers, newAnswererTransceivers));
  ASSERT_TRUE(Equals(answererTransceivers, newOffererTransceivers));
}

static void RemoveLastN(
    std::map<size_t, RefPtr<JsepTransceiver>>& aTransceivers, size_t aNum) {
  while (aNum--) {
    // erase doesn't take reverse_iterator :(
    aTransceivers.erase(--aTransceivers.end());
  }
}

TEST_P(JsepSessionTest, RenegotiationOffererAddsTrack) {
  AddTracks(*mSessionOff);
  AddTracks(*mSessionAns);

  OfferAnswer();

  ValidateSetupAttribute(*mSessionOff, SdpSetupAttribute::kActpass);
  ValidateSetupAttribute(*mSessionAns, SdpSetupAttribute::kActive);

  std::map<size_t, RefPtr<JsepTransceiver>> origOffererTransceivers =
      DeepCopy(mSessionOff->GetTransceivers());
  std::map<size_t, RefPtr<JsepTransceiver>> origAnswererTransceivers =
      DeepCopy(mSessionAns->GetTransceivers());

  std::vector<SdpMediaSection::MediaType> extraTypes;
  extraTypes.push_back(SdpMediaSection::kAudio);
  extraTypes.push_back(SdpMediaSection::kVideo);
  AddTracks(*mSessionOff, extraTypes);
  types.insert(types.end(), extraTypes.begin(), extraTypes.end());

  OfferAnswer(CHECK_SUCCESS);

  ValidateSetupAttribute(*mSessionOff, SdpSetupAttribute::kActpass);
  ValidateSetupAttribute(*mSessionAns, SdpSetupAttribute::kActive);

  auto newOffererTransceivers = mSessionOff->GetTransceivers();
  auto newAnswererTransceivers = mSessionAns->GetTransceivers();

  ASSERT_LE(2U, newOffererTransceivers.size());
  RemoveLastN(newOffererTransceivers, 2);
  ASSERT_TRUE(Equals(origOffererTransceivers, newOffererTransceivers));

  ASSERT_LE(2U, newAnswererTransceivers.size());
  RemoveLastN(newAnswererTransceivers, 2);
  ASSERT_TRUE(Equals(origAnswererTransceivers, newAnswererTransceivers));
}

TEST_P(JsepSessionTest, RenegotiationAnswererAddsTrack) {
  AddTracks(*mSessionOff);
  AddTracks(*mSessionAns);

  OfferAnswer();

  ValidateSetupAttribute(*mSessionOff, SdpSetupAttribute::kActpass);
  ValidateSetupAttribute(*mSessionAns, SdpSetupAttribute::kActive);

  std::map<size_t, RefPtr<JsepTransceiver>> origOffererTransceivers =
      DeepCopy(mSessionOff->GetTransceivers());
  std::map<size_t, RefPtr<JsepTransceiver>> origAnswererTransceivers =
      DeepCopy(mSessionAns->GetTransceivers());

  std::vector<SdpMediaSection::MediaType> extraTypes;
  extraTypes.push_back(SdpMediaSection::kAudio);
  extraTypes.push_back(SdpMediaSection::kVideo);
  AddTracks(*mSessionAns, extraTypes);
  types.insert(types.end(), extraTypes.begin(), extraTypes.end());

  // We need to add a recvonly m-section to the offer for this to work
  mSessionOff->AddTransceiver(new JsepTransceiver(
      SdpMediaSection::kAudio, SdpDirectionAttribute::Direction::kRecvonly));
  mSessionOff->AddTransceiver(new JsepTransceiver(
      SdpMediaSection::kVideo, SdpDirectionAttribute::Direction::kRecvonly));

  std::string offer = CreateOffer();
  SetLocalOffer(offer, CHECK_SUCCESS);
  SetRemoteOffer(offer, CHECK_SUCCESS);

  std::string answer = CreateAnswer();
  SetLocalAnswer(answer, CHECK_SUCCESS);
  SetRemoteAnswer(answer, CHECK_SUCCESS);

  ValidateSetupAttribute(*mSessionOff, SdpSetupAttribute::kActpass);
  ValidateSetupAttribute(*mSessionAns, SdpSetupAttribute::kActive);

  auto newOffererTransceivers = mSessionOff->GetTransceivers();
  auto newAnswererTransceivers = mSessionAns->GetTransceivers();

  ASSERT_LE(2U, newOffererTransceivers.size());
  RemoveLastN(newOffererTransceivers, 2);
  ASSERT_TRUE(Equals(origOffererTransceivers, newOffererTransceivers));

  ASSERT_LE(2U, newAnswererTransceivers.size());
  RemoveLastN(newAnswererTransceivers, 2);
  ASSERT_TRUE(Equals(origAnswererTransceivers, newAnswererTransceivers));
}

TEST_P(JsepSessionTest, RenegotiationBothAddTrack) {
  AddTracks(*mSessionOff);
  AddTracks(*mSessionAns);

  OfferAnswer();

  ValidateSetupAttribute(*mSessionOff, SdpSetupAttribute::kActpass);
  ValidateSetupAttribute(*mSessionAns, SdpSetupAttribute::kActive);

  std::map<size_t, RefPtr<JsepTransceiver>> origOffererTransceivers =
      DeepCopy(mSessionOff->GetTransceivers());
  std::map<size_t, RefPtr<JsepTransceiver>> origAnswererTransceivers =
      DeepCopy(mSessionAns->GetTransceivers());

  std::vector<SdpMediaSection::MediaType> extraTypes;
  extraTypes.push_back(SdpMediaSection::kAudio);
  extraTypes.push_back(SdpMediaSection::kVideo);
  AddTracks(*mSessionAns, extraTypes);
  AddTracks(*mSessionOff, extraTypes);
  types.insert(types.end(), extraTypes.begin(), extraTypes.end());

  OfferAnswer(CHECK_SUCCESS);

  ValidateSetupAttribute(*mSessionOff, SdpSetupAttribute::kActpass);
  ValidateSetupAttribute(*mSessionAns, SdpSetupAttribute::kActive);

  auto newOffererTransceivers = mSessionOff->GetTransceivers();
  auto newAnswererTransceivers = mSessionAns->GetTransceivers();

  ASSERT_LE(2U, newOffererTransceivers.size());
  RemoveLastN(newOffererTransceivers, 2);
  ASSERT_TRUE(Equals(origOffererTransceivers, newOffererTransceivers));

  ASSERT_LE(2U, newAnswererTransceivers.size());
  RemoveLastN(newAnswererTransceivers, 2);
  ASSERT_TRUE(Equals(origAnswererTransceivers, newAnswererTransceivers));
}

TEST_P(JsepSessionTest, RenegotiationBothAddTracksToExistingStream) {
  AddTracks(*mSessionOff);
  AddTracks(*mSessionAns);
  if (GetParam() == "datachannel") {
    return;
  }

  OfferAnswer();

  auto oHasStream = HasMediaStream(GetLocalTracks(*mSessionOff));
  auto aHasStream = HasMediaStream(GetLocalTracks(*mSessionAns));
  ASSERT_EQ(oHasStream, !GetLocalUniqueStreamIds(*mSessionOff).empty());
  ASSERT_EQ(aHasStream, !GetLocalUniqueStreamIds(*mSessionAns).empty());
  ASSERT_EQ(aHasStream, !GetRemoteUniqueStreamIds(*mSessionOff).empty());
  ASSERT_EQ(oHasStream, !GetRemoteUniqueStreamIds(*mSessionAns).empty());

  auto firstOffId = GetFirstLocalStreamId(*mSessionOff);
  auto firstAnsId = GetFirstLocalStreamId(*mSessionAns);

  auto offererTransceivers = DeepCopy(mSessionOff->GetTransceivers());
  auto answererTransceivers = DeepCopy(mSessionAns->GetTransceivers());

  std::vector<SdpMediaSection::MediaType> extraTypes;
  extraTypes.push_back(SdpMediaSection::kAudio);
  extraTypes.push_back(SdpMediaSection::kVideo);
  AddTracksToStream(*mSessionOff, firstOffId, extraTypes);
  AddTracksToStream(*mSessionAns, firstAnsId, extraTypes);
  types.insert(types.end(), extraTypes.begin(), extraTypes.end());

  OfferAnswer(CHECK_SUCCESS);

  oHasStream = HasMediaStream(GetLocalTracks(*mSessionOff));
  aHasStream = HasMediaStream(GetLocalTracks(*mSessionAns));

  ASSERT_EQ(oHasStream, !GetLocalUniqueStreamIds(*mSessionOff).empty());
  ASSERT_EQ(aHasStream, !GetLocalUniqueStreamIds(*mSessionAns).empty());
  ASSERT_EQ(aHasStream, !GetRemoteUniqueStreamIds(*mSessionOff).empty());
  ASSERT_EQ(oHasStream, !GetRemoteUniqueStreamIds(*mSessionAns).empty());
  if (oHasStream) {
    ASSERT_STREQ(firstOffId.c_str(),
                 GetFirstLocalStreamId(*mSessionOff).c_str());
  }
  if (aHasStream) {
    ASSERT_STREQ(firstAnsId.c_str(),
                 GetFirstLocalStreamId(*mSessionAns).c_str());

    auto oHasStream = HasMediaStream(GetLocalTracks(*mSessionOff));
    auto aHasStream = HasMediaStream(GetLocalTracks(*mSessionAns));
    ASSERT_EQ(oHasStream, !GetLocalUniqueStreamIds(*mSessionOff).empty());
    ASSERT_EQ(aHasStream, !GetLocalUniqueStreamIds(*mSessionAns).empty());
  }
}

// The JSEP draft explicitly forbids changing the msid on an m-section, but
// that is a bug.
TEST_P(JsepSessionTest, RenegotiationOffererChangesMsid) {
  AddTracks(*mSessionOff);
  AddTracks(*mSessionAns);

  OfferAnswer();

  std::string offer = CreateOffer();
  SetLocalOffer(offer);

  RefPtr<JsepTransceiver> transceiver =
      GetNegotiatedTransceiver(*mSessionOff, 0);
  ASSERT_TRUE(transceiver);
  if (transceiver->GetMediaType() == SdpMediaSection::kApplication) {
    return;
  }
  std::string streamId = transceiver->mSendTrack.GetStreamIds()[0];
  std::string msidToReplace("a=msid:");
  msidToReplace += streamId;
  size_t msidOffset = offer.find(msidToReplace);
  ASSERT_NE(std::string::npos, msidOffset);
  offer.replace(msidOffset, msidToReplace.size(), "a=msid:foo");

  SetRemoteOffer(offer);
  transceiver = GetNegotiatedTransceiver(*mSessionAns, 0);
  ASSERT_EQ("foo", transceiver->mRecvTrack.GetStreamIds()[0]);

  std::string answer = CreateAnswer();
  SetLocalAnswer(answer);
  SetRemoteAnswer(answer);
}

// The JSEP draft explicitly forbids changing the msid on an m-section, but
// that is a bug.
TEST_P(JsepSessionTest, RenegotiationAnswererChangesMsid) {
  AddTracks(*mSessionOff);
  AddTracks(*mSessionAns);

  OfferAnswer();

  RefPtr<JsepTransceiver> transceiver =
      GetNegotiatedTransceiver(*mSessionOff, 0);
  ASSERT_TRUE(transceiver);
  if (transceiver->GetMediaType() == SdpMediaSection::kApplication) {
    return;
  }

  std::string offer = CreateOffer();
  SetLocalOffer(offer);
  SetRemoteOffer(offer);
  std::string answer = CreateAnswer();
  SetLocalAnswer(answer);

  transceiver = GetNegotiatedTransceiver(*mSessionAns, 0);
  ASSERT_TRUE(transceiver);
  if (transceiver->GetMediaType() == SdpMediaSection::kApplication) {
    return;
  }
  std::string streamId = transceiver->mSendTrack.GetStreamIds()[0];
  std::string msidToReplace("a=msid:");
  msidToReplace += streamId;
  size_t msidOffset = answer.find(msidToReplace);
  ASSERT_NE(std::string::npos, msidOffset);
  answer.replace(msidOffset, msidToReplace.size(), "a=msid:foo");

  SetRemoteAnswer(answer);

  transceiver = GetNegotiatedTransceiver(*mSessionOff, 0);
  ASSERT_EQ("foo", transceiver->mRecvTrack.GetStreamIds()[0]);
}

TEST_P(JsepSessionTest, RenegotiationOffererStopsTransceiver) {
  AddTracks(*mSessionOff);
  AddTracks(*mSessionAns);
  if (types.back() == SdpMediaSection::kApplication) {
    return;
  }

  OfferAnswer();

  std::map<size_t, RefPtr<JsepTransceiver>> origOffererTransceivers =
      DeepCopy(mSessionOff->GetTransceivers());
  std::map<size_t, RefPtr<JsepTransceiver>> origAnswererTransceivers =
      DeepCopy(mSessionAns->GetTransceivers());

  auto lastTransceiver = mSessionOff->GetTransceivers().rbegin()->second;
  // Avoid bundle transport side effects; don't stop the BUNDLE-tag!
  lastTransceiver->Stop();
  JsepTrack removedTrack(lastTransceiver->mSendTrack);

  OfferAnswer(CHECK_SUCCESS);

  // Last m-section should be disabled
  auto offer = GetParsedLocalDescription(*mSessionOff);
  const SdpMediaSection* msection =
      &offer->GetMediaSection(offer->GetMediaSectionCount() - 1);
  ASSERT_TRUE(msection);
  ValidateDisabledMSection(msection);

  // Last m-section should be disabled
  auto answer = GetParsedLocalDescription(*mSessionAns);
  msection = &answer->GetMediaSection(answer->GetMediaSectionCount() - 1);
  ASSERT_TRUE(msection);
  ValidateDisabledMSection(msection);

  auto newOffererTransceivers = mSessionOff->GetTransceivers();
  auto newAnswererTransceivers = mSessionAns->GetTransceivers();

  ASSERT_EQ(origOffererTransceivers.size(), newOffererTransceivers.size());

  ASSERT_FALSE(origOffererTransceivers.rbegin()->second->IsStopped());
  ASSERT_TRUE(newOffererTransceivers.rbegin()->second->IsStopped());

  ASSERT_FALSE(origAnswererTransceivers.rbegin()->second->IsStopped());
  ASSERT_TRUE(newAnswererTransceivers.rbegin()->second->IsStopped());
  RemoveLastN(origOffererTransceivers, 1);   // Ignore this one
  RemoveLastN(newOffererTransceivers, 1);    // Ignore this one
  RemoveLastN(origAnswererTransceivers, 1);  // Ignore this one
  RemoveLastN(newAnswererTransceivers, 1);   // Ignore this one

  ASSERT_TRUE(Equals(origOffererTransceivers, newOffererTransceivers));
  ASSERT_TRUE(Equals(origAnswererTransceivers, newAnswererTransceivers));
}

TEST_P(JsepSessionTest, RenegotiationAnswererStopsTransceiver) {
  AddTracks(*mSessionOff);
  AddTracks(*mSessionAns);
  if (types.back() == SdpMediaSection::kApplication) {
    return;
  }

  OfferAnswer();

  std::map<size_t, RefPtr<JsepTransceiver>> origOffererTransceivers =
      DeepCopy(mSessionOff->GetTransceivers());
  std::map<size_t, RefPtr<JsepTransceiver>> origAnswererTransceivers =
      DeepCopy(mSessionAns->GetTransceivers());

  // Avoid bundle transport side effects; don't stop the BUNDLE-tag!
  mSessionAns->GetTransceivers().rbegin()->second->Stop();
  JsepTrack removedTrack(
      mSessionAns->GetTransceivers().rbegin()->second->mSendTrack);

  OfferAnswer(CHECK_SUCCESS);

  // Last m-section should be sendrecv
  auto offer = GetParsedLocalDescription(*mSessionOff);
  const SdpMediaSection* msection =
      &offer->GetMediaSection(offer->GetMediaSectionCount() - 1);
  ASSERT_TRUE(msection);
  ASSERT_TRUE(msection->IsReceiving());
  ASSERT_TRUE(msection->IsSending());

  // Last m-section should be disabled
  auto answer = GetParsedLocalDescription(*mSessionAns);
  msection = &answer->GetMediaSection(answer->GetMediaSectionCount() - 1);
  ASSERT_TRUE(msection);
  ValidateDisabledMSection(msection);

  auto newOffererTransceivers = mSessionOff->GetTransceivers();
  auto newAnswererTransceivers = mSessionAns->GetTransceivers();

  ASSERT_EQ(origOffererTransceivers.size(), newOffererTransceivers.size());

  ASSERT_FALSE(origOffererTransceivers.rbegin()->second->IsStopped());
  ASSERT_TRUE(newOffererTransceivers.rbegin()->second->IsStopped());
  ASSERT_FALSE(origAnswererTransceivers.rbegin()->second->IsStopped());
  ASSERT_TRUE(newAnswererTransceivers.rbegin()->second->IsStopped());
  RemoveLastN(origOffererTransceivers, 1);   // Ignore this one
  RemoveLastN(newOffererTransceivers, 1);    // Ignore this one
  RemoveLastN(origAnswererTransceivers, 1);  // Ignore this one
  RemoveLastN(newAnswererTransceivers, 1);   // Ignore this one

  ASSERT_TRUE(Equals(origOffererTransceivers, newOffererTransceivers));
  ASSERT_TRUE(Equals(origAnswererTransceivers, newAnswererTransceivers));
}

TEST_P(JsepSessionTest, RenegotiationBothStopSameTransceiver) {
  AddTracks(*mSessionOff);
  AddTracks(*mSessionAns);
  if (types.back() == SdpMediaSection::kApplication) {
    return;
  }

  OfferAnswer();

  std::map<size_t, RefPtr<JsepTransceiver>> origOffererTransceivers =
      DeepCopy(mSessionOff->GetTransceivers());
  std::map<size_t, RefPtr<JsepTransceiver>> origAnswererTransceivers =
      DeepCopy(mSessionAns->GetTransceivers());

  // Avoid bundle transport side effects; don't stop the BUNDLE-tag!
  mSessionOff->GetTransceivers().rbegin()->second->Stop();
  JsepTrack removedTrackOffer(
      mSessionOff->GetTransceivers().rbegin()->second->mSendTrack);
  mSessionAns->GetTransceivers().rbegin()->second->Stop();
  JsepTrack removedTrackAnswer(
      mSessionAns->GetTransceivers().rbegin()->second->mSendTrack);

  OfferAnswer(CHECK_SUCCESS);

  // Last m-section should be disabled
  auto offer = GetParsedLocalDescription(*mSessionOff);
  const SdpMediaSection* msection =
      &offer->GetMediaSection(offer->GetMediaSectionCount() - 1);
  ASSERT_TRUE(msection);
  ValidateDisabledMSection(msection);

  // Last m-section should be disabled
  auto answer = GetParsedLocalDescription(*mSessionAns);
  msection = &answer->GetMediaSection(answer->GetMediaSectionCount() - 1);
  ASSERT_TRUE(msection);
  ValidateDisabledMSection(msection);

  auto newOffererTransceivers = mSessionOff->GetTransceivers();
  auto newAnswererTransceivers = mSessionAns->GetTransceivers();

  ASSERT_EQ(origOffererTransceivers.size(), newOffererTransceivers.size());

  ASSERT_FALSE(origOffererTransceivers.rbegin()->second->IsStopped());
  ASSERT_TRUE(newOffererTransceivers.rbegin()->second->IsStopped());
  ASSERT_FALSE(origAnswererTransceivers.rbegin()->second->IsStopped());
  ASSERT_TRUE(newAnswererTransceivers.rbegin()->second->IsStopped());
  RemoveLastN(origOffererTransceivers, 1);   // Ignore this one
  RemoveLastN(newOffererTransceivers, 1);    // Ignore this one
  RemoveLastN(origAnswererTransceivers, 1);  // Ignore this one
  RemoveLastN(newAnswererTransceivers, 1);   // Ignore this one

  ASSERT_TRUE(Equals(origOffererTransceivers, newOffererTransceivers));
  ASSERT_TRUE(Equals(origAnswererTransceivers, newAnswererTransceivers));
}

TEST_P(JsepSessionTest, RenegotiationBothStopTransceiverThenAddTrack) {
  AddTracks(*mSessionOff);
  AddTracks(*mSessionAns);
  if (types.back() == SdpMediaSection::kApplication) {
    return;
  }

  SdpMediaSection::MediaType removedType = types.back();

  OfferAnswer();

  // Avoid bundle transport side effects; don't stop the BUNDLE-tag!
  mSessionOff->GetTransceivers().rbegin()->second->Stop();
  JsepTrack removedTrackOffer(
      mSessionOff->GetTransceivers().rbegin()->second->mSendTrack);
  mSessionOff->GetTransceivers().rbegin()->second->Stop();
  JsepTrack removedTrackAnswer(
      mSessionOff->GetTransceivers().rbegin()->second->mSendTrack);

  OfferAnswer(CHECK_SUCCESS);

  std::map<size_t, RefPtr<JsepTransceiver>> origOffererTransceivers =
      DeepCopy(mSessionOff->GetTransceivers());
  std::map<size_t, RefPtr<JsepTransceiver>> origAnswererTransceivers =
      DeepCopy(mSessionAns->GetTransceivers());

  std::vector<SdpMediaSection::MediaType> extraTypes;
  extraTypes.push_back(removedType);
  AddTracks(*mSessionAns, extraTypes);
  AddTracks(*mSessionOff, extraTypes);
  types.insert(types.end(), extraTypes.begin(), extraTypes.end());

  OfferAnswer(CHECK_SUCCESS);

  auto newOffererTransceivers = mSessionOff->GetTransceivers();
  auto newAnswererTransceivers = mSessionAns->GetTransceivers();

  ASSERT_EQ(origOffererTransceivers.size() + 1, newOffererTransceivers.size());
  ASSERT_EQ(origAnswererTransceivers.size() + 1,
            newAnswererTransceivers.size());

  // Ensure that the m-section was re-used; no gaps
  ASSERT_EQ(origOffererTransceivers.rbegin()->second->GetLevel(),
            newOffererTransceivers.rbegin()->second->GetLevel());

  ASSERT_EQ(origAnswererTransceivers.rbegin()->second->GetLevel(),
            newAnswererTransceivers.rbegin()->second->GetLevel());
}

TEST_P(JsepSessionTest, RenegotiationBothStopTransceiverDifferentMsection) {
  AddTracks(*mSessionOff);
  AddTracks(*mSessionAns);

  if (types.size() < 2) {
    return;
  }

  if (mSessionOff->GetTransceivers()[0]->GetMediaType() ==
          SdpMediaSection::kApplication ||
      mSessionOff->GetTransceivers()[1]->GetMediaType() ==
          SdpMediaSection::kApplication) {
    return;
  }

  OfferAnswer();

  mSessionOff->GetTransceivers()[0]->Stop();
  mSessionOff->GetTransceivers()[1]->Stop();

  OfferAnswer(CHECK_SUCCESS);
  ASSERT_TRUE(mSessionAns->GetTransceivers()[0]->IsStopped());
  ASSERT_TRUE(mSessionAns->GetTransceivers()[1]->IsStopped());
}

TEST_P(JsepSessionTest, RenegotiationOffererChangesStreamId) {
  AddTracks(*mSessionOff);
  AddTracks(*mSessionAns);

  if (mSessionOff->GetTransceivers()[0]->GetMediaType() ==
      SdpMediaSection::kApplication) {
    return;
  }

  OfferAnswer();

  mSessionOff->GetTransceivers()[0]->mSendTrack.UpdateStreamIds(
      std::vector<std::string>(1, "newstream"));

  OfferAnswer(CHECK_SUCCESS);

  ASSERT_EQ("newstream",
            mSessionAns->GetTransceivers()[0]->mRecvTrack.GetStreamIds()[0]);
}

TEST_P(JsepSessionTest, RenegotiationAnswererChangesStreamId) {
  AddTracks(*mSessionOff);
  AddTracks(*mSessionAns);

  if (mSessionOff->GetTransceivers()[0]->GetMediaType() ==
      SdpMediaSection::kApplication) {
    return;
  }

  OfferAnswer();

  mSessionAns->GetTransceivers()[0]->mSendTrack.UpdateStreamIds(
      std::vector<std::string>(1, "newstream"));

  OfferAnswer(CHECK_SUCCESS);

  ASSERT_EQ("newstream",
            mSessionOff->GetTransceivers()[0]->mRecvTrack.GetStreamIds()[0]);
}

// Tests whether auto-assigned remote msids (ie; what happens when the other
// side doesn't use msid attributes) are stable across renegotiation.
TEST_P(JsepSessionTest, RenegotiationAutoAssignedMsidIsStable) {
  AddTracks(*mSessionOff);
  std::string offer = CreateOffer();
  SetLocalOffer(offer);
  SetRemoteOffer(offer);
  AddTracks(*mSessionAns);
  std::string answer = CreateAnswer();
  SetLocalAnswer(answer);

  DisableMsid(&answer);

  SetRemoteAnswer(answer, CHECK_SUCCESS);

  std::map<size_t, RefPtr<JsepTransceiver>> origOffererTransceivers =
      DeepCopy(mSessionOff->GetTransceivers());
  std::map<size_t, RefPtr<JsepTransceiver>> origAnswererTransceivers =
      DeepCopy(mSessionAns->GetTransceivers());

  ASSERT_EQ(origOffererTransceivers.size(), origAnswererTransceivers.size());
  for (size_t i = 0; i < origOffererTransceivers.size(); ++i) {
    ASSERT_FALSE(IsNull(origOffererTransceivers[i]->mRecvTrack));
    ASSERT_FALSE(IsNull(origAnswererTransceivers[i]->mSendTrack));
    // These should not match since we've monkeyed with the msid
    ASSERT_NE(origOffererTransceivers[i]->mRecvTrack.GetStreamIds(),
              origAnswererTransceivers[i]->mSendTrack.GetStreamIds());
  }

  offer = CreateOffer();
  SetLocalOffer(offer);
  SetRemoteOffer(offer);
  answer = CreateAnswer();
  SetLocalAnswer(answer);

  DisableMsid(&answer);

  SetRemoteAnswer(answer, CHECK_SUCCESS);

  auto newOffererTransceivers = mSessionOff->GetTransceivers();

  ASSERT_TRUE(Equals(origOffererTransceivers, newOffererTransceivers));
}

TEST_P(JsepSessionTest, RenegotiationOffererDisablesTelephoneEvent) {
  AddTracks(*mSessionOff);
  AddTracks(*mSessionAns);
  OfferAnswer();

  // check all the audio tracks to make sure they have 2 codecs (109 and 101),
  // and dtmf is enabled on all audio tracks
  std::vector<JsepTrack> tracks;
  for (const auto& [id, transceiver] : mSessionOff->GetTransceivers()) {
    (void)id;  // Lame, but no better way to do this right now.
    tracks.push_back(transceiver->mSendTrack);
    tracks.push_back(transceiver->mRecvTrack);
  }

  for (const JsepTrack& track : tracks) {
    if (track.GetMediaType() != SdpMediaSection::kAudio) {
      continue;
    }
    const JsepTrackNegotiatedDetails* details = track.GetNegotiatedDetails();
    ASSERT_EQ(1U, details->GetEncodingCount());
    const JsepTrackEncoding& encoding = details->GetEncoding(0);
    ASSERT_EQ(5U, encoding.GetCodecs().size());
    ASSERT_TRUE(encoding.HasFormat("109"));
    ASSERT_TRUE(encoding.HasFormat("101"));
    for (const auto& codec : encoding.GetCodecs()) {
      ASSERT_TRUE(codec);
      // we can cast here because we've already checked for audio track
      const JsepAudioCodecDescription* audioCodec =
          static_cast<const JsepAudioCodecDescription*>(codec.get());
      ASSERT_TRUE(audioCodec->mDtmfEnabled);
    }
  }

  std::string offer = CreateOffer();
  ReplaceInSdp(&offer, "8 101", "8");
  ReplaceInSdp(&offer, "a=fmtp:101 0-15\r\n", "");
  ReplaceInSdp(&offer, "a=rtpmap:101 telephone-event/8000/1\r\n", "");
  std::cerr << "modified OFFER: " << offer << std::endl;

  SetLocalOffer(offer);
  SetRemoteOffer(offer);
  std::string answer = CreateAnswer();
  SetLocalAnswer(answer);
  SetRemoteAnswer(answer);

  // check all the audio tracks to make sure they have 1 codec (109),
  // and dtmf is disabled on all audio tracks
  tracks.clear();
  for (const auto& [id, transceiver] : mSessionOff->GetTransceivers()) {
    (void)id;  // Lame, but no better way to do this right now.
    tracks.push_back(transceiver->mSendTrack);
    tracks.push_back(transceiver->mRecvTrack);
  }

  for (const JsepTrack& track : tracks) {
    if (track.GetMediaType() != SdpMediaSection::kAudio) {
      continue;
    }
    const JsepTrackNegotiatedDetails* details = track.GetNegotiatedDetails();
    ASSERT_EQ(1U, details->GetEncodingCount());
    const JsepTrackEncoding& encoding = details->GetEncoding(0);
    ASSERT_EQ(4U, encoding.GetCodecs().size());
    ASSERT_TRUE(encoding.HasFormat("109"));
    // we can cast here because we've already checked for audio track
    const JsepAudioCodecDescription* audioCodec =
        static_cast<const JsepAudioCodecDescription*>(
            encoding.GetCodecs()[0].get());
    ASSERT_TRUE(audioCodec);
    ASSERT_FALSE(audioCodec->mDtmfEnabled);
  }
}

// Tests behavior when the answerer does not use msid in the initial exchange,
// but does on renegotiation.
TEST_P(JsepSessionTest, RenegotiationAnswererEnablesMsid) {
  AddTracks(*mSessionOff);
  std::string offer = CreateOffer();
  SetLocalOffer(offer);
  SetRemoteOffer(offer);
  AddTracks(*mSessionAns);
  std::string answer = CreateAnswer();
  SetLocalAnswer(answer);

  DisableMsid(&answer);

  SetRemoteAnswer(answer, CHECK_SUCCESS);

  std::map<size_t, RefPtr<JsepTransceiver>> origOffererTransceivers =
      DeepCopy(mSessionOff->GetTransceivers());
  std::map<size_t, RefPtr<JsepTransceiver>> origAnswererTransceivers =
      DeepCopy(mSessionAns->GetTransceivers());

  offer = CreateOffer();
  SetLocalOffer(offer);
  SetRemoteOffer(offer);
  answer = CreateAnswer();
  SetLocalAnswer(answer);
  SetRemoteAnswer(answer, CHECK_SUCCESS);

  auto newOffererTransceivers = mSessionOff->GetTransceivers();

  ASSERT_EQ(origOffererTransceivers.size(), newOffererTransceivers.size());
  for (size_t i = 0; i < origOffererTransceivers.size(); ++i) {
    ASSERT_EQ(origOffererTransceivers[i]->mRecvTrack.GetMediaType(),
              newOffererTransceivers[i]->mRecvTrack.GetMediaType());

    ASSERT_TRUE(Equals(origOffererTransceivers[i]->mSendTrack,
                       newOffererTransceivers[i]->mSendTrack));
    ASSERT_TRUE(Equals(origOffererTransceivers[i]->mTransport,
                       newOffererTransceivers[i]->mTransport));

    if (origOffererTransceivers[i]->mRecvTrack.GetMediaType() ==
        SdpMediaSection::kApplication) {
      ASSERT_TRUE(Equals(origOffererTransceivers[i]->mRecvTrack,
                         newOffererTransceivers[i]->mRecvTrack));
    } else {
      // This should be the only difference
      ASSERT_FALSE(Equals(origOffererTransceivers[i]->mRecvTrack,
                          newOffererTransceivers[i]->mRecvTrack));
    }
  }
}

TEST_P(JsepSessionTest, RenegotiationAnswererDisablesMsid) {
  AddTracks(*mSessionOff);
  std::string offer = CreateOffer();
  SetLocalOffer(offer);
  SetRemoteOffer(offer);
  AddTracks(*mSessionAns);
  std::string answer = CreateAnswer();
  SetLocalAnswer(answer);
  SetRemoteAnswer(answer, CHECK_SUCCESS);

  std::map<size_t, RefPtr<JsepTransceiver>> origOffererTransceivers =
      DeepCopy(mSessionOff->GetTransceivers());
  std::map<size_t, RefPtr<JsepTransceiver>> origAnswererTransceivers =
      DeepCopy(mSessionAns->GetTransceivers());

  offer = CreateOffer();
  SetLocalOffer(offer);
  SetRemoteOffer(offer);
  answer = CreateAnswer();
  SetLocalAnswer(answer);

  DisableMsid(&answer);

  SetRemoteAnswer(answer, CHECK_SUCCESS);

  auto newOffererTransceivers = mSessionOff->GetTransceivers();

  ASSERT_EQ(origOffererTransceivers.size(), newOffererTransceivers.size());
  for (size_t i = 0; i < origOffererTransceivers.size(); ++i) {
    ASSERT_EQ(origOffererTransceivers[i]->mRecvTrack.GetMediaType(),
              newOffererTransceivers[i]->mRecvTrack.GetMediaType());

    ASSERT_TRUE(Equals(origOffererTransceivers[i]->mSendTrack,
                       newOffererTransceivers[i]->mSendTrack));
    ASSERT_TRUE(Equals(origOffererTransceivers[i]->mTransport,
                       newOffererTransceivers[i]->mTransport));

    if (origOffererTransceivers[i]->mRecvTrack.GetMediaType() ==
        SdpMediaSection::kApplication) {
      ASSERT_TRUE(Equals(origOffererTransceivers[i]->mRecvTrack,
                         newOffererTransceivers[i]->mRecvTrack));
    } else {
      // This should be the only difference
      ASSERT_FALSE(Equals(origOffererTransceivers[i]->mRecvTrack,
                          newOffererTransceivers[i]->mRecvTrack));
    }
  }
}

// Tests behavior when offerer does not use bundle on the initial offer/answer,
// but does on renegotiation.
TEST_P(JsepSessionTest, RenegotiationOffererEnablesBundle) {
  AddTracks(*mSessionOff);
  AddTracks(*mSessionAns);

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

  std::map<size_t, RefPtr<JsepTransceiver>> origOffererTransceivers =
      DeepCopy(mSessionOff->GetTransceivers());
  std::map<size_t, RefPtr<JsepTransceiver>> origAnswererTransceivers =
      DeepCopy(mSessionAns->GetTransceivers());

  OfferAnswer();

  auto newOffererTransceivers = mSessionOff->GetTransceivers();
  auto newAnswererTransceivers = mSessionAns->GetTransceivers();

  ASSERT_EQ(newOffererTransceivers.size(), newAnswererTransceivers.size());
  ASSERT_EQ(origOffererTransceivers.size(), newOffererTransceivers.size());
  ASSERT_EQ(origAnswererTransceivers.size(), newAnswererTransceivers.size());

  for (size_t i = 0; i < newOffererTransceivers.size(); ++i) {
    // No bundle initially
    ASSERT_FALSE(origOffererTransceivers[i]->HasBundleLevel());
    ASSERT_FALSE(origAnswererTransceivers[i]->HasBundleLevel());
    if (i != 0) {
      ASSERT_FALSE(Equals(origOffererTransceivers[0]->mTransport,
                          origOffererTransceivers[i]->mTransport));
      ASSERT_FALSE(Equals(origAnswererTransceivers[0]->mTransport,
                          origAnswererTransceivers[i]->mTransport));
    }

    // Verify that bundle worked after renegotiation
    ASSERT_TRUE(newOffererTransceivers[i]->HasBundleLevel());
    ASSERT_TRUE(newAnswererTransceivers[i]->HasBundleLevel());
    ASSERT_TRUE(Equals(newOffererTransceivers[0]->mTransport,
                       newOffererTransceivers[i]->mTransport));
    ASSERT_TRUE(Equals(newAnswererTransceivers[0]->mTransport,
                       newAnswererTransceivers[i]->mTransport));
  }
}

TEST_P(JsepSessionTest, RenegotiationOffererDisablesBundleTransport) {
  AddTracks(*mSessionOff);
  AddTracks(*mSessionAns);

  if (types.size() < 2) {
    return;
  }

  OfferAnswer();

  GetTransceiverByLevel(*mSessionOff, 0)->Stop();

  std::map<size_t, RefPtr<JsepTransceiver>> origOffererTransceivers =
      DeepCopy(mSessionOff->GetTransceivers());
  std::map<size_t, RefPtr<JsepTransceiver>> origAnswererTransceivers =
      DeepCopy(mSessionAns->GetTransceivers());

  OfferAnswer(CHECK_SUCCESS);

  auto newOffererTransceivers = mSessionOff->GetTransceivers();
  auto newAnswererTransceivers = mSessionAns->GetTransceivers();

  ASSERT_EQ(newOffererTransceivers.size(), newAnswererTransceivers.size());
  ASSERT_EQ(origOffererTransceivers.size(), newOffererTransceivers.size());
  ASSERT_EQ(origAnswererTransceivers.size(), newAnswererTransceivers.size());

  JsepTransceiver* ot0 = GetTransceiverByLevel(newOffererTransceivers, 0);
  JsepTransceiver* at0 = GetTransceiverByLevel(newAnswererTransceivers, 0);
  ASSERT_FALSE(ot0->HasBundleLevel());
  ASSERT_FALSE(at0->HasBundleLevel());

  ASSERT_FALSE(
      Equals(ot0->mTransport,
             GetTransceiverByLevel(origOffererTransceivers, 0)->mTransport));
  ASSERT_FALSE(
      Equals(at0->mTransport,
             GetTransceiverByLevel(origAnswererTransceivers, 0)->mTransport));

  ASSERT_EQ(0U, ot0->mTransport.mComponents);
  ASSERT_EQ(0U, at0->mTransport.mComponents);

  for (size_t i = 1; i < types.size() - 1; ++i) {
    JsepTransceiver* ot = GetTransceiverByLevel(newOffererTransceivers, i);
    JsepTransceiver* at = GetTransceiverByLevel(newAnswererTransceivers, i);
    ASSERT_TRUE(ot->HasBundleLevel());
    ASSERT_TRUE(at->HasBundleLevel());
    ASSERT_EQ(1U, ot->BundleLevel());
    ASSERT_EQ(1U, at->BundleLevel());
    ASSERT_FALSE(Equals(ot0->mTransport, ot->mTransport));
    ASSERT_FALSE(Equals(at0->mTransport, at->mTransport));
  }
}

TEST_P(JsepSessionTest, RenegotiationAnswererDisablesBundleTransport) {
  AddTracks(*mSessionOff);
  AddTracks(*mSessionAns);

  if (types.size() < 2) {
    return;
  }

  OfferAnswer();

  std::map<size_t, RefPtr<JsepTransceiver>> origOffererTransceivers =
      DeepCopy(mSessionOff->GetTransceivers());
  std::map<size_t, RefPtr<JsepTransceiver>> origAnswererTransceivers =
      DeepCopy(mSessionAns->GetTransceivers());

  GetTransceiverByLevel(*mSessionAns, 0)->Stop();

  OfferAnswer(CHECK_SUCCESS);

  auto newOffererTransceivers = mSessionOff->GetTransceivers();
  auto newAnswererTransceivers = mSessionAns->GetTransceivers();

  ASSERT_EQ(newOffererTransceivers.size(), newAnswererTransceivers.size());
  ASSERT_EQ(origOffererTransceivers.size(), newOffererTransceivers.size());
  ASSERT_EQ(origAnswererTransceivers.size(), newAnswererTransceivers.size());

  JsepTransceiver* ot0 = GetTransceiverByLevel(newOffererTransceivers, 0);
  JsepTransceiver* at0 = GetTransceiverByLevel(newAnswererTransceivers, 0);
  ASSERT_FALSE(ot0->HasBundleLevel());
  ASSERT_FALSE(at0->HasBundleLevel());

  ASSERT_FALSE(
      Equals(ot0->mTransport,
             GetTransceiverByLevel(origOffererTransceivers, 0)->mTransport));
  ASSERT_FALSE(
      Equals(at0->mTransport,
             GetTransceiverByLevel(origAnswererTransceivers, 0)->mTransport));

  ASSERT_EQ(0U, ot0->mTransport.mComponents);
  ASSERT_EQ(0U, at0->mTransport.mComponents);

  for (size_t i = 1; i < newOffererTransceivers.size(); ++i) {
    JsepTransceiver* ot = GetTransceiverByLevel(newOffererTransceivers, i);
    JsepTransceiver* at = GetTransceiverByLevel(newAnswererTransceivers, i);
    JsepTransceiver* ot1 = GetTransceiverByLevel(newOffererTransceivers, 1);
    JsepTransceiver* at1 = GetTransceiverByLevel(newAnswererTransceivers, 1);
    ASSERT_TRUE(ot->HasBundleLevel());
    ASSERT_TRUE(at->HasBundleLevel());
    ASSERT_EQ(1U, ot->BundleLevel());
    ASSERT_EQ(1U, at->BundleLevel());
    // TODO: When creating an answer where we have rejected the bundle
    // transport, we do not do a good job of creating a sensible SDP. Mainly,
    // when we remove the rejected mid from the bundle group, we can leave a
    // bundle-only mid as the first one when others are available.
    ASSERT_TRUE(Equals(ot1->mTransport, ot->mTransport));
    ASSERT_TRUE(Equals(at1->mTransport, at->mTransport));
  }
}

TEST_P(JsepSessionTest, ParseRejectsBadMediaFormat) {
  AddTracks(*mSessionOff);
  if (types.front() == SdpMediaSection::MediaType::kApplication) {
    return;
  }
  std::string offer = CreateOffer();
  UniquePtr<Sdp> munge(Parse(offer));
  SdpMediaSection& mediaSection = munge->GetMediaSection(0);
  mediaSection.AddCodec("75", "DummyFormatVal", 8000, 1);
  std::string sdpString = munge->ToString();
  JsepSession::Result result =
      mSessionOff->SetLocalDescription(kJsepSdpOffer, sdpString);
  ASSERT_EQ(dom::PCError::OperationError, *result.mError);
}

TEST_P(JsepSessionTest, FullCallWithCandidates) {
  AddTracks(*mSessionOff);
  std::string offer = CreateOffer();
  SetLocalOffer(offer);
  mOffCandidates->Gather(*mSessionOff);

  UniquePtr<Sdp> localOffer(
      Parse(mSessionOff->GetLocalDescription(kJsepDescriptionPending)));
  for (size_t i = 0; i < localOffer->GetMediaSectionCount(); ++i) {
    std::string id = GetTransportId(*mSessionOff, i);
    bool bundleOnly =
        localOffer->GetMediaSection(i).GetAttributeList().HasAttribute(
            SdpAttribute::kBundleOnlyAttribute);
    mOffCandidates->CheckRtpCandidates(
        !bundleOnly, localOffer->GetMediaSection(i), id,
        "Local offer after gathering should have RTP candidates "
        "(unless bundle-only)");
    mOffCandidates->CheckDefaultRtpCandidate(
        !bundleOnly, localOffer->GetMediaSection(i), id,
        "Local offer after gathering should have a default RTP candidate "
        "(unless bundle-only)");
    mOffCandidates->CheckRtcpCandidates(
        !bundleOnly && types[i] != SdpMediaSection::kApplication,
        localOffer->GetMediaSection(i), id,
        "Local offer after gathering should have RTCP candidates "
        "(unless m=application or bundle-only)");
    mOffCandidates->CheckDefaultRtcpCandidate(
        !bundleOnly && types[i] != SdpMediaSection::kApplication,
        localOffer->GetMediaSection(i), id,
        "Local offer after gathering should have a default RTCP candidate "
        "(unless m=application or bundle-only)");
    CheckEndOfCandidates(
        !bundleOnly, localOffer->GetMediaSection(i),
        "Local offer after gathering should have an end-of-candidates "
        "(unless bundle-only)");
  }

  SetRemoteOffer(offer);
  mOffCandidates->Trickle(*mSessionAns);

  UniquePtr<Sdp> remoteOffer(
      Parse(mSessionAns->GetRemoteDescription(kJsepDescriptionPending)));
  for (size_t i = 0; i < remoteOffer->GetMediaSectionCount(); ++i) {
    std::string id = GetTransportId(*mSessionOff, i);
    bool bundleOnly =
        remoteOffer->GetMediaSection(i).GetAttributeList().HasAttribute(
            SdpAttribute::kBundleOnlyAttribute);
    mOffCandidates->CheckRtpCandidates(
        !bundleOnly, remoteOffer->GetMediaSection(i), id,
        "Remote offer after trickle should have RTP candidates "
        "(unless bundle-only)");
    mOffCandidates->CheckDefaultRtpCandidate(
        false, remoteOffer->GetMediaSection(i), id,
        "Remote offer after trickle should not have a default RTP candidate.");
    mOffCandidates->CheckRtcpCandidates(
        !bundleOnly && types[i] != SdpMediaSection::kApplication,
        remoteOffer->GetMediaSection(i), id,
        "Remote offer after trickle should have RTCP candidates "
        "(unless m=application or bundle-only)");
    mOffCandidates->CheckDefaultRtcpCandidate(
        false, remoteOffer->GetMediaSection(i), id,
        "Remote offer after trickle should not have a default RTCP candidate.");
    CheckEndOfCandidates(
        true, remoteOffer->GetMediaSection(i),
        "Remote offer after trickle should have an end-of-candidates.");
  }

  AddTracks(*mSessionAns);
  std::string answer = CreateAnswer();
  SetLocalAnswer(answer);
  // This will gather candidates that mSessionAns knows it doesn't need.
  // They should not be present in the SDP.
  mAnsCandidates->Gather(*mSessionAns);

  UniquePtr<Sdp> localAnswer(
      Parse(mSessionAns->GetLocalDescription(kJsepDescriptionCurrent)));
  std::string id0 = GetTransportId(*mSessionAns, 0);
  for (size_t i = 0; i < localAnswer->GetMediaSectionCount(); ++i) {
    std::string id = GetTransportId(*mSessionAns, i);
    mAnsCandidates->CheckRtpCandidates(
        i == 0, localAnswer->GetMediaSection(i), id,
        "Local answer after gathering should have RTP candidates on level 0.");
    mAnsCandidates->CheckDefaultRtpCandidate(
        true, localAnswer->GetMediaSection(i), id0,
        "Local answer after gathering should have a default RTP candidate "
        "on all levels that matches transport level 0.");
    mAnsCandidates->CheckRtcpCandidates(
        false, localAnswer->GetMediaSection(i), id,
        "Local answer after gathering should not have RTCP candidates "
        "(because we're answering with rtcp-mux)");
    mAnsCandidates->CheckDefaultRtcpCandidate(
        false, localAnswer->GetMediaSection(i), id,
        "Local answer after gathering should not have a default RTCP candidate "
        "(because we're answering with rtcp-mux)");
    CheckEndOfCandidates(
        i == 0, localAnswer->GetMediaSection(i),
        "Local answer after gathering should have an end-of-candidates only for"
        " level 0.");
  }

  SetRemoteAnswer(answer);
  mAnsCandidates->Trickle(*mSessionOff);

  UniquePtr<Sdp> remoteAnswer(
      Parse(mSessionOff->GetRemoteDescription(kJsepDescriptionCurrent)));
  for (size_t i = 0; i < remoteAnswer->GetMediaSectionCount(); ++i) {
    std::string id = GetTransportId(*mSessionAns, i);
    mAnsCandidates->CheckRtpCandidates(
        i == 0, remoteAnswer->GetMediaSection(i), id,
        "Remote answer after trickle should have RTP candidates on level 0.");
    mAnsCandidates->CheckDefaultRtpCandidate(
        false, remoteAnswer->GetMediaSection(i), id,
        "Remote answer after trickle should not have a default RTP candidate.");
    mAnsCandidates->CheckRtcpCandidates(
        false, remoteAnswer->GetMediaSection(i), id,
        "Remote answer after trickle should not have RTCP candidates "
        "(because we're answering with rtcp-mux)");
    mAnsCandidates->CheckDefaultRtcpCandidate(
        false, remoteAnswer->GetMediaSection(i), id,
        "Remote answer after trickle should not have a default RTCP "
        "candidate.");
    CheckEndOfCandidates(
        true, remoteAnswer->GetMediaSection(i),
        "Remote answer after trickle should have an end-of-candidates.");
  }
}

TEST_P(JsepSessionTest, RenegotiationWithCandidates) {
  AddTracks(*mSessionOff);
  std::string offer = CreateOffer();
  SetLocalOffer(offer);
  mOffCandidates->Gather(*mSessionOff);
  SetRemoteOffer(offer);
  mOffCandidates->Trickle(*mSessionAns);
  AddTracks(*mSessionAns);
  std::string answer = CreateAnswer();
  SetLocalAnswer(answer);
  mAnsCandidates->Gather(*mSessionAns);
  SetRemoteAnswer(answer);
  mAnsCandidates->Trickle(*mSessionOff);

  offer = CreateOffer();
  SetLocalOffer(offer);

  UniquePtr<Sdp> parsedOffer(Parse(offer));
  std::string id0 = GetTransportId(*mSessionOff, 0);
  for (size_t i = 0; i < parsedOffer->GetMediaSectionCount(); ++i) {
    std::string id = GetTransportId(*mSessionOff, i);
    mOffCandidates->CheckRtpCandidates(
        i == 0, parsedOffer->GetMediaSection(i), id,
        "Local reoffer before gathering should have RTP candidates on level 0"
        " only.");
    mOffCandidates->CheckDefaultRtpCandidate(
        i == 0, parsedOffer->GetMediaSection(i), id0,
        "Local reoffer before gathering should have a default RTP candidate "
        "on level 0 only.");
    mOffCandidates->CheckRtcpCandidates(
        false, parsedOffer->GetMediaSection(i), id,
        "Local reoffer before gathering should not have RTCP candidates.");
    mOffCandidates->CheckDefaultRtcpCandidate(
        false, parsedOffer->GetMediaSection(i), id,
        "Local reoffer before gathering should not have a default RTCP "
        "candidate.");
    CheckEndOfCandidates(
        i == 0, parsedOffer->GetMediaSection(i),
        "Local reoffer before gathering should have an end-of-candidates "
        "(level 0 only)");
  }

  // mSessionAns should generate a reoffer that is similar
  std::string otherOffer;
  JsepOfferOptions defaultOptions;
  JsepSession::Result result =
      mSessionAns->CreateOffer(defaultOptions, &otherOffer);
  ASSERT_FALSE(result.mError.isSome());
  parsedOffer = Parse(otherOffer);
  id0 = GetTransportId(*mSessionAns, 0);
  for (size_t i = 0; i < parsedOffer->GetMediaSectionCount(); ++i) {
    std::string id = GetTransportId(*mSessionAns, i);
    mAnsCandidates->CheckRtpCandidates(
        i == 0, parsedOffer->GetMediaSection(i), id,
        "Local reoffer before gathering should have RTP candidates on level 0"
        " only. (previous answerer)");
    mAnsCandidates->CheckDefaultRtpCandidate(
        i == 0, parsedOffer->GetMediaSection(i), id0,
        "Local reoffer before gathering should have a default RTP candidate "
        "on level 0 only. (previous answerer)");
    mAnsCandidates->CheckRtcpCandidates(
        false, parsedOffer->GetMediaSection(i), id,
        "Local reoffer before gathering should not have RTCP candidates."
        " (previous answerer)");
    mAnsCandidates->CheckDefaultRtcpCandidate(
        false, parsedOffer->GetMediaSection(i), id,
        "Local reoffer before gathering should not have a default RTCP "
        "candidate. (previous answerer)");
    CheckEndOfCandidates(
        i == 0, parsedOffer->GetMediaSection(i),
        "Local reoffer before gathering should have an end-of-candidates "
        "(level 0 only)");
  }

  // Ok, let's continue with the renegotiation
  SetRemoteOffer(offer);

  // PeerConnection will not re-gather for RTP, but it will for RTCP in case
  // the answerer decides to turn off rtcp-mux.
  if (types[0] != SdpMediaSection::kApplication) {
    mOffCandidates->Gather(*mSessionOff, GetTransportId(*mSessionOff, 0), RTCP);
  }

  // Since the remaining levels were bundled, PeerConnection will re-gather for
  // both RTP and RTCP, in case the answerer rejects bundle, provided
  // bundle-only isn't being used.
  UniquePtr<Sdp> localOffer(
      Parse(mSessionOff->GetLocalDescription(kJsepDescriptionPending)));
  for (size_t level = 1; level < types.size(); ++level) {
    std::string id = GetTransportId(*mSessionOff, level);
    if (!id.empty()) {
      mOffCandidates->Gather(*mSessionOff, id, RTP);
      if (types[level] != SdpMediaSection::kApplication) {
        mOffCandidates->Gather(*mSessionOff, id, RTCP);
      }
    }
  }
  mOffCandidates->FinishGathering(*mSessionOff);
  localOffer = Parse(mSessionOff->GetLocalDescription(kJsepDescriptionPending));

  mOffCandidates->Trickle(*mSessionAns);

  for (size_t i = 0; i < localOffer->GetMediaSectionCount(); ++i) {
    std::string id = GetTransportId(*mSessionOff, i);
    bool bundleOnly =
        localOffer->GetMediaSection(i).GetAttributeList().HasAttribute(
            SdpAttribute::kBundleOnlyAttribute);
    mOffCandidates->CheckRtpCandidates(
        !bundleOnly, localOffer->GetMediaSection(i), id,
        "Local reoffer after gathering should have RTP candidates "
        "(unless bundle-only)");
    mOffCandidates->CheckDefaultRtpCandidate(
        !bundleOnly, localOffer->GetMediaSection(i), id,
        "Local reoffer after gathering should have a default RTP candidate "
        "(unless bundle-only)");
    mOffCandidates->CheckRtcpCandidates(
        !bundleOnly && (types[i] != SdpMediaSection::kApplication),
        localOffer->GetMediaSection(i), id,
        "Local reoffer after gathering should have RTCP candidates "
        "(unless m=application or bundle-only)");
    mOffCandidates->CheckDefaultRtcpCandidate(
        !bundleOnly && (types[i] != SdpMediaSection::kApplication),
        localOffer->GetMediaSection(i), id,
        "Local reoffer after gathering should have a default RTCP candidate "
        "(unless m=application or bundle-only)");
    CheckEndOfCandidates(
        !bundleOnly, localOffer->GetMediaSection(i),
        "Local reoffer after gathering should have an end-of-candidates "
        "(unless bundle-only)");
  }

  UniquePtr<Sdp> remoteOffer(
      Parse(mSessionAns->GetRemoteDescription(kJsepDescriptionPending)));
  for (size_t i = 0; i < remoteOffer->GetMediaSectionCount(); ++i) {
    bool bundleOnly =
        remoteOffer->GetMediaSection(i).GetAttributeList().HasAttribute(
            SdpAttribute::kBundleOnlyAttribute);
    std::string id = GetTransportId(*mSessionOff, i);
    mOffCandidates->CheckRtpCandidates(
        !bundleOnly, remoteOffer->GetMediaSection(i), id,
        "Remote reoffer after trickle should have RTP candidates "
        "(unless bundle-only)");
    mOffCandidates->CheckDefaultRtpCandidate(
        i == 0, remoteOffer->GetMediaSection(i), id,
        "Remote reoffer should have a default RTP candidate on level 0 "
        "(because it was gathered last offer/answer).");
    mOffCandidates->CheckRtcpCandidates(
        !bundleOnly && types[i] != SdpMediaSection::kApplication,
        remoteOffer->GetMediaSection(i), id,
        "Remote reoffer after trickle should have RTCP candidates "
        "(unless m=application or bundle-only)");
    mOffCandidates->CheckDefaultRtcpCandidate(
        false, remoteOffer->GetMediaSection(i), id,
        "Remote reoffer should not have a default RTCP candidate.");
    CheckEndOfCandidates(true, remoteOffer->GetMediaSection(i),
                         "Remote reoffer should have an end-of-candidates.");
  }

  answer = CreateAnswer();
  SetLocalAnswer(answer);
  SetRemoteAnswer(answer);
  // No candidates should be gathered at the answerer, but default candidates
  // should be set.
  mAnsCandidates->FinishGathering(*mSessionAns);

  UniquePtr<Sdp> localAnswer(
      Parse(mSessionAns->GetLocalDescription(kJsepDescriptionCurrent)));
  id0 = GetTransportId(*mSessionAns, 0);
  for (size_t i = 0; i < localAnswer->GetMediaSectionCount(); ++i) {
    std::string id = GetTransportId(*mSessionAns, 0);
    mAnsCandidates->CheckRtpCandidates(
        i == 0, localAnswer->GetMediaSection(i), id,
        "Local reanswer after gathering should have RTP candidates on level "
        "0.");
    mAnsCandidates->CheckDefaultRtpCandidate(
        true, localAnswer->GetMediaSection(i), id0,
        "Local reanswer after gathering should have a default RTP candidate "
        "on all levels that matches transport level 0.");
    mAnsCandidates->CheckRtcpCandidates(
        false, localAnswer->GetMediaSection(i), id,
        "Local reanswer after gathering should not have RTCP candidates "
        "(because we're reanswering with rtcp-mux)");
    mAnsCandidates->CheckDefaultRtcpCandidate(
        false, localAnswer->GetMediaSection(i), id,
        "Local reanswer after gathering should not have a default RTCP "
        "candidate (because we're reanswering with rtcp-mux)");
    CheckEndOfCandidates(
        i == 0, localAnswer->GetMediaSection(i),
        "Local reanswer after gathering should have an end-of-candidates only "
        "for level 0.");
  }

  UniquePtr<Sdp> remoteAnswer(
      Parse(mSessionOff->GetRemoteDescription(kJsepDescriptionCurrent)));
  for (size_t i = 0; i < localAnswer->GetMediaSectionCount(); ++i) {
    std::string id = GetTransportId(*mSessionAns, 0);
    mAnsCandidates->CheckRtpCandidates(
        i == 0, remoteAnswer->GetMediaSection(i), id,
        "Remote reanswer after trickle should have RTP candidates on level 0.");
    mAnsCandidates->CheckDefaultRtpCandidate(
        i == 0, remoteAnswer->GetMediaSection(i), id,
        "Remote reanswer should have a default RTP candidate on level 0 "
        "(because it was gathered last offer/answer).");
    mAnsCandidates->CheckRtcpCandidates(
        false, remoteAnswer->GetMediaSection(i), id,
        "Remote reanswer after trickle should not have RTCP candidates "
        "(because we're reanswering with rtcp-mux)");
    mAnsCandidates->CheckDefaultRtcpCandidate(
        false, remoteAnswer->GetMediaSection(i), id,
        "Remote reanswer after trickle should not have a default RTCP "
        "candidate.");
    CheckEndOfCandidates(i == 0, remoteAnswer->GetMediaSection(i),
                         "Remote reanswer after trickle should have an "
                         "end-of-candidates on level 0 only.");
  }
}

TEST_P(JsepSessionTest, RenegotiationAnswererSendonly) {
  AddTracks(*mSessionOff);
  AddTracks(*mSessionAns);
  OfferAnswer();

  std::string offer = CreateOffer();
  SetLocalOffer(offer);
  SetRemoteOffer(offer);
  std::string answer = CreateAnswer();
  SetLocalAnswer(answer);

  UniquePtr<Sdp> parsedAnswer(Parse(answer));
  for (size_t i = 0; i < parsedAnswer->GetMediaSectionCount(); ++i) {
    SdpMediaSection& msection = parsedAnswer->GetMediaSection(i);
    if (msection.GetMediaType() != SdpMediaSection::kApplication) {
      msection.SetReceiving(false);
    }
  }

  answer = parsedAnswer->ToString();

  SetRemoteAnswer(answer);

  for (const JsepTrack& track : GetLocalTracks(*mSessionOff)) {
    if (track.GetMediaType() != SdpMediaSection::kApplication) {
      ASSERT_FALSE(track.GetActive());
    }
  }

  ASSERT_EQ(types.size(), mSessionOff->GetTransceivers().size());
}

TEST_P(JsepSessionTest, RenegotiationAnswererInactive) {
  AddTracks(*mSessionOff);
  AddTracks(*mSessionAns);
  OfferAnswer();

  std::string offer = CreateOffer();
  SetLocalOffer(offer);
  SetRemoteOffer(offer);
  std::string answer = CreateAnswer();
  SetLocalAnswer(answer);

  UniquePtr<Sdp> parsedAnswer(Parse(answer));
  for (size_t i = 0; i < parsedAnswer->GetMediaSectionCount(); ++i) {
    SdpMediaSection& msection = parsedAnswer->GetMediaSection(i);
    if (msection.GetMediaType() != SdpMediaSection::kApplication) {
      msection.SetReceiving(false);
      msection.SetSending(false);
    }
  }

  answer = parsedAnswer->ToString();

  SetRemoteAnswer(answer, CHECK_SUCCESS);  // Won't have answerer tracks

  for (const JsepTrack& track : GetLocalTracks(*mSessionOff)) {
    if (track.GetMediaType() != SdpMediaSection::kApplication) {
      ASSERT_FALSE(track.GetActive());
    }
  }

  ASSERT_EQ(types.size(), mSessionOff->GetTransceivers().size());
}

INSTANTIATE_TEST_CASE_P(
    Variants, JsepSessionTest,
    ::testing::Values("audio", "video", "datachannel", "audio,video",
                      "video,audio", "audio,datachannel", "video,datachannel",
                      "video,audio,datachannel", "audio,video,datachannel",
                      "datachannel,audio", "datachannel,video",
                      "datachannel,audio,video", "datachannel,video,audio",
                      "audio,datachannel,video", "video,datachannel,audio",
                      "audio,audio", "video,video", "audio,audio,video",
                      "audio,video,video", "audio,audio,video,video",
                      "audio,audio,video,video,datachannel"));

// offerToReceiveXxx variants

TEST_F(JsepSessionTest, OfferAnswerRecvOnlyLines) {
  mSessionOff->AddTransceiver(new JsepTransceiver(
      SdpMediaSection::kAudio, SdpDirectionAttribute::kRecvonly));
  mSessionOff->AddTransceiver(new JsepTransceiver(
      SdpMediaSection::kVideo, SdpDirectionAttribute::kRecvonly));
  mSessionOff->AddTransceiver(new JsepTransceiver(
      SdpMediaSection::kVideo, SdpDirectionAttribute::kRecvonly));
  std::string offer = CreateOffer();

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

  AddTracks(*mSessionAns, "audio,video");
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

  std::map<size_t, RefPtr<JsepTransceiver>> transceivers(
      mSessionOff->GetTransceivers());
  ASSERT_EQ(3U, transceivers.size());
  for (const auto& [id, transceiver] : transceivers) {
    (void)id;  // Lame, but no better way to do this right now.
    const auto& msection =
        parsedOffer->GetMediaSection(transceiver->GetLevel());
    const auto& ssrcs = msection.GetAttributeList().GetSsrc().mSsrcs;
    if (msection.GetMediaType() == SdpMediaSection::kVideo) {
      ASSERT_EQ(2U, ssrcs.size());
    } else {
      ASSERT_EQ(1U, ssrcs.size());
    }
  }
}

TEST_F(JsepSessionTest, OfferAnswerSendOnlyLines) {
  AddTracks(*mSessionOff, "audio,video,video");

  SetDirection(*mSessionOff, 0, SdpDirectionAttribute::kSendonly);
  SetDirection(*mSessionOff, 2, SdpDirectionAttribute::kSendonly);
  std::string offer = CreateOffer();

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

  AddTracks(*mSessionAns, "audio,video");
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

TEST_F(JsepSessionTest, OfferToReceiveAudioNotUsed) {
  mSessionOff->AddTransceiver(new JsepTransceiver(
      SdpMediaSection::kAudio, SdpDirectionAttribute::kRecvonly));

  OfferAnswer(CHECK_SUCCESS);

  UniquePtr<Sdp> offer(
      Parse(mSessionOff->GetLocalDescription(kJsepDescriptionCurrent)));
  ASSERT_TRUE(offer.get());
  ASSERT_EQ(1U, offer->GetMediaSectionCount());
  ASSERT_EQ(SdpMediaSection::kAudio, offer->GetMediaSection(0).GetMediaType());
  ASSERT_EQ(SdpDirectionAttribute::kRecvonly,
            offer->GetMediaSection(0).GetAttributeList().GetDirection());

  UniquePtr<Sdp> answer(
      Parse(mSessionAns->GetLocalDescription(kJsepDescriptionCurrent)));
  ASSERT_TRUE(answer.get());
  ASSERT_EQ(1U, answer->GetMediaSectionCount());
  ASSERT_EQ(SdpMediaSection::kAudio, answer->GetMediaSection(0).GetMediaType());
  ASSERT_EQ(SdpDirectionAttribute::kInactive,
            answer->GetMediaSection(0).GetAttributeList().GetDirection());
}

TEST_F(JsepSessionTest, OfferToReceiveVideoNotUsed) {
  mSessionOff->AddTransceiver(new JsepTransceiver(
      SdpMediaSection::kVideo, SdpDirectionAttribute::kRecvonly));

  OfferAnswer(CHECK_SUCCESS);

  UniquePtr<Sdp> offer(
      Parse(mSessionOff->GetLocalDescription(kJsepDescriptionCurrent)));
  ASSERT_TRUE(offer.get());
  ASSERT_EQ(1U, offer->GetMediaSectionCount());
  ASSERT_EQ(SdpMediaSection::kVideo, offer->GetMediaSection(0).GetMediaType());
  ASSERT_EQ(SdpDirectionAttribute::kRecvonly,
            offer->GetMediaSection(0).GetAttributeList().GetDirection());

  UniquePtr<Sdp> answer(
      Parse(mSessionAns->GetLocalDescription(kJsepDescriptionCurrent)));
  ASSERT_TRUE(answer.get());
  ASSERT_EQ(1U, answer->GetMediaSectionCount());
  ASSERT_EQ(SdpMediaSection::kVideo, answer->GetMediaSection(0).GetMediaType());
  ASSERT_EQ(SdpDirectionAttribute::kInactive,
            answer->GetMediaSection(0).GetAttributeList().GetDirection());
}

TEST_F(JsepSessionTest, CreateOfferNoDatachannelDefault) {
  RefPtr<JsepTransceiver> audio(new JsepTransceiver(SdpMediaSection::kAudio));
  audio->mSendTrack.UpdateStreamIds(
      std::vector<std::string>(1, "offerer_stream"));
  mSessionOff->AddTransceiver(audio);

  RefPtr<JsepTransceiver> video(new JsepTransceiver(SdpMediaSection::kVideo));
  video->mSendTrack.UpdateStreamIds(
      std::vector<std::string>(1, "offerer_stream"));
  mSessionOff->AddTransceiver(video);

  std::string offer = CreateOffer();

  UniquePtr<Sdp> outputSdp(Parse(offer));
  ASSERT_TRUE(!!outputSdp);

  ASSERT_EQ(2U, outputSdp->GetMediaSectionCount());
  ASSERT_EQ(SdpMediaSection::kAudio,
            outputSdp->GetMediaSection(0).GetMediaType());
  ASSERT_EQ(SdpMediaSection::kVideo,
            outputSdp->GetMediaSection(1).GetMediaType());
}

TEST_F(JsepSessionTest, ValidateOfferedVideoCodecParams) {
  types.push_back(SdpMediaSection::kAudio);
  types.push_back(SdpMediaSection::kVideo);

  RefPtr<JsepTransceiver> audio(new JsepTransceiver(SdpMediaSection::kAudio));
  audio->mSendTrack.UpdateStreamIds(
      std::vector<std::string>(1, "offerer_stream"));
  mSessionOff->AddTransceiver(audio);

  RefPtr<JsepTransceiver> video(new JsepTransceiver(SdpMediaSection::kVideo));
  video->mSendTrack.UpdateStreamIds(
      std::vector<std::string>(1, "offerer_stream"));
  mSessionOff->AddTransceiver(video);

  std::string offer = CreateOffer();

  UniquePtr<Sdp> outputSdp(Parse(offer));
  ASSERT_TRUE(!!outputSdp);

  ASSERT_EQ(2U, outputSdp->GetMediaSectionCount());
  const auto& video_section = outputSdp->GetMediaSection(1);
  ASSERT_EQ(SdpMediaSection::kVideo, video_section.GetMediaType());
  const auto& video_attrs = video_section.GetAttributeList();
  ASSERT_EQ(SdpDirectionAttribute::kSendrecv, video_attrs.GetDirection());

  ASSERT_EQ(10U, video_section.GetFormats().size());
  ASSERT_EQ("120", video_section.GetFormats()[0]);
  ASSERT_EQ("124", video_section.GetFormats()[1]);
  ASSERT_EQ("121", video_section.GetFormats()[2]);
  ASSERT_EQ("125", video_section.GetFormats()[3]);
  ASSERT_EQ("126", video_section.GetFormats()[4]);
  ASSERT_EQ("127", video_section.GetFormats()[5]);
  ASSERT_EQ("97", video_section.GetFormats()[6]);
  ASSERT_EQ("98", video_section.GetFormats()[7]);
  ASSERT_EQ("123", video_section.GetFormats()[8]);
  ASSERT_EQ("122", video_section.GetFormats()[9]);

  // Validate rtpmap
  ASSERT_TRUE(video_attrs.HasAttribute(SdpAttribute::kRtpmapAttribute));
  auto& rtpmaps = video_attrs.GetRtpmap();
  ASSERT_TRUE(rtpmaps.HasEntry("120"));
  ASSERT_TRUE(rtpmaps.HasEntry("124"));
  ASSERT_TRUE(rtpmaps.HasEntry("121"));
  ASSERT_TRUE(rtpmaps.HasEntry("125"));
  ASSERT_TRUE(rtpmaps.HasEntry("126"));
  ASSERT_TRUE(rtpmaps.HasEntry("127"));
  ASSERT_TRUE(rtpmaps.HasEntry("97"));
  ASSERT_TRUE(rtpmaps.HasEntry("98"));
  ASSERT_TRUE(rtpmaps.HasEntry("123"));
  ASSERT_TRUE(rtpmaps.HasEntry("122"));

  const auto& vp8_entry = rtpmaps.GetEntry("120");
  const auto& vp8_rtx_entry = rtpmaps.GetEntry("124");
  const auto& vp9_entry = rtpmaps.GetEntry("121");
  const auto& vp9_rtx_entry = rtpmaps.GetEntry("125");
  const auto& h264_1_entry = rtpmaps.GetEntry("126");
  const auto& h264_1_rtx_entry = rtpmaps.GetEntry("127");
  const auto& h264_0_entry = rtpmaps.GetEntry("97");
  const auto& h264_0_rtx_entry = rtpmaps.GetEntry("98");
  const auto& ulpfec_0_entry = rtpmaps.GetEntry("123");
  const auto& red_0_entry = rtpmaps.GetEntry("122");

  ASSERT_EQ("VP8", vp8_entry.name);
  ASSERT_EQ("rtx", vp8_rtx_entry.name);
  ASSERT_EQ("VP9", vp9_entry.name);
  ASSERT_EQ("rtx", vp9_rtx_entry.name);
  ASSERT_EQ("H264", h264_1_entry.name);
  ASSERT_EQ("rtx", h264_1_rtx_entry.name);
  ASSERT_EQ("H264", h264_0_entry.name);
  ASSERT_EQ("rtx", h264_0_rtx_entry.name);
  ASSERT_EQ("red", red_0_entry.name);
  ASSERT_EQ("ulpfec", ulpfec_0_entry.name);

  // Validate fmtps
  ASSERT_TRUE(video_attrs.HasAttribute(SdpAttribute::kFmtpAttribute));
  auto& fmtps = video_attrs.GetFmtp().mFmtps;

  ASSERT_EQ(9U, fmtps.size());

  // VP8
  const SdpFmtpAttributeList::Parameters* vp8_params =
      video_section.FindFmtp("120");
  ASSERT_TRUE(vp8_params);
  ASSERT_EQ(SdpRtpmapAttributeList::kVP8, vp8_params->codec_type);

  auto& parsed_vp8_params =
      *static_cast<const SdpFmtpAttributeList::VP8Parameters*>(vp8_params);

  ASSERT_EQ((uint32_t)12288, parsed_vp8_params.max_fs);
  ASSERT_EQ((uint32_t)60, parsed_vp8_params.max_fr);

  // VP8 RTX
  const SdpFmtpAttributeList::Parameters* vp8_rtx_params =
      video_section.FindFmtp("124");
  ASSERT_TRUE(vp8_rtx_params);
  ASSERT_EQ(SdpRtpmapAttributeList::kRtx, vp8_rtx_params->codec_type);

  const auto& parsed_vp8_rtx_params =
      *static_cast<const SdpFmtpAttributeList::RtxParameters*>(vp8_rtx_params);

  ASSERT_EQ((uint32_t)120, parsed_vp8_rtx_params.apt);

  // VP9
  const SdpFmtpAttributeList::Parameters* vp9_params =
      video_section.FindFmtp("121");
  ASSERT_TRUE(vp9_params);
  ASSERT_EQ(SdpRtpmapAttributeList::kVP9, vp9_params->codec_type);

  const auto& parsed_vp9_params =
      *static_cast<const SdpFmtpAttributeList::VP8Parameters*>(vp9_params);

  ASSERT_EQ((uint32_t)12288, parsed_vp9_params.max_fs);
  ASSERT_EQ((uint32_t)60, parsed_vp9_params.max_fr);

  // VP9 RTX
  const SdpFmtpAttributeList::Parameters* vp9_rtx_params =
      video_section.FindFmtp("125");
  ASSERT_TRUE(vp9_rtx_params);
  ASSERT_EQ(SdpRtpmapAttributeList::kRtx, vp9_rtx_params->codec_type);

  const auto& parsed_vp9_rtx_params =
      *static_cast<const SdpFmtpAttributeList::RtxParameters*>(vp9_rtx_params);
  ASSERT_EQ((uint32_t)121, parsed_vp9_rtx_params.apt);

  // H264 packetization mode 1
  const SdpFmtpAttributeList::Parameters* h264_1_params =
      video_section.FindFmtp("126");
  ASSERT_TRUE(h264_1_params);
  ASSERT_EQ(SdpRtpmapAttributeList::kH264, h264_1_params->codec_type);

  const auto& parsed_h264_1_params =
      *static_cast<const SdpFmtpAttributeList::H264Parameters*>(h264_1_params);

  ASSERT_EQ((uint32_t)0x42e00d, parsed_h264_1_params.profile_level_id);
  ASSERT_TRUE(parsed_h264_1_params.level_asymmetry_allowed);
  ASSERT_EQ(1U, parsed_h264_1_params.packetization_mode);

  // H264 packetization mode 1 RTX
  const SdpFmtpAttributeList::Parameters* h264_1_rtx_params =
      video_section.FindFmtp("127");
  ASSERT_TRUE(h264_1_rtx_params);
  ASSERT_EQ(SdpRtpmapAttributeList::kRtx, h264_1_rtx_params->codec_type);

  const auto& parsed_h264_1_rtx_params =
      *static_cast<const SdpFmtpAttributeList::RtxParameters*>(
          h264_1_rtx_params);

  ASSERT_EQ((uint32_t)126, parsed_h264_1_rtx_params.apt);

  // H264 packetization mode 0
  const SdpFmtpAttributeList::Parameters* h264_0_params =
      video_section.FindFmtp("97");
  ASSERT_TRUE(h264_0_params);
  ASSERT_EQ(SdpRtpmapAttributeList::kH264, h264_0_params->codec_type);

  const auto& parsed_h264_0_params =
      *static_cast<const SdpFmtpAttributeList::H264Parameters*>(h264_0_params);

  ASSERT_EQ((uint32_t)0x42e00d, parsed_h264_0_params.profile_level_id);
  ASSERT_TRUE(parsed_h264_0_params.level_asymmetry_allowed);
  ASSERT_EQ(0U, parsed_h264_0_params.packetization_mode);

  // H264 packetization mode 0 RTX
  const SdpFmtpAttributeList::Parameters* h264_0_rtx_params =
      video_section.FindFmtp("98");
  ASSERT_TRUE(h264_0_rtx_params);
  ASSERT_EQ(SdpRtpmapAttributeList::kRtx, h264_0_rtx_params->codec_type);

  const auto& parsed_h264_0_rtx_params =
      *static_cast<const SdpFmtpAttributeList::RtxParameters*>(
          h264_0_rtx_params);

  ASSERT_EQ((uint32_t)97, parsed_h264_0_rtx_params.apt);

  // red
  const SdpFmtpAttributeList::Parameters* red_params =
      video_section.FindFmtp("122");
  ASSERT_TRUE(red_params);
  ASSERT_EQ(SdpRtpmapAttributeList::kRed, red_params->codec_type);

  const auto& parsed_red_params =
      *static_cast<const SdpFmtpAttributeList::RedParameters*>(red_params);
  ASSERT_EQ(5U, parsed_red_params.encodings.size());
  ASSERT_EQ(120, parsed_red_params.encodings[0]);
  ASSERT_EQ(121, parsed_red_params.encodings[1]);
  ASSERT_EQ(126, parsed_red_params.encodings[2]);
  ASSERT_EQ(97, parsed_red_params.encodings[3]);
  ASSERT_EQ(123, parsed_red_params.encodings[4]);
}

TEST_F(JsepSessionTest, ValidateOfferedAudioCodecParams) {
  types.push_back(SdpMediaSection::kAudio);
  types.push_back(SdpMediaSection::kVideo);

  RefPtr<JsepTransceiver> audio(new JsepTransceiver(SdpMediaSection::kAudio));
  audio->mSendTrack.UpdateStreamIds(
      std::vector<std::string>(1, "offerer_stream"));
  mSessionOff->AddTransceiver(audio);

  RefPtr<JsepTransceiver> video(new JsepTransceiver(SdpMediaSection::kVideo));
  video->mSendTrack.UpdateStreamIds(
      std::vector<std::string>(1, "offerer_stream"));
  mSessionOff->AddTransceiver(video);

  std::string offer = CreateOffer();

  UniquePtr<Sdp> outputSdp(Parse(offer));
  ASSERT_TRUE(!!outputSdp);

  ASSERT_EQ(2U, outputSdp->GetMediaSectionCount());
  auto& audio_section = outputSdp->GetMediaSection(0);
  ASSERT_EQ(SdpMediaSection::kAudio, audio_section.GetMediaType());
  auto& audio_attrs = audio_section.GetAttributeList();
  ASSERT_EQ(SdpDirectionAttribute::kSendrecv, audio_attrs.GetDirection());
  ASSERT_EQ(5U, audio_section.GetFormats().size());
  ASSERT_EQ("109", audio_section.GetFormats()[0]);
  ASSERT_EQ("9", audio_section.GetFormats()[1]);
  ASSERT_EQ("0", audio_section.GetFormats()[2]);
  ASSERT_EQ("8", audio_section.GetFormats()[3]);
  ASSERT_EQ("101", audio_section.GetFormats()[4]);

  // Validate rtpmap
  ASSERT_TRUE(audio_attrs.HasAttribute(SdpAttribute::kRtpmapAttribute));
  auto& rtpmaps = audio_attrs.GetRtpmap();
  ASSERT_TRUE(rtpmaps.HasEntry("109"));
  ASSERT_TRUE(rtpmaps.HasEntry("9"));
  ASSERT_TRUE(rtpmaps.HasEntry("0"));
  ASSERT_TRUE(rtpmaps.HasEntry("8"));
  ASSERT_TRUE(rtpmaps.HasEntry("101"));

  auto& opus_entry = rtpmaps.GetEntry("109");
  auto& g722_entry = rtpmaps.GetEntry("9");
  auto& pcmu_entry = rtpmaps.GetEntry("0");
  auto& pcma_entry = rtpmaps.GetEntry("8");
  auto& telephone_event_entry = rtpmaps.GetEntry("101");

  ASSERT_EQ("opus", opus_entry.name);
  ASSERT_EQ("G722", g722_entry.name);
  ASSERT_EQ("PCMU", pcmu_entry.name);
  ASSERT_EQ("PCMA", pcma_entry.name);
  ASSERT_EQ("telephone-event", telephone_event_entry.name);

  // Validate fmtps
  ASSERT_TRUE(audio_attrs.HasAttribute(SdpAttribute::kFmtpAttribute));
  auto& fmtps = audio_attrs.GetFmtp().mFmtps;

  ASSERT_EQ(2U, fmtps.size());

  // opus
  const SdpFmtpAttributeList::Parameters* opus_params =
      audio_section.FindFmtp("109");
  ASSERT_TRUE(opus_params);
  ASSERT_EQ(SdpRtpmapAttributeList::kOpus, opus_params->codec_type);

  auto& parsed_opus_params =
      *static_cast<const SdpFmtpAttributeList::OpusParameters*>(opus_params);

  ASSERT_EQ((uint32_t)48000, parsed_opus_params.maxplaybackrate);
  ASSERT_EQ((uint32_t)1, parsed_opus_params.stereo);
  ASSERT_EQ((uint32_t)0, parsed_opus_params.useInBandFec);
  ASSERT_EQ((uint32_t)0, parsed_opus_params.maxAverageBitrate);
  ASSERT_EQ((uint32_t)0, parsed_opus_params.useDTX);
  ASSERT_EQ((uint32_t)0, parsed_opus_params.useCbr);
  ASSERT_EQ((uint32_t)0, parsed_opus_params.frameSizeMs);
  ASSERT_EQ((uint32_t)0, parsed_opus_params.minFrameSizeMs);
  ASSERT_EQ((uint32_t)0, parsed_opus_params.maxFrameSizeMs);

  // dtmf
  const SdpFmtpAttributeList::Parameters* dtmf_params =
      audio_section.FindFmtp("101");
  ASSERT_TRUE(dtmf_params);
  ASSERT_EQ(SdpRtpmapAttributeList::kTelephoneEvent, dtmf_params->codec_type);

  auto& parsed_dtmf_params =
      *static_cast<const SdpFmtpAttributeList::TelephoneEventParameters*>(
          dtmf_params);

  ASSERT_EQ("0-15", parsed_dtmf_params.dtmfTones);
}

TEST_F(JsepSessionTest, ValidateNoFmtpLineForRedInOfferAndAnswer) {
  types.push_back(SdpMediaSection::kAudio);
  types.push_back(SdpMediaSection::kVideo);

  AddTracksToStream(*mSessionOff, "offerer_stream", "audio,video");

  std::string offer = CreateOffer();

  // look for line with fmtp:122 and remove it
  size_t start = offer.find("a=fmtp:122");
  size_t end = offer.find("\r\n", start);
  offer.replace(start, end + 2 - start, "");

  SetLocalOffer(offer);
  SetRemoteOffer(offer);

  AddTracksToStream(*mSessionAns, "answerer_stream", "audio,video");

  std::string answer = CreateAnswer();
  // because parsing will throw out the malformed fmtp, make sure it is not
  // in the answer sdp string
  ASSERT_EQ(std::string::npos, answer.find("a=fmtp:122"));

  UniquePtr<Sdp> outputSdp(Parse(answer));
  ASSERT_TRUE(!!outputSdp);

  ASSERT_EQ(2U, outputSdp->GetMediaSectionCount());
  auto& video_section = outputSdp->GetMediaSection(1);
  ASSERT_EQ(SdpMediaSection::kVideo, video_section.GetMediaType());
  auto& video_attrs = video_section.GetAttributeList();
  ASSERT_EQ(SdpDirectionAttribute::kSendrecv, video_attrs.GetDirection());

  ASSERT_EQ(10U, video_section.GetFormats().size());
  ASSERT_EQ("120", video_section.GetFormats()[0]);
  ASSERT_EQ("124", video_section.GetFormats()[1]);
  ASSERT_EQ("121", video_section.GetFormats()[2]);
  ASSERT_EQ("125", video_section.GetFormats()[3]);
  ASSERT_EQ("126", video_section.GetFormats()[4]);
  ASSERT_EQ("127", video_section.GetFormats()[5]);
  ASSERT_EQ("97", video_section.GetFormats()[6]);
  ASSERT_EQ("98", video_section.GetFormats()[7]);
  ASSERT_EQ("123", video_section.GetFormats()[8]);
  ASSERT_EQ("122", video_section.GetFormats()[9]);

  // Validate rtpmap
  ASSERT_TRUE(video_attrs.HasAttribute(SdpAttribute::kRtpmapAttribute));
  auto& rtpmaps = video_attrs.GetRtpmap();
  ASSERT_TRUE(rtpmaps.HasEntry("120"));
  ASSERT_TRUE(rtpmaps.HasEntry("124"));
  ASSERT_TRUE(rtpmaps.HasEntry("121"));
  ASSERT_TRUE(rtpmaps.HasEntry("125"));
  ASSERT_TRUE(rtpmaps.HasEntry("126"));
  ASSERT_TRUE(rtpmaps.HasEntry("127"));
  ASSERT_TRUE(rtpmaps.HasEntry("97"));
  ASSERT_TRUE(rtpmaps.HasEntry("98"));
  ASSERT_TRUE(rtpmaps.HasEntry("123"));
  ASSERT_TRUE(rtpmaps.HasEntry("122"));

  // Validate fmtps
  ASSERT_TRUE(video_attrs.HasAttribute(SdpAttribute::kFmtpAttribute));
  auto& fmtps = video_attrs.GetFmtp().mFmtps;

  ASSERT_EQ(8U, fmtps.size());
  ASSERT_EQ("126", fmtps[0].format);
  ASSERT_EQ("97", fmtps[1].format);
  ASSERT_EQ("120", fmtps[2].format);
  ASSERT_EQ("124", fmtps[3].format);
  ASSERT_EQ("121", fmtps[4].format);
  ASSERT_EQ("125", fmtps[5].format);
  ASSERT_EQ("127", fmtps[6].format);
  ASSERT_EQ("98", fmtps[7].format);

  SetLocalAnswer(answer);
  SetRemoteAnswer(answer);

  auto offerTransceivers = mSessionOff->GetTransceivers();
  ASSERT_EQ(2U, offerTransceivers.size());
  ASSERT_FALSE(IsNull(offerTransceivers[1]->mSendTrack));
  ASSERT_FALSE(IsNull(offerTransceivers[1]->mRecvTrack));
  ASSERT_TRUE(offerTransceivers[1]->mSendTrack.GetNegotiatedDetails());
  ASSERT_TRUE(offerTransceivers[1]->mRecvTrack.GetNegotiatedDetails());
  ASSERT_EQ(6U, offerTransceivers[1]
                    ->mSendTrack.GetNegotiatedDetails()
                    ->GetEncoding(0)
                    .GetCodecs()
                    .size());
  ASSERT_EQ(6U, offerTransceivers[1]
                    ->mRecvTrack.GetNegotiatedDetails()
                    ->GetEncoding(0)
                    .GetCodecs()
                    .size());

  auto answerTransceivers = mSessionAns->GetTransceivers();
  ASSERT_EQ(2U, answerTransceivers.size());
  ASSERT_FALSE(IsNull(answerTransceivers[1]->mSendTrack));
  ASSERT_FALSE(IsNull(answerTransceivers[1]->mRecvTrack));
  ASSERT_TRUE(answerTransceivers[1]->mSendTrack.GetNegotiatedDetails());
  ASSERT_TRUE(answerTransceivers[1]->mRecvTrack.GetNegotiatedDetails());
  ASSERT_EQ(6U, answerTransceivers[1]
                    ->mSendTrack.GetNegotiatedDetails()
                    ->GetEncoding(0)
                    .GetCodecs()
                    .size());
  ASSERT_EQ(6U, answerTransceivers[1]
                    ->mRecvTrack.GetNegotiatedDetails()
                    ->GetEncoding(0)
                    .GetCodecs()
                    .size());
}

TEST_F(JsepSessionTest, ValidateAnsweredCodecParamsNoRed) {
  // TODO(bug 1099351): Once fixed, we can allow red in this offer,
  // which will also cause multiple codecs in answer.  For now,
  // red/ulpfec for video are behind a pref to mitigate potential for
  // errors.
  SetCodecEnabled(*mSessionOff, "red", false);
  for (auto& codec : mSessionAns->Codecs()) {
    if (codec->mName == "H264") {
      JsepVideoCodecDescription* h264 =
          static_cast<JsepVideoCodecDescription*>(codec.get());
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

  AddTracksToStream(*mSessionOff, "offerer_stream", "audio,video");

  std::string offer = CreateOffer();
  SetLocalOffer(offer);
  SetRemoteOffer(offer);

  AddTracksToStream(*mSessionAns, "answerer_stream", "audio,video");

  std::string answer = CreateAnswer();

  UniquePtr<Sdp> outputSdp(Parse(answer));
  ASSERT_TRUE(!!outputSdp);

  ASSERT_EQ(2U, outputSdp->GetMediaSectionCount());
  auto& video_section = outputSdp->GetMediaSection(1);
  ASSERT_EQ(SdpMediaSection::kVideo, video_section.GetMediaType());
  auto& video_attrs = video_section.GetAttributeList();
  ASSERT_EQ(SdpDirectionAttribute::kSendrecv, video_attrs.GetDirection());

  ASSERT_EQ(4U, video_section.GetFormats().size());
  ASSERT_EQ("120", video_section.GetFormats()[0]);
  ASSERT_EQ("124", video_section.GetFormats()[1]);
  ASSERT_EQ("121", video_section.GetFormats()[2]);
  ASSERT_EQ("125", video_section.GetFormats()[3]);

  // Validate rtpmap
  ASSERT_TRUE(video_attrs.HasAttribute(SdpAttribute::kRtpmapAttribute));
  auto& rtpmaps = video_attrs.GetRtpmap();
  ASSERT_TRUE(rtpmaps.HasEntry("120"));
  ASSERT_TRUE(rtpmaps.HasEntry("121"));

  auto& vp8_entry = rtpmaps.GetEntry("120");
  auto& vp9_entry = rtpmaps.GetEntry("121");

  ASSERT_EQ("VP8", vp8_entry.name);
  ASSERT_EQ("VP9", vp9_entry.name);

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
  ASSERT_EQ("121", fmtps[2].format);
  ASSERT_TRUE(!!fmtps[2].parameters);
  ASSERT_EQ(SdpRtpmapAttributeList::kVP9, fmtps[2].parameters->codec_type);

  auto& parsed_vp9_params =
      *static_cast<const SdpFmtpAttributeList::VP8Parameters*>(
          fmtps[2].parameters.get());

  ASSERT_EQ((uint32_t)12288, parsed_vp9_params.max_fs);
  ASSERT_EQ((uint32_t)60, parsed_vp9_params.max_fr);

  SetLocalAnswer(answer);
  SetRemoteAnswer(answer);

  auto offerTransceivers = mSessionOff->GetTransceivers();
  ASSERT_EQ(2U, offerTransceivers.size());
  ASSERT_FALSE(IsNull(offerTransceivers[1]->mSendTrack));
  ASSERT_FALSE(IsNull(offerTransceivers[1]->mRecvTrack));
  ASSERT_TRUE(offerTransceivers[1]->mSendTrack.GetNegotiatedDetails());
  ASSERT_TRUE(offerTransceivers[1]->mRecvTrack.GetNegotiatedDetails());
  ASSERT_EQ(2U, offerTransceivers[1]
                    ->mSendTrack.GetNegotiatedDetails()
                    ->GetEncoding(0)
                    .GetCodecs()
                    .size());
  ASSERT_EQ(2U, offerTransceivers[1]
                    ->mRecvTrack.GetNegotiatedDetails()
                    ->GetEncoding(0)
                    .GetCodecs()
                    .size());

  auto answerTransceivers = mSessionAns->GetTransceivers();
  ASSERT_EQ(2U, answerTransceivers.size());
  ASSERT_FALSE(IsNull(answerTransceivers[1]->mSendTrack));
  ASSERT_FALSE(IsNull(answerTransceivers[1]->mRecvTrack));
  ASSERT_TRUE(answerTransceivers[1]->mSendTrack.GetNegotiatedDetails());
  ASSERT_TRUE(answerTransceivers[1]->mRecvTrack.GetNegotiatedDetails());
  ASSERT_EQ(2U, answerTransceivers[1]
                    ->mSendTrack.GetNegotiatedDetails()
                    ->GetEncoding(0)
                    .GetCodecs()
                    .size());
  ASSERT_EQ(2U, answerTransceivers[1]
                    ->mRecvTrack.GetNegotiatedDetails()
                    ->GetEncoding(0)
                    .GetCodecs()
                    .size());

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

TEST_F(JsepSessionTest, OfferWithBundleGroupNoTags) {
  AddTracks(*mSessionOff, "audio,video");
  AddTracks(*mSessionAns, "audio,video");

  std::string offer = CreateOffer();
  size_t i = offer.find("a=group:BUNDLE");
  offer.insert(i, "a=group:BUNDLE\r\n");

  SetLocalOffer(offer, CHECK_SUCCESS);
  SetRemoteOffer(offer, CHECK_SUCCESS);
  std::string answer(CreateAnswer());
}

static void Replace(const std::string& toReplace, const std::string& with,
                    std::string* in) {
  size_t pos = in->find(toReplace);
  ASSERT_NE(std::string::npos, pos);
  in->replace(pos, toReplace.size(), with);
}

static void ReplaceAll(const std::string& toReplace, const std::string& with,
                       std::string* in) {
  while (in->find(toReplace) != std::string::npos) {
    Replace(toReplace, with, in);
  }
}

static void GetCodec(JsepSession& session, size_t transceiverIndex,
                     sdp::Direction direction, size_t encodingIndex,
                     size_t codecIndex,
                     UniquePtr<JsepCodecDescription>* codecOut) {
  codecOut->reset();
  ASSERT_LT(transceiverIndex, session.GetTransceivers().size());
  RefPtr<JsepTransceiver> transceiver(
      session.GetTransceivers()[transceiverIndex]);
  JsepTrack& track = (direction == sdp::kSend) ? transceiver->mSendTrack
                                               : transceiver->mRecvTrack;
  ASSERT_TRUE(track.GetNegotiatedDetails());
  ASSERT_LT(encodingIndex, track.GetNegotiatedDetails()->GetEncodingCount());
  ASSERT_LT(codecIndex, track.GetNegotiatedDetails()
                            ->GetEncoding(encodingIndex)
                            .GetCodecs()
                            .size());
  codecOut->reset(track.GetNegotiatedDetails()
                      ->GetEncoding(encodingIndex)
                      .GetCodecs()[codecIndex]
                      ->Clone());
}

static void ForceH264(JsepSession& session, uint32_t profileLevelId) {
  for (auto& codec : session.Codecs()) {
    if (codec->mName == "H264") {
      JsepVideoCodecDescription* h264 =
          static_cast<JsepVideoCodecDescription*>(codec.get());
      h264->mProfileLevelId = profileLevelId;
    } else {
      codec->mEnabled = false;
    }
  }
}

TEST_F(JsepSessionTest, TestH264Negotiation) {
  ForceH264(*mSessionOff, 0x42e00b);
  ForceH264(*mSessionAns, 0x42e00d);

  AddTracks(*mSessionOff, "video");
  AddTracks(*mSessionAns, "video");

  std::string offer(CreateOffer());
  SetLocalOffer(offer, CHECK_SUCCESS);

  SetRemoteOffer(offer, CHECK_SUCCESS);
  std::string answer(CreateAnswer());

  SetRemoteAnswer(answer, CHECK_SUCCESS);
  SetLocalAnswer(answer, CHECK_SUCCESS);

  UniquePtr<JsepCodecDescription> offererSendCodec;
  GetCodec(*mSessionOff, 0, sdp::kSend, 0, 0, &offererSendCodec);
  ASSERT_TRUE(offererSendCodec);
  ASSERT_EQ("H264", offererSendCodec->mName);
  const JsepVideoCodecDescription* offererVideoSendCodec(
      static_cast<const JsepVideoCodecDescription*>(offererSendCodec.get()));
  ASSERT_EQ((uint32_t)0x42e00d, offererVideoSendCodec->mProfileLevelId);

  UniquePtr<JsepCodecDescription> offererRecvCodec;
  GetCodec(*mSessionOff, 0, sdp::kRecv, 0, 0, &offererRecvCodec);
  ASSERT_EQ("H264", offererRecvCodec->mName);
  const JsepVideoCodecDescription* offererVideoRecvCodec(
      static_cast<const JsepVideoCodecDescription*>(offererRecvCodec.get()));
  ASSERT_EQ((uint32_t)0x42e00b, offererVideoRecvCodec->mProfileLevelId);

  UniquePtr<JsepCodecDescription> answererSendCodec;
  GetCodec(*mSessionAns, 0, sdp::kSend, 0, 0, &answererSendCodec);
  ASSERT_TRUE(answererSendCodec);
  ASSERT_EQ("H264", answererSendCodec->mName);
  const JsepVideoCodecDescription* answererVideoSendCodec(
      static_cast<const JsepVideoCodecDescription*>(answererSendCodec.get()));
  ASSERT_EQ((uint32_t)0x42e00b, answererVideoSendCodec->mProfileLevelId);

  UniquePtr<JsepCodecDescription> answererRecvCodec;
  GetCodec(*mSessionAns, 0, sdp::kRecv, 0, 0, &answererRecvCodec);
  ASSERT_EQ("H264", answererRecvCodec->mName);
  const JsepVideoCodecDescription* answererVideoRecvCodec(
      static_cast<const JsepVideoCodecDescription*>(answererRecvCodec.get()));
  ASSERT_EQ((uint32_t)0x42e00d, answererVideoRecvCodec->mProfileLevelId);
}

TEST_F(JsepSessionTest, TestH264NegotiationFails) {
  ForceH264(*mSessionOff, 0x42000b);
  ForceH264(*mSessionAns, 0x42e00d);

  AddTracks(*mSessionOff, "video");
  AddTracks(*mSessionAns, "video");

  std::string offer(CreateOffer());
  SetLocalOffer(offer, CHECK_SUCCESS);

  SetRemoteOffer(offer, CHECK_SUCCESS);
  std::string answer(CreateAnswer());

  SetRemoteAnswer(answer, CHECK_SUCCESS);
  SetLocalAnswer(answer, CHECK_SUCCESS);

  ASSERT_EQ(nullptr, GetNegotiatedTransceiver(*mSessionOff, 0));
  ASSERT_EQ(nullptr, GetNegotiatedTransceiver(*mSessionAns, 0));
}

TEST_F(JsepSessionTest, TestH264NegotiationOffererDefault) {
  ForceH264(*mSessionOff, 0x42000d);
  ForceH264(*mSessionAns, 0x42000d);

  AddTracks(*mSessionOff, "video");
  AddTracks(*mSessionAns, "video");

  std::string offer(CreateOffer());
  SetLocalOffer(offer, CHECK_SUCCESS);

  Replace("profile-level-id=42000d", "some-unknown-param=0", &offer);

  SetRemoteOffer(offer, CHECK_SUCCESS);
  std::string answer(CreateAnswer());

  SetRemoteAnswer(answer, CHECK_SUCCESS);
  SetLocalAnswer(answer, CHECK_SUCCESS);

  UniquePtr<JsepCodecDescription> answererSendCodec;
  GetCodec(*mSessionAns, 0, sdp::kSend, 0, 0, &answererSendCodec);
  ASSERT_TRUE(answererSendCodec);
  ASSERT_EQ("H264", answererSendCodec->mName);
  const JsepVideoCodecDescription* answererVideoSendCodec(
      static_cast<const JsepVideoCodecDescription*>(answererSendCodec.get()));
  ASSERT_EQ((uint32_t)0x420010, answererVideoSendCodec->mProfileLevelId);
}

TEST_F(JsepSessionTest, TestH264NegotiationOffererNoFmtp) {
  ForceH264(*mSessionOff, 0x42000d);
  ForceH264(*mSessionAns, 0x42001e);

  AddTracks(*mSessionOff, "video");
  AddTracks(*mSessionAns, "video");

  std::string offer(CreateOffer());
  SetLocalOffer(offer, CHECK_SUCCESS);

  Replace("a=fmtp", "a=oops", &offer);

  SetRemoteOffer(offer, CHECK_SUCCESS);
  std::string answer(CreateAnswer());

  SetRemoteAnswer(answer, CHECK_SUCCESS);
  SetLocalAnswer(answer, CHECK_SUCCESS);

  UniquePtr<JsepCodecDescription> answererSendCodec;
  GetCodec(*mSessionAns, 0, sdp::kSend, 0, 0, &answererSendCodec);
  ASSERT_TRUE(answererSendCodec);
  ASSERT_EQ("H264", answererSendCodec->mName);
  const JsepVideoCodecDescription* answererVideoSendCodec(
      static_cast<const JsepVideoCodecDescription*>(answererSendCodec.get()));
  ASSERT_EQ((uint32_t)0x420010, answererVideoSendCodec->mProfileLevelId);

  UniquePtr<JsepCodecDescription> answererRecvCodec;
  GetCodec(*mSessionAns, 0, sdp::kRecv, 0, 0, &answererRecvCodec);
  ASSERT_EQ("H264", answererRecvCodec->mName);
  const JsepVideoCodecDescription* answererVideoRecvCodec(
      static_cast<const JsepVideoCodecDescription*>(answererRecvCodec.get()));
  ASSERT_EQ((uint32_t)0x420010, answererVideoRecvCodec->mProfileLevelId);
}

TEST_F(JsepSessionTest, TestH264LevelAsymmetryDisallowedByOffererWithLowLevel) {
  ForceH264(*mSessionOff, 0x42e00b);
  ForceH264(*mSessionAns, 0x42e00d);

  AddTracks(*mSessionOff, "video");
  AddTracks(*mSessionAns, "video");

  std::string offer(CreateOffer());
  SetLocalOffer(offer, CHECK_SUCCESS);

  Replace("level-asymmetry-allowed=1", "level-asymmetry-allowed=0", &offer);

  SetRemoteOffer(offer, CHECK_SUCCESS);
  std::string answer(CreateAnswer());

  SetRemoteAnswer(answer, CHECK_SUCCESS);
  SetLocalAnswer(answer, CHECK_SUCCESS);

  // Offerer doesn't know about the shenanigans we've pulled here, so will
  // behave normally, and we test the normal behavior elsewhere.

  UniquePtr<JsepCodecDescription> answererSendCodec;
  GetCodec(*mSessionAns, 0, sdp::kSend, 0, 0, &answererSendCodec);
  ASSERT_TRUE(answererSendCodec);
  ASSERT_EQ("H264", answererSendCodec->mName);
  const JsepVideoCodecDescription* answererVideoSendCodec(
      static_cast<const JsepVideoCodecDescription*>(answererSendCodec.get()));
  ASSERT_EQ((uint32_t)0x42e00b, answererVideoSendCodec->mProfileLevelId);

  UniquePtr<JsepCodecDescription> answererRecvCodec;
  GetCodec(*mSessionAns, 0, sdp::kRecv, 0, 0, &answererRecvCodec);
  ASSERT_EQ("H264", answererRecvCodec->mName);
  const JsepVideoCodecDescription* answererVideoRecvCodec(
      static_cast<const JsepVideoCodecDescription*>(answererRecvCodec.get()));
  ASSERT_EQ((uint32_t)0x42e00b, answererVideoRecvCodec->mProfileLevelId);
}

TEST_F(JsepSessionTest,
       TestH264LevelAsymmetryDisallowedByOffererWithHighLevel) {
  ForceH264(*mSessionOff, 0x42e00d);
  ForceH264(*mSessionAns, 0x42e00b);

  AddTracks(*mSessionOff, "video");
  AddTracks(*mSessionAns, "video");

  std::string offer(CreateOffer());
  SetLocalOffer(offer, CHECK_SUCCESS);

  Replace("level-asymmetry-allowed=1", "level-asymmetry-allowed=0", &offer);

  SetRemoteOffer(offer, CHECK_SUCCESS);
  std::string answer(CreateAnswer());

  SetRemoteAnswer(answer, CHECK_SUCCESS);
  SetLocalAnswer(answer, CHECK_SUCCESS);

  // Offerer doesn't know about the shenanigans we've pulled here, so will
  // behave normally, and we test the normal behavior elsewhere.

  UniquePtr<JsepCodecDescription> answererSendCodec;
  GetCodec(*mSessionAns, 0, sdp::kSend, 0, 0, &answererSendCodec);
  ASSERT_TRUE(answererSendCodec);
  ASSERT_EQ("H264", answererSendCodec->mName);
  const JsepVideoCodecDescription* answererVideoSendCodec(
      static_cast<const JsepVideoCodecDescription*>(answererSendCodec.get()));
  ASSERT_EQ((uint32_t)0x42e00b, answererVideoSendCodec->mProfileLevelId);

  UniquePtr<JsepCodecDescription> answererRecvCodec;
  GetCodec(*mSessionAns, 0, sdp::kRecv, 0, 0, &answererRecvCodec);
  ASSERT_EQ("H264", answererRecvCodec->mName);
  const JsepVideoCodecDescription* answererVideoRecvCodec(
      static_cast<const JsepVideoCodecDescription*>(answererRecvCodec.get()));
  ASSERT_EQ((uint32_t)0x42e00b, answererVideoRecvCodec->mProfileLevelId);
}

TEST_F(JsepSessionTest,
       TestH264LevelAsymmetryDisallowedByAnswererWithLowLevel) {
  ForceH264(*mSessionOff, 0x42e00d);
  ForceH264(*mSessionAns, 0x42e00b);

  AddTracks(*mSessionOff, "video");
  AddTracks(*mSessionAns, "video");

  std::string offer(CreateOffer());
  SetLocalOffer(offer, CHECK_SUCCESS);
  SetRemoteOffer(offer, CHECK_SUCCESS);
  std::string answer(CreateAnswer());

  Replace("level-asymmetry-allowed=1", "level-asymmetry-allowed=0", &answer);

  SetRemoteAnswer(answer, CHECK_SUCCESS);
  SetLocalAnswer(answer, CHECK_SUCCESS);

  UniquePtr<JsepCodecDescription> offererSendCodec;
  GetCodec(*mSessionOff, 0, sdp::kSend, 0, 0, &offererSendCodec);
  ASSERT_TRUE(offererSendCodec);
  ASSERT_EQ("H264", offererSendCodec->mName);
  const JsepVideoCodecDescription* offererVideoSendCodec(
      static_cast<const JsepVideoCodecDescription*>(offererSendCodec.get()));
  ASSERT_EQ((uint32_t)0x42e00b, offererVideoSendCodec->mProfileLevelId);

  UniquePtr<JsepCodecDescription> offererRecvCodec;
  GetCodec(*mSessionOff, 0, sdp::kRecv, 0, 0, &offererRecvCodec);
  ASSERT_EQ("H264", offererRecvCodec->mName);
  const JsepVideoCodecDescription* offererVideoRecvCodec(
      static_cast<const JsepVideoCodecDescription*>(offererRecvCodec.get()));
  ASSERT_EQ((uint32_t)0x42e00b, offererVideoRecvCodec->mProfileLevelId);

  // Answerer doesn't know we've pulled these shenanigans, it should act as if
  // it did not set level-asymmetry-required, and we already check that
  // elsewhere
}

TEST_F(JsepSessionTest,
       TestH264LevelAsymmetryDisallowedByAnswererWithHighLevel) {
  ForceH264(*mSessionOff, 0x42e00b);
  ForceH264(*mSessionAns, 0x42e00d);

  AddTracks(*mSessionOff, "video");
  AddTracks(*mSessionAns, "video");

  std::string offer(CreateOffer());
  SetLocalOffer(offer, CHECK_SUCCESS);
  SetRemoteOffer(offer, CHECK_SUCCESS);
  std::string answer(CreateAnswer());

  Replace("level-asymmetry-allowed=1", "level-asymmetry-allowed=0", &answer);

  SetRemoteAnswer(answer, CHECK_SUCCESS);
  SetLocalAnswer(answer, CHECK_SUCCESS);

  UniquePtr<JsepCodecDescription> offererSendCodec;
  GetCodec(*mSessionOff, 0, sdp::kSend, 0, 0, &offererSendCodec);
  ASSERT_TRUE(offererSendCodec);
  ASSERT_EQ("H264", offererSendCodec->mName);
  const JsepVideoCodecDescription* offererVideoSendCodec(
      static_cast<const JsepVideoCodecDescription*>(offererSendCodec.get()));
  ASSERT_EQ((uint32_t)0x42e00b, offererVideoSendCodec->mProfileLevelId);

  UniquePtr<JsepCodecDescription> offererRecvCodec;
  GetCodec(*mSessionOff, 0, sdp::kRecv, 0, 0, &offererRecvCodec);
  ASSERT_EQ("H264", offererRecvCodec->mName);
  const JsepVideoCodecDescription* offererVideoRecvCodec(
      static_cast<const JsepVideoCodecDescription*>(offererRecvCodec.get()));
  ASSERT_EQ((uint32_t)0x42e00b, offererVideoRecvCodec->mProfileLevelId);

  // Answerer doesn't know we've pulled these shenanigans, it should act as if
  // it did not set level-asymmetry-required, and we already check that
  // elsewhere
}

TEST_P(JsepSessionTest, TestRejectMline) {
  // We need to do this before adding tracks
  types = BuildTypes(GetParam());

  SdpMediaSection::MediaType type = types.front();

  switch (type) {
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
      ASSERT_TRUE(false)
      << "Unknown media type";
  }

  AddTracks(*mSessionOff);
  AddTracks(*mSessionAns);

  std::string offer = CreateOffer();
  mSessionOff->SetLocalDescription(kJsepSdpOffer, offer);
  mSessionAns->SetRemoteDescription(kJsepSdpOffer, offer);

  std::string answer = CreateAnswer();

  UniquePtr<Sdp> outputSdp(Parse(answer));
  ASSERT_TRUE(!!outputSdp);

  ASSERT_NE(0U, outputSdp->GetMediaSectionCount());
  SdpMediaSection* failed_section = nullptr;

  for (size_t i = 0; i < outputSdp->GetMediaSectionCount(); ++i) {
    if (outputSdp->GetMediaSection(i).GetMediaType() == type) {
      failed_section = &outputSdp->GetMediaSection(i);
    }
  }

  ASSERT_TRUE(failed_section)
  << "Failed type was entirely absent from SDP";
  auto& failed_attrs = failed_section->GetAttributeList();
  ASSERT_EQ(SdpDirectionAttribute::kInactive, failed_attrs.GetDirection());
  ASSERT_EQ(0U, failed_section->GetPort());

  mSessionAns->SetLocalDescription(kJsepSdpAnswer, answer);
  mSessionOff->SetRemoteDescription(kJsepSdpAnswer, answer);

  size_t numRejected = std::count(types.begin(), types.end(), type);
  size_t numAccepted = types.size() - numRejected;

  if (type == SdpMediaSection::MediaType::kApplication) {
    ASSERT_TRUE(GetDatachannelTransceiver(*mSessionOff));
    ASSERT_FALSE(
        GetDatachannelTransceiver(*mSessionOff)->mRecvTrack.GetActive());
    ASSERT_TRUE(GetDatachannelTransceiver(*mSessionAns));
    ASSERT_FALSE(
        GetDatachannelTransceiver(*mSessionAns)->mRecvTrack.GetActive());
  } else {
    ASSERT_EQ(types.size(), GetLocalTracks(*mSessionOff).size());
    ASSERT_EQ(numAccepted, GetRemoteTracks(*mSessionOff).size());

    ASSERT_EQ(types.size(), GetLocalTracks(*mSessionAns).size());
    ASSERT_EQ(types.size(), GetRemoteTracks(*mSessionAns).size());
  }
}

TEST_F(JsepSessionTest, NegotiationNoMlines) { OfferAnswer(); }

TEST_F(JsepSessionTest, TestIceLite) {
  AddTracks(*mSessionOff, "audio");
  AddTracks(*mSessionAns, "audio");
  std::string offer = CreateOffer();
  SetLocalOffer(offer, CHECK_SUCCESS);

  UniquePtr<Sdp> parsedOffer(Parse(offer));
  parsedOffer->GetAttributeList().SetAttribute(
      new SdpFlagAttribute(SdpAttribute::kIceLiteAttribute));

  std::ostringstream os;
  parsedOffer->Serialize(os);
  SetRemoteOffer(os.str(), CHECK_SUCCESS);

  ASSERT_TRUE(mSessionAns->RemoteIsIceLite());
  ASSERT_FALSE(mSessionOff->RemoteIsIceLite());
}

TEST_F(JsepSessionTest, TestIceOptions) {
  AddTracks(*mSessionOff, "audio");
  AddTracks(*mSessionAns, "audio");
  std::string offer = CreateOffer();
  SetLocalOffer(offer, CHECK_SUCCESS);
  SetRemoteOffer(offer, CHECK_SUCCESS);
  std::string answer = CreateAnswer();
  SetLocalAnswer(answer, CHECK_SUCCESS);
  SetRemoteAnswer(answer, CHECK_SUCCESS);

  ASSERT_EQ(1U, mSessionOff->GetIceOptions().size());
  ASSERT_EQ("trickle", mSessionOff->GetIceOptions()[0]);

  ASSERT_EQ(1U, mSessionAns->GetIceOptions().size());
  ASSERT_EQ("trickle", mSessionAns->GetIceOptions()[0]);
}

TEST_F(JsepSessionTest, TestIceRestart) {
  AddTracks(*mSessionOff, "audio");
  AddTracks(*mSessionAns, "audio");
  std::string offer = CreateOffer();
  SetLocalOffer(offer, CHECK_SUCCESS);
  SetRemoteOffer(offer, CHECK_SUCCESS);
  std::string answer = CreateAnswer();
  SetLocalAnswer(answer, CHECK_SUCCESS);
  SetRemoteAnswer(answer, CHECK_SUCCESS);

  JsepOfferOptions options;
  options.mIceRestart = Some(true);

  std::string reoffer = CreateOffer(Some(options));
  SetLocalOffer(reoffer, CHECK_SUCCESS);
  SetRemoteOffer(reoffer, CHECK_SUCCESS);
  std::string reanswer = CreateAnswer();
  SetLocalAnswer(reanswer, CHECK_SUCCESS);
  SetRemoteAnswer(reanswer, CHECK_SUCCESS);

  UniquePtr<Sdp> parsedOffer(Parse(offer));
  ASSERT_EQ(1U, parsedOffer->GetMediaSectionCount());
  UniquePtr<Sdp> parsedReoffer(Parse(reoffer));
  ASSERT_EQ(1U, parsedReoffer->GetMediaSectionCount());

  UniquePtr<Sdp> parsedAnswer(Parse(answer));
  ASSERT_EQ(1U, parsedAnswer->GetMediaSectionCount());
  UniquePtr<Sdp> parsedReanswer(Parse(reanswer));
  ASSERT_EQ(1U, parsedReanswer->GetMediaSectionCount());

  // verify ice pwd/ufrag are present in offer/answer and reoffer/reanswer
  auto& offerMediaAttrs = parsedOffer->GetMediaSection(0).GetAttributeList();
  ASSERT_TRUE(offerMediaAttrs.HasAttribute(SdpAttribute::kIcePwdAttribute));
  ASSERT_TRUE(offerMediaAttrs.HasAttribute(SdpAttribute::kIceUfragAttribute));

  auto& reofferMediaAttrs =
      parsedReoffer->GetMediaSection(0).GetAttributeList();
  ASSERT_TRUE(reofferMediaAttrs.HasAttribute(SdpAttribute::kIcePwdAttribute));
  ASSERT_TRUE(reofferMediaAttrs.HasAttribute(SdpAttribute::kIceUfragAttribute));

  auto& answerMediaAttrs = parsedAnswer->GetMediaSection(0).GetAttributeList();
  ASSERT_TRUE(answerMediaAttrs.HasAttribute(SdpAttribute::kIcePwdAttribute));
  ASSERT_TRUE(answerMediaAttrs.HasAttribute(SdpAttribute::kIceUfragAttribute));

  auto& reanswerMediaAttrs =
      parsedReanswer->GetMediaSection(0).GetAttributeList();
  ASSERT_TRUE(reanswerMediaAttrs.HasAttribute(SdpAttribute::kIcePwdAttribute));
  ASSERT_TRUE(
      reanswerMediaAttrs.HasAttribute(SdpAttribute::kIceUfragAttribute));

  // make sure offer/reoffer ice pwd/ufrag changed on ice restart
  ASSERT_NE(offerMediaAttrs.GetIcePwd().c_str(),
            reofferMediaAttrs.GetIcePwd().c_str());
  ASSERT_NE(offerMediaAttrs.GetIceUfrag().c_str(),
            reofferMediaAttrs.GetIceUfrag().c_str());

  // make sure answer/reanswer ice pwd/ufrag changed on ice restart
  ASSERT_NE(answerMediaAttrs.GetIcePwd().c_str(),
            reanswerMediaAttrs.GetIcePwd().c_str());
  ASSERT_NE(answerMediaAttrs.GetIceUfrag().c_str(),
            reanswerMediaAttrs.GetIceUfrag().c_str());

  auto offererTransceivers = mSessionOff->GetTransceivers();
  auto answererTransceivers = mSessionAns->GetTransceivers();
  ASSERT_EQ(reofferMediaAttrs.GetIceUfrag(),
            offererTransceivers[0]->mTransport.mLocalUfrag);
  ASSERT_EQ(reofferMediaAttrs.GetIceUfrag(),
            answererTransceivers[0]->mTransport.mIce->GetUfrag());
  ASSERT_EQ(reofferMediaAttrs.GetIcePwd(),
            offererTransceivers[0]->mTransport.mLocalPwd);
  ASSERT_EQ(reofferMediaAttrs.GetIcePwd(),
            answererTransceivers[0]->mTransport.mIce->GetPassword());

  ASSERT_EQ(reanswerMediaAttrs.GetIceUfrag(),
            answererTransceivers[0]->mTransport.mLocalUfrag);
  ASSERT_EQ(reanswerMediaAttrs.GetIceUfrag(),
            offererTransceivers[0]->mTransport.mIce->GetUfrag());
  ASSERT_EQ(reanswerMediaAttrs.GetIcePwd(),
            answererTransceivers[0]->mTransport.mLocalPwd);
  ASSERT_EQ(reanswerMediaAttrs.GetIcePwd(),
            offererTransceivers[0]->mTransport.mIce->GetPassword());
}

TEST_F(JsepSessionTest, TestAnswererIndicatingIceRestart) {
  AddTracks(*mSessionOff, "audio");
  AddTracks(*mSessionAns, "audio");
  std::string offer = CreateOffer();
  SetLocalOffer(offer, CHECK_SUCCESS);
  SetRemoteOffer(offer, CHECK_SUCCESS);
  std::string answer = CreateAnswer();
  SetLocalAnswer(answer, CHECK_SUCCESS);
  SetRemoteAnswer(answer, CHECK_SUCCESS);

  // reoffer, but we'll improperly indicate an ice restart in the answer by
  // modifying the ice pwd and ufrag
  std::string reoffer = CreateOffer();
  SetLocalOffer(reoffer, CHECK_SUCCESS);
  SetRemoteOffer(reoffer, CHECK_SUCCESS);
  std::string reanswer = CreateAnswer();

  // change the ice pwd and ufrag
  ReplaceInSdp(&reanswer, "a=ice-ufrag:", "a=ice-ufrag:bad-");
  ReplaceInSdp(&reanswer, "a=ice-pwd:", "a=ice-pwd:bad-");
  SetLocalAnswer(reanswer, CHECK_SUCCESS);
  JsepSession::Result result =
      mSessionOff->SetRemoteDescription(kJsepSdpAnswer, reanswer);
  ASSERT_EQ(dom::PCError::InvalidAccessError, *result.mError);
}

TEST_F(JsepSessionTest, TestExtmap) {
  AddTracks(*mSessionOff, "audio");
  AddTracks(*mSessionAns, "audio");
  // ssrc-audio-level will be extmap 1 for both
  // csrc-audio-level will be 2 for both
  // mid will be 3 for both
  // video related extensions take 4 - 7
  mSessionOff->AddAudioRtpExtension("foo");  // Default mapping of 8
  mSessionOff->AddAudioRtpExtension("bar");  // Default mapping of 9
  mSessionAns->AddAudioRtpExtension("bar");  // Default mapping of 8
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
  ASSERT_EQ(5U, offerExtmap.size());
  ASSERT_EQ("urn:ietf:params:rtp-hdrext:ssrc-audio-level",
            offerExtmap[0].extensionname);
  ASSERT_EQ(1U, offerExtmap[0].entry);
  ASSERT_EQ("urn:ietf:params:rtp-hdrext:csrc-audio-level",
            offerExtmap[1].extensionname);
  ASSERT_EQ(2U, offerExtmap[1].entry);
  ASSERT_EQ("urn:ietf:params:rtp-hdrext:sdes:mid",
            offerExtmap[2].extensionname);
  ASSERT_EQ(3U, offerExtmap[2].entry);
  ASSERT_EQ("foo", offerExtmap[3].extensionname);
  ASSERT_EQ(8U, offerExtmap[3].entry);
  ASSERT_EQ("bar", offerExtmap[4].extensionname);
  ASSERT_EQ(9U, offerExtmap[4].entry);

  UniquePtr<Sdp> parsedAnswer(Parse(answer));
  ASSERT_EQ(1U, parsedAnswer->GetMediaSectionCount());

  auto& answerMediaAttrs = parsedAnswer->GetMediaSection(0).GetAttributeList();
  ASSERT_TRUE(answerMediaAttrs.HasAttribute(SdpAttribute::kExtmapAttribute));
  auto& answerExtmap = answerMediaAttrs.GetExtmap().mExtmaps;
  ASSERT_EQ(3U, answerExtmap.size());
  ASSERT_EQ("urn:ietf:params:rtp-hdrext:ssrc-audio-level",
            answerExtmap[0].extensionname);
  ASSERT_EQ(1U, answerExtmap[0].entry);
  ASSERT_EQ("urn:ietf:params:rtp-hdrext:sdes:mid",
            answerExtmap[1].extensionname);
  ASSERT_EQ(3U, answerExtmap[1].entry);
  // We ensure that the entry for "bar" matches what was in the offer
  ASSERT_EQ("bar", answerExtmap[2].extensionname);
  ASSERT_EQ(9U, answerExtmap[2].entry);
}

TEST_F(JsepSessionTest, TestExtmapDefaults) {
  types.push_back(SdpMediaSection::kAudio);
  types.push_back(SdpMediaSection::kVideo);
  AddTracks(*mSessionOff, "audio,video");

  std::string offer = CreateOffer();
  SetLocalOffer(offer, CHECK_SUCCESS);
  SetRemoteOffer(offer, CHECK_SUCCESS);

  std::string answer = CreateAnswer();
  SetLocalAnswer(answer, CHECK_SUCCESS);
  SetRemoteAnswer(answer, CHECK_SUCCESS);

  UniquePtr<Sdp> parsedOffer(Parse(offer));
  ASSERT_EQ(2U, parsedOffer->GetMediaSectionCount());

  auto& offerAudioMediaAttrs =
      parsedOffer->GetMediaSection(0).GetAttributeList();
  ASSERT_TRUE(
      offerAudioMediaAttrs.HasAttribute(SdpAttribute::kExtmapAttribute));
  auto& offerAudioExtmap = offerAudioMediaAttrs.GetExtmap().mExtmaps;
  ASSERT_EQ(3U, offerAudioExtmap.size());

  ASSERT_EQ("urn:ietf:params:rtp-hdrext:ssrc-audio-level",
            offerAudioExtmap[0].extensionname);
  ASSERT_EQ(1U, offerAudioExtmap[0].entry);
  ASSERT_EQ("urn:ietf:params:rtp-hdrext:csrc-audio-level",
            offerAudioExtmap[1].extensionname);
  ASSERT_EQ(2U, offerAudioExtmap[1].entry);
  ASSERT_EQ("urn:ietf:params:rtp-hdrext:sdes:mid",
            offerAudioExtmap[2].extensionname);

  auto& offerVideoMediaAttrs =
      parsedOffer->GetMediaSection(1).GetAttributeList();
  ASSERT_TRUE(
      offerVideoMediaAttrs.HasAttribute(SdpAttribute::kExtmapAttribute));
  auto& offerVideoExtmap = offerVideoMediaAttrs.GetExtmap().mExtmaps;
  ASSERT_EQ(5U, offerVideoExtmap.size());

  ASSERT_EQ(3U, offerVideoExtmap[0].entry);
  ASSERT_EQ("urn:ietf:params:rtp-hdrext:sdes:mid",
            offerVideoExtmap[0].extensionname);
  ASSERT_EQ("http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time",
            offerVideoExtmap[1].extensionname);
  ASSERT_EQ(4U, offerVideoExtmap[1].entry);
  ASSERT_EQ("urn:ietf:params:rtp-hdrext:toffset",
            offerVideoExtmap[2].extensionname);
  ASSERT_EQ(5U, offerVideoExtmap[2].entry);
  ASSERT_EQ("http://www.webrtc.org/experiments/rtp-hdrext/playout-delay",
            offerVideoExtmap[3].extensionname);
  ASSERT_EQ(6U, offerVideoExtmap[3].entry);
  ASSERT_EQ(
      "http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-"
      "extensions-01",
      offerVideoExtmap[4].extensionname);
  ASSERT_EQ(7U, offerVideoExtmap[4].entry);

  UniquePtr<Sdp> parsedAnswer(Parse(answer));
  ASSERT_EQ(2U, parsedAnswer->GetMediaSectionCount());

  auto& answerAudioMediaAttrs =
      parsedAnswer->GetMediaSection(0).GetAttributeList();
  ASSERT_TRUE(
      answerAudioMediaAttrs.HasAttribute(SdpAttribute::kExtmapAttribute));
  auto& answerAudioExtmap = answerAudioMediaAttrs.GetExtmap().mExtmaps;
  ASSERT_EQ(2U, answerAudioExtmap.size());

  ASSERT_EQ("urn:ietf:params:rtp-hdrext:ssrc-audio-level",
            answerAudioExtmap[0].extensionname);
  ASSERT_EQ(1U, answerAudioExtmap[0].entry);
  ASSERT_EQ("urn:ietf:params:rtp-hdrext:sdes:mid",
            answerAudioExtmap[1].extensionname);
  ASSERT_EQ(3U, answerAudioExtmap[1].entry);

  auto& answerVideoMediaAttrs =
      parsedAnswer->GetMediaSection(1).GetAttributeList();
  ASSERT_TRUE(
      answerVideoMediaAttrs.HasAttribute(SdpAttribute::kExtmapAttribute));
  auto& answerVideoExtmap = answerVideoMediaAttrs.GetExtmap().mExtmaps;
  ASSERT_EQ(4U, answerVideoExtmap.size());

  ASSERT_EQ(3U, answerVideoExtmap[0].entry);
  ASSERT_EQ("urn:ietf:params:rtp-hdrext:sdes:mid",
            answerVideoExtmap[0].extensionname);
  ASSERT_EQ("http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time",
            answerVideoExtmap[1].extensionname);
  ASSERT_EQ(4U, answerVideoExtmap[1].entry);
  ASSERT_EQ("urn:ietf:params:rtp-hdrext:toffset",
            answerVideoExtmap[2].extensionname);
  ASSERT_EQ(5U, answerVideoExtmap[2].entry);
  ASSERT_EQ(
      "http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-"
      "extensions-01",
      answerVideoExtmap[3].extensionname);
  ASSERT_EQ(7U, answerVideoExtmap[3].entry);
}

TEST_F(JsepSessionTest, TestExtmapWithDuplicates) {
  AddTracks(*mSessionOff, "audio");
  AddTracks(*mSessionAns, "audio");
  // ssrc-audio-level will be extmap 1 for both
  // csrc-audio-level will be 2 for both
  // mid will be 3 for both
  // video related extensions take 4 - 7
  mSessionOff->AddAudioRtpExtension("foo");  // Default mapping of 8
  mSessionOff->AddAudioRtpExtension("bar");  // Default mapping of 9
  mSessionOff->AddAudioRtpExtension("bar");  // Should be ignored
  mSessionOff->AddAudioRtpExtension("bar");  // Should be ignored
  mSessionOff->AddAudioRtpExtension("baz");  // Default mapping of 10
  mSessionOff->AddAudioRtpExtension("bar");  // Should be ignored

  std::string offer = CreateOffer();
  UniquePtr<Sdp> parsedOffer(Parse(offer));
  ASSERT_EQ(1U, parsedOffer->GetMediaSectionCount());

  auto& offerMediaAttrs = parsedOffer->GetMediaSection(0).GetAttributeList();
  ASSERT_TRUE(offerMediaAttrs.HasAttribute(SdpAttribute::kExtmapAttribute));
  auto& offerExtmap = offerMediaAttrs.GetExtmap().mExtmaps;
  ASSERT_EQ(6U, offerExtmap.size());
  ASSERT_EQ("urn:ietf:params:rtp-hdrext:ssrc-audio-level",
            offerExtmap[0].extensionname);
  ASSERT_EQ(1U, offerExtmap[0].entry);
  ASSERT_EQ("urn:ietf:params:rtp-hdrext:csrc-audio-level",
            offerExtmap[1].extensionname);
  ASSERT_EQ(2U, offerExtmap[1].entry);
  ASSERT_EQ("urn:ietf:params:rtp-hdrext:sdes:mid",
            offerExtmap[2].extensionname);
  ASSERT_EQ(3U, offerExtmap[2].entry);
  ASSERT_EQ("foo", offerExtmap[3].extensionname);
  ASSERT_EQ(8U, offerExtmap[3].entry);
  ASSERT_EQ("bar", offerExtmap[4].extensionname);
  ASSERT_EQ(9U, offerExtmap[4].entry);
  ASSERT_EQ("baz", offerExtmap[5].extensionname);
  ASSERT_EQ(10U, offerExtmap[5].entry);
}

TEST_F(JsepSessionTest, TestExtmapZeroId) {
  AddTracks(*mSessionOff, "video");
  AddTracks(*mSessionAns, "video");

  std::string sdp =
      "v=0\r\n"
      "o=- 6 2 IN IP4 1r\r\n"
      "t=0 0a\r\n"
      "a=ice-ufrag:Xp\r\n"
      "a=ice-pwd:he\r\n"
      "a=setup:actpass\r\n"
      "a=fingerprint:sha-256 "
      "DC:FC:25:56:2B:88:77:2F:E4:FA:97:4E:2E:F1:D6:34:A6:A0:11:E2:E4:38:B3:98:"
      "08:D2:F7:9D:F5:E2:C1:15\r\n"
      "m=video 9 UDP/TLS/RTP/SAVPF 100\r\n"
      "c=IN IP4 0\r\n"
      "a=rtpmap:100 VP8/90000\r\n"
      "a=extmap:0 urn:ietf:params:rtp-hdrext:toffset\r\n";
  auto result = mSessionAns->SetRemoteDescription(kJsepSdpOffer, sdp);
  ASSERT_TRUE(result.mError == Some(dom::PCError::OperationError));
  ASSERT_EQ(
      "Description contains invalid extension id 0 on level 0 which is"
      " unsupported until 2-byte rtp header extensions are supported in"
      " webrtc.org",
      mSessionAns->GetLastError());
}

TEST_F(JsepSessionTest, TestExtmapInvalidId) {
  AddTracks(*mSessionOff, "video");
  AddTracks(*mSessionAns, "video");

  std::string sdp =
      "v=0\r\n"
      "o=- 6 2 IN IP4 1r\r\n"
      "t=0 0a\r\n"
      "a=ice-ufrag:Xp\r\n"
      "a=ice-pwd:he\r\n"
      "a=setup:actpass\r\n"
      "a=fingerprint:sha-256 "
      "DC:FC:25:56:2B:88:77:2F:E4:FA:97:4E:2E:F1:D6:34:A6:A0:11:E2:E4:38:B3:98:"
      "08:D2:F7:9D:F5:E2:C1:15\r\n"
      "m=video 9 UDP/TLS/RTP/SAVPF 100\r\n"
      "c=IN IP4 0\r\n"
      "a=rtpmap:100 VP8/90000\r\n"
      "a=extmap:15 urn:ietf:params:rtp-hdrext:toffset\r\n";
  auto result = mSessionAns->SetRemoteDescription(kJsepSdpOffer, sdp);
  ASSERT_TRUE(result.mError == Some(dom::PCError::OperationError));
  ASSERT_EQ(
      "Description contains invalid extension id 15 on level 0 which is"
      " unsupported until 2-byte rtp header extensions are supported in"
      " webrtc.org",
      mSessionAns->GetLastError());
}

TEST_F(JsepSessionTest, TestExtmapDuplicateId) {
  AddTracks(*mSessionOff, "video");
  AddTracks(*mSessionAns, "video");

  std::string sdp =
      "v=0\r\n"
      "o=- 6 2 IN IP4 1r\r\n"
      "t=0 0a\r\n"
      "a=ice-ufrag:Xp\r\n"
      "a=ice-pwd:he\r\n"
      "a=setup:actpass\r\n"
      "a=fingerprint:sha-256 "
      "DC:FC:25:56:2B:88:77:2F:E4:FA:97:4E:2E:F1:D6:34:A6:A0:11:E2:E4:38:B3:98:"
      "08:D2:F7:9D:F5:E2:C1:15\r\n"
      "m=video 9 UDP/TLS/RTP/SAVPF 100\r\n"
      "c=IN IP4 0\r\n"
      "a=rtpmap:100 VP8/90000\r\n"
      "a=extmap:2 urn:ietf:params:rtp-hdrext:toffset\r\n"
      "a=extmap:2 "
      "http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time\r\n";
  auto result = mSessionAns->SetRemoteDescription(kJsepSdpOffer, sdp);
  ASSERT_TRUE(result.mError == Some(dom::PCError::OperationError));
  ASSERT_EQ("Description contains duplicate extension id 2 on level 0",
            mSessionAns->GetLastError());
}

TEST_F(JsepSessionTest, TestRtcpFbStar) {
  AddTracks(*mSessionOff, "video");
  AddTracks(*mSessionAns, "video");

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

  ASSERT_EQ(1U, GetRemoteTracks(*mSessionAns).size());
  JsepTrack track = GetRemoteTracks(*mSessionAns)[0];
  ASSERT_TRUE(track.GetNegotiatedDetails());
  auto* details = track.GetNegotiatedDetails();
  for (const auto& codec : details->GetEncoding(0).GetCodecs()) {
    const JsepVideoCodecDescription* videoCodec =
        static_cast<const JsepVideoCodecDescription*>(codec.get());
    ASSERT_EQ(1U, videoCodec->mNackFbTypes.size());
    ASSERT_EQ("", videoCodec->mNackFbTypes[0]);
  }
}

TEST_F(JsepSessionTest, TestUniquePayloadTypes) {
  // The audio payload types will all appear more than once, but the video
  // payload types will be unique.
  AddTracks(*mSessionOff, "audio,audio,video");
  AddTracks(*mSessionAns, "audio,audio,video");

  std::string offer = CreateOffer();
  SetLocalOffer(offer, CHECK_SUCCESS);
  SetRemoteOffer(offer, CHECK_SUCCESS);
  std::string answer = CreateAnswer();
  SetLocalAnswer(answer, CHECK_SUCCESS);
  SetRemoteAnswer(answer, CHECK_SUCCESS);

  auto offerTransceivers = mSessionOff->GetTransceivers();
  auto answerTransceivers = mSessionAns->GetTransceivers();
  ASSERT_EQ(3U, offerTransceivers.size());
  ASSERT_EQ(3U, answerTransceivers.size());

  ASSERT_FALSE(IsNull(offerTransceivers[0]->mRecvTrack));
  ASSERT_TRUE(offerTransceivers[0]->mRecvTrack.GetNegotiatedDetails());
  ASSERT_EQ(0U, offerTransceivers[0]
                    ->mRecvTrack.GetNegotiatedDetails()
                    ->GetUniquePayloadTypes()
                    .size());

  ASSERT_FALSE(IsNull(offerTransceivers[1]->mRecvTrack));
  ASSERT_TRUE(offerTransceivers[1]->mRecvTrack.GetNegotiatedDetails());
  ASSERT_EQ(0U, offerTransceivers[1]
                    ->mRecvTrack.GetNegotiatedDetails()
                    ->GetUniquePayloadTypes()
                    .size());

  ASSERT_FALSE(IsNull(offerTransceivers[2]->mRecvTrack));
  ASSERT_TRUE(offerTransceivers[2]->mRecvTrack.GetNegotiatedDetails());
  ASSERT_NE(0U, offerTransceivers[2]
                    ->mRecvTrack.GetNegotiatedDetails()
                    ->GetUniquePayloadTypes()
                    .size());

  ASSERT_FALSE(IsNull(answerTransceivers[0]->mRecvTrack));
  ASSERT_TRUE(answerTransceivers[0]->mRecvTrack.GetNegotiatedDetails());
  ASSERT_EQ(0U, answerTransceivers[0]
                    ->mRecvTrack.GetNegotiatedDetails()
                    ->GetUniquePayloadTypes()
                    .size());

  ASSERT_FALSE(IsNull(answerTransceivers[1]->mRecvTrack));
  ASSERT_TRUE(answerTransceivers[1]->mRecvTrack.GetNegotiatedDetails());
  ASSERT_EQ(0U, answerTransceivers[1]
                    ->mRecvTrack.GetNegotiatedDetails()
                    ->GetUniquePayloadTypes()
                    .size());

  ASSERT_FALSE(IsNull(answerTransceivers[2]->mRecvTrack));
  ASSERT_TRUE(answerTransceivers[2]->mRecvTrack.GetNegotiatedDetails());
  ASSERT_NE(0U, answerTransceivers[2]
                    ->mRecvTrack.GetNegotiatedDetails()
                    ->GetUniquePayloadTypes()
                    .size());
}

TEST_F(JsepSessionTest, UnknownFingerprintAlgorithm) {
  types.push_back(SdpMediaSection::kAudio);
  AddTracks(*mSessionOff, "audio");
  AddTracks(*mSessionAns, "audio");

  std::string offer(CreateOffer());
  SetLocalOffer(offer);
  ReplaceAll("fingerprint:sha", "fingerprint:foo", &offer);
  JsepSession::Result result =
      mSessionAns->SetRemoteDescription(kJsepSdpOffer, offer);
  ASSERT_EQ(dom::PCError::OperationError, *result.mError);
  ASSERT_NE("", mSessionAns->GetLastError());
}

TEST(H264ProfileLevelIdTest, TestLevelComparisons)
{
  ASSERT_LT(JsepVideoCodecDescription::GetSaneH264Level(0x421D0B),   // 1b
            JsepVideoCodecDescription::GetSaneH264Level(0x420D0B));  // 1.1
  ASSERT_LT(JsepVideoCodecDescription::GetSaneH264Level(0x420D0A),   // 1.0
            JsepVideoCodecDescription::GetSaneH264Level(0x421D0B));  // 1b
  ASSERT_LT(JsepVideoCodecDescription::GetSaneH264Level(0x420D0A),   // 1.0
            JsepVideoCodecDescription::GetSaneH264Level(0x420D0B));  // 1.1

  ASSERT_LT(JsepVideoCodecDescription::GetSaneH264Level(0x640009),   // 1b
            JsepVideoCodecDescription::GetSaneH264Level(0x64000B));  // 1.1
  ASSERT_LT(JsepVideoCodecDescription::GetSaneH264Level(0x64000A),   // 1.0
            JsepVideoCodecDescription::GetSaneH264Level(0x640009));  // 1b
  ASSERT_LT(JsepVideoCodecDescription::GetSaneH264Level(0x64000A),   // 1.0
            JsepVideoCodecDescription::GetSaneH264Level(0x64000B));  // 1.1
}

TEST(H264ProfileLevelIdTest, TestLevelSetting)
{
  uint32_t profileLevelId = 0x420D0A;
  JsepVideoCodecDescription::SetSaneH264Level(
      JsepVideoCodecDescription::GetSaneH264Level(0x42100B), &profileLevelId);
  ASSERT_EQ((uint32_t)0x421D0B, profileLevelId);

  JsepVideoCodecDescription::SetSaneH264Level(
      JsepVideoCodecDescription::GetSaneH264Level(0x42000A), &profileLevelId);
  ASSERT_EQ((uint32_t)0x420D0A, profileLevelId);

  profileLevelId = 0x6E100A;
  JsepVideoCodecDescription::SetSaneH264Level(
      JsepVideoCodecDescription::GetSaneH264Level(0x640009), &profileLevelId);
  ASSERT_EQ((uint32_t)0x6E1009, profileLevelId);

  JsepVideoCodecDescription::SetSaneH264Level(
      JsepVideoCodecDescription::GetSaneH264Level(0x64000B), &profileLevelId);
  ASSERT_EQ((uint32_t)0x6E100B, profileLevelId);
}

TEST_F(JsepSessionTest, StronglyPreferredCodec) {
  for (auto& codec : mSessionAns->Codecs()) {
    if (codec->mName == "H264") {
      codec->mStronglyPreferred = true;
    }
  }

  types.push_back(SdpMediaSection::kVideo);
  AddTracks(*mSessionOff, "video");
  AddTracks(*mSessionAns, "video");

  OfferAnswer();

  UniquePtr<JsepCodecDescription> codec;
  GetCodec(*mSessionAns, 0, sdp::kSend, 0, 0, &codec);
  ASSERT_TRUE(codec);
  ASSERT_EQ("H264", codec->mName);
  GetCodec(*mSessionAns, 0, sdp::kRecv, 0, 0, &codec);
  ASSERT_TRUE(codec);
  ASSERT_EQ("H264", codec->mName);
}

TEST_F(JsepSessionTest, LowDynamicPayloadType) {
  SetPayloadTypeNumber(*mSessionOff, "opus", "12");
  types.push_back(SdpMediaSection::kAudio);
  AddTracks(*mSessionOff, "audio");
  AddTracks(*mSessionAns, "audio");

  OfferAnswer();
  UniquePtr<JsepCodecDescription> codec;
  GetCodec(*mSessionAns, 0, sdp::kSend, 0, 0, &codec);
  ASSERT_TRUE(codec);
  ASSERT_EQ("opus", codec->mName);
  ASSERT_EQ("12", codec->mDefaultPt);
  GetCodec(*mSessionAns, 0, sdp::kRecv, 0, 0, &codec);
  ASSERT_TRUE(codec);
  ASSERT_EQ("opus", codec->mName);
  ASSERT_EQ("12", codec->mDefaultPt);
}

TEST_F(JsepSessionTest, TestOfferPTAsymmetry) {
  SetPayloadTypeNumber(*mSessionAns, "opus", "105");
  types.push_back(SdpMediaSection::kAudio);
  AddTracks(*mSessionOff, "audio");
  AddTracks(*mSessionAns, "audio");
  JsepOfferOptions options;

  // Ensure that mSessionAns is appropriately configured. Also ensure that
  // creating an offer with 105 does not prompt mSessionAns to ignore the
  // PT in the offer.
  std::string offer;
  JsepSession::Result result = mSessionAns->CreateOffer(options, &offer);
  ASSERT_FALSE(result.mError.isSome());
  ASSERT_NE(std::string::npos, offer.find("a=rtpmap:105 opus")) << offer;

  OfferAnswer();

  // Answerer should use what the offerer suggested
  UniquePtr<JsepCodecDescription> codec;
  GetCodec(*mSessionAns, 0, sdp::kSend, 0, 0, &codec);
  ASSERT_TRUE(codec);
  ASSERT_EQ("opus", codec->mName);
  ASSERT_EQ("109", codec->mDefaultPt);
  GetCodec(*mSessionAns, 0, sdp::kRecv, 0, 0, &codec);
  ASSERT_TRUE(codec);
  ASSERT_EQ("opus", codec->mName);
  ASSERT_EQ("109", codec->mDefaultPt);

  // Answerer should not change when it reoffers
  result = mSessionAns->CreateOffer(options, &offer);
  ASSERT_FALSE(result.mError.isSome());
  ASSERT_NE(std::string::npos, offer.find("a=rtpmap:109 opus")) << offer;
}

TEST_F(JsepSessionTest, TestAnswerPTAsymmetry) {
  // JsepSessionImpl will never answer with an asymmetric payload type
  // (tested in TestOfferPTAsymmetry), so we have to rewrite SDP a little.
  types.push_back(SdpMediaSection::kAudio);
  AddTracks(*mSessionOff, "audio");
  AddTracks(*mSessionAns, "audio");

  std::string offer = CreateOffer();
  SetLocalOffer(offer);

  Replace("a=rtpmap:109 opus", "a=rtpmap:105 opus", &offer);
  Replace("m=audio 9 UDP/TLS/RTP/SAVPF 109", "m=audio 9 UDP/TLS/RTP/SAVPF 105",
          &offer);
  ReplaceAll("a=fmtp:109", "a=fmtp:105", &offer);
  SetRemoteOffer(offer);

  std::string answer = CreateAnswer();
  SetLocalAnswer(answer);
  SetRemoteAnswer(answer);

  UniquePtr<JsepCodecDescription> codec;
  GetCodec(*mSessionOff, 0, sdp::kSend, 0, 0, &codec);
  ASSERT_TRUE(codec);
  ASSERT_EQ("opus", codec->mName);
  ASSERT_EQ("105", codec->mDefaultPt);
  GetCodec(*mSessionOff, 0, sdp::kRecv, 0, 0, &codec);
  ASSERT_TRUE(codec);
  ASSERT_EQ("opus", codec->mName);
  ASSERT_EQ("109", codec->mDefaultPt);

  GetCodec(*mSessionAns, 0, sdp::kSend, 0, 0, &codec);
  ASSERT_TRUE(codec);
  ASSERT_EQ("opus", codec->mName);
  ASSERT_EQ("105", codec->mDefaultPt);
  GetCodec(*mSessionAns, 0, sdp::kRecv, 0, 0, &codec);
  ASSERT_TRUE(codec);
  ASSERT_EQ("opus", codec->mName);
  ASSERT_EQ("105", codec->mDefaultPt);
}

TEST_F(JsepSessionTest, PayloadTypeClash) {
  // Set up a scenario where mSessionAns' favorite codec (opus) is unsupported
  // by mSessionOff, and mSessionOff uses that payload type for something else.
  SetCodecEnabled(*mSessionOff, "opus", false);
  SetPayloadTypeNumber(*mSessionOff, "opus", "0");
  SetPayloadTypeNumber(*mSessionOff, "G722", "109");
  SetPayloadTypeNumber(*mSessionAns, "opus", "109");
  types.push_back(SdpMediaSection::kAudio);
  AddTracks(*mSessionOff, "audio");
  AddTracks(*mSessionAns, "audio");

  OfferAnswer();
  UniquePtr<JsepCodecDescription> codec;
  GetCodec(*mSessionAns, 0, sdp::kSend, 0, 0, &codec);
  ASSERT_TRUE(codec);
  ASSERT_EQ("G722", codec->mName);
  ASSERT_EQ("109", codec->mDefaultPt);
  GetCodec(*mSessionAns, 0, sdp::kRecv, 0, 0, &codec);
  ASSERT_TRUE(codec);
  ASSERT_EQ("G722", codec->mName);
  ASSERT_EQ("109", codec->mDefaultPt);

  // Now, make sure that mSessionAns does not put a=rtpmap:109 opus in a
  // reoffer, since pt 109 is taken for PCMU (the answerer still supports opus,
  // and will reoffer it, but it should choose a new payload type for it)
  JsepOfferOptions options;
  std::string reoffer;
  JsepSession::Result result = mSessionAns->CreateOffer(options, &reoffer);
  ASSERT_FALSE(result.mError.isSome());
  ASSERT_EQ(std::string::npos, reoffer.find("a=rtpmap:109 opus")) << reoffer;
  ASSERT_NE(std::string::npos, reoffer.find(" opus")) << reoffer;
}

TEST_P(JsepSessionTest, TestGlareRollback) {
  AddTracks(*mSessionOff);
  AddTracks(*mSessionAns);
  JsepOfferOptions options;

  std::string offer;
  ASSERT_FALSE(mSessionAns->CreateOffer(options, &offer).mError.isSome());
  ASSERT_FALSE(
      mSessionAns->SetLocalDescription(kJsepSdpOffer, offer).mError.isSome());
  ASSERT_EQ(kJsepStateHaveLocalOffer, mSessionAns->GetState());

  ASSERT_FALSE(mSessionOff->CreateOffer(options, &offer).mError.isSome());
  ASSERT_FALSE(
      mSessionOff->SetLocalDescription(kJsepSdpOffer, offer).mError.isSome());
  ASSERT_EQ(kJsepStateHaveLocalOffer, mSessionOff->GetState());

  ASSERT_EQ(dom::PCError::InvalidStateError,
            *mSessionAns->SetRemoteDescription(kJsepSdpOffer, offer).mError);
  ASSERT_FALSE(
      mSessionAns->SetLocalDescription(kJsepSdpRollback, "").mError.isSome());
  ASSERT_EQ(kJsepStateStable, mSessionAns->GetState());

  SetRemoteOffer(offer);

  std::string answer = CreateAnswer();
  SetLocalAnswer(answer);
  SetRemoteAnswer(answer);
}

TEST_P(JsepSessionTest, TestRejectOfferRollback) {
  AddTracks(*mSessionOff);
  AddTracks(*mSessionAns);

  std::string offer = CreateOffer();
  SetLocalOffer(offer);
  SetRemoteOffer(offer);

  ASSERT_FALSE(
      mSessionAns->SetRemoteDescription(kJsepSdpRollback, "").mError.isSome());
  ASSERT_EQ(kJsepStateStable, mSessionAns->GetState());
  for (const auto& [id, transceiver] : mSessionAns->GetTransceivers()) {
    (void)id;  // Lame, but no better way to do this right now.
    ASSERT_EQ(0U, transceiver->mRecvTrack.GetStreamIds().size());
  }

  ASSERT_FALSE(
      mSessionOff->SetLocalDescription(kJsepSdpRollback, "").mError.isSome());
  ASSERT_EQ(kJsepStateStable, mSessionOff->GetState());

  OfferAnswer();
}

TEST_P(JsepSessionTest, TestInvalidRollback) {
  AddTracks(*mSessionOff);
  AddTracks(*mSessionAns);

  ASSERT_EQ(dom::PCError::InvalidStateError,
            *mSessionOff->SetLocalDescription(kJsepSdpRollback, "").mError);
  ASSERT_EQ(dom::PCError::InvalidStateError,
            *mSessionOff->SetRemoteDescription(kJsepSdpRollback, "").mError);

  std::string offer = CreateOffer();
  ASSERT_EQ(dom::PCError::InvalidStateError,
            *mSessionOff->SetLocalDescription(kJsepSdpRollback, "").mError);
  ASSERT_EQ(dom::PCError::InvalidStateError,
            *mSessionOff->SetRemoteDescription(kJsepSdpRollback, "").mError);

  SetLocalOffer(offer);
  ASSERT_EQ(dom::PCError::InvalidStateError,
            *mSessionOff->SetRemoteDescription(kJsepSdpRollback, "").mError);

  SetRemoteOffer(offer);
  ASSERT_EQ(dom::PCError::InvalidStateError,
            *mSessionAns->SetLocalDescription(kJsepSdpRollback, "").mError);

  std::string answer = CreateAnswer();
  ASSERT_EQ(dom::PCError::InvalidStateError,
            *mSessionAns->SetLocalDescription(kJsepSdpRollback, "").mError);

  SetLocalAnswer(answer);
  ASSERT_EQ(dom::PCError::InvalidStateError,
            *mSessionAns->SetLocalDescription(kJsepSdpRollback, "").mError);
  ASSERT_EQ(dom::PCError::InvalidStateError,
            *mSessionAns->SetRemoteDescription(kJsepSdpRollback, "").mError);

  SetRemoteAnswer(answer);
  ASSERT_EQ(dom::PCError::InvalidStateError,
            *mSessionOff->SetLocalDescription(kJsepSdpRollback, "").mError);
  ASSERT_EQ(dom::PCError::InvalidStateError,
            *mSessionOff->SetRemoteDescription(kJsepSdpRollback, "").mError);
}

size_t GetActiveTransportCount(const JsepSession& session) {
  size_t activeTransportCount = 0;
  for (const auto& [id, transceiver] : session.GetTransceivers()) {
    (void)id;  // Lame, but no better way to do this right now.
    if (!transceiver->HasBundleLevel() ||
        (transceiver->BundleLevel() == transceiver->GetLevel())) {
      activeTransportCount += transceiver->mTransport.mComponents;
    }
  }
  return activeTransportCount;
}

TEST_P(JsepSessionTest, TestBalancedBundle) {
  AddTracks(*mSessionOff);
  AddTracks(*mSessionAns);

  mSessionOff->SetBundlePolicy(kBundleBalanced);

  std::string offer = CreateOffer();
  UniquePtr<Sdp> parsedOffer = std::move(SipccSdpParser().Parse(offer)->Sdp());

  ASSERT_TRUE(parsedOffer.get());

  std::map<SdpMediaSection::MediaType, SdpMediaSection*> firstByType;

  for (size_t i = 0; i < parsedOffer->GetMediaSectionCount(); ++i) {
    SdpMediaSection& msection(parsedOffer->GetMediaSection(i));
    bool firstOfType = !firstByType.count(msection.GetMediaType());
    if (firstOfType) {
      firstByType[msection.GetMediaType()] = &msection;
    }
    ASSERT_EQ(!firstOfType, msection.GetAttributeList().HasAttribute(
                                SdpAttribute::kBundleOnlyAttribute));
  }

  SetLocalOffer(offer);
  SetRemoteOffer(offer);
  std::string answer = CreateAnswer();
  SetLocalAnswer(answer);
  SetRemoteAnswer(answer);

  CheckTransceiversAreBundled(*mSessionOff, "Offerer transceivers");
  CheckTransceiversAreBundled(*mSessionAns, "Answerer transceivers");
  EXPECT_EQ(1U, GetActiveTransportCount(*mSessionOff));
  EXPECT_EQ(1U, GetActiveTransportCount(*mSessionAns));
}

TEST_P(JsepSessionTest, TestMaxBundle) {
  AddTracks(*mSessionOff);
  AddTracks(*mSessionAns);

  mSessionOff->SetBundlePolicy(kBundleMaxBundle);
  OfferAnswer();

  std::string offer = mSessionOff->GetLocalDescription(kJsepDescriptionCurrent);
  UniquePtr<Sdp> parsedOffer = std::move(SipccSdpParser().Parse(offer)->Sdp());
  ASSERT_TRUE(parsedOffer.get());

  ASSERT_FALSE(parsedOffer->GetMediaSection(0).GetAttributeList().HasAttribute(
      SdpAttribute::kBundleOnlyAttribute));
  ASSERT_NE(0U, parsedOffer->GetMediaSection(0).GetPort());
  for (size_t i = 1; i < parsedOffer->GetMediaSectionCount(); ++i) {
    ASSERT_TRUE(parsedOffer->GetMediaSection(i).GetAttributeList().HasAttribute(
        SdpAttribute::kBundleOnlyAttribute));
    ASSERT_EQ(0U, parsedOffer->GetMediaSection(i).GetPort());
  }

  CheckTransceiversAreBundled(*mSessionOff, "Offerer transceivers");
  CheckTransceiversAreBundled(*mSessionAns, "Answerer transceivers");
  EXPECT_EQ(1U, GetActiveTransportCount(*mSessionOff));
  EXPECT_EQ(1U, GetActiveTransportCount(*mSessionAns));
}

TEST_F(JsepSessionTest, TestNonDefaultProtocol) {
  AddTracks(*mSessionOff, "audio,video,datachannel");
  AddTracks(*mSessionAns, "audio,video,datachannel");

  std::string offer;
  ASSERT_FALSE(
      mSessionOff->CreateOffer(JsepOfferOptions(), &offer).mError.isSome());
  offer.replace(offer.find("UDP/TLS/RTP/SAVPF"), strlen("UDP/TLS/RTP/SAVPF"),
                "RTP/SAVPF");
  offer.replace(offer.find("UDP/TLS/RTP/SAVPF"), strlen("UDP/TLS/RTP/SAVPF"),
                "RTP/SAVPF");
  mSessionOff->SetLocalDescription(kJsepSdpOffer, offer);
  mSessionAns->SetRemoteDescription(kJsepSdpOffer, offer);

  std::string answer;
  mSessionAns->CreateAnswer(JsepAnswerOptions(), &answer);
  UniquePtr<Sdp> parsedAnswer = Parse(answer);
  ASSERT_EQ(3U, parsedAnswer->GetMediaSectionCount());
  ASSERT_EQ(SdpMediaSection::kRtpSavpf,
            parsedAnswer->GetMediaSection(0).GetProtocol());
  ASSERT_EQ(SdpMediaSection::kRtpSavpf,
            parsedAnswer->GetMediaSection(1).GetProtocol());

  mSessionAns->SetLocalDescription(kJsepSdpAnswer, answer);
  mSessionOff->SetRemoteDescription(kJsepSdpAnswer, answer);

  // Make sure reoffer uses the same protocol as before
  mSessionOff->CreateOffer(JsepOfferOptions(), &offer);
  UniquePtr<Sdp> parsedOffer = Parse(offer);
  ASSERT_EQ(3U, parsedOffer->GetMediaSectionCount());
  ASSERT_EQ(SdpMediaSection::kRtpSavpf,
            parsedOffer->GetMediaSection(0).GetProtocol());
  ASSERT_EQ(SdpMediaSection::kRtpSavpf,
            parsedOffer->GetMediaSection(1).GetProtocol());

  // Make sure reoffer from other side uses the same protocol as before
  mSessionAns->CreateOffer(JsepOfferOptions(), &offer);
  parsedOffer = Parse(offer);
  ASSERT_EQ(3U, parsedOffer->GetMediaSectionCount());
  ASSERT_EQ(SdpMediaSection::kRtpSavpf,
            parsedOffer->GetMediaSection(0).GetProtocol());
  ASSERT_EQ(SdpMediaSection::kRtpSavpf,
            parsedOffer->GetMediaSection(1).GetProtocol());
}

TEST_F(JsepSessionTest, CreateOfferNoVideoStreamRecvVideo) {
  types.push_back(SdpMediaSection::kAudio);
  AddTracks(*mSessionOff, "audio");

  JsepOfferOptions options;
  options.mOfferToReceiveAudio = Some(static_cast<size_t>(1U));
  options.mOfferToReceiveVideo = Some(static_cast<size_t>(1U));

  CreateOffer(Some(options));
}

TEST_F(JsepSessionTest, CreateOfferNoAudioStreamRecvAudio) {
  types.push_back(SdpMediaSection::kVideo);
  AddTracks(*mSessionOff, "video");

  JsepOfferOptions options;
  options.mOfferToReceiveAudio = Some(static_cast<size_t>(1U));
  options.mOfferToReceiveVideo = Some(static_cast<size_t>(1U));

  CreateOffer(Some(options));
}

TEST_F(JsepSessionTest, CreateOfferNoVideoStream) {
  types.push_back(SdpMediaSection::kAudio);
  AddTracks(*mSessionOff, "audio");

  JsepOfferOptions options;
  options.mOfferToReceiveAudio = Some(static_cast<size_t>(1U));
  options.mOfferToReceiveVideo = Some(static_cast<size_t>(0U));

  CreateOffer(Some(options));
}

TEST_F(JsepSessionTest, CreateOfferNoAudioStream) {
  types.push_back(SdpMediaSection::kVideo);
  AddTracks(*mSessionOff, "video");

  JsepOfferOptions options;
  options.mOfferToReceiveAudio = Some(static_cast<size_t>(0U));
  options.mOfferToReceiveVideo = Some(static_cast<size_t>(1U));

  CreateOffer(Some(options));
}

TEST_F(JsepSessionTest, CreateOfferDontReceiveAudio) {
  types.push_back(SdpMediaSection::kAudio);
  types.push_back(SdpMediaSection::kVideo);
  AddTracks(*mSessionOff, "audio,video");

  JsepOfferOptions options;
  options.mOfferToReceiveAudio = Some(static_cast<size_t>(0U));
  options.mOfferToReceiveVideo = Some(static_cast<size_t>(1U));

  CreateOffer(Some(options));
}

TEST_F(JsepSessionTest, CreateOfferDontReceiveVideo) {
  types.push_back(SdpMediaSection::kAudio);
  types.push_back(SdpMediaSection::kVideo);
  AddTracks(*mSessionOff, "audio,video");

  JsepOfferOptions options;
  options.mOfferToReceiveAudio = Some(static_cast<size_t>(1U));
  options.mOfferToReceiveVideo = Some(static_cast<size_t>(0U));

  CreateOffer(Some(options));
}

TEST_F(JsepSessionTest, CreateOfferRemoveAudioTrack) {
  types.push_back(SdpMediaSection::kAudio);
  types.push_back(SdpMediaSection::kVideo);
  AddTracks(*mSessionOff, "audio,video");

  SetDirection(*mSessionOff, 1, SdpDirectionAttribute::kSendonly);
  JsepTrack removedTrack = RemoveTrack(*mSessionOff, 0);
  ASSERT_FALSE(IsNull(removedTrack));

  CreateOffer();
}

TEST_F(JsepSessionTest, CreateOfferDontReceiveAudioRemoveAudioTrack) {
  types.push_back(SdpMediaSection::kAudio);
  types.push_back(SdpMediaSection::kVideo);
  AddTracks(*mSessionOff, "audio,video");

  SetDirection(*mSessionOff, 0, SdpDirectionAttribute::kSendonly);
  JsepTrack removedTrack = RemoveTrack(*mSessionOff, 0);
  ASSERT_FALSE(IsNull(removedTrack));

  CreateOffer();
}

TEST_F(JsepSessionTest, CreateOfferDontReceiveVideoRemoveVideoTrack) {
  types.push_back(SdpMediaSection::kAudio);
  types.push_back(SdpMediaSection::kVideo);
  AddTracks(*mSessionOff, "audio,video");

  JsepOfferOptions options;
  options.mOfferToReceiveAudio = Some(static_cast<size_t>(1U));
  options.mOfferToReceiveVideo = Some(static_cast<size_t>(0U));

  JsepTrack removedTrack = RemoveTrack(*mSessionOff, 0);
  ASSERT_FALSE(IsNull(removedTrack));

  CreateOffer(Some(options));
}

static const std::string strSampleCandidate =
    "a=candidate:1 1 UDP 2130706431 192.168.2.1 50005 typ host\r\n";

static const unsigned short nSamplelevel = 2;

TEST_F(JsepSessionTest, CreateOfferAddCandidate) {
  types.push_back(SdpMediaSection::kAudio);
  AddTracks(*mSessionOff, "audio");
  std::string offer = CreateOffer();
  SetLocalOffer(offer);

  uint16_t level;
  std::string mid;
  bool skipped;
  nsresult rv;
  rv = mSessionOff->AddLocalIceCandidate(strSampleCandidate,
                                         GetTransportId(*mSessionOff, 0), "",
                                         &level, &mid, &skipped);
  ASSERT_EQ(NS_OK, rv);
}

TEST_F(JsepSessionTest, AddIceCandidateEarly) {
  uint16_t level;
  std::string mid;
  bool skipped;
  nsresult rv;
  rv = mSessionOff->AddLocalIceCandidate(strSampleCandidate,
                                         GetTransportId(*mSessionOff, 0), "",
                                         &level, &mid, &skipped);

  // This can't succeed without a local description
  ASSERT_NE(NS_OK, rv);
}

TEST_F(JsepSessionTest, OfferAnswerDontAddAudioStreamOnAnswerNoOptions) {
  types.push_back(SdpMediaSection::kAudio);
  types.push_back(SdpMediaSection::kVideo);
  AddTracks(*mSessionOff, "audio,video");
  AddTracks(*mSessionAns, "video");

  JsepOfferOptions options;
  options.mOfferToReceiveAudio = Some(static_cast<size_t>(1U));
  options.mOfferToReceiveVideo = Some(static_cast<size_t>(1U));

  CreateOffer(Some(options));
  std::string offer = CreateOffer(Some(options));
  SetLocalOffer(offer);
  SetRemoteOffer(offer);
  std::string answer = CreateAnswer();
  SetLocalAnswer(answer, CHECK_SUCCESS);
  SetRemoteAnswer(answer, CHECK_SUCCESS);
}

TEST_F(JsepSessionTest, OfferAnswerDontAddVideoStreamOnAnswerNoOptions) {
  types.push_back(SdpMediaSection::kAudio);
  types.push_back(SdpMediaSection::kVideo);
  AddTracks(*mSessionOff, "audio,video");
  AddTracks(*mSessionAns, "audio");

  JsepOfferOptions options;
  options.mOfferToReceiveAudio = Some(static_cast<size_t>(1U));
  options.mOfferToReceiveVideo = Some(static_cast<size_t>(1U));

  CreateOffer(Some(options));
  std::string offer = CreateOffer(Some(options));
  SetLocalOffer(offer);
  SetRemoteOffer(offer);
  std::string answer = CreateAnswer();
  SetLocalAnswer(answer, CHECK_SUCCESS);
  SetRemoteAnswer(answer, CHECK_SUCCESS);
}

TEST_F(JsepSessionTest, OfferAnswerDontAddAudioVideoStreamsOnAnswerNoOptions) {
  types.push_back(SdpMediaSection::kAudio);
  types.push_back(SdpMediaSection::kVideo);
  AddTracks(*mSessionOff, "audio,video");
  AddTracks(*mSessionAns);

  JsepOfferOptions options;
  options.mOfferToReceiveAudio = Some(static_cast<size_t>(1U));
  options.mOfferToReceiveVideo = Some(static_cast<size_t>(1U));

  OfferAnswer();
}

TEST_F(JsepSessionTest, OfferAndAnswerWithExtraCodec) {
  types.push_back(SdpMediaSection::kAudio);
  AddTracks(*mSessionOff, "audio");
  AddTracks(*mSessionAns, "audio");
  std::string offer = CreateOffer();
  SetLocalOffer(offer);
  SetRemoteOffer(offer);
  std::string answer = CreateAnswer();

  UniquePtr<Sdp> munge = Parse(answer);
  SdpMediaSection& mediaSection = munge->GetMediaSection(0);
  mediaSection.AddCodec("8", "PCMA", 8000, 1);
  std::string sdpString = munge->ToString();

  SetLocalAnswer(sdpString);
  SetRemoteAnswer(answer);
}

TEST_F(JsepSessionTest, AddCandidateInHaveLocalOffer) {
  types.push_back(SdpMediaSection::kAudio);
  AddTracks(*mSessionOff, "audio");
  std::string offer = CreateOffer();
  SetLocalOffer(offer);

  std::string mid;
  std::string transportId;
  JsepSession::Result result = mSessionOff->AddRemoteIceCandidate(
      strSampleCandidate, mid, Some(nSamplelevel), "", &transportId);
  ASSERT_EQ(dom::PCError::InvalidStateError, *result.mError);
}

TEST_F(JsepSessionTest, SetLocalWithoutCreateOffer) {
  types.push_back(SdpMediaSection::kAudio);
  AddTracks(*mSessionOff, "audio");
  AddTracks(*mSessionAns, "audio");

  std::string offer = CreateOffer();
  JsepSession::Result result =
      mSessionAns->SetLocalDescription(kJsepSdpOffer, offer);
  ASSERT_EQ(dom::PCError::InvalidModificationError, *result.mError);
}

TEST_F(JsepSessionTest, SetLocalWithoutCreateAnswer) {
  types.push_back(SdpMediaSection::kAudio);
  AddTracks(*mSessionOff, "audio");
  AddTracks(*mSessionAns, "audio");

  std::string offer = CreateOffer();
  SetRemoteOffer(offer);
  JsepSession::Result result =
      mSessionAns->SetLocalDescription(kJsepSdpAnswer, offer);
  ASSERT_EQ(dom::PCError::InvalidModificationError, *result.mError);
}

// Test for Bug 843595
TEST_F(JsepSessionTest, missingUfrag) {
  types.push_back(SdpMediaSection::kAudio);
  AddTracks(*mSessionOff, "audio");
  AddTracks(*mSessionAns, "audio");
  std::string offer = CreateOffer();
  std::string ufrag = "ice-ufrag";
  std::size_t pos = offer.find(ufrag);
  ASSERT_NE(pos, std::string::npos);
  offer.replace(pos, ufrag.length(), "ice-ufrog");
  JsepSession::Result result =
      mSessionAns->SetRemoteDescription(kJsepSdpOffer, offer);
  ASSERT_EQ(dom::PCError::OperationError, *result.mError);
}

TEST_F(JsepSessionTest, AudioOnlyCalleeNoRtcpMux) {
  types.push_back(SdpMediaSection::kAudio);
  AddTracks(*mSessionOff, "audio");
  AddTracks(*mSessionAns, "audio");
  std::string offer = CreateOffer();
  std::string rtcp_mux = "a=rtcp-mux\r\n";
  std::size_t pos = offer.find(rtcp_mux);
  ASSERT_NE(pos, std::string::npos);
  offer.replace(pos, rtcp_mux.length(), "");
  SetLocalOffer(offer);
  SetRemoteOffer(offer);
  std::string answer = CreateAnswer();
  pos = answer.find(rtcp_mux);
  ASSERT_EQ(pos, std::string::npos);
}

// This test comes from Bug 810220
TEST_F(JsepSessionTest, AudioOnlyG711Call) {
  std::string offer =
      "v=0\r\n"
      "o=- 1 1 IN IP4 148.147.200.251\r\n"
      "s=-\r\n"
      "b=AS:64\r\n"
      "t=0 0\r\n"
      "a=fingerprint:sha-256 F3:FA:20:C0:CD:48:C4:5F:02:5F:A5:D3:21:D0:2D:48:"
      "7B:31:60:5C:5A:D8:0D:CD:78:78:6C:6D:CE:CC:0C:67\r\n"
      "m=audio 9000 UDP/TLS/RTP/SAVPF 0 8 126\r\n"
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

  types.push_back(SdpMediaSection::kAudio);
  AddTracks(*mSessionAns, "audio");
  SetRemoteOffer(offer, CHECK_SUCCESS);
  std::string answer = CreateAnswer();

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

TEST_F(JsepSessionTest, AudioOnlyG722Only) {
  types.push_back(SdpMediaSection::kAudio);
  AddTracks(*mSessionOff, "audio");
  AddTracks(*mSessionAns, "audio");
  std::string offer = CreateOffer();
  SetLocalOffer(offer);

  std::string audio = "m=audio 9 UDP/TLS/RTP/SAVPF 109 9 0 8 101\r\n";
  std::size_t pos = offer.find(audio);
  ASSERT_NE(pos, std::string::npos);
  offer.replace(pos, audio.length(), "m=audio 65375 UDP/TLS/RTP/SAVPF 9\r\n");
  SetRemoteOffer(offer);

  std::string answer = CreateAnswer();
  SetLocalAnswer(answer);
  ASSERT_NE(mSessionAns->GetLocalDescription(kJsepDescriptionCurrent)
                .find("UDP/TLS/RTP/SAVPF 9\r"),
            std::string::npos);
  ASSERT_NE(mSessionAns->GetLocalDescription(kJsepDescriptionCurrent)
                .find("a=rtpmap:9 G722/8000"),
            std::string::npos);
}

TEST_F(JsepSessionTest, AudioOnlyG722Rejected) {
  types.push_back(SdpMediaSection::kAudio);
  AddTracks(*mSessionOff, "audio");
  AddTracks(*mSessionAns, "audio");
  std::string offer = CreateOffer();
  SetLocalOffer(offer);

  std::string audio = "m=audio 9 UDP/TLS/RTP/SAVPF 109 9 0 8 101\r\n";
  std::size_t pos = offer.find(audio);
  ASSERT_NE(pos, std::string::npos);
  offer.replace(pos, audio.length(), "m=audio 65375 UDP/TLS/RTP/SAVPF 0 8\r\n");
  SetRemoteOffer(offer);

  std::string answer = CreateAnswer();
  SetLocalAnswer(answer);
  SetRemoteAnswer(answer);

  ASSERT_NE(mSessionAns->GetLocalDescription(kJsepDescriptionCurrent)
                .find("UDP/TLS/RTP/SAVPF 0 8\r"),
            std::string::npos);
  ASSERT_NE(mSessionAns->GetLocalDescription(kJsepDescriptionCurrent)
                .find("a=rtpmap:0 PCMU/8000"),
            std::string::npos);
  ASSERT_EQ(mSessionAns->GetLocalDescription(kJsepDescriptionCurrent)
                .find("a=rtpmap:109 opus/48000/2"),
            std::string::npos);
  ASSERT_EQ(mSessionAns->GetLocalDescription(kJsepDescriptionCurrent)
                .find("a=rtpmap:9 G722/8000"),
            std::string::npos);
}

// This test doesn't make sense for bundle
TEST_F(JsepSessionTest, DISABLED_FullCallAudioNoMuxVideoMux) {
  types.push_back(SdpMediaSection::kAudio);
  AddTracks(*mSessionOff, "audio,video");
  AddTracks(*mSessionAns, "audio,video");
  std::string offer = CreateOffer();
  SetLocalOffer(offer);
  std::string rtcp_mux = "a=rtcp-mux\r\n";
  std::size_t pos = offer.find(rtcp_mux);
  ASSERT_NE(pos, std::string::npos);
  offer.replace(pos, rtcp_mux.length(), "");
  SetRemoteOffer(offer);
  std::string answer = CreateAnswer();

  size_t match = mSessionAns->GetLocalDescription(kJsepDescriptionCurrent)
                     .find("\r\na=rtcp-mux");
  ASSERT_NE(match, std::string::npos);
  match = mSessionAns->GetLocalDescription(kJsepDescriptionCurrent)
              .find("\r\na=rtcp-mux", match + 1);
  ASSERT_EQ(match, std::string::npos);
}

// Disabled pending resolution of bug 818640.
// Actually, this test is completely broken; you can't just call
// SetRemote/CreateAnswer over and over again.
TEST_F(JsepSessionTest, DISABLED_OfferAllDynamicTypes) {
  types.push_back(SdpMediaSection::kAudio);
  AddTracks(*mSessionAns, "audio");

  std::string offer;
  for (int i = 96; i < 128; i++) {
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
        "m=audio 9000 RTP/AVP " +
        ss.str() +
        "\r\n"
        "c=IN IP4 148.147.200.251\r\n"
        "b=TIAS:64000\r\n"
        "a=rtpmap:" +
        ss.str() +
        " opus/48000/2\r\n"
        "a=candidate:0 1 udp 2130706432 148.147.200.251 9000 typ host\r\n"
        "a=candidate:0 2 udp 2130706432 148.147.200.251 9005 typ host\r\n"
        "a=ice-ufrag:cYuakxkEKH+RApYE\r\n"
        "a=ice-pwd:bwtpzLZD+3jbu8vQHvEa6Xuq\r\n"
        "a=sendrecv\r\n";

    SetRemoteOffer(offer, CHECK_SUCCESS);
    std::string answer = CreateAnswer();
    ASSERT_NE(answer.find(ss.str() + " opus/"), std::string::npos);
  }
}

TEST_F(JsepSessionTest, ipAddrAnyOffer) {
  std::string offer =
      "v=0\r\n"
      "o=- 1 1 IN IP4 127.0.0.1\r\n"
      "s=-\r\n"
      "b=AS:64\r\n"
      "t=0 0\r\n"
      "a=fingerprint:sha-256 F3:FA:20:C0:CD:48:C4:5F:02:5F:A5:D3:21:D0:2D:48:"
      "7B:31:60:5C:5A:D8:0D:CD:78:78:6C:6D:CE:CC:0C:67\r\n"
      "m=audio 9000 UDP/TLS/RTP/SAVPF 99\r\n"
      "c=IN IP4 0.0.0.0\r\n"
      "a=rtpmap:99 opus/48000/2\r\n"
      "a=ice-ufrag:cYuakxkEKH+RApYE\r\n"
      "a=ice-pwd:bwtpzLZD+3jbu8vQHvEa6Xuq\r\n"
      "a=setup:active\r\n"
      "a=sendrecv\r\n";

  types.push_back(SdpMediaSection::kAudio);
  AddTracks(*mSessionAns, "audio");
  SetRemoteOffer(offer, CHECK_SUCCESS);
  std::string answer = CreateAnswer();

  ASSERT_NE(answer.find("a=sendrecv"), std::string::npos);
}

static void CreateSDPForBigOTests(std::string& offer,
                                  const std::string& number) {
  offer =
      "v=0\r\n"
      "o=- ";
  offer += number;
  offer += " ";
  offer += number;
  offer +=
      " IN IP4 127.0.0.1\r\n"
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

TEST_F(JsepSessionTest, BigOValues) {
  std::string offer;

  CreateSDPForBigOTests(offer, "12345678901234567");

  types.push_back(SdpMediaSection::kAudio);
  AddTracks(*mSessionAns, "audio");
  SetRemoteOffer(offer, CHECK_SUCCESS);
}

TEST_F(JsepSessionTest, BigOValuesExtraChars) {
  std::string offer;

  CreateSDPForBigOTests(offer, "12345678901234567FOOBAR");

  types.push_back(SdpMediaSection::kAudio);
  AddTracks(*mSessionAns, "audio");
  // The signaling state will remain "stable" because the unparsable
  // SDP leads to a failure in SetRemoteDescription.
  SetRemoteOffer(offer, NO_CHECKS);
  ASSERT_EQ(kJsepStateStable, mSessionAns->GetState());
}

TEST_F(JsepSessionTest, BigOValuesTooBig) {
  std::string offer;

  CreateSDPForBigOTests(offer, "18446744073709551615");
  types.push_back(SdpMediaSection::kAudio);
  AddTracks(*mSessionAns, "audio");

  // The signaling state will remain "stable" because the unparsable
  // SDP leads to a failure in SetRemoteDescription.
  SetRemoteOffer(offer, NO_CHECKS);
  ASSERT_EQ(kJsepStateStable, mSessionAns->GetState());
}

TEST_F(JsepSessionTest, SetLocalAnswerInStable) {
  types.push_back(SdpMediaSection::kAudio);
  AddTracks(*mSessionOff, "audio");
  std::string offer = CreateOffer();

  // The signaling state will remain "stable" because the
  // SetLocalDescription call fails.
  SetLocalAnswer(offer, NO_CHECKS);
  ASSERT_EQ(kJsepStateStable, mSessionOff->GetState());
}

TEST_F(JsepSessionTest, SetRemoteAnswerInStable) {
  const std::string answer =
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

  // The signaling state will remain "stable" because the
  // SetRemoteDescription call fails.
  JsepSession::Result result =
      mSessionOff->SetRemoteDescription(kJsepSdpAnswer, answer);
  ASSERT_EQ(dom::PCError::InvalidStateError, *result.mError);
  ASSERT_EQ(kJsepStateStable, mSessionOff->GetState());
}

TEST_F(JsepSessionTest, SetLocalAnswerInHaveLocalOffer) {
  types.push_back(SdpMediaSection::kAudio);
  AddTracks(*mSessionOff, "audio");
  std::string offer = CreateOffer();

  SetLocalOffer(offer);
  ASSERT_EQ(kJsepStateHaveLocalOffer, mSessionOff->GetState());

  // The signaling state will remain "have-local-offer" because the
  // SetLocalDescription call fails.
  JsepSession::Result result =
      mSessionOff->SetLocalDescription(kJsepSdpAnswer, offer);
  ASSERT_EQ(dom::PCError::InvalidModificationError, *result.mError);
  ASSERT_EQ(kJsepStateHaveLocalOffer, mSessionOff->GetState());
}

TEST_F(JsepSessionTest, SetRemoteOfferInHaveLocalOffer) {
  types.push_back(SdpMediaSection::kAudio);
  AddTracks(*mSessionOff, "audio");
  std::string offer = CreateOffer();

  SetLocalOffer(offer);
  ASSERT_EQ(kJsepStateHaveLocalOffer, mSessionOff->GetState());

  // The signaling state will remain "have-local-offer" because the
  // SetRemoteDescription call fails.
  JsepSession::Result result =
      mSessionOff->SetRemoteDescription(kJsepSdpOffer, offer);
  ASSERT_EQ(dom::PCError::InvalidStateError, *result.mError);
  ASSERT_EQ(kJsepStateHaveLocalOffer, mSessionOff->GetState());
}

TEST_F(JsepSessionTest, SetLocalOfferInHaveRemoteOffer) {
  types.push_back(SdpMediaSection::kAudio);
  AddTracks(*mSessionOff, "audio");
  std::string offer = CreateOffer();

  SetRemoteOffer(offer);
  ASSERT_EQ(kJsepStateHaveRemoteOffer, mSessionAns->GetState());

  // The signaling state will remain "have-remote-offer" because the
  // SetLocalDescription call fails.
  JsepSession::Result result =
      mSessionAns->SetLocalDescription(kJsepSdpOffer, offer);
  ASSERT_EQ(dom::PCError::InvalidModificationError, *result.mError);
  ASSERT_EQ(kJsepStateHaveRemoteOffer, mSessionAns->GetState());
}

TEST_F(JsepSessionTest, SetRemoteAnswerInHaveRemoteOffer) {
  types.push_back(SdpMediaSection::kAudio);
  AddTracks(*mSessionOff, "audio");
  std::string offer = CreateOffer();

  SetRemoteOffer(offer);
  ASSERT_EQ(kJsepStateHaveRemoteOffer, mSessionAns->GetState());

  // The signaling state will remain "have-remote-offer" because the
  // SetRemoteDescription call fails.
  JsepSession::Result result =
      mSessionAns->SetRemoteDescription(kJsepSdpAnswer, offer);
  ASSERT_EQ(dom::PCError::InvalidStateError, *result.mError);

  ASSERT_EQ(kJsepStateHaveRemoteOffer, mSessionAns->GetState());
}

TEST_F(JsepSessionTest, RtcpFbInOffer) {
  types.push_back(SdpMediaSection::kAudio);
  types.push_back(SdpMediaSection::kVideo);
  AddTracks(*mSessionOff, "audio,video");
  std::string offer = CreateOffer();

  std::map<std::string, bool> expected;
  expected["nack"] = false;
  expected["nack pli"] = false;
  expected["ccm fir"] = false;

  size_t prev = 0;
  size_t found = 0;
  for (;;) {
    found = offer.find('\n', found + 1);
    if (found == std::string::npos) break;

    std::string line = offer.substr(prev, (found - prev));

    // ensure no other rtcp-fb values are present
    if (line.find("a=rtcp-fb:") != std::string::npos) {
      size_t space = line.find(' ');
      // strip trailing \r\n
      std::string value = line.substr(space + 1, line.length() - space - 2);
      std::map<std::string, bool>::iterator entry = expected.find(value);
      ASSERT_NE(entry, expected.end());
      entry->second = true;
    }

    prev = found + 1;
  }

  // ensure all values are present
  for (std::map<std::string, bool>::iterator it = expected.begin();
       it != expected.end(); ++it) {
    ASSERT_EQ(it->second, true);
  }
}

// In this test we will change the offer SDP's a=setup value
// from actpass to passive. This will force the answer to do active.
TEST_F(JsepSessionTest, AudioCallForceDtlsRoles) {
  types.push_back(SdpMediaSection::kAudio);
  AddTracks(*mSessionOff, "audio");
  AddTracks(*mSessionAns, "audio");
  std::string offer = CreateOffer();

  std::string actpass = "\r\na=setup:actpass";
  size_t match = offer.find(actpass);
  ASSERT_NE(match, std::string::npos);
  offer.replace(match, actpass.length(), "\r\na=setup:passive");

  SetLocalOffer(offer);
  SetRemoteOffer(offer);
  ASSERT_EQ(kJsepStateHaveRemoteOffer, mSessionAns->GetState());
  std::string answer = CreateAnswer();
  match = answer.find("\r\na=setup:active");
  ASSERT_NE(match, std::string::npos);

  SetLocalAnswer(answer);
  SetRemoteAnswer(answer);
  ASSERT_EQ(kJsepStateStable, mSessionAns->GetState());
}

// In this test we will change the offer SDP's a=setup value
// from actpass to active. This will force the answer to do passive.
TEST_F(JsepSessionTest, AudioCallReverseDtlsRoles) {
  types.push_back(SdpMediaSection::kAudio);
  AddTracks(*mSessionOff, "audio");
  AddTracks(*mSessionAns, "audio");
  std::string offer = CreateOffer();

  std::string actpass = "\r\na=setup:actpass";
  size_t match = offer.find(actpass);
  ASSERT_NE(match, std::string::npos);
  offer.replace(match, actpass.length(), "\r\na=setup:active");

  SetLocalOffer(offer);
  SetRemoteOffer(offer);
  ASSERT_EQ(kJsepStateHaveRemoteOffer, mSessionAns->GetState());
  std::string answer = CreateAnswer();
  match = answer.find("\r\na=setup:passive");
  ASSERT_NE(match, std::string::npos);

  SetLocalAnswer(answer);
  SetRemoteAnswer(answer);
  ASSERT_EQ(kJsepStateStable, mSessionAns->GetState());
}

// In this test we will change the answer SDP's a=setup value
// from active to passive.  This will make both sides do
// active and should not connect.
TEST_F(JsepSessionTest, AudioCallMismatchDtlsRoles) {
  types.push_back(SdpMediaSection::kAudio);
  AddTracks(*mSessionOff, "audio");
  AddTracks(*mSessionAns, "audio");
  std::string offer = CreateOffer();

  std::string actpass = "\r\na=setup:actpass";
  size_t match = offer.find(actpass);
  ASSERT_NE(match, std::string::npos);
  SetLocalOffer(offer);
  SetRemoteOffer(offer);
  ASSERT_EQ(kJsepStateHaveRemoteOffer, mSessionAns->GetState());
  std::string answer = CreateAnswer();
  SetLocalAnswer(answer);

  std::string active = "\r\na=setup:active";
  match = answer.find(active);
  ASSERT_NE(match, std::string::npos);
  answer.replace(match, active.length(), "\r\na=setup:passive");
  SetRemoteAnswer(answer);

  // This is as good as it gets in a JSEP test (w/o starting DTLS)
  ASSERT_EQ(JsepDtlsTransport::kJsepDtlsClient,
            mSessionOff->GetTransceivers()[0]->mTransport.mDtls->GetRole());
  ASSERT_EQ(JsepDtlsTransport::kJsepDtlsClient,
            mSessionAns->GetTransceivers()[0]->mTransport.mDtls->GetRole());
}

// Verify that missing a=setup in offer gets rejected
TEST_F(JsepSessionTest, AudioCallOffererNoSetup) {
  types.push_back(SdpMediaSection::kAudio);
  AddTracks(*mSessionOff, "audio");
  AddTracks(*mSessionAns, "audio");
  std::string offer = CreateOffer();
  SetLocalOffer(offer);

  std::string actpass = "\r\na=setup:actpass";
  size_t match = offer.find(actpass);
  ASSERT_NE(match, std::string::npos);
  offer.replace(match, actpass.length(), "");

  // The signaling state will remain "stable" because the unparsable
  // SDP leads to a failure in SetRemoteDescription.
  SetRemoteOffer(offer, NO_CHECKS);
  ASSERT_EQ(kJsepStateStable, mSessionAns->GetState());
  ASSERT_EQ(kJsepStateHaveLocalOffer, mSessionOff->GetState());
}

// In this test we will change the answer SDP to remove the
// a=setup line, which results in active being assumed.
TEST_F(JsepSessionTest, AudioCallAnswerNoSetup) {
  types.push_back(SdpMediaSection::kAudio);
  AddTracks(*mSessionOff, "audio");
  AddTracks(*mSessionAns, "audio");
  std::string offer = CreateOffer();
  size_t match = offer.find("\r\na=setup:actpass");
  ASSERT_NE(match, std::string::npos);

  SetLocalOffer(offer);
  SetRemoteOffer(offer);
  ASSERT_EQ(kJsepStateHaveRemoteOffer, mSessionAns->GetState());
  std::string answer = CreateAnswer();
  SetLocalAnswer(answer);

  std::string active = "\r\na=setup:active";
  match = answer.find(active);
  ASSERT_NE(match, std::string::npos);
  answer.replace(match, active.length(), "");
  SetRemoteAnswer(answer);
  ASSERT_EQ(kJsepStateStable, mSessionAns->GetState());

  // This is as good as it gets in a JSEP test (w/o starting DTLS)
  ASSERT_EQ(JsepDtlsTransport::kJsepDtlsServer,
            mSessionOff->GetTransceivers()[0]->mTransport.mDtls->GetRole());
  ASSERT_EQ(JsepDtlsTransport::kJsepDtlsClient,
            mSessionAns->GetTransceivers()[0]->mTransport.mDtls->GetRole());
}

// Verify that 'holdconn' gets rejected
TEST_F(JsepSessionTest, AudioCallDtlsRoleHoldconn) {
  types.push_back(SdpMediaSection::kAudio);
  AddTracks(*mSessionOff, "audio");
  AddTracks(*mSessionAns, "audio");
  std::string offer = CreateOffer();
  SetLocalOffer(offer);

  std::string actpass = "\r\na=setup:actpass";
  size_t match = offer.find(actpass);
  ASSERT_NE(match, std::string::npos);
  offer.replace(match, actpass.length(), "\r\na=setup:holdconn");

  // The signaling state will remain "stable" because the unparsable
  // SDP leads to a failure in SetRemoteDescription.
  SetRemoteOffer(offer, NO_CHECKS);
  ASSERT_EQ(kJsepStateStable, mSessionAns->GetState());
  ASSERT_EQ(kJsepStateHaveLocalOffer, mSessionOff->GetState());
}

// Verify that 'actpass' in answer gets rejected
TEST_F(JsepSessionTest, AudioCallAnswererUsesActpass) {
  types.push_back(SdpMediaSection::kAudio);
  AddTracks(*mSessionOff, "audio");
  AddTracks(*mSessionAns, "audio");
  std::string offer = CreateOffer();
  SetLocalOffer(offer);
  SetRemoteOffer(offer);
  std::string answer = CreateAnswer();
  SetLocalAnswer(answer);

  std::string active = "\r\na=setup:active";
  size_t match = answer.find(active);
  ASSERT_NE(match, std::string::npos);
  answer.replace(match, active.length(), "\r\na=setup:actpass");

  // The signaling state will remain "stable" because the unparsable
  // SDP leads to a failure in SetRemoteDescription.
  SetRemoteAnswer(answer, NO_CHECKS);
  ASSERT_EQ(kJsepStateStable, mSessionAns->GetState());
  ASSERT_EQ(kJsepStateHaveLocalOffer, mSessionOff->GetState());
}

// Verify that 'actpass' in reoffer from previous answerer doesn't result
// in a role switch.
TEST_F(JsepSessionTest, AudioCallPreviousAnswererUsesActpassInReoffer) {
  types.push_back(SdpMediaSection::kAudio);
  AddTracks(*mSessionOff, "audio");
  AddTracks(*mSessionAns, "audio");

  OfferAnswer();

  ValidateSetupAttribute(*mSessionOff, SdpSetupAttribute::kActpass);
  ValidateSetupAttribute(*mSessionAns, SdpSetupAttribute::kActive);

  SwapOfferAnswerRoles();

  OfferAnswer();

  ValidateSetupAttribute(*mSessionOff, SdpSetupAttribute::kActpass);
  ValidateSetupAttribute(*mSessionAns, SdpSetupAttribute::kPassive);
}

// Disabled: See Bug 1329028
TEST_F(JsepSessionTest, DISABLED_AudioCallOffererAttemptsSetupRoleSwitch) {
  types.push_back(SdpMediaSection::kAudio);
  AddTracks(*mSessionOff, "audio");
  AddTracks(*mSessionAns, "audio");

  OfferAnswer();

  ValidateSetupAttribute(*mSessionOff, SdpSetupAttribute::kActpass);
  ValidateSetupAttribute(*mSessionAns, SdpSetupAttribute::kActive);

  std::string reoffer = CreateOffer();
  SetLocalOffer(reoffer);

  std::string actpass = "\r\na=setup:actpass";
  size_t match = reoffer.find(actpass);
  ASSERT_NE(match, std::string::npos);
  reoffer.replace(match, actpass.length(), "\r\na=setup:active");

  // This is expected to fail.
  SetRemoteOffer(reoffer, NO_CHECKS);
  ASSERT_EQ(kJsepStateHaveLocalOffer, mSessionOff->GetState());
  ASSERT_EQ(kJsepStateStable, mSessionAns->GetState());
}

// Disabled: See Bug 1329028
TEST_F(JsepSessionTest, DISABLED_AudioCallAnswererAttemptsSetupRoleSwitch) {
  types.push_back(SdpMediaSection::kAudio);
  AddTracks(*mSessionOff, "audio");
  AddTracks(*mSessionAns, "audio");

  OfferAnswer();

  ValidateSetupAttribute(*mSessionOff, SdpSetupAttribute::kActpass);
  ValidateSetupAttribute(*mSessionAns, SdpSetupAttribute::kActive);

  std::string reoffer = CreateOffer();
  SetLocalOffer(reoffer);
  SetRemoteOffer(reoffer);

  std::string reanswer = CreateAnswer();
  SetLocalAnswer(reanswer);

  std::string actpass = "\r\na=setup:active";
  size_t match = reanswer.find(actpass);
  ASSERT_NE(match, std::string::npos);
  reanswer.replace(match, actpass.length(), "\r\na=setup:passive");

  // This is expected to fail.
  SetRemoteAnswer(reanswer, NO_CHECKS);
  ASSERT_EQ(kJsepStateHaveLocalOffer, mSessionOff->GetState());
  ASSERT_EQ(kJsepStateStable, mSessionAns->GetState());
}

// Remove H.264 P1 and VP8 from offer, check answer negotiates H.264 P0
TEST_F(JsepSessionTest, OfferWithOnlyH264P0) {
  for (auto& codec : mSessionOff->Codecs()) {
    if (codec->mName != "H264" || codec->mDefaultPt == "126") {
      codec->mEnabled = false;
    }
  }

  types.push_back(SdpMediaSection::kAudio);
  types.push_back(SdpMediaSection::kVideo);
  AddTracks(*mSessionOff, "audio,video");
  AddTracks(*mSessionAns, "audio,video");
  std::string offer = CreateOffer();

  ASSERT_EQ(offer.find("a=rtpmap:126 H264/90000"), std::string::npos);
  ASSERT_EQ(offer.find("a=rtpmap:120 VP8/90000"), std::string::npos);

  SetLocalOffer(offer);
  SetRemoteOffer(offer);
  std::string answer = CreateAnswer();
  size_t match = answer.find("\r\na=setup:active");
  ASSERT_NE(match, std::string::npos);

  // validate answer SDP
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

// Test negotiating an answer which has only H.264 P1
// Which means replace VP8 with H.264 P1 in answer
TEST_F(JsepSessionTest, AnswerWithoutVP8) {
  types.push_back(SdpMediaSection::kAudio);
  types.push_back(SdpMediaSection::kVideo);
  AddTracks(*mSessionOff, "audio,video");
  AddTracks(*mSessionAns, "audio,video");
  std::string offer = CreateOffer();
  SetLocalOffer(offer);
  SetRemoteOffer(offer);

  for (auto& codec : mSessionOff->Codecs()) {
    if (codec->mName != "H264" || codec->mDefaultPt == "126") {
      codec->mEnabled = false;
    }
  }

  std::string answer = CreateAnswer();

  SetLocalAnswer(answer);
  SetRemoteAnswer(answer);
}

// Ok. Hear me out.
// The JSEP spec specifies very different behavior for the following two cases:
// 1. AddTrack either caused a transceiver to be created, or set the send
// track on a preexisting transceiver.
// 2. The transceiver was not created as a side-effect of AddTrack, and the
// send track was put in place by some other means than AddTrack.
//
// All together now...
//
// SADFACE :(
//
// Ok, enough of that. The upshot is we need to test two different codepaths for
// the same thing here. Most of this unit-test suite tests the "magic" case
// (case 1 above). Case 2 (the non-magic case) is simpler, so we have just a
// handful of tests.
TEST_F(JsepSessionTest, OffererNoAddTrackMagic) {
  types = BuildTypes("audio,video");
  AddTracks(*mSessionOff, NO_ADDTRACK_MAGIC);
  AddTracks(*mSessionAns);

  // Offerer's transceivers aren't "magic"; they will not associate with the
  // remote side's m-sections automatically. But, since they went into the
  // offer, everything works normally.
  OfferAnswer();

  ASSERT_EQ(2U, mSessionOff->GetTransceivers().size());
  ASSERT_EQ(2U, mSessionAns->GetTransceivers().size());
}

TEST_F(JsepSessionTest, AnswererNoAddTrackMagic) {
  types = BuildTypes("audio,video");
  AddTracks(*mSessionOff);
  AddTracks(*mSessionAns, NO_ADDTRACK_MAGIC);

  OfferAnswer(CHECK_SUCCESS);

  ASSERT_EQ(2U, mSessionOff->GetTransceivers().size());
  // Since answerer's transceivers aren't "magic", they cannot automatically be
  // attached to the offerer's m-sections.
  ASSERT_EQ(4U, mSessionAns->GetTransceivers().size());

  SwapOfferAnswerRoles();

  OfferAnswer(CHECK_SUCCESS);
  ASSERT_EQ(4U, mSessionOff->GetTransceivers().size());
  ASSERT_EQ(4U, mSessionAns->GetTransceivers().size());
}

// JSEP has rules about when a disabled m-section can be reused; the gist is
// that the m-section has to be negotiated disabled, then it becomes a candidate
// for reuse on the next renegotiation. Stopping a transceiver does not allow
// you to reuse on the next negotiation.
TEST_F(JsepSessionTest, OffererRecycle) {
  types = BuildTypes("audio,video");
  AddTracks(*mSessionOff);
  AddTracks(*mSessionAns);

  OfferAnswer();

  ASSERT_EQ(2U, mSessionOff->GetTransceivers().size());
  ASSERT_EQ(2U, mSessionAns->GetTransceivers().size());
  mSessionOff->GetTransceivers()[0]->Stop();
  AddTracks(*mSessionOff, "audio");
  ASSERT_EQ(3U, mSessionOff->GetTransceivers().size());

  OfferAnswer(CHECK_SUCCESS);

  // It is too soon to recycle msection 0, so the new track should have been
  // given a new msection.
  ASSERT_EQ(3U, mSessionOff->GetTransceivers().size());
  ASSERT_EQ(0U, mSessionOff->GetTransceivers()[0]->GetLevel());
  ASSERT_TRUE(mSessionOff->GetTransceivers()[0]->IsStopped());
  ASSERT_EQ(1U, mSessionOff->GetTransceivers()[1]->GetLevel());
  ASSERT_FALSE(mSessionOff->GetTransceivers()[1]->IsStopped());
  ASSERT_EQ(2U, mSessionOff->GetTransceivers()[2]->GetLevel());
  ASSERT_FALSE(mSessionOff->GetTransceivers()[2]->IsStopped());

  ASSERT_EQ(3U, mSessionAns->GetTransceivers().size());
  ASSERT_EQ(0U, mSessionAns->GetTransceivers()[0]->GetLevel());
  ASSERT_TRUE(mSessionAns->GetTransceivers()[0]->IsStopped());
  ASSERT_EQ(1U, mSessionAns->GetTransceivers()[1]->GetLevel());
  ASSERT_FALSE(mSessionAns->GetTransceivers()[1]->IsStopped());
  ASSERT_EQ(2U, mSessionAns->GetTransceivers()[2]->GetLevel());
  ASSERT_FALSE(mSessionAns->GetTransceivers()[2]->IsStopped());

  UniquePtr<Sdp> offer = GetParsedLocalDescription(*mSessionOff);
  ASSERT_EQ(3U, offer->GetMediaSectionCount());
  ValidateDisabledMSection(&offer->GetMediaSection(0));

  UniquePtr<Sdp> answer = GetParsedLocalDescription(*mSessionAns);
  ASSERT_EQ(3U, answer->GetMediaSectionCount());
  ValidateDisabledMSection(&answer->GetMediaSection(0));

  // Ok. Now renegotiating should recycle m-section 0.
  AddTracks(*mSessionOff, "audio");
  ASSERT_EQ(4U, mSessionOff->GetTransceivers().size());
  OfferAnswer(CHECK_SUCCESS);

  // Transceiver 3 should now be attached to m-section 0
  ASSERT_EQ(4U, mSessionOff->GetTransceivers().size());
  ASSERT_FALSE(mSessionOff->GetTransceivers()[0]->HasLevel());
  ASSERT_TRUE(mSessionOff->GetTransceivers()[0]->IsStopped());
  ASSERT_EQ(1U, mSessionOff->GetTransceivers()[1]->GetLevel());
  ASSERT_FALSE(mSessionOff->GetTransceivers()[1]->IsStopped());
  ASSERT_EQ(2U, mSessionOff->GetTransceivers()[2]->GetLevel());
  ASSERT_FALSE(mSessionOff->GetTransceivers()[2]->IsStopped());
  ASSERT_EQ(0U, mSessionOff->GetTransceivers()[3]->GetLevel());
  ASSERT_FALSE(mSessionOff->GetTransceivers()[3]->IsStopped());

  ASSERT_EQ(4U, mSessionAns->GetTransceivers().size());
  ASSERT_FALSE(mSessionAns->GetTransceivers()[0]->HasLevel());
  ASSERT_TRUE(mSessionAns->GetTransceivers()[0]->IsStopped());
  ASSERT_EQ(1U, mSessionAns->GetTransceivers()[1]->GetLevel());
  ASSERT_FALSE(mSessionAns->GetTransceivers()[1]->IsStopped());
  ASSERT_EQ(2U, mSessionAns->GetTransceivers()[2]->GetLevel());
  ASSERT_FALSE(mSessionAns->GetTransceivers()[2]->IsStopped());
  ASSERT_EQ(0U, mSessionAns->GetTransceivers()[3]->GetLevel());
  ASSERT_FALSE(mSessionAns->GetTransceivers()[3]->IsStopped());
}

TEST_F(JsepSessionTest, RecycleAnswererStopsTransceiver) {
  types = BuildTypes("audio,video");
  AddTracks(*mSessionOff);
  AddTracks(*mSessionAns);

  OfferAnswer();

  ASSERT_EQ(2U, mSessionOff->GetTransceivers().size());
  ASSERT_EQ(2U, mSessionAns->GetTransceivers().size());
  mSessionAns->GetTransceivers()[0]->Stop();

  OfferAnswer(CHECK_SUCCESS);

  ASSERT_EQ(2U, mSessionOff->GetTransceivers().size());
  ASSERT_EQ(0U, mSessionOff->GetTransceivers()[0]->GetLevel());
  ASSERT_TRUE(mSessionOff->GetTransceivers()[0]->IsStopped());
  ASSERT_EQ(1U, mSessionOff->GetTransceivers()[1]->GetLevel());
  ASSERT_FALSE(mSessionOff->GetTransceivers()[1]->IsStopped());

  ASSERT_EQ(2U, mSessionAns->GetTransceivers().size());
  ASSERT_EQ(0U, mSessionAns->GetTransceivers()[0]->GetLevel());
  ASSERT_TRUE(mSessionAns->GetTransceivers()[0]->IsStopped());
  ASSERT_EQ(1U, mSessionAns->GetTransceivers()[1]->GetLevel());
  ASSERT_FALSE(mSessionAns->GetTransceivers()[1]->IsStopped());

  UniquePtr<Sdp> offer = GetParsedLocalDescription(*mSessionOff);
  ASSERT_EQ(2U, offer->GetMediaSectionCount());

  UniquePtr<Sdp> answer = GetParsedLocalDescription(*mSessionAns);
  ASSERT_EQ(2U, answer->GetMediaSectionCount());
  ValidateDisabledMSection(&answer->GetMediaSection(0));

  // Renegotiating should recycle m-section 0.
  AddTracks(*mSessionOff, "audio");
  ASSERT_EQ(3U, mSessionOff->GetTransceivers().size());
  OfferAnswer(CHECK_SUCCESS);

  // Transceiver 3 should now be attached to m-section 0
  ASSERT_EQ(3U, mSessionOff->GetTransceivers().size());
  ASSERT_FALSE(mSessionOff->GetTransceivers()[0]->HasLevel());
  ASSERT_TRUE(mSessionOff->GetTransceivers()[0]->IsStopped());
  ASSERT_EQ(1U, mSessionOff->GetTransceivers()[1]->GetLevel());
  ASSERT_FALSE(mSessionOff->GetTransceivers()[1]->IsStopped());
  ASSERT_EQ(0U, mSessionOff->GetTransceivers()[2]->GetLevel());
  ASSERT_FALSE(mSessionOff->GetTransceivers()[2]->IsStopped());

  ASSERT_EQ(3U, mSessionAns->GetTransceivers().size());
  ASSERT_FALSE(mSessionAns->GetTransceivers()[0]->HasLevel());
  ASSERT_TRUE(mSessionAns->GetTransceivers()[0]->IsStopped());
  ASSERT_EQ(1U, mSessionAns->GetTransceivers()[1]->GetLevel());
  ASSERT_FALSE(mSessionAns->GetTransceivers()[1]->IsStopped());
  ASSERT_EQ(0U, mSessionAns->GetTransceivers()[2]->GetLevel());
  ASSERT_FALSE(mSessionAns->GetTransceivers()[2]->IsStopped());
}

// TODO: Have a test where offerer stops, and answerer adds a track and reoffers
// once Nils' role swap code lands.

// TODO: Have a test where answerer stops and adds a track.

TEST_F(JsepSessionTest, OffererRecycleNoMagic) {
  types = BuildTypes("audio,video");
  AddTracks(*mSessionOff);
  AddTracks(*mSessionAns);

  OfferAnswer();

  ASSERT_EQ(2U, mSessionOff->GetTransceivers().size());
  ASSERT_EQ(2U, mSessionAns->GetTransceivers().size());
  mSessionOff->GetTransceivers()[0]->Stop();

  OfferAnswer(CHECK_SUCCESS);

  // Ok. Now renegotiating should recycle m-section 0.
  AddTracks(*mSessionOff, "audio", NO_ADDTRACK_MAGIC);
  ASSERT_EQ(3U, mSessionOff->GetTransceivers().size());
  OfferAnswer(CHECK_SUCCESS);

  // Transceiver 2 should now be attached to m-section 0
  ASSERT_EQ(3U, mSessionOff->GetTransceivers().size());
  ASSERT_FALSE(mSessionOff->GetTransceivers()[0]->HasLevel());
  ASSERT_TRUE(mSessionOff->GetTransceivers()[0]->IsStopped());
  ASSERT_EQ(1U, mSessionOff->GetTransceivers()[1]->GetLevel());
  ASSERT_FALSE(mSessionOff->GetTransceivers()[1]->IsStopped());
  ASSERT_EQ(0U, mSessionOff->GetTransceivers()[2]->GetLevel());
  ASSERT_FALSE(mSessionOff->GetTransceivers()[2]->IsStopped());

  ASSERT_EQ(3U, mSessionAns->GetTransceivers().size());
  ASSERT_FALSE(mSessionAns->GetTransceivers()[0]->HasLevel());
  ASSERT_TRUE(mSessionAns->GetTransceivers()[0]->IsStopped());
  ASSERT_EQ(1U, mSessionAns->GetTransceivers()[1]->GetLevel());
  ASSERT_FALSE(mSessionAns->GetTransceivers()[1]->IsStopped());
  ASSERT_EQ(0U, mSessionAns->GetTransceivers()[2]->GetLevel());
  ASSERT_FALSE(mSessionAns->GetTransceivers()[2]->IsStopped());
}

TEST_F(JsepSessionTest, OffererRecycleNoMagicAnswererStopsTransceiver) {
  types = BuildTypes("audio,video");
  AddTracks(*mSessionOff);
  AddTracks(*mSessionAns);

  OfferAnswer();

  ASSERT_EQ(2U, mSessionOff->GetTransceivers().size());
  ASSERT_EQ(2U, mSessionAns->GetTransceivers().size());
  mSessionAns->GetTransceivers()[0]->Stop();

  OfferAnswer(CHECK_SUCCESS);

  // Ok. Now renegotiating should recycle m-section 0.
  AddTracks(*mSessionOff, "audio", NO_ADDTRACK_MAGIC);
  ASSERT_EQ(3U, mSessionOff->GetTransceivers().size());
  OfferAnswer(CHECK_SUCCESS);

  // Transceiver 2 should now be attached to m-section 0
  ASSERT_EQ(3U, mSessionOff->GetTransceivers().size());
  ASSERT_FALSE(mSessionOff->GetTransceivers()[0]->HasLevel());
  ASSERT_TRUE(mSessionOff->GetTransceivers()[0]->IsStopped());
  ASSERT_EQ(1U, mSessionOff->GetTransceivers()[1]->GetLevel());
  ASSERT_FALSE(mSessionOff->GetTransceivers()[1]->IsStopped());
  ASSERT_EQ(0U, mSessionOff->GetTransceivers()[2]->GetLevel());
  ASSERT_FALSE(mSessionOff->GetTransceivers()[2]->IsStopped());

  ASSERT_EQ(3U, mSessionAns->GetTransceivers().size());
  ASSERT_FALSE(mSessionAns->GetTransceivers()[0]->HasLevel());
  ASSERT_TRUE(mSessionAns->GetTransceivers()[0]->IsStopped());
  ASSERT_EQ(1U, mSessionAns->GetTransceivers()[1]->GetLevel());
  ASSERT_FALSE(mSessionAns->GetTransceivers()[1]->IsStopped());
  ASSERT_EQ(0U, mSessionAns->GetTransceivers()[2]->GetLevel());
  ASSERT_FALSE(mSessionAns->GetTransceivers()[2]->IsStopped());
}

TEST_F(JsepSessionTest, RecycleRollback) {
  types = BuildTypes("audio,video");
  AddTracks(*mSessionOff);
  AddTracks(*mSessionAns);

  OfferAnswer();

  ASSERT_EQ(2U, mSessionOff->GetTransceivers().size());
  ASSERT_EQ(2U, mSessionAns->GetTransceivers().size());
  mSessionOff->GetTransceivers()[0]->Stop();

  OfferAnswer(CHECK_SUCCESS);

  AddTracks(*mSessionOff, "audio");

  ASSERT_EQ(3U, mSessionOff->GetTransceivers().size());
  ASSERT_EQ(0U, mSessionOff->GetTransceivers()[0]->GetLevel());
  ASSERT_TRUE(mSessionOff->GetTransceivers()[0]->IsStopped());
  ASSERT_FALSE(mSessionOff->GetTransceivers()[0]->IsAssociated());
  ASSERT_EQ(1U, mSessionOff->GetTransceivers()[1]->GetLevel());
  ASSERT_FALSE(mSessionOff->GetTransceivers()[1]->IsStopped());
  ASSERT_TRUE(mSessionOff->GetTransceivers()[1]->IsAssociated());
  ASSERT_FALSE(mSessionOff->GetTransceivers()[2]->HasLevel());
  ASSERT_FALSE(mSessionOff->GetTransceivers()[2]->IsStopped());
  ASSERT_FALSE(mSessionOff->GetTransceivers()[2]->IsAssociated());

  std::string offer = CreateOffer();
  ASSERT_EQ(3U, mSessionOff->GetTransceivers().size());
  ASSERT_FALSE(mSessionOff->GetTransceivers()[0]->HasLevel());
  ASSERT_TRUE(mSessionOff->GetTransceivers()[0]->IsStopped());
  ASSERT_FALSE(mSessionOff->GetTransceivers()[0]->IsAssociated());
  ASSERT_EQ(1U, mSessionOff->GetTransceivers()[1]->GetLevel());
  ASSERT_FALSE(mSessionOff->GetTransceivers()[1]->IsStopped());
  ASSERT_TRUE(mSessionOff->GetTransceivers()[1]->IsAssociated());
  ASSERT_EQ(0U, mSessionOff->GetTransceivers()[2]->GetLevel());
  ASSERT_FALSE(mSessionOff->GetTransceivers()[2]->IsStopped());
  ASSERT_FALSE(mSessionOff->GetTransceivers()[2]->IsAssociated());

  SetLocalOffer(offer, CHECK_SUCCESS);

  ASSERT_EQ(3U, mSessionOff->GetTransceivers().size());
  ASSERT_FALSE(mSessionOff->GetTransceivers()[0]->HasLevel());
  ASSERT_TRUE(mSessionOff->GetTransceivers()[0]->IsStopped());
  ASSERT_FALSE(mSessionOff->GetTransceivers()[0]->IsAssociated());
  ASSERT_EQ(1U, mSessionOff->GetTransceivers()[1]->GetLevel());
  ASSERT_FALSE(mSessionOff->GetTransceivers()[1]->IsStopped());
  ASSERT_TRUE(mSessionOff->GetTransceivers()[1]->IsAssociated());
  ASSERT_EQ(0U, mSessionOff->GetTransceivers()[2]->GetLevel());
  ASSERT_FALSE(mSessionOff->GetTransceivers()[2]->IsStopped());
  // This should now be associated
  ASSERT_TRUE(mSessionOff->GetTransceivers()[2]->IsAssociated());

  ASSERT_FALSE(
      mSessionOff->SetLocalDescription(kJsepSdpRollback, "").mError.isSome());

  // Rollback should not change the levels of any of these, since those are set
  // in CreateOffer.
  ASSERT_EQ(3U, mSessionOff->GetTransceivers().size());
  ASSERT_FALSE(mSessionOff->GetTransceivers()[0]->HasLevel());
  ASSERT_TRUE(mSessionOff->GetTransceivers()[0]->IsStopped());
  ASSERT_FALSE(mSessionOff->GetTransceivers()[0]->IsAssociated());
  ASSERT_EQ(1U, mSessionOff->GetTransceivers()[1]->GetLevel());
  ASSERT_FALSE(mSessionOff->GetTransceivers()[1]->IsStopped());
  ASSERT_TRUE(mSessionOff->GetTransceivers()[1]->IsAssociated());
  ASSERT_EQ(0U, mSessionOff->GetTransceivers()[2]->GetLevel());
  ASSERT_FALSE(mSessionOff->GetTransceivers()[2]->IsStopped());
  // This should no longer be associated
  ASSERT_FALSE(mSessionOff->GetTransceivers()[2]->IsAssociated());
}

TEST_F(JsepSessionTest, AddTrackMagicWithNullReplaceTrack) {
  types = BuildTypes("audio,video");
  AddTracks(*mSessionOff);
  AddTracks(*mSessionAns);

  OfferAnswer();

  ASSERT_EQ(2U, mSessionOff->GetTransceivers().size());
  ASSERT_EQ(2U, mSessionAns->GetTransceivers().size());

  AddTracks(*mSessionAns, "audio");
  AddTracks(*mSessionOff, "audio");

  ASSERT_EQ(3U, mSessionAns->GetTransceivers().size());
  ASSERT_EQ(0U, mSessionAns->GetTransceivers()[0]->GetLevel());
  ASSERT_FALSE(mSessionAns->GetTransceivers()[0]->IsStopped());
  ASSERT_TRUE(mSessionAns->GetTransceivers()[0]->IsAssociated());
  ASSERT_EQ(1U, mSessionAns->GetTransceivers()[1]->GetLevel());
  ASSERT_FALSE(mSessionAns->GetTransceivers()[1]->IsStopped());
  ASSERT_TRUE(mSessionAns->GetTransceivers()[1]->IsAssociated());
  ASSERT_FALSE(mSessionAns->GetTransceivers()[2]->HasLevel());
  ASSERT_FALSE(mSessionAns->GetTransceivers()[2]->IsStopped());
  ASSERT_FALSE(mSessionAns->GetTransceivers()[2]->IsAssociated());
  ASSERT_TRUE(mSessionAns->GetTransceivers()[2]->HasAddTrackMagic());

  // Ok, transceiver 2 is "magical". Ensure it still has this "magical"
  // auto-matching property even if we null it out with replaceTrack.
  mSessionAns->GetTransceivers()[2]->mSendTrack.ClearStreamIds();
  mSessionAns->GetTransceivers()[2]->mJsDirection =
      SdpDirectionAttribute::Direction::kRecvonly;

  OfferAnswer(CHECK_SUCCESS);

  ASSERT_EQ(3U, mSessionAns->GetTransceivers().size());
  ASSERT_EQ(0U, mSessionAns->GetTransceivers()[0]->GetLevel());
  ASSERT_FALSE(mSessionAns->GetTransceivers()[0]->IsStopped());
  ASSERT_TRUE(mSessionAns->GetTransceivers()[0]->IsAssociated());
  ASSERT_EQ(1U, mSessionAns->GetTransceivers()[1]->GetLevel());
  ASSERT_FALSE(mSessionAns->GetTransceivers()[1]->IsStopped());
  ASSERT_TRUE(mSessionAns->GetTransceivers()[1]->IsAssociated());
  ASSERT_EQ(2U, mSessionAns->GetTransceivers()[2]->GetLevel());
  ASSERT_FALSE(mSessionAns->GetTransceivers()[2]->IsStopped());
  ASSERT_TRUE(mSessionAns->GetTransceivers()[2]->IsAssociated());
  ASSERT_TRUE(mSessionAns->GetTransceivers()[2]->HasAddTrackMagic());

  ASSERT_EQ(3U, mSessionOff->GetTransceivers().size());
  ASSERT_EQ(0U, mSessionOff->GetTransceivers()[0]->GetLevel());
  ASSERT_FALSE(mSessionOff->GetTransceivers()[0]->IsStopped());
  ASSERT_TRUE(mSessionOff->GetTransceivers()[0]->IsAssociated());
  ASSERT_EQ(1U, mSessionOff->GetTransceivers()[1]->GetLevel());
  ASSERT_FALSE(mSessionOff->GetTransceivers()[1]->IsStopped());
  ASSERT_TRUE(mSessionOff->GetTransceivers()[1]->IsAssociated());
  ASSERT_EQ(2U, mSessionOff->GetTransceivers()[2]->GetLevel());
  ASSERT_FALSE(mSessionOff->GetTransceivers()[2]->IsStopped());
  ASSERT_TRUE(mSessionOff->GetTransceivers()[2]->IsAssociated());
  ASSERT_TRUE(mSessionOff->GetTransceivers()[2]->HasAddTrackMagic());
}

// Flipside of AddTrackMagicWithNullReplaceTrack; we want to check that
// auto-matching does not work for transceivers that were created without a
// track, but were later given a track with replaceTrack.
TEST_F(JsepSessionTest, NoAddTrackMagicReplaceTrack) {
  types = BuildTypes("audio,video");
  AddTracks(*mSessionOff);
  AddTracks(*mSessionAns);

  OfferAnswer();

  ASSERT_EQ(2U, mSessionOff->GetTransceivers().size());
  ASSERT_EQ(2U, mSessionAns->GetTransceivers().size());
  AddTracks(*mSessionOff, "audio");
  mSessionAns->AddTransceiver(
      new JsepTransceiver(SdpMediaSection::MediaType::kAudio));

  mSessionAns->GetTransceivers()[2]->mSendTrack.UpdateStreamIds({"newstream"});

  ASSERT_EQ(3U, mSessionAns->GetTransceivers().size());
  ASSERT_EQ(0U, mSessionAns->GetTransceivers()[0]->GetLevel());
  ASSERT_FALSE(mSessionAns->GetTransceivers()[0]->IsStopped());
  ASSERT_TRUE(mSessionAns->GetTransceivers()[0]->IsAssociated());
  ASSERT_EQ(1U, mSessionAns->GetTransceivers()[1]->GetLevel());
  ASSERT_FALSE(mSessionAns->GetTransceivers()[1]->IsStopped());
  ASSERT_TRUE(mSessionAns->GetTransceivers()[1]->IsAssociated());
  ASSERT_FALSE(mSessionAns->GetTransceivers()[2]->HasLevel());
  ASSERT_FALSE(mSessionAns->GetTransceivers()[2]->IsStopped());
  ASSERT_FALSE(mSessionAns->GetTransceivers()[2]->IsAssociated());
  ASSERT_FALSE(mSessionAns->GetTransceivers()[2]->HasAddTrackMagic());

  OfferAnswer(CHECK_SUCCESS);

  ASSERT_EQ(4U, mSessionAns->GetTransceivers().size());
  ASSERT_EQ(0U, mSessionAns->GetTransceivers()[0]->GetLevel());
  ASSERT_FALSE(mSessionAns->GetTransceivers()[0]->IsStopped());
  ASSERT_TRUE(mSessionAns->GetTransceivers()[0]->IsAssociated());
  ASSERT_EQ(1U, mSessionAns->GetTransceivers()[1]->GetLevel());
  ASSERT_FALSE(mSessionAns->GetTransceivers()[1]->IsStopped());
  ASSERT_TRUE(mSessionAns->GetTransceivers()[1]->IsAssociated());
  ASSERT_FALSE(mSessionAns->GetTransceivers()[2]->HasLevel());
  ASSERT_FALSE(mSessionAns->GetTransceivers()[2]->IsStopped());
  ASSERT_FALSE(mSessionAns->GetTransceivers()[2]->IsAssociated());
  ASSERT_FALSE(mSessionAns->GetTransceivers()[2]->HasAddTrackMagic());
  ASSERT_EQ(2U, mSessionAns->GetTransceivers()[3]->GetLevel());
  ASSERT_FALSE(mSessionAns->GetTransceivers()[3]->IsStopped());
  ASSERT_TRUE(mSessionAns->GetTransceivers()[3]->IsAssociated());
}

// Check that transceivers that were created without a send track, but that
// were subsequently given a send track with addTrack, are now "magical".
TEST_F(JsepSessionTest, AddTrackMakesTransceiverMagical) {
  types = BuildTypes("audio,video");
  AddTracks(*mSessionOff);
  AddTracks(*mSessionAns);

  OfferAnswer();

  ASSERT_EQ(2U, mSessionOff->GetTransceivers().size());
  ASSERT_EQ(2U, mSessionAns->GetTransceivers().size());
  AddTracks(*mSessionOff, "audio");
  mSessionAns->AddTransceiver(
      new JsepTransceiver(SdpMediaSection::MediaType::kAudio));

  ASSERT_EQ(3U, mSessionAns->GetTransceivers().size());
  ASSERT_EQ(0U, mSessionAns->GetTransceivers()[0]->GetLevel());
  ASSERT_FALSE(mSessionAns->GetTransceivers()[0]->IsStopped());
  ASSERT_TRUE(mSessionAns->GetTransceivers()[0]->IsAssociated());
  ASSERT_EQ(1U, mSessionAns->GetTransceivers()[1]->GetLevel());
  ASSERT_FALSE(mSessionAns->GetTransceivers()[1]->IsStopped());
  ASSERT_TRUE(mSessionAns->GetTransceivers()[1]->IsAssociated());
  ASSERT_FALSE(mSessionAns->GetTransceivers()[2]->HasLevel());
  ASSERT_FALSE(mSessionAns->GetTransceivers()[2]->IsStopped());
  ASSERT_FALSE(mSessionAns->GetTransceivers()[2]->IsAssociated());
  ASSERT_FALSE(mSessionAns->GetTransceivers()[2]->HasAddTrackMagic());

  // :D MAGIC! D:
  AddTracks(*mSessionAns, "audio");

  ASSERT_EQ(3U, mSessionAns->GetTransceivers().size());
  ASSERT_EQ(0U, mSessionAns->GetTransceivers()[0]->GetLevel());
  ASSERT_FALSE(mSessionAns->GetTransceivers()[0]->IsStopped());
  ASSERT_TRUE(mSessionAns->GetTransceivers()[0]->IsAssociated());
  ASSERT_EQ(1U, mSessionAns->GetTransceivers()[1]->GetLevel());
  ASSERT_FALSE(mSessionAns->GetTransceivers()[1]->IsStopped());
  ASSERT_TRUE(mSessionAns->GetTransceivers()[1]->IsAssociated());
  ASSERT_FALSE(mSessionAns->GetTransceivers()[2]->HasLevel());
  ASSERT_FALSE(mSessionAns->GetTransceivers()[2]->IsStopped());
  ASSERT_FALSE(mSessionAns->GetTransceivers()[2]->IsAssociated());
  ASSERT_TRUE(mSessionAns->GetTransceivers()[2]->HasAddTrackMagic());

  OfferAnswer(CHECK_SUCCESS);

  ASSERT_EQ(3U, mSessionAns->GetTransceivers().size());
  ASSERT_EQ(0U, mSessionAns->GetTransceivers()[0]->GetLevel());
  ASSERT_FALSE(mSessionAns->GetTransceivers()[0]->IsStopped());
  ASSERT_TRUE(mSessionAns->GetTransceivers()[0]->IsAssociated());
  ASSERT_EQ(1U, mSessionAns->GetTransceivers()[1]->GetLevel());
  ASSERT_FALSE(mSessionAns->GetTransceivers()[1]->IsStopped());
  ASSERT_TRUE(mSessionAns->GetTransceivers()[1]->IsAssociated());
  ASSERT_EQ(2U, mSessionAns->GetTransceivers()[2]->GetLevel());
  ASSERT_FALSE(mSessionAns->GetTransceivers()[2]->IsStopped());
  ASSERT_TRUE(mSessionAns->GetTransceivers()[2]->IsAssociated());
  ASSERT_TRUE(mSessionAns->GetTransceivers()[2]->HasAddTrackMagic());
}

TEST_F(JsepSessionTest, ComplicatedRemoteRollback) {
  AddTracks(*mSessionOff, "audio,audio,audio,video");
  AddTracks(*mSessionAns, "video,video");

  std::string offer = CreateOffer();
  SetLocalOffer(offer, CHECK_SUCCESS);
  SetRemoteOffer(offer, CHECK_SUCCESS);

  // Three recvonly for audio, one sendrecv for video, and one (unmapped) for
  // the second video track.
  ASSERT_EQ(5U, mSessionAns->GetTransceivers().size());
  // First video transceiver; auto matched with offer
  ASSERT_EQ(3U, mSessionAns->GetTransceivers()[0]->GetLevel());
  ASSERT_FALSE(mSessionAns->GetTransceivers()[0]->IsStopped());
  ASSERT_TRUE(mSessionAns->GetTransceivers()[0]->IsAssociated());
  ASSERT_TRUE(mSessionAns->GetTransceivers()[0]->HasAddTrackMagic());
  ASSERT_FALSE(mSessionAns->GetTransceivers()[0]->WasCreatedBySetRemote());

  // Second video transceiver, not matched with offer
  ASSERT_FALSE(mSessionAns->GetTransceivers()[1]->HasLevel());
  ASSERT_FALSE(mSessionAns->GetTransceivers()[1]->IsStopped());
  ASSERT_FALSE(mSessionAns->GetTransceivers()[1]->IsAssociated());
  ASSERT_TRUE(mSessionAns->GetTransceivers()[1]->HasAddTrackMagic());
  ASSERT_FALSE(mSessionAns->GetTransceivers()[1]->WasCreatedBySetRemote());

  // Audio transceiver, created due to application of SetRemote
  ASSERT_EQ(0U, mSessionAns->GetTransceivers()[2]->GetLevel());
  ASSERT_FALSE(mSessionAns->GetTransceivers()[2]->IsStopped());
  ASSERT_TRUE(mSessionAns->GetTransceivers()[2]->IsAssociated());
  ASSERT_FALSE(mSessionAns->GetTransceivers()[2]->HasAddTrackMagic());
  ASSERT_TRUE(mSessionAns->GetTransceivers()[2]->WasCreatedBySetRemote());

  // Audio transceiver, created due to application of SetRemote
  ASSERT_EQ(1U, mSessionAns->GetTransceivers()[3]->GetLevel());
  ASSERT_FALSE(mSessionAns->GetTransceivers()[3]->IsStopped());
  ASSERT_TRUE(mSessionAns->GetTransceivers()[3]->IsAssociated());
  ASSERT_FALSE(mSessionAns->GetTransceivers()[3]->HasAddTrackMagic());
  ASSERT_TRUE(mSessionAns->GetTransceivers()[3]->WasCreatedBySetRemote());

  // Audio transceiver, created due to application of SetRemote
  ASSERT_EQ(2U, mSessionAns->GetTransceivers()[4]->GetLevel());
  ASSERT_FALSE(mSessionAns->GetTransceivers()[4]->IsStopped());
  ASSERT_TRUE(mSessionAns->GetTransceivers()[4]->IsAssociated());
  ASSERT_FALSE(mSessionAns->GetTransceivers()[4]->HasAddTrackMagic());
  ASSERT_TRUE(mSessionAns->GetTransceivers()[4]->WasCreatedBySetRemote());

  // This will cause the first audio transceiver to become "magical", and
  // thereby it will stick around after rollback, even though we clear it out
  // with replaceTrack.
  AddTracks(*mSessionAns, "audio");
  ASSERT_TRUE(mSessionAns->GetTransceivers()[2]->HasAddTrackMagic());
  mSessionAns->GetTransceivers()[2]->mSendTrack.ClearStreamIds();
  mSessionAns->GetTransceivers()[2]->mJsDirection =
      SdpDirectionAttribute::Direction::kRecvonly;

  // We do nothing with the second audio transceiver; when we rollback, it will
  // disappear entirely.

  // This will not cause the third audio transceiver to stick around; having a
  // track is _not_ enough to preserve it. It must have addTrack "magic"!
  mSessionAns->GetTransceivers()[4]->mSendTrack.UpdateStreamIds({"newstream"});

  // Create a fourth audio transceiver. Rollback will leave it alone, since we
  // created it.
  mSessionAns->AddTransceiver(
      new JsepTransceiver(SdpMediaSection::MediaType::kAudio,
                          SdpDirectionAttribute::Direction::kRecvonly));

  ASSERT_FALSE(
      mSessionAns->SetRemoteDescription(kJsepSdpRollback, "").mError.isSome());

  // Three recvonly for audio, one sendrecv for video, and one (unmapped) for
  // the second video track.
  ASSERT_EQ(4U, mSessionAns->GetTransceivers().size());

  // First video transceiver
  ASSERT_FALSE(mSessionAns->GetTransceivers()[0]->HasLevel());
  ASSERT_FALSE(mSessionAns->GetTransceivers()[0]->IsStopped());
  ASSERT_FALSE(mSessionAns->GetTransceivers()[0]->IsAssociated());
  ASSERT_TRUE(mSessionAns->GetTransceivers()[0]->HasAddTrackMagic());
  ASSERT_FALSE(IsNull(mSessionAns->GetTransceivers()[0]->mSendTrack));

  // Second video transceiver
  ASSERT_FALSE(mSessionAns->GetTransceivers()[1]->HasLevel());
  ASSERT_FALSE(mSessionAns->GetTransceivers()[1]->IsStopped());
  ASSERT_FALSE(mSessionAns->GetTransceivers()[1]->IsAssociated());
  ASSERT_TRUE(mSessionAns->GetTransceivers()[1]->HasAddTrackMagic());
  ASSERT_FALSE(IsNull(mSessionAns->GetTransceivers()[1]->mSendTrack));

  // First audio transceiver, kept because AddTrack touched it, even though we
  // removed the send track after.
  ASSERT_FALSE(mSessionAns->GetTransceivers()[2]->HasLevel());
  ASSERT_FALSE(mSessionAns->GetTransceivers()[2]->IsStopped());
  ASSERT_FALSE(mSessionAns->GetTransceivers()[2]->IsAssociated());
  ASSERT_TRUE(mSessionAns->GetTransceivers()[2]->HasAddTrackMagic());
  ASSERT_TRUE(IsNull(mSessionAns->GetTransceivers()[2]->mSendTrack));

  // Second audio transceiver should be gone.

  // Third audio transceiver should also be gone.

  // Fourth audio transceiver, created after SetRemote
  ASSERT_FALSE(mSessionAns->GetTransceivers()[5]->HasLevel());
  ASSERT_FALSE(mSessionAns->GetTransceivers()[5]->IsStopped());
  ASSERT_FALSE(mSessionAns->GetTransceivers()[5]->IsAssociated());
  ASSERT_FALSE(mSessionAns->GetTransceivers()[5]->HasAddTrackMagic());
  ASSERT_TRUE(
      mSessionAns->GetTransceivers()[5]->mSendTrack.GetStreamIds().empty());
}

TEST_F(JsepSessionTest, LocalRollback) {
  AddTracks(*mSessionOff, "audio,video");
  AddTracks(*mSessionAns, "audio,video");

  std::string offer = CreateOffer();
  SetLocalOffer(offer, CHECK_SUCCESS);

  ASSERT_TRUE(mSessionOff->GetTransceivers()[0]->IsAssociated());
  ASSERT_TRUE(mSessionOff->GetTransceivers()[1]->IsAssociated());
  ASSERT_FALSE(
      mSessionOff->SetLocalDescription(kJsepSdpRollback, "").mError.isSome());
  ASSERT_FALSE(mSessionOff->GetTransceivers()[0]->IsAssociated());
  ASSERT_FALSE(mSessionOff->GetTransceivers()[1]->IsAssociated());
}

TEST_F(JsepSessionTest, JsStopsTransceiverBeforeAnswer) {
  AddTracks(*mSessionOff, "audio,video");
  AddTracks(*mSessionAns, "audio,video");

  std::string offer = CreateOffer();
  SetLocalOffer(offer, CHECK_SUCCESS);
  SetRemoteOffer(offer, CHECK_SUCCESS);

  std::string answer = CreateAnswer();
  SetLocalAnswer(answer, CHECK_SUCCESS);

  // Now JS decides to stop a transceiver. Make sure transport stuff is still
  // ready to go when the answer is set. This should only prevent the flow of
  // media for that transceiver.

  mSessionOff->GetTransceivers()[0]->Stop();
  SetRemoteAnswer(answer, CHECK_SUCCESS);

  ASSERT_TRUE(mSessionOff->GetTransceivers()[0]->IsStopped());
  ASSERT_EQ(1U, mSessionOff->GetTransceivers()[0]->mTransport.mComponents);
  ASSERT_FALSE(mSessionOff->GetTransceivers()[0]->mSendTrack.GetActive());
  ASSERT_FALSE(mSessionOff->GetTransceivers()[0]->mRecvTrack.GetActive());
}

TEST_F(JsepSessionTest, TestOfferPTAsymmetryRtxApt) {
  for (auto& codec : mSessionAns->Codecs()) {
    if (codec->mName == "VP8") {
      JsepVideoCodecDescription* vp8 =
          static_cast<JsepVideoCodecDescription*>(codec.get());
      vp8->EnableRtx("42");
      break;
    }
  }

  types.push_back(SdpMediaSection::kVideo);
  AddTracks(*mSessionOff, "video");
  AddTracks(*mSessionAns, "video");
  JsepOfferOptions options;

  // Ensure that mSessionAns is appropriately configured.
  std::string offer;
  JsepSession::Result result = mSessionAns->CreateOffer(options, &offer);
  ASSERT_FALSE(result.mError.isSome());
  ASSERT_NE(std::string::npos, offer.find("a=rtpmap:42 rtx")) << offer;

  OfferAnswer();

  // Answerer should use what the offerer suggested
  UniquePtr<JsepCodecDescription> codec;
  GetCodec(*mSessionAns, 0, sdp::kSend, 0, 0, &codec);
  ASSERT_TRUE(codec);
  ASSERT_EQ("VP8", codec->mName);
  JsepVideoCodecDescription* vp8 =
      static_cast<JsepVideoCodecDescription*>(codec.get());
  ASSERT_EQ("120", vp8->mDefaultPt);
  ASSERT_EQ("124", vp8->mRtxPayloadType);
  GetCodec(*mSessionAns, 0, sdp::kRecv, 0, 0, &codec);
  ASSERT_TRUE(codec);
  ASSERT_EQ("VP8", codec->mName);
  vp8 = static_cast<JsepVideoCodecDescription*>(codec.get());
  ASSERT_EQ("120", vp8->mDefaultPt);
  ASSERT_EQ("124", vp8->mRtxPayloadType);

  // Answerer should not change back when it reoffers
  result = mSessionAns->CreateOffer(options, &offer);
  ASSERT_FALSE(result.mError.isSome());
  ASSERT_NE(std::string::npos, offer.find("a=rtpmap:124 rtx")) << offer;
}

TEST_F(JsepSessionTest, TestAnswerPTAsymmetryRtx) {
  // JsepSessionImpl will never answer with an asymmetric payload type
  // (tested in TestOfferPTAsymmetry), so we have to rewrite SDP a little.
  types.push_back(SdpMediaSection::kVideo);
  AddTracks(*mSessionOff, "video");
  AddTracks(*mSessionAns, "video");

  std::string offer = CreateOffer();
  SetLocalOffer(offer);

  Replace("a=rtpmap:120 VP8", "a=rtpmap:119 VP8", &offer);
  Replace("m=video 9 UDP/TLS/RTP/SAVPF 120", "m=video 9 UDP/TLS/RTP/SAVPF 119",
          &offer);
  ReplaceAll("a=fmtp:120", "a=fmtp:119", &offer);
  ReplaceAll("a=fmtp:122 120", "a=fmtp:122 119", &offer);
  ReplaceAll("a=fmtp:124 apt=120", "a=fmtp:124 apt=119", &offer);
  ReplaceAll("a=rtcp-fb:120", "a=rtcp-fb:119", &offer);

  SetRemoteOffer(offer);

  std::string answer = CreateAnswer();
  SetLocalAnswer(answer);
  SetRemoteAnswer(answer);

  UniquePtr<JsepCodecDescription> codec;
  GetCodec(*mSessionOff, 0, sdp::kSend, 0, 0, &codec);
  ASSERT_TRUE(codec);
  ASSERT_EQ("VP8", codec->mName);
  ASSERT_EQ("119", codec->mDefaultPt);
  JsepVideoCodecDescription* vp8 =
      static_cast<JsepVideoCodecDescription*>(codec.get());
  ASSERT_EQ("124", vp8->mRtxPayloadType);
  GetCodec(*mSessionOff, 0, sdp::kRecv, 0, 0, &codec);
  ASSERT_TRUE(codec);
  ASSERT_EQ("VP8", codec->mName);
  ASSERT_EQ("120", codec->mDefaultPt);
  vp8 = static_cast<JsepVideoCodecDescription*>(codec.get());
  ASSERT_EQ("124", vp8->mRtxPayloadType);

  GetCodec(*mSessionAns, 0, sdp::kSend, 0, 0, &codec);
  ASSERT_TRUE(codec);
  ASSERT_EQ("VP8", codec->mName);
  ASSERT_EQ("119", codec->mDefaultPt);
  vp8 = static_cast<JsepVideoCodecDescription*>(codec.get());
  ASSERT_EQ("124", vp8->mRtxPayloadType);
  GetCodec(*mSessionAns, 0, sdp::kRecv, 0, 0, &codec);
  ASSERT_TRUE(codec);
  ASSERT_EQ("VP8", codec->mName);
  ASSERT_EQ("119", codec->mDefaultPt);
  vp8 = static_cast<JsepVideoCodecDescription*>(codec.get());
  ASSERT_EQ("124", vp8->mRtxPayloadType);
}

TEST_F(JsepSessionTest, TestAnswerPTAsymmetryRtxApt) {
  // JsepSessionImpl will never answer with an asymmetric payload type
  // so we have to rewrite SDP a little.
  types.push_back(SdpMediaSection::kVideo);
  AddTracks(*mSessionOff, "video");
  AddTracks(*mSessionAns, "video");

  std::string offer = CreateOffer();
  SetLocalOffer(offer);

  Replace("a=rtpmap:124 rtx", "a=rtpmap:42 rtx", &offer);
  Replace("m=video 9 UDP/TLS/RTP/SAVPF 120 124",
          "m=video 9 UDP/TLS/RTP/SAVPF 120 42", &offer);
  ReplaceAll("a=fmtp:124", "a=fmtp:42", &offer);

  SetRemoteOffer(offer);

  std::string answer = CreateAnswer();
  SetLocalAnswer(answer);
  SetRemoteAnswer(answer);

  UniquePtr<JsepCodecDescription> codec;
  GetCodec(*mSessionOff, 0, sdp::kSend, 0, 0, &codec);
  ASSERT_TRUE(codec);
  ASSERT_EQ("VP8", codec->mName);
  ASSERT_EQ("120", codec->mDefaultPt);
  JsepVideoCodecDescription* vp8 =
      static_cast<JsepVideoCodecDescription*>(codec.get());
  ASSERT_EQ("42", vp8->mRtxPayloadType);
  GetCodec(*mSessionOff, 0, sdp::kRecv, 0, 0, &codec);
  ASSERT_TRUE(codec);
  ASSERT_EQ("VP8", codec->mName);
  ASSERT_EQ("120", codec->mDefaultPt);
  vp8 = static_cast<JsepVideoCodecDescription*>(codec.get());
  ASSERT_EQ("124", vp8->mRtxPayloadType);

  GetCodec(*mSessionAns, 0, sdp::kSend, 0, 0, &codec);
  ASSERT_TRUE(codec);
  ASSERT_EQ("VP8", codec->mName);
  vp8 = static_cast<JsepVideoCodecDescription*>(codec.get());
  ASSERT_EQ("120", vp8->mDefaultPt);
  ASSERT_EQ("42", vp8->mRtxPayloadType);
  GetCodec(*mSessionAns, 0, sdp::kRecv, 0, 0, &codec);
  ASSERT_TRUE(codec);
  ASSERT_EQ("VP8", codec->mName);
  vp8 = static_cast<JsepVideoCodecDescription*>(codec.get());
  ASSERT_EQ("120", vp8->mDefaultPt);
  ASSERT_EQ("42", vp8->mRtxPayloadType);
}

TEST_F(JsepSessionTest, TestOfferNoRtx) {
  for (auto& codec : mSessionOff->Codecs()) {
    if (codec->mType == SdpMediaSection::kVideo) {
      JsepVideoCodecDescription* videoCodec =
          static_cast<JsepVideoCodecDescription*>(codec.get());
      videoCodec->mRtxEnabled = false;
    }
  }

  types.push_back(SdpMediaSection::kVideo);
  AddTracks(*mSessionOff, "video");
  AddTracks(*mSessionAns, "video");
  JsepOfferOptions options;

  std::string offer;
  JsepSession::Result result = mSessionOff->CreateOffer(options, &offer);
  ASSERT_FALSE(result.mError.isSome());
  ASSERT_EQ(std::string::npos, offer.find("rtx")) << offer;

  OfferAnswer();

  // Answerer should use what the offerer suggested
  UniquePtr<JsepCodecDescription> codec;
  for (size_t i = 0; i < 4; ++i) {
    GetCodec(*mSessionAns, 0, sdp::kSend, 0, i, &codec);
    ASSERT_TRUE(codec);
    JsepVideoCodecDescription* videoCodec =
        static_cast<JsepVideoCodecDescription*>(codec.get());
    ASSERT_FALSE(videoCodec->mRtxEnabled);
    GetCodec(*mSessionAns, 0, sdp::kRecv, 0, i, &codec);
    ASSERT_TRUE(codec);
    videoCodec = static_cast<JsepVideoCodecDescription*>(codec.get());
    ASSERT_FALSE(videoCodec->mRtxEnabled);
  }
}

TEST_F(JsepSessionTest, TestOneWayRtx) {
  for (auto& codec : mSessionAns->Codecs()) {
    if (codec->mType == SdpMediaSection::kVideo) {
      JsepVideoCodecDescription* videoCodec =
          static_cast<JsepVideoCodecDescription*>(codec.get());
      videoCodec->mRtxEnabled = false;
    }
  }

  types.push_back(SdpMediaSection::kVideo);
  AddTracks(*mSessionOff, "video");
  AddTracks(*mSessionAns, "video");
  JsepOfferOptions options;

  std::string offer;
  JsepSession::Result result = mSessionAns->CreateOffer(options, &offer);
  ASSERT_FALSE(result.mError.isSome());
  ASSERT_EQ(std::string::npos, offer.find("rtx")) << offer;

  OfferAnswer();

  // If the answerer does not support rtx, the offerer should not send it,
  // but it is too late to turn off recv on the offerer side.
  UniquePtr<JsepCodecDescription> codec;
  for (size_t i = 0; i < 4; ++i) {
    GetCodec(*mSessionOff, 0, sdp::kSend, 0, i, &codec);
    ASSERT_TRUE(codec);
    JsepVideoCodecDescription* videoCodec =
        static_cast<JsepVideoCodecDescription*>(codec.get());
    ASSERT_FALSE(videoCodec->mRtxEnabled);
    GetCodec(*mSessionOff, 0, sdp::kRecv, 0, i, &codec);
    ASSERT_TRUE(codec);
    videoCodec = static_cast<JsepVideoCodecDescription*>(codec.get());
    ASSERT_TRUE(videoCodec->mRtxEnabled);
  }
}

}  // namespace mozilla
