/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#define GTEST_HAS_RTTI 0
#include "gtest/gtest.h"

#include "signaling/src/jsep/JsepTrack.h"
#include "signaling/src/sdp/SipccSdp.h"
#include "signaling/src/sdp/SdpHelper.h"

namespace mozilla {

class JsepTrackTest : public ::testing::Test {
 public:
  JsepTrackTest()
      : mSendOff(SdpMediaSection::kAudio, sdp::kSend),
        mRecvOff(SdpMediaSection::kAudio, sdp::kRecv),
        mSendAns(SdpMediaSection::kAudio, sdp::kSend),
        mRecvAns(SdpMediaSection::kAudio, sdp::kRecv) {}

  std::vector<UniquePtr<JsepCodecDescription>> MakeCodecs(
      bool addFecCodecs = false, bool preferRed = false,
      bool addDtmfCodec = false) const {
    std::vector<UniquePtr<JsepCodecDescription>> results;
    results.emplace_back(new JsepAudioCodecDescription("1", "opus", 48000, 2));
    results.emplace_back(new JsepAudioCodecDescription("9", "G722", 8000, 1));
    if (addDtmfCodec) {
      results.emplace_back(
          new JsepAudioCodecDescription("101", "telephone-event", 8000, 1));
    }

    if (addFecCodecs && preferRed) {
      results.emplace_back(new JsepVideoCodecDescription("122", "red", 90000));
    }

    JsepVideoCodecDescription* vp8 =
        new JsepVideoCodecDescription("120", "VP8", 90000);
    vp8->mConstraints.maxFs = 12288;
    vp8->mConstraints.maxFps = 60;
    results.emplace_back(vp8);

    JsepVideoCodecDescription* h264 =
        new JsepVideoCodecDescription("126", "H264", 90000);
    h264->mPacketizationMode = 1;
    h264->mProfileLevelId = 0x42E00D;
    results.emplace_back(h264);

    if (addFecCodecs) {
      if (!preferRed) {
        results.emplace_back(
            new JsepVideoCodecDescription("122", "red", 90000));
      }
      results.emplace_back(
          new JsepVideoCodecDescription("123", "ulpfec", 90000));
    }

    results.emplace_back(new JsepApplicationCodecDescription(
        "webrtc-datachannel", 256, 5999, 499));

    // if we're doing something with red, it needs
    // to update the redundant encodings list
    for (auto& codec : results) {
      if (codec->mName == "red") {
        JsepVideoCodecDescription& red =
            static_cast<JsepVideoCodecDescription&>(*codec);
        red.UpdateRedundantEncodings(results);
      }
    }

    return results;
  }

  void Init(SdpMediaSection::MediaType type) {
    InitCodecs();
    InitTracks(type);
    InitSdp(type);
  }

  void InitCodecs() {
    mOffCodecs = MakeCodecs();
    mAnsCodecs = MakeCodecs();
  }

  void InitTracks(SdpMediaSection::MediaType type) {
    mSendOff = JsepTrack(type, sdp::kSend);
    if (type != SdpMediaSection::MediaType::kApplication) {
      mSendOff.UpdateStreamIds(std::vector<std::string>(1, "stream_id"));
    }
    mRecvOff = JsepTrack(type, sdp::kRecv);
    mSendOff.PopulateCodecs(mOffCodecs);
    mRecvOff.PopulateCodecs(mOffCodecs);

    mSendAns = JsepTrack(type, sdp::kSend);
    if (type != SdpMediaSection::MediaType::kApplication) {
      mSendAns.UpdateStreamIds(std::vector<std::string>(1, "stream_id"));
    }
    mRecvAns = JsepTrack(type, sdp::kRecv);
    mSendAns.PopulateCodecs(mAnsCodecs);
    mRecvAns.PopulateCodecs(mAnsCodecs);
  }

  void InitSdp(SdpMediaSection::MediaType type) {
    std::vector<std::string> msids(1, "*");
    std::string error;
    SdpHelper helper(&error);

    mOffer.reset(new SipccSdp(SdpOrigin("", 0, 0, sdp::kIPv4, "")));
    mOffer->AddMediaSection(type, SdpDirectionAttribute::kSendrecv, 0,
                            SdpHelper::GetProtocolForMediaType(type),
                            sdp::kIPv4, "0.0.0.0");
    // JsepTrack doesn't set msid-semantic
    helper.SetupMsidSemantic(msids, mOffer.get());

    mAnswer.reset(new SipccSdp(SdpOrigin("", 0, 0, sdp::kIPv4, "")));
    mAnswer->AddMediaSection(type, SdpDirectionAttribute::kSendrecv, 0,
                             SdpHelper::GetProtocolForMediaType(type),
                             sdp::kIPv4, "0.0.0.0");
    // JsepTrack doesn't set msid-semantic
    helper.SetupMsidSemantic(msids, mAnswer.get());
  }

  SdpMediaSection& GetOffer() { return mOffer->GetMediaSection(0); }

  SdpMediaSection& GetAnswer() { return mAnswer->GetMediaSection(0); }

  void CreateOffer() {
    mSendOff.AddToOffer(mSsrcGenerator, &GetOffer());
    mRecvOff.AddToOffer(mSsrcGenerator, &GetOffer());
  }

  void CreateAnswer() {
    if (mRecvAns.GetMediaType() != SdpMediaSection::MediaType::kApplication) {
      mRecvAns.UpdateRecvTrack(*mOffer, GetOffer());
    }

    mSendAns.AddToAnswer(GetOffer(), mSsrcGenerator, &GetAnswer());
    mRecvAns.AddToAnswer(GetOffer(), mSsrcGenerator, &GetAnswer());
  }

  void Negotiate() {
    std::cerr << "Offer SDP: " << std::endl;
    mOffer->Serialize(std::cerr);

    std::cerr << "Answer SDP: " << std::endl;
    mAnswer->Serialize(std::cerr);

    if (mRecvOff.GetMediaType() != SdpMediaSection::MediaType::kApplication) {
      mRecvOff.UpdateRecvTrack(*mAnswer, GetAnswer());
    }

    if (GetAnswer().IsSending()) {
      mSendAns.Negotiate(GetAnswer(), GetOffer());
      mRecvOff.Negotiate(GetAnswer(), GetAnswer());
    }

    if (GetAnswer().IsReceiving()) {
      mRecvAns.Negotiate(GetAnswer(), GetOffer());
      mSendOff.Negotiate(GetAnswer(), GetAnswer());
    }
  }

  void OfferAnswer() {
    CreateOffer();
    CreateAnswer();
    Negotiate();
    SanityCheck();
  }

  // TODO: Look into writing a macro that wraps an ASSERT_ and returns false
  // if it fails (probably requires writing a bool-returning function that
  // takes a void-returning lambda with a bool outparam, which will in turn
  // invokes the ASSERT_)
  static void CheckEncodingCount(size_t expected, const JsepTrack& send,
                                 const JsepTrack& recv) {
    if (expected) {
      ASSERT_TRUE(send.GetNegotiatedDetails());
      ASSERT_TRUE(recv.GetNegotiatedDetails());
    }

    if (!send.GetStreamIds().empty() && send.GetNegotiatedDetails()) {
      ASSERT_EQ(expected, send.GetNegotiatedDetails()->GetEncodingCount());
    }

    if (!recv.GetStreamIds().empty() && recv.GetNegotiatedDetails()) {
      ASSERT_EQ(expected, recv.GetNegotiatedDetails()->GetEncodingCount());
    }
  }

  void CheckOffEncodingCount(size_t expected) const {
    CheckEncodingCount(expected, mSendOff, mRecvAns);
  }

  void CheckAnsEncodingCount(size_t expected) const {
    CheckEncodingCount(expected, mSendAns, mRecvOff);
  }

  UniquePtr<JsepCodecDescription> GetCodec(const JsepTrack& track,
                                           SdpMediaSection::MediaType type,
                                           size_t expectedSize,
                                           size_t codecIndex) const {
    if (!track.GetNegotiatedDetails() ||
        track.GetNegotiatedDetails()->GetEncodingCount() != 1U ||
        track.GetMediaType() != type) {
      return nullptr;
    }
    const auto& codecs =
        track.GetNegotiatedDetails()->GetEncoding(0).GetCodecs();
    // it should not be possible for codecs to have a different type
    // than the track, but we'll check the codec here just in case.
    if (codecs.size() != expectedSize || codecIndex >= expectedSize ||
        codecs[codecIndex]->mType != type) {
      return nullptr;
    }
    return UniquePtr<JsepCodecDescription>(codecs[codecIndex]->Clone());
  }

