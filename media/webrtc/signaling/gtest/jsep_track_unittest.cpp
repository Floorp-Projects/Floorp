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

class JsepTrackTest : public ::testing::Test
{
  public:
    JsepTrackTest() {}

    std::vector<JsepCodecDescription*>
    MakeCodecs(bool addFecCodecs = false,
               bool preferRed = false,
               bool addDtmfCodec = false) const
    {
      std::vector<JsepCodecDescription*> results;
      results.push_back(
          new JsepAudioCodecDescription("1", "opus", 48000, 2, 960, 40000));
      results.push_back(
          new JsepAudioCodecDescription("9", "G722", 8000, 1, 320, 64000));
      if (addDtmfCodec) {
        results.push_back(
            new JsepAudioCodecDescription("101", "telephone-event",
                                          8000, 1, 0, 0));
      }

      JsepVideoCodecDescription* red = nullptr;
      if (addFecCodecs && preferRed) {
        red = new JsepVideoCodecDescription(
            "122",
            "red",
            90000
            );
        results.push_back(red);
      }

      JsepVideoCodecDescription* vp8 =
          new JsepVideoCodecDescription("120", "VP8", 90000);
      vp8->mConstraints.maxFs = 12288;
      vp8->mConstraints.maxFps = 60;
      results.push_back(vp8);

      JsepVideoCodecDescription* h264 =
          new JsepVideoCodecDescription("126", "H264", 90000);
      h264->mPacketizationMode = 1;
      h264->mProfileLevelId = 0x42E00D;
      results.push_back(h264);

      if (addFecCodecs) {
        if (!preferRed) {
          red = new JsepVideoCodecDescription(
              "122",
              "red",
              90000
              );
          results.push_back(red);
        }
        JsepVideoCodecDescription* ulpfec = new JsepVideoCodecDescription(
            "123",
            "ulpfec",
            90000
            );
        results.push_back(ulpfec);
      }

      results.push_back(
          new JsepApplicationCodecDescription(
            "5000",
            "webrtc-datachannel",
            16
            ));

      // if we're doing something with red, it needs
      // to update the redundant encodings list
      if (red) {
        red->UpdateRedundantEncodings(results);
      }

      return results;
    }

    void Init(SdpMediaSection::MediaType type) {
      InitCodecs();
      InitTracks(type);
      InitSdp(type);
    }

    void InitCodecs() {
      mOffCodecs.values = MakeCodecs();
      mAnsCodecs.values = MakeCodecs();
    }

    void InitTracks(SdpMediaSection::MediaType type)
    {
      mSendOff = new JsepTrack(type, "stream_id", "track_id", sdp::kSend);
      mRecvOff = new JsepTrack(type, "stream_id", "track_id", sdp::kRecv);
      mSendOff->PopulateCodecs(mOffCodecs.values);
      mRecvOff->PopulateCodecs(mOffCodecs.values);

      mSendAns = new JsepTrack(type, "stream_id", "track_id", sdp::kSend);
      mRecvAns = new JsepTrack(type, "stream_id", "track_id", sdp::kRecv);
      mSendAns->PopulateCodecs(mAnsCodecs.values);
      mRecvAns->PopulateCodecs(mAnsCodecs.values);
    }

    void InitSdp(SdpMediaSection::MediaType type)
    {
      mOffer.reset(new SipccSdp(SdpOrigin("", 0, 0, sdp::kIPv4, "")));
      mOffer->AddMediaSection(
          type,
          SdpDirectionAttribute::kInactive,
          0,
          SdpHelper::GetProtocolForMediaType(type),
          sdp::kIPv4,
          "0.0.0.0");
      mAnswer.reset(new SipccSdp(SdpOrigin("", 0, 0, sdp::kIPv4, "")));
      mAnswer->AddMediaSection(
          type,
          SdpDirectionAttribute::kInactive,
          0,
          SdpHelper::GetProtocolForMediaType(type),
          sdp::kIPv4,
          "0.0.0.0");
    }

    SdpMediaSection& GetOffer()
    {
      return mOffer->GetMediaSection(0);
    }

    SdpMediaSection& GetAnswer()
    {
      return mAnswer->GetMediaSection(0);
    }

    void CreateOffer()
    {
      if (mSendOff) {
        mSendOff->AddToOffer(&GetOffer());
      }

      if (mRecvOff) {
        mRecvOff->AddToOffer(&GetOffer());
      }
    }

    void CreateAnswer()
    {
      if (mSendAns && GetOffer().IsReceiving()) {
        mSendAns->AddToAnswer(GetOffer(), &GetAnswer());
      }

      if (mRecvAns && GetOffer().IsSending()) {
        mRecvAns->AddToAnswer(GetOffer(), &GetAnswer());
      }
    }

    void Negotiate()
    {
      std::cerr << "Offer SDP: " << std::endl;
      mOffer->Serialize(std::cerr);

      std::cerr << "Answer SDP: " << std::endl;
      mAnswer->Serialize(std::cerr);

      if (mSendAns && GetAnswer().IsSending()) {
        mSendAns->Negotiate(GetAnswer(), GetOffer());
      }

      if (mRecvAns && GetAnswer().IsReceiving()) {
        mRecvAns->Negotiate(GetAnswer(), GetOffer());
      }

      if (mSendOff && GetAnswer().IsReceiving()) {
        mSendOff->Negotiate(GetAnswer(), GetAnswer());
      }

      if (mRecvOff && GetAnswer().IsSending()) {
        mRecvOff->Negotiate(GetAnswer(), GetAnswer());
      }
    }

