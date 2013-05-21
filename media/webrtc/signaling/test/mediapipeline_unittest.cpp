/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Original author: ekr@rtfm.com

#include <iostream>

#include "sigslot.h"

#include "logging.h"
#include "nsThreadUtils.h"
#include "nsXPCOM.h"
#include "nss.h"
#include "ssl.h"
#include "sslproto.h"

#include "dtlsidentity.h"
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
#include "mozilla/SyncRunnable.h"


#include "mtransport_test_utils.h"
#include "runnable_utils.h"

#define GTEST_HAS_RTTI 0
#include "gtest/gtest.h"
#include "gtest_utils.h"

using namespace mozilla;
MOZ_MTLOG_MODULE("mediapipeline")

MtransportTestUtils *test_utils;

namespace {
class TestAgent {
 public:
  TestAgent() :
      audio_flow_(new TransportFlow()),
      audio_prsock_(new TransportLayerPrsock()),
      audio_dtls_(new TransportLayerDtls()),
      audio_config_(109, "opus", 48000, 960, 2, 64000),
      audio_conduit_(mozilla::AudioSessionConduit::Create(NULL)),
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

    mozilla::SyncRunnable::DispatchToThread(
      test_utils->sts_target(),
      WrapRunnable(audio_prsock_, &TransportLayerPrsock::Import, fd, &res));

    ASSERT_TRUE(NS_SUCCEEDED(res));

    ASSERT_EQ((nsresult)NS_OK, audio_flow_->PushLayer(audio_prsock_));

    std::vector<uint16_t> ciphers;
    ciphers.push_back(SRTP_AES128_CM_HMAC_SHA1_80);
    audio_dtls_->SetSrtpCiphers(ciphers);
    audio_dtls_->SetIdentity(DtlsIdentity::Generate());
    audio_dtls_->SetRole(client ? TransportLayerDtls::CLIENT :
                         TransportLayerDtls::SERVER);
    audio_flow_->PushLayer(audio_dtls_);
  }

  virtual void CreatePipelines_s() = 0;

  void Start() {
    nsresult ret;

    MOZ_MTLOG(PR_LOG_DEBUG, "Starting");

    mozilla::SyncRunnable::DispatchToThread(
      test_utils->sts_target(),
      WrapRunnableRet(audio_->GetStream(), &Fake_MediaStream::Start, &ret));

    ASSERT_TRUE(NS_SUCCEEDED(ret));
  }

  void StopInt() {
    audio_->GetStream()->Stop();
    audio_flow_ = NULL;
    video_flow_ = NULL;
    if (audio_pipeline_)
      audio_pipeline_->ShutdownTransport_s();
    if (video_pipeline_)
      video_pipeline_->ShutdownTransport_s();
  }

  void Stop() {
    MOZ_MTLOG(PR_LOG_DEBUG, "Stopping");

    if (audio_pipeline_)
      audio_pipeline_->ShutdownMedia_m();
    if (video_pipeline_)
      video_pipeline_->ShutdownMedia_m();

    mozilla::SyncRunnable::DispatchToThread(
      test_utils->sts_target(),
      WrapRunnable(this, &TestAgent::StopInt));

    audio_pipeline_ = NULL;
    video_pipeline_ = NULL;

    PR_Sleep(1000); // Deal with race condition
  }


 protected:
  mozilla::RefPtr<TransportFlow> audio_flow_;
  TransportLayerPrsock *audio_prsock_;
  TransportLayerDtls *audio_dtls_;
  mozilla::AudioCodecConfig audio_config_;
  mozilla::RefPtr<mozilla::MediaSessionConduit> audio_conduit_;
  nsRefPtr<DOMMediaStream> audio_;
  mozilla::RefPtr<mozilla::MediaPipeline> audio_pipeline_;
  mozilla::RefPtr<TransportFlow> video_flow_;
  TransportLayerPrsock *video_prsock_;
  mozilla::VideoCodecConfig video_config_;
  mozilla::RefPtr<mozilla::MediaSessionConduit> video_conduit_;
  nsRefPtr<DOMMediaStream> video_;
  mozilla::RefPtr<mozilla::MediaPipeline> video_pipeline_;
};

class TestAgentSend : public TestAgent {
 public:
  virtual void CreatePipelines_s() {
    audio_ = new Fake_DOMMediaStream(new Fake_AudioStreamSource());

    mozilla::MediaConduitErrorCode err =
        static_cast<mozilla::AudioSessionConduit *>(audio_conduit_.get())->
        ConfigureSendMediaCodec(&audio_config_);
    EXPECT_EQ(mozilla::kMediaConduitNoError, err);

    std::string test_pc("PC");

    audio_pipeline_ = new mozilla::MediaPipelineTransmit(
        test_pc,
        NULL,
        test_utils->sts_target(),
        audio_->GetStream(), 1, audio_conduit_, audio_flow_, NULL);

    audio_pipeline_->Init();

//    video_ = new Fake_DOMMediaStream(new Fake_VideoStreamSource());
//    video_pipeline_ = new mozilla::MediaPipelineTransmit(video_, video_conduit_, &video_flow_, &video_flow_);
  }

 private:
};


class TestAgentReceive : public TestAgent {
 public:
  virtual void CreatePipelines_s() {
    mozilla::SourceMediaStream *audio = new Fake_SourceMediaStream();
    audio->SetPullEnabled(true);

    mozilla::AudioSegment* segment= new mozilla::AudioSegment();
    audio->AddTrack(0, 100, 0, segment);
    audio->AdvanceKnownTracksTime(mozilla::STREAM_TIME_MAX);

    audio_ = new Fake_DOMMediaStream(audio);

    std::vector<mozilla::AudioCodecConfig *> codecs;
    codecs.push_back(&audio_config_);

    mozilla::MediaConduitErrorCode err =
        static_cast<mozilla::AudioSessionConduit *>(audio_conduit_.get())->
        ConfigureRecvMediaCodecs(codecs);
    EXPECT_EQ(mozilla::kMediaConduitNoError, err);

    std::string test_pc("PC");
    audio_pipeline_ = new mozilla::MediaPipelineReceiveAudio(
        test_pc,
        NULL,
        test_utils->sts_target(),
        audio_->GetStream(), 1,
        static_cast<mozilla::AudioSessionConduit *>(audio_conduit_.get()),
        audio_flow_, NULL);

    audio_pipeline_->Init();
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

    mozilla::SyncRunnable::DispatchToThread(
      test_utils->sts_target(),
      WrapRunnable(&p1_, &TestAgent::ConnectSocket, fds_[0], false));

    mozilla::SyncRunnable::DispatchToThread(
      test_utils->sts_target(),
      WrapRunnable(&p2_, &TestAgent::ConnectSocket, fds_[1], false));

    mozilla::SyncRunnable::DispatchToThread(
      test_utils->sts_target(),
      WrapRunnable(&p1_, &TestAgent::CreatePipelines_s));

    mozilla::SyncRunnable::DispatchToThread(
      test_utils->sts_target(),
      WrapRunnable(&p2_, &TestAgent::CreatePipelines_s));
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


int main(int argc, char **argv) {
  test_utils = new MtransportTestUtils();
  // Start the tests
  NSS_NoDB_Init(NULL);
  NSS_SetDomesticPolicy();
  ::testing::InitGoogleTest(&argc, argv);

  int rv = RUN_ALL_TESTS();
  delete test_utils;
  return rv;
}



