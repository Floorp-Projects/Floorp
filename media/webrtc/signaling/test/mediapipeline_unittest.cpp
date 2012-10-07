/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Original author: ekr@rtfm.com

#include <iostream>

#include "sigslot.h"

#include "nsThreadUtils.h"
#include "nsXPCOM.h"
#include "ssl.h"
#include "sslproto.h"

#include "dtlsidentity.h"
#include "logging.h"
#include "mozilla/RefPtr.h"
#include "FakeMediaStreams.h"
#include "FakeMediaStreamsImpl.h"
#include "MediaConduitErrors.h"
#include "MediaConduitInterface.h"
#include "MediaPipeline.h"
#include "runnable_utils.h"
#include "transportflow.h"
#include "transportlayerprsock.h"
#include "transportlayerdtls.h"


#include "mtransport_test_utils.h"
#include "runnable_utils.h"

#define GTEST_HAS_RTTI 0
#include "gtest/gtest.h"
#include "gtest_utils.h"

using namespace mozilla;
MOZ_MTLOG_MODULE("mediapipeline");

MtransportTestUtils test_utils;


namespace {
class TestAgent {
 public:
  TestAgent() :
      audio_flow_(new TransportFlow()),
      audio_prsock_(new TransportLayerPrsock()),
      audio_dtls_(new TransportLayerDtls()),
      audio_config_(109, "opus", 48000, 480, 1, 64000),
      audio_conduit_(mozilla::AudioSessionConduit::Create()),
      audio_(),
      audio_pipeline_(),
      video_flow_(new TransportFlow()),
      video_prsock_(new TransportLayerPrsock()),
      video_config_(120, "VP8", 640, 480),
      video_conduit_(mozilla::VideoSessionConduit::Create()),
      video_(),
      video_pipeline_() {
  }

  void ConnectSocket(PRFileDesc *fd, bool client) {
    nsresult res;
    res = audio_prsock_->Init();
    ASSERT_EQ((nsresult)NS_OK, res);

    test_utils.sts_target()->Dispatch(WrapRunnable(audio_prsock_, &TransportLayerPrsock::Import,
                                   fd, &res), NS_DISPATCH_SYNC);
    ASSERT_TRUE(NS_SUCCEEDED(res));

    ASSERT_EQ((nsresult)NS_OK, audio_flow_->PushLayer(audio_prsock_));

    std::vector<PRUint16> ciphers;
    ciphers.push_back(SRTP_AES128_CM_HMAC_SHA1_80);
    audio_dtls_->SetSrtpCiphers(ciphers);
    audio_dtls_->SetIdentity(DtlsIdentity::Generate());
    audio_dtls_->SetRole(client ? TransportLayerDtls::CLIENT :
                         TransportLayerDtls::SERVER);
    audio_flow_->PushLayer(audio_dtls_);
  }

  void Start() {
    nsresult ret;

    MOZ_MTLOG(PR_LOG_DEBUG, "Starting");

    test_utils.sts_target()->Dispatch(
        WrapRunnableRet(audio_->GetStream(),
                        &Fake_MediaStream::Start, &ret),
        NS_DISPATCH_SYNC);
    ASSERT_TRUE(NS_SUCCEEDED(ret));
  }

  void StopInt() {
    audio_->GetStream()->Stop();
    audio_flow_ = NULL;
    video_flow_ = NULL;
    audio_pipeline_ = NULL;
    video_pipeline_ = NULL;
  }

  void Stop() {
    MOZ_MTLOG(PR_LOG_DEBUG, "Stopping");

    test_utils.sts_target()->Dispatch(
        WrapRunnable(this, &TestAgent::StopInt),
        NS_DISPATCH_SYNC);
    
    PR_Sleep(1000); // Deal with race condition
  }


