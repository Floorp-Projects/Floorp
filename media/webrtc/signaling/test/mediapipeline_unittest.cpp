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

class TransportInfo {
 public:
  TransportInfo() :
    flow_(nullptr),
    prsock_(nullptr),
    dtls_(nullptr) {}

  void Init(bool client) {
    nsresult res;

    flow_ = new TransportFlow();
    prsock_ = new TransportLayerPrsock();
    dtls_ = new TransportLayerDtls();

    res = prsock_->Init();
    if (res != NS_OK) {
      FreeLayers();
    }
    ASSERT_EQ((nsresult)NS_OK, res);

    std::vector<uint16_t> ciphers;
    ciphers.push_back(SRTP_AES128_CM_HMAC_SHA1_80);
    dtls_->SetSrtpCiphers(ciphers);
    dtls_->SetIdentity(DtlsIdentity::Generate());
    dtls_->SetRole(client ? TransportLayerDtls::CLIENT :
      TransportLayerDtls::SERVER);
    dtls_->SetVerificationAllowAll();
  }

  void PushLayers() {
    nsresult res;

    nsAutoPtr<std::queue<TransportLayer *> > layers(
      new std::queue<TransportLayer *>);
    layers->push(prsock_);
    layers->push(dtls_);
    res = flow_->PushLayers(layers);
    if (res != NS_OK) {
      FreeLayers();
    }
    ASSERT_EQ((nsresult)NS_OK, res);
  }

  // Free the memory allocated at the beginning of Init
  // if failure occurs before layers setup.
  void FreeLayers() {
    delete prsock_;
    delete dtls_;
  }

  void Stop() {
    flow_ = nullptr;
  }

  mozilla::RefPtr<TransportFlow> flow_;
  TransportLayerPrsock *prsock_;
  TransportLayerDtls *dtls_;
};

class TestAgent {
 public:
  TestAgent() :
      audio_config_(109, "opus", 48000, 960, 2, 64000),
      audio_conduit_(mozilla::AudioSessionConduit::Create(nullptr)),
      audio_(),
      audio_pipeline_() {
  }

  void ConnectSocket(PRFileDesc *fd, bool client, bool isRtcp) {
    nsresult res;
    TransportInfo *transport = isRtcp ?
      &audio_rtcp_transport_ : &audio_rtp_transport_;

    transport->Init(client);

    mozilla::SyncRunnable::DispatchToThread(
      test_utils->sts_target(),
      WrapRunnable(transport->prsock_, &TransportLayerPrsock::Import, fd, &res));
    if (!NS_SUCCEEDED(res)) {
      transport->FreeLayers();
    }
    ASSERT_TRUE(NS_SUCCEEDED(res));

    transport->PushLayers();
  }

  virtual void CreatePipelines_s(bool aIsRtcpMux) = 0;

  void Start() {
    nsresult ret;

    MOZ_MTLOG(ML_DEBUG, "Starting");

    mozilla::SyncRunnable::DispatchToThread(
      test_utils->sts_target(),
      WrapRunnableRet(audio_->GetStream(), &Fake_MediaStream::Start, &ret));

    ASSERT_TRUE(NS_SUCCEEDED(ret));
  }

  void StopInt() {
    audio_->GetStream()->Stop();
    audio_rtp_transport_.Stop();
    audio_rtcp_transport_.Stop();
    if (audio_pipeline_)
      audio_pipeline_->ShutdownTransport_s();
  }

  void Stop() {
    MOZ_MTLOG(ML_DEBUG, "Stopping");

    if (audio_pipeline_)
      audio_pipeline_->ShutdownMedia_m();

    mozilla::SyncRunnable::DispatchToThread(
      test_utils->sts_target(),
      WrapRunnable(this, &TestAgent::StopInt));

    audio_pipeline_ = nullptr;

    PR_Sleep(1000); // Deal with race condition
  }

 protected:
  mozilla::AudioCodecConfig audio_config_;
  mozilla::RefPtr<mozilla::MediaSessionConduit> audio_conduit_;
  nsRefPtr<DOMMediaStream> audio_;
  mozilla::RefPtr<mozilla::MediaPipeline> audio_pipeline_;
  TransportInfo audio_rtp_transport_;
  TransportInfo audio_rtcp_transport_;
};

class TestAgentSend : public TestAgent {
 public:
  virtual void CreatePipelines_s(bool aIsRtcpMux) {
    audio_ = new Fake_DOMMediaStream(new Fake_AudioStreamSource());

    mozilla::MediaConduitErrorCode err =
        static_cast<mozilla::AudioSessionConduit *>(audio_conduit_.get())->
        ConfigureSendMediaCodec(&audio_config_);
    EXPECT_EQ(mozilla::kMediaConduitNoError, err);

    std::string test_pc("PC");

    if (aIsRtcpMux) {
      ASSERT_FALSE(audio_rtcp_transport_.flow_);
    }

    audio_pipeline_ = new mozilla::MediaPipelineTransmit(
        test_pc,
        nullptr,
        test_utils->sts_target(),
        audio_,
        1,
        1,
        audio_conduit_,
        audio_rtp_transport_.flow_,
        audio_rtcp_transport_.flow_);

    audio_pipeline_->Init();
  }