    void OfferAnswer()
    {
      CreateOffer();
      CreateAnswer();
      Negotiate();
      SanityCheck();
    }

    static size_t EncodingCount(const RefPtr<JsepTrack>& track)
    {
      return track->GetNegotiatedDetails()->GetEncodingCount();
    }

    // TODO: Look into writing a macro that wraps an ASSERT_ and returns false
    // if it fails (probably requires writing a bool-returning function that
    // takes a void-returning lambda with a bool outparam, which will in turn
    // invokes the ASSERT_)
    static void CheckEncodingCount(size_t expected,
                                   const RefPtr<JsepTrack>& send,
                                   const RefPtr<JsepTrack>& recv)
    {
      if (expected) {
        ASSERT_TRUE(!!send);
        ASSERT_TRUE(send->GetNegotiatedDetails());
        ASSERT_TRUE(!!recv);
        ASSERT_TRUE(recv->GetNegotiatedDetails());
      }

      if (send && send->GetNegotiatedDetails()) {
        ASSERT_EQ(expected, send->GetNegotiatedDetails()->GetEncodingCount());
      }

      if (recv && recv->GetNegotiatedDetails()) {
        ASSERT_EQ(expected, recv->GetNegotiatedDetails()->GetEncodingCount());
      }
    }

    void CheckOffEncodingCount(size_t expected) const
    {
      CheckEncodingCount(expected, mSendOff, mRecvAns);
    }

    void CheckAnsEncodingCount(size_t expected) const
    {
      CheckEncodingCount(expected, mSendAns, mRecvOff);
    }

    const JsepCodecDescription*
    GetCodec(const JsepTrack& track,
             SdpMediaSection::MediaType type,
             size_t expectedSize,
             size_t codecIndex) const
    {
      if (!track.GetNegotiatedDetails() ||
          track.GetNegotiatedDetails()->GetEncodingCount() != 1U ||
          track.GetMediaType() != type) {
        return nullptr;
      }
      const std::vector<JsepCodecDescription*>& codecs =
        track.GetNegotiatedDetails()->GetEncoding(0).GetCodecs();
      // it should not be possible for codecs to have a different type
      // than the track, but we'll check the codec here just in case.
      if (codecs.size() != expectedSize || codecIndex >= expectedSize ||
          codecs[codecIndex]->mType != type) {
        return nullptr;
      }
      return codecs[codecIndex];
    }

    const JsepVideoCodecDescription*
    GetVideoCodec(const JsepTrack& track,
                  size_t expectedSize = 1,
                  size_t codecIndex = 0) const
    {
      return static_cast<const JsepVideoCodecDescription*>
        (GetCodec(track, SdpMediaSection::kVideo, expectedSize, codecIndex));
    }

    const JsepAudioCodecDescription*
    GetAudioCodec(const JsepTrack& track,
                  size_t expectedSize = 1,
                  size_t codecIndex = 0) const
    {
      return static_cast<const JsepAudioCodecDescription*>
        (GetCodec(track, SdpMediaSection::kAudio, expectedSize, codecIndex));
    }

    void CheckOtherFbsSize(const JsepTrack& track, size_t expected) const
    {
      const JsepVideoCodecDescription* videoCodec = GetVideoCodec(track);
      ASSERT_NE(videoCodec, nullptr);
      ASSERT_EQ(videoCodec->mOtherFbTypes.size(), expected);
    }

    void CheckOtherFbExists(const JsepTrack& track,
                            SdpRtcpFbAttributeList::Type type) const
    {
      const JsepVideoCodecDescription* videoCodec = GetVideoCodec(track);
      ASSERT_NE(videoCodec, nullptr);
      for (const auto& fb : videoCodec->mOtherFbTypes) {
          if (fb.type == type) {
            return; // found the RtcpFb type, so stop looking
          }
      }
      FAIL();  // RtcpFb type not found
    }

    void SanityCheckRtcpFbs(const JsepVideoCodecDescription& a,
                            const JsepVideoCodecDescription& b) const
    {
      ASSERT_EQ(a.mNackFbTypes.size(), b.mNackFbTypes.size());
      ASSERT_EQ(a.mAckFbTypes.size(), b.mAckFbTypes.size());
      ASSERT_EQ(a.mCcmFbTypes.size(), b.mCcmFbTypes.size());
      ASSERT_EQ(a.mOtherFbTypes.size(), b.mOtherFbTypes.size());
    }

    void SanityCheckCodecs(const JsepCodecDescription& a,
                           const JsepCodecDescription& b) const
    {
      ASSERT_EQ(a.mType, b.mType);
      ASSERT_EQ(a.mDefaultPt, b.mDefaultPt);
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
                              const JsepTrackEncoding& b) const
    {
      ASSERT_EQ(a.GetCodecs().size(), b.GetCodecs().size());
      for (size_t i = 0; i < a.GetCodecs().size(); ++i) {
        SanityCheckCodecs(*a.GetCodecs()[i], *b.GetCodecs()[i]);
      }

      ASSERT_EQ(a.mRid, b.mRid);
      // mConstraints will probably differ, since they are not signaled to the
      // other side.
    }