  UniquePtr<JsepVideoCodecDescription> GetVideoCodec(
      const JsepTrack& track, size_t expectedSize = 1,
      size_t codecIndex = 0) const {
    auto codec =
        GetCodec(track, SdpMediaSection::kVideo, expectedSize, codecIndex);
    return UniquePtr<JsepVideoCodecDescription>(
        static_cast<JsepVideoCodecDescription*>(codec.release()));
  }

  UniquePtr<JsepAudioCodecDescription> GetAudioCodec(
      const JsepTrack& track, size_t expectedSize = 1,
      size_t codecIndex = 0) const {
    auto codec =
        GetCodec(track, SdpMediaSection::kAudio, expectedSize, codecIndex);
    return UniquePtr<JsepAudioCodecDescription>(
        static_cast<JsepAudioCodecDescription*>(codec.release()));
  }

  void CheckOtherFbExists(const JsepVideoCodecDescription& videoCodec,
                          SdpRtcpFbAttributeList::Type type) const {
    for (const auto& fb : videoCodec.mOtherFbTypes) {
      if (fb.type == type) {
        return;  // found the RtcpFb type, so stop looking
      }
    }
    FAIL();  // RtcpFb type not found
  }

  void SanityCheckRtcpFbs(const JsepVideoCodecDescription& a,
                          const JsepVideoCodecDescription& b) const {
    ASSERT_EQ(a.mNackFbTypes.size(), b.mNackFbTypes.size());
    ASSERT_EQ(a.mAckFbTypes.size(), b.mAckFbTypes.size());
    ASSERT_EQ(a.mCcmFbTypes.size(), b.mCcmFbTypes.size());
    ASSERT_EQ(a.mOtherFbTypes.size(), b.mOtherFbTypes.size());
  }

  void SanityCheckCodecs(const JsepCodecDescription& a,
                         const JsepCodecDescription& b) const {
    ASSERT_EQ(a.mType, b.mType);
    if (a.mType != SdpMediaSection::kApplication) {
      ASSERT_EQ(a.mDefaultPt, b.mDefaultPt);
    }
    std::cerr << a.mName << " vs " << b.mName << std::endl;
    ASSERT_EQ(a.mName, b.mName);
    ASSERT_EQ(a.mClock, b.mClock);
    ASSERT_EQ(a.mChannels, b.mChannels);
    ASSERT_NE(a.mDirection, b.mDirection);
    // These constraints are for fmtp and rid, which _are_ signaled
    ASSERT_EQ(a.mConstraints, b.mConstraints);

    if (a.mType == SdpMediaSection::kVideo) {
      SanityCheckRtcpFbs(static_cast<const JsepVideoCodecDescription&>(a),
                         static_cast<const JsepVideoCodecDescription&>(b));
    }
  }

  void SanityCheckEncodings(const JsepTrackEncoding& a,
                            const JsepTrackEncoding& b) const {
    ASSERT_EQ(a.GetCodecs().size(), b.GetCodecs().size());
    for (size_t i = 0; i < a.GetCodecs().size(); ++i) {
      SanityCheckCodecs(*a.GetCodecs()[i], *b.GetCodecs()[i]);
    }

    ASSERT_EQ(a.mRid, b.mRid);
    // mConstraints will probably differ, since they are not signaled to the
    // other side.
  }

  void SanityCheckNegotiatedDetails(const JsepTrackNegotiatedDetails& a,
                                    const JsepTrackNegotiatedDetails& b) const {
    ASSERT_EQ(a.GetEncodingCount(), b.GetEncodingCount());
    for (size_t i = 0; i < a.GetEncodingCount(); ++i) {
      SanityCheckEncodings(a.GetEncoding(i), b.GetEncoding(i));
    }

    ASSERT_EQ(a.GetUniquePayloadTypes().size(),
              b.GetUniquePayloadTypes().size());
    for (size_t i = 0; i < a.GetUniquePayloadTypes().size(); ++i) {
      ASSERT_EQ(a.GetUniquePayloadTypes()[i], b.GetUniquePayloadTypes()[i]);
    }
  }

  void SanityCheckTracks(const JsepTrack& a, const JsepTrack& b) const {
    if (!a.GetNegotiatedDetails()) {
      ASSERT_FALSE(!!b.GetNegotiatedDetails());
      return;
    }

    ASSERT_TRUE(!!a.GetNegotiatedDetails());
    ASSERT_TRUE(!!b.GetNegotiatedDetails());
    ASSERT_EQ(a.GetMediaType(), b.GetMediaType());
    ASSERT_EQ(a.GetStreamIds(), b.GetStreamIds());
    ASSERT_EQ(a.GetCNAME(), b.GetCNAME());
    ASSERT_NE(a.GetDirection(), b.GetDirection());
    ASSERT_EQ(a.GetSsrcs().size(), b.GetSsrcs().size());
    for (size_t i = 0; i < a.GetSsrcs().size(); ++i) {
      ASSERT_EQ(a.GetSsrcs()[i], b.GetSsrcs()[i]);
    }

    SanityCheckNegotiatedDetails(*a.GetNegotiatedDetails(),
                                 *b.GetNegotiatedDetails());
  }

  void SanityCheck() const {
    SanityCheckTracks(mSendOff, mRecvAns);
    SanityCheckTracks(mRecvOff, mSendAns);
  }

 protected:
  JsepTrack mSendOff;
  JsepTrack mRecvOff;
  JsepTrack mSendAns;
  JsepTrack mRecvAns;
  std::vector<UniquePtr<JsepCodecDescription>> mOffCodecs;
  std::vector<UniquePtr<JsepCodecDescription>> mAnsCodecs;
  UniquePtr<Sdp> mOffer;
  UniquePtr<Sdp> mAnswer;
  SsrcGenerator mSsrcGenerator;
};

TEST_F(JsepTrackTest, CreateDestroy) { Init(SdpMediaSection::kAudio); }

TEST_F(JsepTrackTest, AudioNegotiation) {
  Init(SdpMediaSection::kAudio);
  OfferAnswer();
  CheckOffEncodingCount(1);
  CheckAnsEncodingCount(1);
}

TEST_F(JsepTrackTest, VideoNegotiation) {
  Init(SdpMediaSection::kVideo);
  OfferAnswer();
  CheckOffEncodingCount(1);
  CheckAnsEncodingCount(1);
}

class CheckForCodecType {
 public:
  explicit CheckForCodecType(SdpMediaSection::MediaType type, bool* result)
      : mResult(result), mType(type) {}

  void operator()(const UniquePtr<JsepCodecDescription>& codec) {
    if (codec->mType == mType) {
      *mResult = true;
    }
  }