 protected:
  mozilla::RefPtr<TransportFlow> audio_flow_;
  TransportLayerPrsock *audio_prsock_;
  TransportLayerDtls *audio_dtls_;
  mozilla::AudioCodecConfig audio_config_;
  mozilla::RefPtr<mozilla::MediaSessionConduit> audio_conduit_;
  nsRefPtr<nsDOMMediaStream> audio_;
  mozilla::RefPtr<mozilla::MediaPipeline> audio_pipeline_;
  mozilla::RefPtr<TransportFlow> video_flow_;
  TransportLayerPrsock *video_prsock_;
  mozilla::VideoCodecConfig video_config_;
  mozilla::RefPtr<mozilla::MediaSessionConduit> video_conduit_;
  nsRefPtr<nsDOMMediaStream> video_;
  mozilla::RefPtr<mozilla::MediaPipeline> video_pipeline_;
};

class TestAgentSend : public TestAgent {
 public:
  TestAgentSend() {
    audio_ = new Fake_nsDOMMediaStream(new Fake_AudioStreamSource());

    mozilla::MediaConduitErrorCode err =
        static_cast<mozilla::AudioSessionConduit *>(audio_conduit_.get())->
        ConfigureSendMediaCodec(&audio_config_);
    EXPECT_EQ(mozilla::kMediaConduitNoError, err);

    audio_pipeline_ = new mozilla::MediaPipelineTransmit(NULL,
      test_utils.sts_target(),
      audio_, audio_conduit_, audio_flow_, NULL);

//    video_ = new Fake_nsDOMMediaStream(new Fake_VideoStreamSource());
//    video_pipeline_ = new mozilla::MediaPipelineTransmit(video_, video_conduit_, &video_flow_, &video_flow_);
  }

 private:
};


class TestAgentReceive : public TestAgent {
 public:
  TestAgentReceive() {
    mozilla::SourceMediaStream *audio = new Fake_SourceMediaStream();
    audio->SetPullEnabled(true);

    mozilla::AudioSegment* segment= new mozilla::AudioSegment();
    segment->Init(1);
    audio->AddTrack(0, 100, 0, segment);
    audio->AdvanceKnownTracksTime(mozilla::STREAM_TIME_MAX);

    audio_ = new Fake_nsDOMMediaStream(audio);

    std::vector<mozilla::AudioCodecConfig *> codecs;
    codecs.push_back(&audio_config_);

    mozilla::MediaConduitErrorCode err =
        static_cast<mozilla::AudioSessionConduit *>(audio_conduit_.get())->
        ConfigureRecvMediaCodecs(codecs);
    EXPECT_EQ(mozilla::kMediaConduitNoError, err);

    audio_pipeline_ = new mozilla::MediaPipelineReceiveAudio(NULL,
      test_utils.sts_target(),
      audio_,
      static_cast<mozilla::AudioSessionConduit *>(audio_conduit_.get()),
      audio_flow_, NULL);
  }

 private:
};


class MediaPipelineTest : public ::testing::Test {
 public:
  MediaPipelineTest() : p1_() {
    fds_[0] = fds_[1] = NULL;
  }

  void SetUp() {
    PRStatus status = PR_NewTCPSocketPair(fds_);
    ASSERT_EQ(status, PR_SUCCESS);

    p1_.ConnectSocket(fds_[0], false);
    p2_.ConnectSocket(fds_[1], true);
  }

 protected:
  PRFileDesc *fds_[2];
  TestAgentSend p1_;
  TestAgentReceive p2_;
};

TEST_F(MediaPipelineTest, AudioSend) {
  p2_.Start();
  p1_.Start();
  PR_Sleep(1000);
  p1_.Stop();
  p2_.Stop();
}


}  // end namespace


int main(int argc, char **argv)
{
  test_utils.InitServices();
  // Start the tests
  NSS_NoDB_Init(NULL);
  NSS_SetDomesticPolicy();
  ::testing::InitGoogleTest(&argc, argv);

  return RUN_ALL_TESTS();
}