    void SanityCheckNegotiatedDetails(const JsepTrackNegotiatedDetails& a,
                                      const JsepTrackNegotiatedDetails& b) const
    {
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

    void SanityCheckTracks(const JsepTrack& a, const JsepTrack& b) const
    {
      if (!a.GetNegotiatedDetails()) {
        ASSERT_FALSE(!!b.GetNegotiatedDetails());
        return;
      }

      ASSERT_TRUE(!!a.GetNegotiatedDetails());
      ASSERT_TRUE(!!b.GetNegotiatedDetails());
      ASSERT_EQ(a.GetMediaType(), b.GetMediaType());
      ASSERT_EQ(a.GetStreamId(), b.GetStreamId());
      ASSERT_EQ(a.GetTrackId(), b.GetTrackId());
      ASSERT_EQ(a.GetCNAME(), b.GetCNAME());
      ASSERT_NE(a.GetDirection(), b.GetDirection());
      ASSERT_EQ(a.GetSsrcs().size(), b.GetSsrcs().size());
      for (size_t i = 0; i < a.GetSsrcs().size(); ++i) {
        ASSERT_EQ(a.GetSsrcs()[i], b.GetSsrcs()[i]);
      }

      SanityCheckNegotiatedDetails(*a.GetNegotiatedDetails(),
                                   *b.GetNegotiatedDetails());
    }

    void SanityCheck() const
    {
      if (mSendOff && mRecvAns) {
        SanityCheckTracks(*mSendOff, *mRecvAns);
      }
      if (mRecvOff && mSendAns) {
        SanityCheckTracks(*mRecvOff, *mSendAns);
      }
    }

  protected:
    RefPtr<JsepTrack> mSendOff;
    RefPtr<JsepTrack> mRecvOff;
    RefPtr<JsepTrack> mSendAns;
    RefPtr<JsepTrack> mRecvAns;
    PtrVector<JsepCodecDescription> mOffCodecs;
    PtrVector<JsepCodecDescription> mAnsCodecs;
    UniquePtr<Sdp> mOffer;
    UniquePtr<Sdp> mAnswer;
};

TEST_F(JsepTrackTest, CreateDestroy)
{
  Init(SdpMediaSection::kAudio);
}

TEST_F(JsepTrackTest, AudioNegotiation)
{
  Init(SdpMediaSection::kAudio);
  OfferAnswer();
  CheckOffEncodingCount(1);
  CheckAnsEncodingCount(1);
}

TEST_F(JsepTrackTest, VideoNegotiation)
{
  Init(SdpMediaSection::kVideo);
  OfferAnswer();
  CheckOffEncodingCount(1);
  CheckAnsEncodingCount(1);
}

class CheckForCodecType
{
public:
  explicit CheckForCodecType(SdpMediaSection::MediaType type,
                             bool *result) :
    mResult(result),
    mType(type) {}

  void operator()(JsepCodecDescription* codec) {
    if (codec->mType == mType) {
      *mResult = true;
    }
  }

private:
  bool *mResult;
  SdpMediaSection::MediaType mType;
};

TEST_F(JsepTrackTest, CheckForMismatchedAudioCodecAndVideoTrack)
{
  PtrVector<JsepCodecDescription> offerCodecs;

  // make codecs including telephone-event (an audio codec)
  offerCodecs.values = MakeCodecs(false, false, true);
  RefPtr<JsepTrack> videoTrack = new JsepTrack(SdpMediaSection::kVideo,
                                               "stream_id",
                                               "track_id",
                                               sdp::kSend);
  // populate codecs and then make sure we don't have any audio codecs
  // in the video track
  videoTrack->PopulateCodecs(offerCodecs.values);

  bool found = false;
  videoTrack->ForEachCodec(CheckForCodecType(SdpMediaSection::kAudio, &found));
  ASSERT_FALSE(found);

  found = false;
  videoTrack->ForEachCodec(CheckForCodecType(SdpMediaSection::kVideo, &found));
  ASSERT_TRUE(found); // for sanity, make sure we did find video codecs
}

TEST_F(JsepTrackTest, CheckVideoTrackWithHackedDtmfSdp)
{
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

  ASSERT_TRUE(mSendOff.get());
  ASSERT_TRUE(mRecvOff.get());
  ASSERT_TRUE(mSendAns.get());
  ASSERT_TRUE(mRecvAns.get());

  // make sure we still don't find any audio codecs in the video track after
  // hacking the sdp
  bool found = false;
  mSendOff->ForEachCodec(CheckForCodecType(SdpMediaSection::kAudio, &found));
  ASSERT_FALSE(found);
  mRecvOff->ForEachCodec(CheckForCodecType(SdpMediaSection::kAudio, &found));
  ASSERT_FALSE(found);
  mSendAns->ForEachCodec(CheckForCodecType(SdpMediaSection::kAudio, &found));
  ASSERT_FALSE(found);
  mRecvAns->ForEachCodec(CheckForCodecType(SdpMediaSection::kAudio, &found));
  ASSERT_FALSE(found);
}

TEST_F(JsepTrackTest, AudioNegotiationOffererDtmf)
{
  mOffCodecs.values = MakeCodecs(false, false, true);
  mAnsCodecs.values = MakeCodecs(false, false, false);

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

  const JsepAudioCodecDescription* track = nullptr;
  ASSERT_TRUE((track = GetAudioCodec(*mSendOff)));
  ASSERT_EQ("1", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(*mRecvOff)));
  ASSERT_EQ("1", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(*mSendAns)));
  ASSERT_EQ("1", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(*mRecvAns)));
  ASSERT_EQ("1", track->mDefaultPt);
}