 private:
  bool* mResult;
  SdpMediaSection::MediaType mType;
};

TEST_F(JsepTrackTest, CheckForMismatchedAudioCodecAndVideoTrack) {
  std::vector<UniquePtr<JsepCodecDescription>> offerCodecs;

  // make codecs including telephone-event (an audio codec)
  offerCodecs = MakeCodecs(false, false, true);
  JsepTrack videoTrack(SdpMediaSection::kVideo, sdp::kSend);
  videoTrack.UpdateStreamIds(std::vector<std::string>(1, "stream_id"));
  // populate codecs and then make sure we don't have any audio codecs
  // in the video track
  videoTrack.PopulateCodecs(offerCodecs);

  bool found = false;
  videoTrack.ForEachCodec(CheckForCodecType(SdpMediaSection::kAudio, &found));
  ASSERT_FALSE(found);

  found = false;
  videoTrack.ForEachCodec(CheckForCodecType(SdpMediaSection::kVideo, &found));
  ASSERT_TRUE(found);  // for sanity, make sure we did find video codecs
}

TEST_F(JsepTrackTest, CheckVideoTrackWithHackedDtmfSdp) {
  Init(SdpMediaSection::kVideo);
  CreateOffer();
  // make sure we don't find sdp containing telephone-event in video track
  ASSERT_EQ(mOffer->ToString().find("a=rtpmap:101 telephone-event"),
            std::string::npos);
  // force audio codec telephone-event into video m= section of offer
  GetOffer().AddCodec("101", "telephone-event", 8000, 1);
  // make sure we _do_ find sdp containing telephone-event in video track
  ASSERT_NE(mOffer->ToString().find("a=rtpmap:101 telephone-event"),
            std::string::npos);

  CreateAnswer();
  // make sure we don't find sdp containing telephone-event in video track
  ASSERT_EQ(mAnswer->ToString().find("a=rtpmap:101 telephone-event"),
            std::string::npos);
  // force audio codec telephone-event into video m= section of answer
  GetAnswer().AddCodec("101", "telephone-event", 8000, 1);
  // make sure we _do_ find sdp containing telephone-event in video track
  ASSERT_NE(mAnswer->ToString().find("a=rtpmap:101 telephone-event"),
            std::string::npos);

  Negotiate();
  SanityCheck();

  CheckOffEncodingCount(1);
  CheckAnsEncodingCount(1);

  // make sure we still don't find any audio codecs in the video track after
  // hacking the sdp
  bool found = false;
  mSendOff.ForEachCodec(CheckForCodecType(SdpMediaSection::kAudio, &found));
  ASSERT_FALSE(found);
  mRecvOff.ForEachCodec(CheckForCodecType(SdpMediaSection::kAudio, &found));
  ASSERT_FALSE(found);
  mSendAns.ForEachCodec(CheckForCodecType(SdpMediaSection::kAudio, &found));
  ASSERT_FALSE(found);
  mRecvAns.ForEachCodec(CheckForCodecType(SdpMediaSection::kAudio, &found));
  ASSERT_FALSE(found);
}

TEST_F(JsepTrackTest, AudioNegotiationOffererDtmf) {
  mOffCodecs = MakeCodecs(false, false, true);
  mAnsCodecs = MakeCodecs(false, false, false);

  InitTracks(SdpMediaSection::kAudio);
  InitSdp(SdpMediaSection::kAudio);
  OfferAnswer();

  CheckOffEncodingCount(1);
  CheckAnsEncodingCount(1);

  ASSERT_NE(mOffer->ToString().find("a=rtpmap:101 telephone-event"),
            std::string::npos);
  ASSERT_EQ(mAnswer->ToString().find("a=rtpmap:101 telephone-event"),
            std::string::npos);

  ASSERT_NE(mOffer->ToString().find("a=fmtp:101 0-15"), std::string::npos);
  ASSERT_EQ(mAnswer->ToString().find("a=fmtp:101"), std::string::npos);

  UniquePtr<JsepAudioCodecDescription> track;
  ASSERT_TRUE((track = GetAudioCodec(mSendOff, 2, 0)));
  ASSERT_EQ("1", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(mRecvOff, 2, 0)));
  ASSERT_EQ("1", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(mSendAns, 2, 0)));
  ASSERT_EQ("1", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(mRecvAns, 2, 0)));
  ASSERT_EQ("1", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(mSendOff, 2, 1)));
  ASSERT_EQ("9", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(mRecvOff, 2, 1)));
  ASSERT_EQ("9", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(mSendAns, 2, 1)));
  ASSERT_EQ("9", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(mRecvAns, 2, 1)));
  ASSERT_EQ("9", track->mDefaultPt);
}

TEST_F(JsepTrackTest, AudioNegotiationAnswererDtmf) {
  mOffCodecs = MakeCodecs(false, false, false);
  mAnsCodecs = MakeCodecs(false, false, true);

  InitTracks(SdpMediaSection::kAudio);
  InitSdp(SdpMediaSection::kAudio);
  OfferAnswer();

  CheckOffEncodingCount(1);
  CheckAnsEncodingCount(1);

  ASSERT_EQ(mOffer->ToString().find("a=rtpmap:101 telephone-event"),
            std::string::npos);
  ASSERT_EQ(mAnswer->ToString().find("a=rtpmap:101 telephone-event"),
            std::string::npos);

  ASSERT_EQ(mOffer->ToString().find("a=fmtp:101 0-15"), std::string::npos);
  ASSERT_EQ(mAnswer->ToString().find("a=fmtp:101"), std::string::npos);

  UniquePtr<JsepAudioCodecDescription> track;
  ASSERT_TRUE((track = GetAudioCodec(mSendOff, 2, 0)));
  ASSERT_EQ("1", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(mRecvOff, 2, 0)));
  ASSERT_EQ("1", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(mSendAns, 2, 0)));
  ASSERT_EQ("1", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(mRecvAns, 2, 0)));
  ASSERT_EQ("1", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(mSendOff, 2, 1)));
  ASSERT_EQ("9", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(mRecvOff, 2, 1)));
  ASSERT_EQ("9", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(mSendAns, 2, 1)));
  ASSERT_EQ("9", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(mRecvAns, 2, 1)));
  ASSERT_EQ("9", track->mDefaultPt);
}

TEST_F(JsepTrackTest, AudioNegotiationOffererAnswererDtmf) {
  mOffCodecs = MakeCodecs(false, false, true);
  mAnsCodecs = MakeCodecs(false, false, true);

  InitTracks(SdpMediaSection::kAudio);
  InitSdp(SdpMediaSection::kAudio);
  OfferAnswer();

  CheckOffEncodingCount(1);
  CheckAnsEncodingCount(1);

  ASSERT_NE(mOffer->ToString().find("a=rtpmap:101 telephone-event"),
            std::string::npos);
  ASSERT_NE(mAnswer->ToString().find("a=rtpmap:101 telephone-event"),
            std::string::npos);

  ASSERT_NE(mOffer->ToString().find("a=fmtp:101 0-15"), std::string::npos);
  ASSERT_NE(mAnswer->ToString().find("a=fmtp:101 0-15"), std::string::npos);

  UniquePtr<JsepAudioCodecDescription> track;
  ASSERT_TRUE((track = GetAudioCodec(mSendOff, 3, 0)));
  ASSERT_EQ("1", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(mRecvOff, 3, 0)));
  ASSERT_EQ("1", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(mSendAns, 3, 0)));
  ASSERT_EQ("1", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(mRecvAns, 3, 0)));
  ASSERT_EQ("1", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(mSendOff, 3, 1)));
  ASSERT_EQ("9", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(mRecvOff, 3, 1)));
  ASSERT_EQ("9", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(mSendAns, 3, 1)));
  ASSERT_EQ("9", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(mRecvAns, 3, 1)));
  ASSERT_EQ("9", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(mSendOff, 3, 2)));
  ASSERT_EQ("101", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(mRecvOff, 3, 2)));
  ASSERT_EQ("101", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(mSendAns, 3, 2)));
  ASSERT_EQ("101", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(mRecvAns, 3, 2)));
  ASSERT_EQ("101", track->mDefaultPt);
}

TEST_F(JsepTrackTest, AudioNegotiationDtmfOffererNoFmtpAnswererFmtp) {
  mOffCodecs = MakeCodecs(false, false, true);
  mAnsCodecs = MakeCodecs(false, false, true);

  InitTracks(SdpMediaSection::kAudio);
  InitSdp(SdpMediaSection::kAudio);

  CreateOffer();
  GetOffer().RemoveFmtp("101");

  CreateAnswer();

  Negotiate();
  SanityCheck();

  CheckOffEncodingCount(1);
  CheckAnsEncodingCount(1);

  ASSERT_NE(mOffer->ToString().find("a=rtpmap:101 telephone-event"),
            std::string::npos);
  ASSERT_NE(mAnswer->ToString().find("a=rtpmap:101 telephone-event"),
            std::string::npos);

  ASSERT_EQ(mOffer->ToString().find("a=fmtp:101"), std::string::npos);
  ASSERT_NE(mAnswer->ToString().find("a=fmtp:101 0-15"), std::string::npos);

  UniquePtr<JsepAudioCodecDescription> track;
  ASSERT_TRUE((track = GetAudioCodec(mSendOff, 3, 0)));
  ASSERT_EQ("1", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(mRecvOff, 3, 0)));
  ASSERT_EQ("1", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(mSendAns, 3, 0)));
  ASSERT_EQ("1", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(mRecvAns, 3, 0)));
  ASSERT_EQ("1", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(mSendOff, 3, 1)));
  ASSERT_EQ("9", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(mRecvOff, 3, 1)));
  ASSERT_EQ("9", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(mSendAns, 3, 1)));
  ASSERT_EQ("9", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(mRecvAns, 3, 1)));
  ASSERT_EQ("9", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(mSendOff, 3, 2)));
  ASSERT_EQ("101", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(mRecvOff, 3, 2)));
  ASSERT_EQ("101", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(mSendAns, 3, 2)));
  ASSERT_EQ("101", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(mRecvAns, 3, 2)));
  ASSERT_EQ("101", track->mDefaultPt);
}