  int GetAudioRtpCount() {
    return audio_pipeline_->rtp_packets_sent();
  }

  int GetAudioRtcpCount() {
    return audio_pipeline_->rtcp_packets_received();
  }

 private:
};


class TestAgentReceive : public TestAgent {
 public:
  virtual void CreatePipelines_s(bool aIsRtcpMux) {
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

    if (aIsRtcpMux) {
      ASSERT_FALSE(audio_rtcp_transport_.flow_);
    }

    audio_pipeline_ = new mozilla::MediaPipelineReceiveAudio(
        test_pc,
        nullptr,
        test_utils->sts_target(),
        audio_->GetStream(), 1, 1,
        static_cast<mozilla::AudioSessionConduit *>(audio_conduit_.get()),
        audio_rtp_transport_.flow_, audio_rtcp_transport_.flow_);

    audio_pipeline_->Init();
  }

  int GetAudioRtpCount() {
    return audio_pipeline_->rtp_packets_received();
  }

  int GetAudioRtcpCount() {
    return audio_pipeline_->rtcp_packets_sent();
  }

 private:
};


class MediaPipelineTest : public ::testing::Test {
 public:
  MediaPipelineTest() : p1_() {
    rtp_fds_[0] = rtp_fds_[1] = nullptr;
    rtcp_fds_[0] = rtcp_fds_[1] = nullptr;
  }

  // Setup transport.
  void InitTransports(bool aIsRtcpMux) {
    // Create RTP related transport.
    PRStatus status =  PR_NewTCPSocketPair(rtp_fds_);
    ASSERT_EQ(status, PR_SUCCESS);

    // RTP, DTLS server
    mozilla::SyncRunnable::DispatchToThread(
      test_utils->sts_target(),
      WrapRunnable(&p1_, &TestAgent::ConnectSocket, rtp_fds_[0], false, false));

    // RTP, DTLS client
    mozilla::SyncRunnable::DispatchToThread(
      test_utils->sts_target(),
      WrapRunnable(&p2_, &TestAgent::ConnectSocket, rtp_fds_[1], true, false));

    // Create RTCP flows separately if we are not muxing them.
    if(!aIsRtcpMux) {
      status = PR_NewTCPSocketPair(rtcp_fds_);
      ASSERT_EQ(status, PR_SUCCESS);

      // RTCP, DTLS server
      mozilla::SyncRunnable::DispatchToThread(
        test_utils->sts_target(),
        WrapRunnable(&p1_, &TestAgent::ConnectSocket, rtcp_fds_[0], false, true));

      // RTCP, DTLS client
      mozilla::SyncRunnable::DispatchToThread(
        test_utils->sts_target(),
        WrapRunnable(&p2_, &TestAgent::ConnectSocket, rtcp_fds_[1], true, true));
    }
  }

  // Verify RTP and RTCP
  void TestAudioSend(bool aIsRtcpMux) {
    // Setup transport flows
    InitTransports(aIsRtcpMux);

    mozilla::SyncRunnable::DispatchToThread(
      test_utils->sts_target(),
      WrapRunnable(&p1_, &TestAgent::CreatePipelines_s, aIsRtcpMux));

    mozilla::SyncRunnable::DispatchToThread(
      test_utils->sts_target(),
      WrapRunnable(&p2_, &TestAgent::CreatePipelines_s, aIsRtcpMux));

    p2_.Start();
    p1_.Start();

    // wait for some RTP/RTCP tx and rx to happen
    PR_Sleep(10000);

    ASSERT_GE(p1_.GetAudioRtpCount(), 40);
// TODO: Fix to not fail or crash (Bug 947663)
//    ASSERT_GE(p2_.GetAudioRtpCount(), 40);
    ASSERT_GE(p1_.GetAudioRtcpCount(), 1);
    ASSERT_GE(p2_.GetAudioRtcpCount(), 1);

    p1_.Stop();
    p2_.Stop();
  }

protected:
  PRFileDesc *rtp_fds_[2];
  PRFileDesc *rtcp_fds_[2];
  TestAgentSend p1_;
  TestAgentReceive p2_;
};

TEST_F(MediaPipelineTest, TestAudioSendNoMux) {
  TestAudioSend(false);
}

TEST_F(MediaPipelineTest, TestAudioSendMux) {
  TestAudioSend(true);
}

}  // end namespace


int main(int argc, char **argv) {
  test_utils = new MtransportTestUtils();
  // Start the tests
  NSS_NoDB_Init(nullptr);
  NSS_SetDomesticPolicy();
  ::testing::InitGoogleTest(&argc, argv);

  int rv = RUN_ALL_TESTS();
  delete test_utils;
  return rv;
}



