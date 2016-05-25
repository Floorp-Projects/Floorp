/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#define GTEST_HAS_RTTI 0
#include "gtest/gtest.h"
#include "gtest_utils.h"

// Magic linker includes :(
#include "FakeMediaStreams.h"
#include "FakeMediaStreamsImpl.h"
#include "FakeLogging.h"

#include "signaling/src/jsep/JsepTrack.h"
#include "signaling/src/sdp/SipccSdp.h"
#include "signaling/src/sdp/SdpHelper.h"

#include "mtransport_test_utils.h"

#include "FakeIPC.h"
#include "FakeIPC.cpp"

#include "TestHarness.h"

namespace mozilla {

class JsepTrackTest : public ::testing::Test
{
  public:
    JsepTrackTest() {}

    std::vector<JsepCodecDescription*>
    MakeCodecs() const
    {
      std::vector<JsepCodecDescription*> results;
      results.push_back(
          new JsepAudioCodecDescription("1", "opus", 48000, 2, 960, 40000));
      results.push_back(
          new JsepAudioCodecDescription("9", "G722", 8000, 1, 320, 64000));

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

      results.push_back(
          new JsepApplicationCodecDescription(
            "5000",
            "webrtc-datachannel",
            16
            ));

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

    const JsepVideoCodecDescription*
    GetVideoCodec(const JsepTrack& track) const
    {
      if (!track.GetNegotiatedDetails() ||
          track.GetNegotiatedDetails()->GetEncodingCount() != 1U) {
        return nullptr;
      }
      const std::vector<JsepCodecDescription*>& codecs =
        track.GetNegotiatedDetails()->GetEncoding(0).GetCodecs();
      if (codecs.size() != 1U ||
          codecs[0]->mType != SdpMediaSection::kVideo) {
        return nullptr;
      }
      return static_cast<const JsepVideoCodecDescription*>(codecs[0]);
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

int
main(int argc, char** argv)
{
  // Prevents some log spew
  ScopedXPCOM xpcom("jsep_track_unittest");

  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