TEST_F(JsepTrackTest, AudioNegotiationDtmfOffererFmtpAnswererNoFmtp) {
  mOffCodecs = MakeCodecs(false, false, true);
  mAnsCodecs = MakeCodecs(false, false, true);

  InitTracks(SdpMediaSection::kAudio);
  InitSdp(SdpMediaSection::kAudio);

  CreateOffer();

  CreateAnswer();
  GetAnswer().RemoveFmtp("101");

  Negotiate();
  SanityCheck();

  CheckOffEncodingCount(1);
  CheckAnsEncodingCount(1);

  ASSERT_NE(mOffer->ToString().find("a=rtpmap:101 telephone-event"),
            std::string::npos);
  ASSERT_NE(mAnswer->ToString().find("a=rtpmap:101 telephone-event"),
            std::string::npos);

  ASSERT_NE(mOffer->ToString().find("a=fmtp:101 0-15"), std::string::npos);
  ASSERT_EQ(mAnswer->ToString().find("a=fmtp:101"), std::string::npos);

  UniquePtr<JsepAudioCodecDescription> track;
  ASSERT_TRUE((track = GetAudioCodec(mSendOff, 3, 0)));
  ASSERT_EQ("1", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(mRecvOff, 3, 0)));
  ASSERT_EQ("1", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(mSendAns, 3, 0)));
  ASSERT_EQ("1", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(mRecvAns, 3, 0)));
  ASSERT_EQ("1", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(mSendOff, 3, 1)));
  ASSERT_EQ("9", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(mRecvOff, 3, 1)));
  ASSERT_EQ("9", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(mSendAns, 3, 1)));
  ASSERT_EQ("9", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(mRecvAns, 3, 1)));
  ASSERT_EQ("9", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(mSendOff, 3, 2)));
  ASSERT_EQ("101", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(mRecvOff, 3, 2)));
  ASSERT_EQ("101", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(mSendAns, 3, 2)));
  ASSERT_EQ("101", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(mRecvAns, 3, 2)));
  ASSERT_EQ("101", track->mDefaultPt);
}

TEST_F(JsepTrackTest, AudioNegotiationDtmfOffererNoFmtpAnswererNoFmtp) {
  mOffCodecs = MakeCodecs(false, false, true);
  mAnsCodecs = MakeCodecs(false, false, true);

  InitTracks(SdpMediaSection::kAudio);
  InitSdp(SdpMediaSection::kAudio);

  CreateOffer();
  GetOffer().RemoveFmtp("101");

  CreateAnswer();
  GetAnswer().RemoveFmtp("101");

  Negotiate();
  SanityCheck();

  CheckOffEncodingCount(1);
  CheckAnsEncodingCount(1);

  ASSERT_NE(mOffer->ToString().find("a=rtpmap:101 telephone-event"),
            std::string::npos);
  ASSERT_NE(mAnswer->ToString().find("a=rtpmap:101 telephone-event"),
            std::string::npos);

  ASSERT_EQ(mOffer->ToString().find("a=fmtp:101"), std::string::npos);
  ASSERT_EQ(mAnswer->ToString().find("a=fmtp:101"), std::string::npos);

  UniquePtr<JsepAudioCodecDescription> track;
  ASSERT_TRUE((track = GetAudioCodec(mSendOff, 3, 0)));
  ASSERT_EQ("1", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(mRecvOff, 3, 0)));
  ASSERT_EQ("1", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(mSendAns, 3, 0)));
  ASSERT_EQ("1", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(mRecvAns, 3, 0)));
  ASSERT_EQ("1", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(mSendOff, 3, 1)));
  ASSERT_EQ("9", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(mRecvOff, 3, 1)));
  ASSERT_EQ("9", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(mSendAns, 3, 1)));
  ASSERT_EQ("9", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(mRecvAns, 3, 1)));
  ASSERT_EQ("9", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(mSendOff, 3, 2)));
  ASSERT_EQ("101", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(mRecvOff, 3, 2)));
  ASSERT_EQ("101", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(mSendAns, 3, 2)));
  ASSERT_EQ("101", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(mRecvAns, 3, 2)));
  ASSERT_EQ("101", track->mDefaultPt);
}

TEST_F(JsepTrackTest, VideoNegotationOffererFEC) {
  mOffCodecs = MakeCodecs(true);
  mAnsCodecs = MakeCodecs(false);

  InitTracks(SdpMediaSection::kVideo);
  InitSdp(SdpMediaSection::kVideo);
  OfferAnswer();

  CheckOffEncodingCount(1);
  CheckAnsEncodingCount(1);

  ASSERT_NE(mOffer->ToString().find("a=rtpmap:122 red"), std::string::npos);
  ASSERT_NE(mOffer->ToString().find("a=rtpmap:123 ulpfec"), std::string::npos);
  ASSERT_EQ(mAnswer->ToString().find("a=rtpmap:122 red"), std::string::npos);
  ASSERT_EQ(mAnswer->ToString().find("a=rtpmap:123 ulpfec"), std::string::npos);

  ASSERT_NE(mOffer->ToString().find("a=fmtp:122 120/126/123"),
            std::string::npos);
  ASSERT_EQ(mAnswer->ToString().find("a=fmtp:122"), std::string::npos);

  UniquePtr<JsepVideoCodecDescription> track;
  ASSERT_TRUE((track = GetVideoCodec(mSendOff, 2, 0)));
  ASSERT_EQ("120", track->mDefaultPt);
  ASSERT_TRUE((track = GetVideoCodec(mRecvOff, 2, 0)));
  ASSERT_EQ("120", track->mDefaultPt);
  ASSERT_TRUE((track = GetVideoCodec(mSendAns, 2, 0)));
  ASSERT_EQ("120", track->mDefaultPt);
  ASSERT_TRUE((track = GetVideoCodec(mRecvAns, 2, 0)));
  ASSERT_EQ("120", track->mDefaultPt);
  ASSERT_TRUE((track = GetVideoCodec(mSendOff, 2, 1)));
  ASSERT_EQ("126", track->mDefaultPt);
  ASSERT_TRUE((track = GetVideoCodec(mRecvOff, 2, 1)));
  ASSERT_EQ("126", track->mDefaultPt);
  ASSERT_TRUE((track = GetVideoCodec(mSendAns, 2, 1)));
  ASSERT_EQ("126", track->mDefaultPt);
  ASSERT_TRUE((track = GetVideoCodec(mRecvAns, 2, 1)));
  ASSERT_EQ("126", track->mDefaultPt);
}

TEST_F(JsepTrackTest, VideoNegotationAnswererFEC) {
  mOffCodecs = MakeCodecs(false);
  mAnsCodecs = MakeCodecs(true);

  InitTracks(SdpMediaSection::kVideo);
  InitSdp(SdpMediaSection::kVideo);
  OfferAnswer();

  CheckOffEncodingCount(1);
  CheckAnsEncodingCount(1);

  ASSERT_EQ(mOffer->ToString().find("a=rtpmap:122 red"), std::string::npos);
  ASSERT_EQ(mOffer->ToString().find("a=rtpmap:123 ulpfec"), std::string::npos);
  ASSERT_EQ(mAnswer->ToString().find("a=rtpmap:122 red"), std::string::npos);
  ASSERT_EQ(mAnswer->ToString().find("a=rtpmap:123 ulpfec"), std::string::npos);

  ASSERT_EQ(mOffer->ToString().find("a=fmtp:122"), std::string::npos);
  ASSERT_EQ(mAnswer->ToString().find("a=fmtp:122"), std::string::npos);

  UniquePtr<JsepVideoCodecDescription> track;
  ASSERT_TRUE((track = GetVideoCodec(mSendOff, 2, 0)));
  ASSERT_EQ("120", track->mDefaultPt);
  ASSERT_TRUE((track = GetVideoCodec(mRecvOff, 2, 0)));
  ASSERT_EQ("120", track->mDefaultPt);
  ASSERT_TRUE((track = GetVideoCodec(mSendAns, 2, 0)));
  ASSERT_EQ("120", track->mDefaultPt);
  ASSERT_TRUE((track = GetVideoCodec(mRecvAns, 2, 0)));
  ASSERT_EQ("120", track->mDefaultPt);
  ASSERT_TRUE((track = GetVideoCodec(mSendOff, 2, 1)));
  ASSERT_EQ("126", track->mDefaultPt);
  ASSERT_TRUE((track = GetVideoCodec(mRecvOff, 2, 1)));
  ASSERT_EQ("126", track->mDefaultPt);
  ASSERT_TRUE((track = GetVideoCodec(mSendAns, 2, 1)));
  ASSERT_EQ("126", track->mDefaultPt);
  ASSERT_TRUE((track = GetVideoCodec(mRecvAns, 2, 1)));
  ASSERT_EQ("126", track->mDefaultPt);
}