TEST_F(JsepTrackTest, AudioNegotiationAnswererDtmf)
{
  mOffCodecs.values = MakeCodecs(false, false, false);
  mAnsCodecs.values = MakeCodecs(false, false, true);

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

  const JsepAudioCodecDescription* track = nullptr;
  ASSERT_TRUE((track = GetAudioCodec(*mSendOff)));
  ASSERT_EQ("1", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(*mRecvOff)));
  ASSERT_EQ("1", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(*mSendAns)));
  ASSERT_EQ("1", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(*mRecvAns)));
  ASSERT_EQ("1", track->mDefaultPt);
}

TEST_F(JsepTrackTest, AudioNegotiationOffererAnswererDtmf)
{
  mOffCodecs.values = MakeCodecs(false, false, true);
  mAnsCodecs.values = MakeCodecs(false, false, true);

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

  const JsepAudioCodecDescription* track = nullptr;
  ASSERT_TRUE((track = GetAudioCodec(*mSendOff, 2)));
  ASSERT_EQ("1", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(*mRecvOff, 2)));
  ASSERT_EQ("1", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(*mSendAns, 2)));
  ASSERT_EQ("1", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(*mRecvAns, 2)));
  ASSERT_EQ("1", track->mDefaultPt);

  ASSERT_TRUE((track = GetAudioCodec(*mSendOff, 2, 1)));
  ASSERT_EQ("101", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(*mRecvOff, 2, 1)));
  ASSERT_EQ("101", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(*mSendAns, 2, 1)));
  ASSERT_EQ("101", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(*mRecvAns, 2, 1)));
  ASSERT_EQ("101", track->mDefaultPt);
}

TEST_F(JsepTrackTest, AudioNegotiationDtmfOffererNoFmtpAnswererFmtp)
{
  mOffCodecs.values = MakeCodecs(false, false, true);
  mAnsCodecs.values = MakeCodecs(false, false, true);

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

  const JsepAudioCodecDescription* track = nullptr;
  ASSERT_TRUE((track = GetAudioCodec(*mSendOff, 2)));
  ASSERT_EQ("1", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(*mRecvOff, 2)));
  ASSERT_EQ("1", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(*mSendAns, 2)));
  ASSERT_EQ("1", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(*mRecvAns, 2)));
  ASSERT_EQ("1", track->mDefaultPt);

  ASSERT_TRUE((track = GetAudioCodec(*mSendOff, 2, 1)));
  ASSERT_EQ("101", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(*mRecvOff, 2, 1)));
  ASSERT_EQ("101", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(*mSendAns, 2, 1)));
  ASSERT_EQ("101", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(*mRecvAns, 2, 1)));
  ASSERT_EQ("101", track->mDefaultPt);
}

TEST_F(JsepTrackTest, AudioNegotiationDtmfOffererFmtpAnswererNoFmtp)
{
  mOffCodecs.values = MakeCodecs(false, false, true);
  mAnsCodecs.values = MakeCodecs(false, false, true);

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

  const JsepAudioCodecDescription* track = nullptr;
  ASSERT_TRUE((track = GetAudioCodec(*mSendOff, 2)));
  ASSERT_EQ("1", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(*mRecvOff, 2)));
  ASSERT_EQ("1", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(*mSendAns, 2)));
  ASSERT_EQ("1", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(*mRecvAns, 2)));
  ASSERT_EQ("1", track->mDefaultPt);

  ASSERT_TRUE((track = GetAudioCodec(*mSendOff, 2, 1)));
  ASSERT_EQ("101", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(*mRecvOff, 2, 1)));
  ASSERT_EQ("101", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(*mSendAns, 2, 1)));
  ASSERT_EQ("101", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(*mRecvAns, 2, 1)));
  ASSERT_EQ("101", track->mDefaultPt);
}

TEST_F(JsepTrackTest, AudioNegotiationDtmfOffererNoFmtpAnswererNoFmtp)
{
  mOffCodecs.values = MakeCodecs(false, false, true);
  mAnsCodecs.values = MakeCodecs(false, false, true);

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

  const JsepAudioCodecDescription* track = nullptr;
  ASSERT_TRUE((track = GetAudioCodec(*mSendOff, 2)));
  ASSERT_EQ("1", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(*mRecvOff, 2)));
  ASSERT_EQ("1", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(*mSendAns, 2)));
  ASSERT_EQ("1", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(*mRecvAns, 2)));
  ASSERT_EQ("1", track->mDefaultPt);

  ASSERT_TRUE((track = GetAudioCodec(*mSendOff, 2, 1)));
  ASSERT_EQ("101", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(*mRecvOff, 2, 1)));
  ASSERT_EQ("101", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(*mSendAns, 2, 1)));
  ASSERT_EQ("101", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(*mRecvAns, 2, 1)));
  ASSERT_EQ("101", track->mDefaultPt);
}