TEST_F(JsepTrackTest, VideoNegotationOffererAnswererFEC) {
  mOffCodecs = MakeCodecs(true);
  mAnsCodecs = MakeCodecs(true);

  InitTracks(SdpMediaSection::kVideo);
  InitSdp(SdpMediaSection::kVideo);
  OfferAnswer();

  CheckOffEncodingCount(1);
  CheckAnsEncodingCount(1);

  ASSERT_NE(mOffer->ToString().find("a=rtpmap:122 red"), std::string::npos);
  ASSERT_NE(mOffer->ToString().find("a=rtpmap:123 ulpfec"), std::string::npos);
  ASSERT_NE(mAnswer->ToString().find("a=rtpmap:122 red"), std::string::npos);
  ASSERT_NE(mAnswer->ToString().find("a=rtpmap:123 ulpfec"), std::string::npos);

  ASSERT_NE(mOffer->ToString().find("a=fmtp:122 120/126/123"),
            std::string::npos);
  ASSERT_NE(mAnswer->ToString().find("a=fmtp:122 120/126/123"),
            std::string::npos);

  UniquePtr<JsepVideoCodecDescription> track;
  ASSERT_TRUE((track = GetVideoCodec(mSendOff, 4)));
  ASSERT_EQ("120", track->mDefaultPt);
  ASSERT_TRUE((track = GetVideoCodec(mRecvOff, 4)));
  ASSERT_EQ("120", track->mDefaultPt);
  ASSERT_TRUE((track = GetVideoCodec(mSendAns, 4)));
  ASSERT_EQ("120", track->mDefaultPt);
  ASSERT_TRUE((track = GetVideoCodec(mRecvAns, 4)));
  ASSERT_EQ("120", track->mDefaultPt);
}

TEST_F(JsepTrackTest, VideoNegotationOffererAnswererFECPreferred) {
  mOffCodecs = MakeCodecs(true, true);
  mAnsCodecs = MakeCodecs(true);

  InitTracks(SdpMediaSection::kVideo);
  InitSdp(SdpMediaSection::kVideo);
  OfferAnswer();

  CheckOffEncodingCount(1);
  CheckAnsEncodingCount(1);

  ASSERT_NE(mOffer->ToString().find("a=rtpmap:122 red"), std::string::npos);
  ASSERT_NE(mOffer->ToString().find("a=rtpmap:123 ulpfec"), std::string::npos);
  ASSERT_NE(mAnswer->ToString().find("a=rtpmap:122 red"), std::string::npos);
  ASSERT_NE(mAnswer->ToString().find("a=rtpmap:123 ulpfec"), std::string::npos);

  ASSERT_NE(mOffer->ToString().find("a=fmtp:122 120/126/123"),
            std::string::npos);
  ASSERT_NE(mAnswer->ToString().find("a=fmtp:122 120/126/123"),
            std::string::npos);

  UniquePtr<JsepVideoCodecDescription> track;
  ASSERT_TRUE((track = GetVideoCodec(mSendOff, 4)));
  ASSERT_EQ("122", track->mDefaultPt);
  ASSERT_TRUE((track = GetVideoCodec(mRecvOff, 4)));
  ASSERT_EQ("122", track->mDefaultPt);
  ASSERT_TRUE((track = GetVideoCodec(mSendAns, 4)));
  ASSERT_EQ("122", track->mDefaultPt);
  ASSERT_TRUE((track = GetVideoCodec(mRecvAns, 4)));
  ASSERT_EQ("122", track->mDefaultPt);
}

// Make sure we only put the right things in the fmtp:122 120/.... line
TEST_F(JsepTrackTest, VideoNegotationOffererAnswererFECMismatch) {
  mOffCodecs = MakeCodecs(true, true);
  mAnsCodecs = MakeCodecs(true);
  // remove h264 from answer codecs
  ASSERT_EQ("H264", mAnsCodecs[3]->mName);
  mAnsCodecs.erase(mAnsCodecs.begin() + 3);

  InitTracks(SdpMediaSection::kVideo);
  InitSdp(SdpMediaSection::kVideo);
  OfferAnswer();

  CheckOffEncodingCount(1);
  CheckAnsEncodingCount(1);

  ASSERT_NE(mOffer->ToString().find("a=rtpmap:122 red"), std::string::npos);
  ASSERT_NE(mOffer->ToString().find("a=rtpmap:123 ulpfec"), std::string::npos);
  ASSERT_NE(mAnswer->ToString().find("a=rtpmap:122 red"), std::string::npos);
  ASSERT_NE(mAnswer->ToString().find("a=rtpmap:123 ulpfec"), std::string::npos);

  ASSERT_NE(mOffer->ToString().find("a=fmtp:122 120/126/123"),
            std::string::npos);
  ASSERT_NE(mAnswer->ToString().find("a=fmtp:122 120/123"), std::string::npos);

  UniquePtr<JsepVideoCodecDescription> track;
  ASSERT_TRUE((track = GetVideoCodec(mSendOff, 3)));
  ASSERT_EQ("122", track->mDefaultPt);
  ASSERT_TRUE((track = GetVideoCodec(mRecvOff, 3)));
  ASSERT_EQ("122", track->mDefaultPt);
  ASSERT_TRUE((track = GetVideoCodec(mSendAns, 3)));
  ASSERT_EQ("122", track->mDefaultPt);
  ASSERT_TRUE((track = GetVideoCodec(mRecvAns, 3)));
  ASSERT_EQ("122", track->mDefaultPt);
}

TEST_F(JsepTrackTest, VideoNegotationOffererAnswererFECZeroVP9Codec) {
  mOffCodecs = MakeCodecs(true);
  JsepVideoCodecDescription* vp9 =
      new JsepVideoCodecDescription("0", "VP9", 90000);
  vp9->mConstraints.maxFs = 12288;
  vp9->mConstraints.maxFps = 60;
  mOffCodecs.emplace_back(vp9);

  ASSERT_EQ(8U, mOffCodecs.size());
  JsepVideoCodecDescription& red =
      static_cast<JsepVideoCodecDescription&>(*mOffCodecs[4]);
  ASSERT_EQ("red", red.mName);
  // rebuild the redundant encodings with our newly added "wacky" VP9
  red.mRedundantEncodings.clear();
  red.UpdateRedundantEncodings(mOffCodecs);

  mAnsCodecs = MakeCodecs(true);

  InitTracks(SdpMediaSection::kVideo);
  InitSdp(SdpMediaSection::kVideo);
  OfferAnswer();

  CheckOffEncodingCount(1);
  CheckAnsEncodingCount(1);

  ASSERT_NE(mOffer->ToString().find("a=rtpmap:122 red"), std::string::npos);
  ASSERT_NE(mOffer->ToString().find("a=rtpmap:123 ulpfec"), std::string::npos);
  ASSERT_NE(mAnswer->ToString().find("a=rtpmap:122 red"), std::string::npos);
  ASSERT_NE(mAnswer->ToString().find("a=rtpmap:123 ulpfec"), std::string::npos);

  ASSERT_NE(mOffer->ToString().find("a=fmtp:122 120/126/123/0"),
            std::string::npos);
  ASSERT_NE(mAnswer->ToString().find("a=fmtp:122 120/126/123\r\n"),
            std::string::npos);
}

TEST_F(JsepTrackTest, VideoNegotiationOfferRemb) {
  InitCodecs();
  // enable remb on the offer codecs
  ((JsepVideoCodecDescription&)*mOffCodecs[2]).EnableRemb();
  InitTracks(SdpMediaSection::kVideo);
  InitSdp(SdpMediaSection::kVideo);
  OfferAnswer();

  // make sure REMB is on offer and not on answer
  ASSERT_NE(mOffer->ToString().find("a=rtcp-fb:120 goog-remb"),
            std::string::npos);
  ASSERT_EQ(mAnswer->ToString().find("a=rtcp-fb:120 goog-remb"),
            std::string::npos);
  CheckOffEncodingCount(1);
  CheckAnsEncodingCount(1);

  UniquePtr<JsepVideoCodecDescription> codec;
  ASSERT_TRUE((codec = GetVideoCodec(mSendOff, 2, 0)));
  ASSERT_EQ(codec->mOtherFbTypes.size(), 0U);
  ASSERT_TRUE((codec = GetVideoCodec(mRecvAns, 2, 0)));
  ASSERT_EQ(codec->mOtherFbTypes.size(), 0U);
  ASSERT_TRUE((codec = GetVideoCodec(mSendAns, 2, 0)));
  ASSERT_EQ(codec->mOtherFbTypes.size(), 0U);
  ASSERT_TRUE((codec = GetVideoCodec(mRecvOff, 2, 0)));
  ASSERT_EQ(codec->mOtherFbTypes.size(), 0U);
}

TEST_F(JsepTrackTest, VideoNegotiationAnswerRemb) {
  InitCodecs();
  // enable remb on the answer codecs
  ((JsepVideoCodecDescription&)*mAnsCodecs[2]).EnableRemb();
  InitTracks(SdpMediaSection::kVideo);
  InitSdp(SdpMediaSection::kVideo);
  OfferAnswer();

  // make sure REMB is not on offer and not on answer
  ASSERT_EQ(mOffer->ToString().find("a=rtcp-fb:120 goog-remb"),
            std::string::npos);
  ASSERT_EQ(mAnswer->ToString().find("a=rtcp-fb:120 goog-remb"),
            std::string::npos);
  CheckOffEncodingCount(1);
  CheckAnsEncodingCount(1);

  UniquePtr<JsepVideoCodecDescription> codec;
  ASSERT_TRUE((codec = GetVideoCodec(mSendOff, 2, 0)));
  ASSERT_EQ(codec->mOtherFbTypes.size(), 0U);
  ASSERT_TRUE((codec = GetVideoCodec(mRecvAns, 2, 0)));
  ASSERT_EQ(codec->mOtherFbTypes.size(), 0U);
  ASSERT_TRUE((codec = GetVideoCodec(mSendAns, 2, 0)));
  ASSERT_EQ(codec->mOtherFbTypes.size(), 0U);
  ASSERT_TRUE((codec = GetVideoCodec(mRecvOff, 2, 0)));
  ASSERT_EQ(codec->mOtherFbTypes.size(), 0U);
}

TEST_F(JsepTrackTest, VideoNegotiationOfferAnswerRemb) {
  InitCodecs();
  // enable remb on the offer and answer codecs
  ((JsepVideoCodecDescription&)*mOffCodecs[2]).EnableRemb();
  ((JsepVideoCodecDescription&)*mAnsCodecs[2]).EnableRemb();
  InitTracks(SdpMediaSection::kVideo);
  InitSdp(SdpMediaSection::kVideo);
  OfferAnswer();

  // make sure REMB is on offer and on answer
  ASSERT_NE(mOffer->ToString().find("a=rtcp-fb:120 goog-remb"),
            std::string::npos);
  ASSERT_NE(mAnswer->ToString().find("a=rtcp-fb:120 goog-remb"),
            std::string::npos);
  CheckOffEncodingCount(1);
  CheckAnsEncodingCount(1);

  UniquePtr<JsepVideoCodecDescription> codec;
  ASSERT_TRUE((codec = GetVideoCodec(mSendOff, 2, 0)));
  ASSERT_EQ(codec->mOtherFbTypes.size(), 1U);
  CheckOtherFbExists(*codec, SdpRtcpFbAttributeList::kRemb);
  ASSERT_TRUE((codec = GetVideoCodec(mRecvAns, 2, 0)));
  ASSERT_EQ(codec->mOtherFbTypes.size(), 1U);
  CheckOtherFbExists(*codec, SdpRtcpFbAttributeList::kRemb);
  ASSERT_TRUE((codec = GetVideoCodec(mSendAns, 2, 0)));
  ASSERT_EQ(codec->mOtherFbTypes.size(), 1U);
  CheckOtherFbExists(*codec, SdpRtcpFbAttributeList::kRemb);
  ASSERT_TRUE((codec = GetVideoCodec(mRecvOff, 2, 0)));
  ASSERT_EQ(codec->mOtherFbTypes.size(), 1U);
  CheckOtherFbExists(*codec, SdpRtcpFbAttributeList::kRemb);
}

TEST_F(JsepTrackTest, VideoNegotiationOfferTransportCC) {
  InitCodecs();
  // enable TransportCC on the offer codecs
  ((JsepVideoCodecDescription&)*mOffCodecs[2]).EnableTransportCC();
  InitTracks(SdpMediaSection::kVideo);
  InitSdp(SdpMediaSection::kVideo);
  OfferAnswer();

  // make sure TransportCC is on offer and not on answer
  ASSERT_NE(mOffer->ToString().find("a=rtcp-fb:120 transport-cc"),
            std::string::npos);
  ASSERT_EQ(mAnswer->ToString().find("a=rtcp-fb:120 transport-cc"),
            std::string::npos);
  CheckOffEncodingCount(1);
  CheckAnsEncodingCount(1);

  UniquePtr<JsepVideoCodecDescription> codec;
  ASSERT_TRUE((codec = GetVideoCodec(mSendOff, 2, 0)));
  ASSERT_EQ(codec->mOtherFbTypes.size(), 0U);
  ASSERT_TRUE((codec = GetVideoCodec(mRecvAns, 2, 0)));
  ASSERT_EQ(codec->mOtherFbTypes.size(), 0U);
  ASSERT_TRUE((codec = GetVideoCodec(mSendAns, 2, 0)));
  ASSERT_EQ(codec->mOtherFbTypes.size(), 0U);
  ASSERT_TRUE((codec = GetVideoCodec(mRecvOff, 2, 0)));
  ASSERT_EQ(codec->mOtherFbTypes.size(), 0U);
}

TEST_F(JsepTrackTest, VideoNegotiationAnswerTransportCC) {
  InitCodecs();
  // enable TransportCC on the answer codecs
  ((JsepVideoCodecDescription&)*mAnsCodecs[2]).EnableTransportCC();
  InitTracks(SdpMediaSection::kVideo);
  InitSdp(SdpMediaSection::kVideo);
  OfferAnswer();

  // make sure TransportCC is not on offer and not on answer
  ASSERT_EQ(mOffer->ToString().find("a=rtcp-fb:120 transport-cc"),
            std::string::npos);
  ASSERT_EQ(mAnswer->ToString().find("a=rtcp-fb:120 transport-cc"),
            std::string::npos);
  CheckOffEncodingCount(1);
  CheckAnsEncodingCount(1);

  UniquePtr<JsepVideoCodecDescription> codec;
  ASSERT_TRUE((codec = GetVideoCodec(mSendOff, 2, 0)));
  ASSERT_EQ(codec->mOtherFbTypes.size(), 0U);
  ASSERT_TRUE((codec = GetVideoCodec(mRecvAns, 2, 0)));
  ASSERT_EQ(codec->mOtherFbTypes.size(), 0U);
  ASSERT_TRUE((codec = GetVideoCodec(mSendAns, 2, 0)));
  ASSERT_EQ(codec->mOtherFbTypes.size(), 0U);
  ASSERT_TRUE((codec = GetVideoCodec(mRecvOff, 2, 0)));
  ASSERT_EQ(codec->mOtherFbTypes.size(), 0U);
}