TEST_F(JsepTrackTest, VideoNegotationOffererFEC)
{
  mOffCodecs.values = MakeCodecs(true);
  mAnsCodecs.values = MakeCodecs(false);

  InitTracks(SdpMediaSection::kVideo);
  InitSdp(SdpMediaSection::kVideo);
  OfferAnswer();

  CheckOffEncodingCount(1);
  CheckAnsEncodingCount(1);

  ASSERT_NE(mOffer->ToString().find("a=rtpmap:122 red"), std::string::npos);
  ASSERT_NE(mOffer->ToString().find("a=rtpmap:123 ulpfec"), std::string::npos);
  ASSERT_EQ(mAnswer->ToString().find("a=rtpmap:122 red"), std::string::npos);
  ASSERT_EQ(mAnswer->ToString().find("a=rtpmap:123 ulpfec"), std::string::npos);

  ASSERT_NE(mOffer->ToString().find("a=fmtp:122 120/126/123"), std::string::npos);
  ASSERT_EQ(mAnswer->ToString().find("a=fmtp:122"), std::string::npos);

  const JsepVideoCodecDescription* track = nullptr;
  ASSERT_TRUE((track = GetVideoCodec(*mSendOff)));
  ASSERT_EQ("120", track->mDefaultPt);
  ASSERT_TRUE((track = GetVideoCodec(*mRecvOff)));
  ASSERT_EQ("120", track->mDefaultPt);
  ASSERT_TRUE((track = GetVideoCodec(*mSendAns)));
  ASSERT_EQ("120", track->mDefaultPt);
  ASSERT_TRUE((track = GetVideoCodec(*mRecvAns)));
  ASSERT_EQ("120", track->mDefaultPt);
}

TEST_F(JsepTrackTest, VideoNegotationAnswererFEC)
{
  mOffCodecs.values = MakeCodecs(false);
  mAnsCodecs.values = MakeCodecs(true);

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

  const JsepVideoCodecDescription* track = nullptr;
  ASSERT_TRUE((track = GetVideoCodec(*mSendOff)));
  ASSERT_EQ("120", track->mDefaultPt);
  ASSERT_TRUE((track = GetVideoCodec(*mRecvOff)));
  ASSERT_EQ("120", track->mDefaultPt);
  ASSERT_TRUE((track = GetVideoCodec(*mSendAns)));
  ASSERT_EQ("120", track->mDefaultPt);
  ASSERT_TRUE((track = GetVideoCodec(*mRecvAns)));
  ASSERT_EQ("120", track->mDefaultPt);
}

TEST_F(JsepTrackTest, VideoNegotationOffererAnswererFEC)
{
  mOffCodecs.values = MakeCodecs(true);
  mAnsCodecs.values = MakeCodecs(true);

  InitTracks(SdpMediaSection::kVideo);
  InitSdp(SdpMediaSection::kVideo);
  OfferAnswer();

  CheckOffEncodingCount(1);
  CheckAnsEncodingCount(1);

  ASSERT_NE(mOffer->ToString().find("a=rtpmap:122 red"), std::string::npos);
  ASSERT_NE(mOffer->ToString().find("a=rtpmap:123 ulpfec"), std::string::npos);
  ASSERT_NE(mAnswer->ToString().find("a=rtpmap:122 red"), std::string::npos);
  ASSERT_NE(mAnswer->ToString().find("a=rtpmap:123 ulpfec"), std::string::npos);

  ASSERT_NE(mOffer->ToString().find("a=fmtp:122 120/126/123"), std::string::npos);
  ASSERT_NE(mAnswer->ToString().find("a=fmtp:122 120/126/123"), std::string::npos);

  const JsepVideoCodecDescription* track = nullptr;
  ASSERT_TRUE((track = GetVideoCodec(*mSendOff, 4)));
  ASSERT_EQ("120", track->mDefaultPt);
  ASSERT_TRUE((track = GetVideoCodec(*mRecvOff, 4)));
  ASSERT_EQ("120", track->mDefaultPt);
  ASSERT_TRUE((track = GetVideoCodec(*mSendAns, 4)));
  ASSERT_EQ("120", track->mDefaultPt);
  ASSERT_TRUE((track = GetVideoCodec(*mRecvAns, 4)));
  ASSERT_EQ("120", track->mDefaultPt);
}

TEST_F(JsepTrackTest, VideoNegotationOffererAnswererFECPreferred)
{
  mOffCodecs.values = MakeCodecs(true, true);
  mAnsCodecs.values = MakeCodecs(true);

  InitTracks(SdpMediaSection::kVideo);
  InitSdp(SdpMediaSection::kVideo);
  OfferAnswer();

  CheckOffEncodingCount(1);
  CheckAnsEncodingCount(1);

  ASSERT_NE(mOffer->ToString().find("a=rtpmap:122 red"), std::string::npos);
  ASSERT_NE(mOffer->ToString().find("a=rtpmap:123 ulpfec"), std::string::npos);
  ASSERT_NE(mAnswer->ToString().find("a=rtpmap:122 red"), std::string::npos);
  ASSERT_NE(mAnswer->ToString().find("a=rtpmap:123 ulpfec"), std::string::npos);

  ASSERT_NE(mOffer->ToString().find("a=fmtp:122 120/126/123"), std::string::npos);
  ASSERT_NE(mAnswer->ToString().find("a=fmtp:122 120/126/123"), std::string::npos);

  const JsepVideoCodecDescription* track = nullptr;
  ASSERT_TRUE((track = GetVideoCodec(*mSendOff, 4)));
  ASSERT_EQ("122", track->mDefaultPt);
  ASSERT_TRUE((track = GetVideoCodec(*mRecvOff, 4)));
  ASSERT_EQ("122", track->mDefaultPt);
  ASSERT_TRUE((track = GetVideoCodec(*mSendAns, 4)));
  ASSERT_EQ("122", track->mDefaultPt);
  ASSERT_TRUE((track = GetVideoCodec(*mRecvAns, 4)));
  ASSERT_EQ("122", track->mDefaultPt);
}