TEST_F(JsepTrackTest, VideoNegotiationOfferAnswerTransportCC) {
  InitCodecs();
  // enable TransportCC on the offer and answer codecs
  ((JsepVideoCodecDescription&)*mOffCodecs[2]).EnableTransportCC();
  ((JsepVideoCodecDescription&)*mAnsCodecs[2]).EnableTransportCC();
  InitTracks(SdpMediaSection::kVideo);
  InitSdp(SdpMediaSection::kVideo);
  OfferAnswer();

  // make sure TransportCC is on offer and on answer
  ASSERT_NE(mOffer->ToString().find("a=rtcp-fb:120 transport-cc"),
            std::string::npos);
  ASSERT_NE(mAnswer->ToString().find("a=rtcp-fb:120 transport-cc"),
            std::string::npos);
  CheckOffEncodingCount(1);
  CheckAnsEncodingCount(1);

  UniquePtr<JsepVideoCodecDescription> codec;
  ASSERT_TRUE((codec = GetVideoCodec(mSendOff, 2, 0)));
  ASSERT_EQ(codec->mOtherFbTypes.size(), 1U);
  CheckOtherFbExists(*codec, SdpRtcpFbAttributeList::kTransportCC);
  ASSERT_TRUE((codec = GetVideoCodec(mRecvAns, 2, 0)));
  ASSERT_EQ(codec->mOtherFbTypes.size(), 1U);
  CheckOtherFbExists(*codec, SdpRtcpFbAttributeList::kTransportCC);
  ASSERT_TRUE((codec = GetVideoCodec(mSendAns, 2, 0)));
  ASSERT_EQ(codec->mOtherFbTypes.size(), 1U);
  CheckOtherFbExists(*codec, SdpRtcpFbAttributeList::kTransportCC);
  ASSERT_TRUE((codec = GetVideoCodec(mRecvOff, 2, 0)));
  ASSERT_EQ(codec->mOtherFbTypes.size(), 1U);
  CheckOtherFbExists(*codec, SdpRtcpFbAttributeList::kTransportCC);
}

TEST_F(JsepTrackTest, AudioOffSendonlyAnsRecvonly) {
  Init(SdpMediaSection::kAudio);
  GetOffer().SetDirection(SdpDirectionAttribute::kSendonly);
  GetAnswer().SetDirection(SdpDirectionAttribute::kRecvonly);
  OfferAnswer();
  CheckOffEncodingCount(1);
  CheckAnsEncodingCount(0);
}

TEST_F(JsepTrackTest, VideoOffSendonlyAnsRecvonly) {
  Init(SdpMediaSection::kVideo);
  GetOffer().SetDirection(SdpDirectionAttribute::kSendonly);
  GetAnswer().SetDirection(SdpDirectionAttribute::kRecvonly);
  OfferAnswer();
  CheckOffEncodingCount(1);
  CheckAnsEncodingCount(0);
}

TEST_F(JsepTrackTest, AudioOffSendrecvAnsRecvonly) {
  Init(SdpMediaSection::kAudio);
  GetAnswer().SetDirection(SdpDirectionAttribute::kRecvonly);
  OfferAnswer();
  CheckOffEncodingCount(1);
  CheckAnsEncodingCount(0);
}

TEST_F(JsepTrackTest, VideoOffSendrecvAnsRecvonly) {
  Init(SdpMediaSection::kVideo);
  GetAnswer().SetDirection(SdpDirectionAttribute::kRecvonly);
  OfferAnswer();
  CheckOffEncodingCount(1);
  CheckAnsEncodingCount(0);
}

TEST_F(JsepTrackTest, AudioOffRecvonlyAnsSendonly) {
  Init(SdpMediaSection::kAudio);
  GetOffer().SetDirection(SdpDirectionAttribute::kRecvonly);
  GetAnswer().SetDirection(SdpDirectionAttribute::kSendonly);
  OfferAnswer();
  CheckOffEncodingCount(0);
  CheckAnsEncodingCount(1);
}

TEST_F(JsepTrackTest, VideoOffRecvonlyAnsSendonly) {
  Init(SdpMediaSection::kVideo);
  GetOffer().SetDirection(SdpDirectionAttribute::kRecvonly);
  GetAnswer().SetDirection(SdpDirectionAttribute::kSendonly);
  OfferAnswer();
  CheckOffEncodingCount(0);
  CheckAnsEncodingCount(1);
}

TEST_F(JsepTrackTest, AudioOffSendrecvAnsSendonly) {
  Init(SdpMediaSection::kAudio);
  GetAnswer().SetDirection(SdpDirectionAttribute::kSendonly);
  OfferAnswer();
  CheckOffEncodingCount(0);
  CheckAnsEncodingCount(1);
}

TEST_F(JsepTrackTest, VideoOffSendrecvAnsSendonly) {
  Init(SdpMediaSection::kVideo);
  GetAnswer().SetDirection(SdpDirectionAttribute::kSendonly);
  OfferAnswer();
  CheckOffEncodingCount(0);
  CheckAnsEncodingCount(1);
}

TEST_F(JsepTrackTest, DataChannelDraft05) {
  mOffCodecs = MakeCodecs(false, false, false);
  mAnsCodecs = MakeCodecs(false, false, false);
  InitTracks(SdpMediaSection::kApplication);

  mOffer.reset(new SipccSdp(SdpOrigin("", 0, 0, sdp::kIPv4, "")));
  mOffer->AddMediaSection(SdpMediaSection::kApplication,
                          SdpDirectionAttribute::kSendrecv, 0,
                          SdpMediaSection::kDtlsSctp, sdp::kIPv4, "0.0.0.0");
  mAnswer.reset(new SipccSdp(SdpOrigin("", 0, 0, sdp::kIPv4, "")));
  mAnswer->AddMediaSection(SdpMediaSection::kApplication,
                           SdpDirectionAttribute::kSendrecv, 0,
                           SdpMediaSection::kDtlsSctp, sdp::kIPv4, "0.0.0.0");

  OfferAnswer();
  CheckOffEncodingCount(1);
  CheckAnsEncodingCount(1);

  ASSERT_NE(std::string::npos,
            mOffer->ToString().find("a=sctpmap:5999 webrtc-datachannel 256"));
  ASSERT_NE(std::string::npos,
            mAnswer->ToString().find("a=sctpmap:5999 webrtc-datachannel 256"));
  // Note: this is testing for a workaround, see bug 1335262 for details
  ASSERT_NE(std::string::npos,
            mOffer->ToString().find("a=max-message-size:499"));
  ASSERT_NE(std::string::npos,
            mAnswer->ToString().find("a=max-message-size:499"));
  ASSERT_EQ(std::string::npos, mOffer->ToString().find("a=sctp-port"));
  ASSERT_EQ(std::string::npos, mAnswer->ToString().find("a=sctp-port"));
}

TEST_F(JsepTrackTest, DataChannelDraft21) {
  Init(SdpMediaSection::kApplication);
  OfferAnswer();
  CheckOffEncodingCount(1);
  CheckAnsEncodingCount(1);

  ASSERT_NE(std::string::npos, mOffer->ToString().find("a=sctp-port:5999"));
  ASSERT_NE(std::string::npos, mAnswer->ToString().find("a=sctp-port:5999"));
  ASSERT_NE(std::string::npos,
            mOffer->ToString().find("a=max-message-size:499"));
  ASSERT_NE(std::string::npos,
            mAnswer->ToString().find("a=max-message-size:499"));
  ASSERT_EQ(std::string::npos, mOffer->ToString().find("a=sctpmap"));
  ASSERT_EQ(std::string::npos, mAnswer->ToString().find("a=sctpmap"));
}

TEST_F(JsepTrackTest, DataChannelDraft21AnswerWithDifferentPort) {
  mOffCodecs = MakeCodecs(false, false, false);
  mAnsCodecs = MakeCodecs(false, false, false);

  mOffCodecs.pop_back();
  mOffCodecs.emplace_back(new JsepApplicationCodecDescription(
      "webrtc-datachannel", 256, 4555, 10544));

  InitTracks(SdpMediaSection::kApplication);
  InitSdp(SdpMediaSection::kApplication);

  OfferAnswer();
  CheckOffEncodingCount(1);
  CheckAnsEncodingCount(1);

  ASSERT_NE(std::string::npos, mOffer->ToString().find("a=sctp-port:4555"));
  ASSERT_NE(std::string::npos, mAnswer->ToString().find("a=sctp-port:5999"));
  ASSERT_NE(std::string::npos,
            mOffer->ToString().find("a=max-message-size:10544"));
  ASSERT_NE(std::string::npos,
            mAnswer->ToString().find("a=max-message-size:499"));
  ASSERT_EQ(std::string::npos, mOffer->ToString().find("a=sctpmap"));
  ASSERT_EQ(std::string::npos, mAnswer->ToString().find("a=sctpmap"));
}

static JsepTrack::JsConstraints MakeConstraints(const std::string& rid,
                                                uint32_t maxBitrate) {
  JsepTrack::JsConstraints constraints;
  constraints.rid = rid;
  constraints.constraints.maxBr = maxBitrate;
  return constraints;
}

TEST_F(JsepTrackTest, SimulcastRejected) {
  Init(SdpMediaSection::kVideo);
  std::vector<JsepTrack::JsConstraints> constraints;
  constraints.push_back(MakeConstraints("foo", 40000));
  constraints.push_back(MakeConstraints("bar", 10000));
  mSendOff.SetJsConstraints(constraints);
  OfferAnswer();
  CheckOffEncodingCount(1);
  CheckAnsEncodingCount(1);
}