// Make sure we only put the right things in the fmtp:122 120/.... line
TEST_F(JsepTrackTest, VideoNegotationOffererAnswererFECMismatch)
{
  mOffCodecs.values = MakeCodecs(true, true);
  mAnsCodecs.values = MakeCodecs(true);
  // remove h264 from answer codecs
  ASSERT_EQ("H264", mAnsCodecs.values[3]->mName);
  mAnsCodecs.values.erase(mAnsCodecs.values.begin()+3);

  InitTracks(SdpMediaSection::kVideo);
  InitSdp(SdpMediaSection::kVideo);
  OfferAnswer();

  CheckOffEncodingCount(1);
  CheckAnsEncodingCount(1);

  ASSERT_NE(mOffer->ToString().find("a=rtpmap:122 red"), std::string::npos);
  ASSERT_NE(mOffer->ToString().find("a=rtpmap:123 ulpfec"), std::string::npos);
  ASSERT_NE(mAnswer->ToString().find("a=rtpmap:122 red"), std::string::npos);
  ASSERT_NE(mAnswer->ToString().find("a=rtpmap:123 ulpfec"), std::string::npos);

  ASSERT_NE(mOffer->ToString().find("a=fmtp:122 120/126/123"), std::string::npos);
  ASSERT_NE(mAnswer->ToString().find("a=fmtp:122 120/123"), std::string::npos);

  const JsepVideoCodecDescription* track = nullptr;
  ASSERT_TRUE((track = GetVideoCodec(*mSendOff, 3)));
  ASSERT_EQ("122", track->mDefaultPt);
  ASSERT_TRUE((track = GetVideoCodec(*mRecvOff, 3)));
  ASSERT_EQ("122", track->mDefaultPt);
  ASSERT_TRUE((track = GetVideoCodec(*mSendAns, 3)));
  ASSERT_EQ("122", track->mDefaultPt);
  ASSERT_TRUE((track = GetVideoCodec(*mRecvAns, 3)));
  ASSERT_EQ("122", track->mDefaultPt);
}

TEST_F(JsepTrackTest, VideoNegotationOffererAnswererFECZeroVP9Codec)
{
  mOffCodecs.values = MakeCodecs(true);
  JsepVideoCodecDescription* vp9 =
    new JsepVideoCodecDescription("0", "VP9", 90000);
  vp9->mConstraints.maxFs = 12288;
  vp9->mConstraints.maxFps = 60;
  mOffCodecs.values.push_back(vp9);

  ASSERT_EQ(8U, mOffCodecs.values.size());
  JsepVideoCodecDescription* red =
      static_cast<JsepVideoCodecDescription*>(mOffCodecs.values[4]);
  ASSERT_EQ("red", red->mName);
  // rebuild the redundant encodings with our newly added "wacky" VP9
  red->mRedundantEncodings.clear();
  red->UpdateRedundantEncodings(mOffCodecs.values);

  mAnsCodecs.values = MakeCodecs(true);

  InitTracks(SdpMediaSection::kVideo);
  InitSdp(SdpMediaSection::kVideo);
  OfferAnswer();

  CheckOffEncodingCount(1);
  CheckAnsEncodingCount(1);

  ASSERT_NE(mOffer->ToString().find("a=rtpmap:122 red"), std::string::npos);
  ASSERT_NE(mOffer->ToString().find("a=rtpmap:123 ulpfec"), std::string::npos);
  ASSERT_NE(mAnswer->ToString().find("a=rtpmap:122 red"), std::string::npos);
  ASSERT_NE(mAnswer->ToString().find("a=rtpmap:123 ulpfec"), std::string::npos);

  ASSERT_NE(mOffer->ToString().find("a=fmtp:122 120/126/123/0"), std::string::npos);
  ASSERT_NE(mAnswer->ToString().find("a=fmtp:122 120/126/123\r\n"), std::string::npos);
}

TEST_F(JsepTrackTest, VideoNegotiationOfferRemb)
{
  InitCodecs();
  // enable remb on the offer codecs
  ((JsepVideoCodecDescription*)mOffCodecs.values[2])->EnableRemb();
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

  CheckOtherFbsSize(*mSendOff, 0);
  CheckOtherFbsSize(*mRecvAns, 0);

  CheckOtherFbsSize(*mSendAns, 0);
  CheckOtherFbsSize(*mRecvOff, 0);
}

TEST_F(JsepTrackTest, VideoNegotiationAnswerRemb)
{
  InitCodecs();
  // enable remb on the answer codecs
  ((JsepVideoCodecDescription*)mAnsCodecs.values[2])->EnableRemb();
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

  CheckOtherFbsSize(*mSendOff, 0);
  CheckOtherFbsSize(*mRecvAns, 0);

  CheckOtherFbsSize(*mSendAns, 0);
  CheckOtherFbsSize(*mRecvOff, 0);
}

TEST_F(JsepTrackTest, VideoNegotiationOfferAnswerRemb)
{
  InitCodecs();
  // enable remb on the offer and answer codecs
  ((JsepVideoCodecDescription*)mOffCodecs.values[2])->EnableRemb();
  ((JsepVideoCodecDescription*)mAnsCodecs.values[2])->EnableRemb();
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

  CheckOtherFbsSize(*mSendOff, 1);
  CheckOtherFbsSize(*mRecvAns, 1);
  CheckOtherFbExists(*mSendOff, SdpRtcpFbAttributeList::kRemb);
  CheckOtherFbExists(*mRecvAns, SdpRtcpFbAttributeList::kRemb);

  CheckOtherFbsSize(*mSendAns, 1);
  CheckOtherFbsSize(*mRecvOff, 1);
  CheckOtherFbExists(*mSendAns, SdpRtcpFbAttributeList::kRemb);
  CheckOtherFbExists(*mRecvOff, SdpRtcpFbAttributeList::kRemb);
}

TEST_F(JsepTrackTest, AudioOffSendonlyAnsRecvonly)
{
  Init(SdpMediaSection::kAudio);
  mRecvOff = nullptr;
  mSendAns = nullptr;
  OfferAnswer();
  CheckOffEncodingCount(1);
  CheckAnsEncodingCount(0);
}

TEST_F(JsepTrackTest, VideoOffSendonlyAnsRecvonly)
{
  Init(SdpMediaSection::kVideo);
  mRecvOff = nullptr;
  mSendAns = nullptr;
  OfferAnswer();
  CheckOffEncodingCount(1);
  CheckAnsEncodingCount(0);
}

TEST_F(JsepTrackTest, AudioOffSendrecvAnsRecvonly)
{
  Init(SdpMediaSection::kAudio);
  mSendAns = nullptr;
  OfferAnswer();
  CheckOffEncodingCount(1);
  CheckAnsEncodingCount(0);
}

TEST_F(JsepTrackTest, VideoOffSendrecvAnsRecvonly)
{
  Init(SdpMediaSection::kVideo);
  mSendAns = nullptr;
  OfferAnswer();
  CheckOffEncodingCount(1);
  CheckAnsEncodingCount(0);
}

TEST_F(JsepTrackTest, AudioOffRecvonlyAnsSendrecv)
{
  Init(SdpMediaSection::kAudio);
  mSendOff = nullptr;
  OfferAnswer();
  CheckOffEncodingCount(0);
  CheckAnsEncodingCount(1);
}

TEST_F(JsepTrackTest, VideoOffRecvonlyAnsSendrecv)
{
  Init(SdpMediaSection::kVideo);
  mSendOff = nullptr;
  OfferAnswer();
  CheckOffEncodingCount(0);
  CheckAnsEncodingCount(1);
}

TEST_F(JsepTrackTest, AudioOffSendrecvAnsSendonly)
{
  Init(SdpMediaSection::kAudio);
  mRecvAns = nullptr;
  OfferAnswer();
  CheckOffEncodingCount(0);
  CheckAnsEncodingCount(1);
}

TEST_F(JsepTrackTest, VideoOffSendrecvAnsSendonly)
{
  Init(SdpMediaSection::kVideo);
  mRecvAns = nullptr;
  OfferAnswer();
  CheckOffEncodingCount(0);
  CheckAnsEncodingCount(1);
}

static JsepTrack::JsConstraints
MakeConstraints(const std::string& rid, uint32_t maxBitrate)
{
  JsepTrack::JsConstraints constraints;
  constraints.rid = rid;
  constraints.constraints.maxBr = maxBitrate;
  return constraints;
}

TEST_F(JsepTrackTest, SimulcastRejected)
{
  Init(SdpMediaSection::kVideo);
  std::vector<JsepTrack::JsConstraints> constraints;
  constraints.push_back(MakeConstraints("foo", 40000));
  constraints.push_back(MakeConstraints("bar", 10000));
  mSendOff->SetJsConstraints(constraints);
  OfferAnswer();
  CheckOffEncodingCount(1);
  CheckAnsEncodingCount(1);
}

TEST_F(JsepTrackTest, SimulcastPrevented)
{
  Init(SdpMediaSection::kVideo);
  std::vector<JsepTrack::JsConstraints> constraints;
  constraints.push_back(MakeConstraints("foo", 40000));
  constraints.push_back(MakeConstraints("bar", 10000));
  mSendAns->SetJsConstraints(constraints);
  OfferAnswer();
  CheckOffEncodingCount(1);
  CheckAnsEncodingCount(1);
}

TEST_F(JsepTrackTest, SimulcastOfferer)
{
  Init(SdpMediaSection::kVideo);
  std::vector<JsepTrack::JsConstraints> constraints;
  constraints.push_back(MakeConstraints("foo", 40000));
  constraints.push_back(MakeConstraints("bar", 10000));
  mSendOff->SetJsConstraints(constraints);
  CreateOffer();
  CreateAnswer();
  // Add simulcast/rid to answer
  JsepTrack::AddToMsection(constraints, sdp::kRecv, &GetAnswer());
  Negotiate();
  ASSERT_TRUE(mSendOff->GetNegotiatedDetails());
  ASSERT_EQ(2U, mSendOff->GetNegotiatedDetails()->GetEncodingCount());
  ASSERT_EQ("foo", mSendOff->GetNegotiatedDetails()->GetEncoding(0).mRid);
  ASSERT_EQ(40000U,
      mSendOff->GetNegotiatedDetails()->GetEncoding(0).mConstraints.maxBr);
  ASSERT_EQ("bar", mSendOff->GetNegotiatedDetails()->GetEncoding(1).mRid);
  ASSERT_EQ(10000U,
      mSendOff->GetNegotiatedDetails()->GetEncoding(1).mConstraints.maxBr);
}