TEST_F(JsepTrackTest, SimulcastPrevented) {
  Init(SdpMediaSection::kVideo);
  std::vector<JsepTrack::JsConstraints> constraints;
  constraints.push_back(MakeConstraints("foo", 40000));
  constraints.push_back(MakeConstraints("bar", 10000));
  mSendAns.SetJsConstraints(constraints);
  OfferAnswer();
  CheckOffEncodingCount(1);
  CheckAnsEncodingCount(1);
}

TEST_F(JsepTrackTest, SimulcastOfferer) {
  Init(SdpMediaSection::kVideo);
  std::vector<JsepTrack::JsConstraints> constraints;
  constraints.push_back(MakeConstraints("foo", 40000));
  constraints.push_back(MakeConstraints("bar", 10000));
  mSendOff.SetJsConstraints(constraints);
  CreateOffer();
  CreateAnswer();
  // Add simulcast/rid to answer
  mRecvAns.AddToMsection(constraints, sdp::kRecv, mSsrcGenerator, false,
                         &GetAnswer());
  Negotiate();
  ASSERT_TRUE(mSendOff.GetNegotiatedDetails());
  ASSERT_EQ(2U, mSendOff.GetNegotiatedDetails()->GetEncodingCount());
  ASSERT_EQ("foo", mSendOff.GetNegotiatedDetails()->GetEncoding(0).mRid);
  ASSERT_EQ(40000U,
            mSendOff.GetNegotiatedDetails()->GetEncoding(0).mConstraints.maxBr);
  ASSERT_EQ("bar", mSendOff.GetNegotiatedDetails()->GetEncoding(1).mRid);
  ASSERT_EQ(10000U,
            mSendOff.GetNegotiatedDetails()->GetEncoding(1).mConstraints.maxBr);
  ASSERT_NE(std::string::npos,
            mOffer->ToString().find("a=simulcast:send foo;bar"));
  ASSERT_NE(std::string::npos,
            mAnswer->ToString().find("a=simulcast:recv foo;bar"));
  ASSERT_NE(std::string::npos, mOffer->ToString().find("a=rid:foo send"));
  ASSERT_NE(std::string::npos, mOffer->ToString().find("a=rid:bar send"));
  ASSERT_NE(std::string::npos, mAnswer->ToString().find("a=rid:foo recv"));
  ASSERT_NE(std::string::npos, mAnswer->ToString().find("a=rid:bar recv"));
}

TEST_F(JsepTrackTest, SimulcastAnswerer) {
  Init(SdpMediaSection::kVideo);
  std::vector<JsepTrack::JsConstraints> constraints;
  constraints.push_back(MakeConstraints("foo", 40000));
  constraints.push_back(MakeConstraints("bar", 10000));
  mSendAns.SetJsConstraints(constraints);
  CreateOffer();
  // Add simulcast/rid to offer
  mRecvOff.AddToMsection(constraints, sdp::kRecv, mSsrcGenerator, false,
                         &GetOffer());
  CreateAnswer();
  Negotiate();
  ASSERT_TRUE(mSendAns.GetNegotiatedDetails());
  ASSERT_EQ(2U, mSendAns.GetNegotiatedDetails()->GetEncodingCount());
  ASSERT_EQ("foo", mSendAns.GetNegotiatedDetails()->GetEncoding(0).mRid);
  ASSERT_EQ(40000U,
            mSendAns.GetNegotiatedDetails()->GetEncoding(0).mConstraints.maxBr);
  ASSERT_EQ("bar", mSendAns.GetNegotiatedDetails()->GetEncoding(1).mRid);
  ASSERT_EQ(10000U,
            mSendAns.GetNegotiatedDetails()->GetEncoding(1).mConstraints.maxBr);
  ASSERT_NE(std::string::npos,
            mOffer->ToString().find("a=simulcast:recv foo;bar"));
  ASSERT_NE(std::string::npos,
            mAnswer->ToString().find("a=simulcast:send foo;bar"));
  ASSERT_NE(std::string::npos, mOffer->ToString().find("a=rid:foo recv"));
  ASSERT_NE(std::string::npos, mOffer->ToString().find("a=rid:bar recv"));
  ASSERT_NE(std::string::npos, mAnswer->ToString().find("a=rid:foo send"));
  ASSERT_NE(std::string::npos, mAnswer->ToString().find("a=rid:bar send"));
}

#define VERIFY_OPUS_MAX_PLAYBACK_RATE(track, expectedRate)          \
  {                                                                 \
    JsepTrack& copy(track);                                         \
    ASSERT_TRUE(copy.GetNegotiatedDetails());                       \
    ASSERT_TRUE(copy.GetNegotiatedDetails()->GetEncodingCount());   \
    for (const auto& codec :                                        \
         copy.GetNegotiatedDetails()->GetEncoding(0).GetCodecs()) { \
      if (codec->mName == "opus") {                                 \
        JsepAudioCodecDescription& audioCodec =                     \
            static_cast<JsepAudioCodecDescription&>(*codec);        \
        ASSERT_EQ((expectedRate), audioCodec.mMaxPlaybackRate);     \
      }                                                             \
    };                                                              \
  }

#define VERIFY_OPUS_FORCE_MONO(track, expected)                       \
  {                                                                   \
    JsepTrack& copy(track);                                           \
    ASSERT_TRUE(copy.GetNegotiatedDetails());                         \
    ASSERT_TRUE(copy.GetNegotiatedDetails()->GetEncodingCount());     \
    for (const auto& codec :                                          \
         copy.GetNegotiatedDetails()->GetEncoding(0).GetCodecs()) {   \
      if (codec->mName == "opus") {                                   \
        JsepAudioCodecDescription& audioCodec =                       \
            static_cast<JsepAudioCodecDescription&>(*codec);          \
        /* gtest has some compiler warnings when using ASSERT_EQ with \
         * booleans. */                                               \
        ASSERT_EQ((int)(expected), (int)audioCodec.mForceMono);       \
      }                                                               \
    };                                                                \
  }

TEST_F(JsepTrackTest, DefaultOpusParameters) {
  Init(SdpMediaSection::kAudio);
  OfferAnswer();

  VERIFY_OPUS_MAX_PLAYBACK_RATE(
      mSendOff, SdpFmtpAttributeList::OpusParameters::kDefaultMaxPlaybackRate);
  VERIFY_OPUS_MAX_PLAYBACK_RATE(
      mSendAns, SdpFmtpAttributeList::OpusParameters::kDefaultMaxPlaybackRate);
  VERIFY_OPUS_MAX_PLAYBACK_RATE(mRecvOff, 0U);
  VERIFY_OPUS_FORCE_MONO(mRecvOff, false);
  VERIFY_OPUS_MAX_PLAYBACK_RATE(mRecvAns, 0U);
  VERIFY_OPUS_FORCE_MONO(mRecvAns, false);
}

TEST_F(JsepTrackTest, NonDefaultOpusParameters) {
  InitCodecs();
  for (auto& codec : mAnsCodecs) {
    if (codec->mName == "opus") {
      JsepAudioCodecDescription* audioCodec =
          static_cast<JsepAudioCodecDescription*>(codec.get());
      audioCodec->mMaxPlaybackRate = 16000;
      audioCodec->mForceMono = true;
    }
  }
  InitTracks(SdpMediaSection::kAudio);
  InitSdp(SdpMediaSection::kAudio);
  OfferAnswer();

  VERIFY_OPUS_MAX_PLAYBACK_RATE(mSendOff, 16000U);
  VERIFY_OPUS_FORCE_MONO(mSendOff, true);
  VERIFY_OPUS_MAX_PLAYBACK_RATE(
      mSendAns, SdpFmtpAttributeList::OpusParameters::kDefaultMaxPlaybackRate);
  VERIFY_OPUS_FORCE_MONO(mSendAns, false);
  VERIFY_OPUS_MAX_PLAYBACK_RATE(mRecvOff, 0U);
  VERIFY_OPUS_FORCE_MONO(mRecvOff, false);
  VERIFY_OPUS_MAX_PLAYBACK_RATE(mRecvAns, 16000U);
  VERIFY_OPUS_FORCE_MONO(mRecvAns, true);
}

}  // namespace mozilla