TEST_F(JsepTrackTest, SimulcastAnswerer)
{
  Init(SdpMediaSection::kVideo);
  std::vector<JsepTrack::JsConstraints> constraints;
  constraints.push_back(MakeConstraints("foo", 40000));
  constraints.push_back(MakeConstraints("bar", 10000));
  mSendAns->SetJsConstraints(constraints);
  CreateOffer();
  // Add simulcast/rid to offer
  JsepTrack::AddToMsection(constraints, sdp::kRecv, &GetOffer());
  CreateAnswer();
  Negotiate();
  ASSERT_TRUE(mSendAns->GetNegotiatedDetails());
  ASSERT_EQ(2U, mSendAns->GetNegotiatedDetails()->GetEncodingCount());
  ASSERT_EQ("foo", mSendAns->GetNegotiatedDetails()->GetEncoding(0).mRid);
  ASSERT_EQ(40000U,
      mSendAns->GetNegotiatedDetails()->GetEncoding(0).mConstraints.maxBr);
  ASSERT_EQ("bar", mSendAns->GetNegotiatedDetails()->GetEncoding(1).mRid);
  ASSERT_EQ(10000U,
      mSendAns->GetNegotiatedDetails()->GetEncoding(1).mConstraints.maxBr);
}

#define VERIFY_OPUS_MAX_PLAYBACK_RATE(track, expectedRate)  \
{  \
  JsepTrack& copy(track); \
  ASSERT_TRUE(copy.GetNegotiatedDetails());  \
  ASSERT_TRUE(copy.GetNegotiatedDetails()->GetEncodingCount());  \
  for (auto codec : copy.GetNegotiatedDetails()->GetEncoding(0).GetCodecs()) {\
    if (codec->mName == "opus") {  \
      JsepAudioCodecDescription* audioCodec =  \
        static_cast<JsepAudioCodecDescription*>(codec);  \
      ASSERT_EQ((expectedRate), audioCodec->mMaxPlaybackRate);  \
    }  \
  };  \
}

#define VERIFY_OPUS_FORCE_MONO(track, expected) \
{ \
  JsepTrack& copy(track); \
  ASSERT_TRUE(copy.GetNegotiatedDetails());  \
  ASSERT_TRUE(copy.GetNegotiatedDetails()->GetEncodingCount());  \
  for (auto codec : copy.GetNegotiatedDetails()->GetEncoding(0).GetCodecs()) {\
    if (codec->mName == "opus") {  \
      JsepAudioCodecDescription* audioCodec =  \
        static_cast<JsepAudioCodecDescription*>(codec);  \
      /* gtest has some compiler warnings when using ASSERT_EQ with booleans. */ \
      ASSERT_EQ((int)(expected), (int)audioCodec->mForceMono);  \
    }  \
  };  \
}

TEST_F(JsepTrackTest, DefaultOpusParameters)
{
  Init(SdpMediaSection::kAudio);
  OfferAnswer();

  VERIFY_OPUS_MAX_PLAYBACK_RATE(*mSendOff,
      SdpFmtpAttributeList::OpusParameters::kDefaultMaxPlaybackRate);
  VERIFY_OPUS_MAX_PLAYBACK_RATE(*mSendAns,
      SdpFmtpAttributeList::OpusParameters::kDefaultMaxPlaybackRate);
  VERIFY_OPUS_MAX_PLAYBACK_RATE(*mRecvOff, 0U);
  VERIFY_OPUS_FORCE_MONO(*mRecvOff, false);
  VERIFY_OPUS_MAX_PLAYBACK_RATE(*mRecvAns, 0U);
  VERIFY_OPUS_FORCE_MONO(*mRecvAns, false);
}

TEST_F(JsepTrackTest, NonDefaultOpusParameters)
{
  InitCodecs();
  for (auto& codec : mAnsCodecs.values) {
    if (codec->mName == "opus") {
      JsepAudioCodecDescription* audioCodec =
        static_cast<JsepAudioCodecDescription*>(codec);
      audioCodec->mMaxPlaybackRate = 16000;
      audioCodec->mForceMono = true;
    }
  }
  InitTracks(SdpMediaSection::kAudio);
  InitSdp(SdpMediaSection::kAudio);
  OfferAnswer();

  VERIFY_OPUS_MAX_PLAYBACK_RATE(*mSendOff, 16000U);
  VERIFY_OPUS_FORCE_MONO(*mSendOff, true);
  VERIFY_OPUS_MAX_PLAYBACK_RATE(*mSendAns,
      SdpFmtpAttributeList::OpusParameters::kDefaultMaxPlaybackRate);
  VERIFY_OPUS_FORCE_MONO(*mSendAns, false);
  VERIFY_OPUS_MAX_PLAYBACK_RATE(*mRecvOff, 0U);
  VERIFY_OPUS_FORCE_MONO(*mRecvOff, false);
  VERIFY_OPUS_MAX_PLAYBACK_RATE(*mRecvAns, 16000U);
  VERIFY_OPUS_FORCE_MONO(*mRecvAns, true);
}

} // namespace mozilla
