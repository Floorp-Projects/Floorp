/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Original author: ekr@rtfm.com

#include <iostream>

#include "logging.h"
#include "nss.h"
#include "ssl.h"
#include "sslproto.h"

#include "AudioSegment.h"
#include "AudioStreamTrack.h"
#include "DOMMediaStream.h"
#include "mozilla/Mutex.h"
#include "mozilla/RefPtr.h"
#include "MediaPipeline.h"
#include "MediaPipelineFilter.h"
#include "MediaStreamGraph.h"
#include "MediaStreamListener.h"
#include "MediaStreamTrack.h"
#include "transportflow.h"
#include "transportlayerloopback.h"
#include "transportlayerdtls.h"
#include "transportlayersrtp.h"
#include "mozilla/SyncRunnable.h"
#include "mtransport_test_utils.h"
#include "SharedBuffer.h"

#define GTEST_HAS_RTTI 0
#include "gtest/gtest.h"

using namespace mozilla;
MOZ_MTLOG_MODULE("mediapipeline")

static MtransportTestUtils *test_utils;

namespace {

class FakeSourceMediaStream : public mozilla::SourceMediaStream {

public:

  FakeSourceMediaStream()
    : SourceMediaStream()
  {
  }

  virtual ~FakeSourceMediaStream() override
  {
    mMainThreadDestroyed = true;
  }

  virtual bool AppendToTrack(TrackID aID, MediaSegment* aSegment, MediaSegment *aRawSegment = nullptr) override
  {
    return true;
  }
};

class FakeMediaStreamTrackSource : public mozilla::dom::MediaStreamTrackSource {

public:

  FakeMediaStreamTrackSource()
    : MediaStreamTrackSource(nullptr, nsString())
  {
  }

  virtual mozilla::dom::MediaSourceEnum GetMediaSource() const override
  {
    return mozilla::dom::MediaSourceEnum::Microphone;
  }


  virtual void Disable() override
  {
  }

  virtual void Enable() override
  {
  }

  virtual void Stop() override
  {
  }

};

class FakeAudioStreamTrack : public mozilla::dom::AudioStreamTrack {

public:

  FakeAudioStreamTrack()
    : AudioStreamTrack(new DOMMediaStream(nullptr, nullptr), 0, 1,
                       new FakeMediaStreamTrackSource())
    , mMutex("Fake AudioStreamTrack")
    , mStop(false)
    , mCount(0)
  {
    NS_NewTimerWithFuncCallback(getter_AddRefs(mTimer),
                                FakeAudioStreamTrackGenerateData, this, 20,
                                nsITimer::TYPE_REPEATING_SLACK,
                                "FakeAudioStreamTrack::FakeAudioStreamTrackGenerateData",
                                test_utils->sts_target());

  }

  void Stop()
  {
    mozilla::MutexAutoLock lock(mMutex);
    mStop = true;
    mTimer->Cancel();
  }

  virtual void AddListener(MediaStreamTrackListener* aListener) override
  {
    mozilla::MutexAutoLock lock(mMutex);
    mListeners.push_back(aListener);
  }

  virtual already_AddRefed<mozilla::dom::MediaStreamTrack> CloneInternal(DOMMediaStream* aOwningStream, TrackID aTrackID) override
  {
    return RefPtr<MediaStreamTrack>(new FakeAudioStreamTrack).forget();
  }

  private:
    std::vector<MediaStreamTrackListener*> mListeners;
    mozilla::Mutex mMutex;
    bool mStop;
    nsCOMPtr<nsITimer> mTimer;
    int mCount;

    static void FakeAudioStreamTrackGenerateData(nsITimer* timer, void* closure)
    {
      auto mst = static_cast<FakeAudioStreamTrack*>(closure);
      const int AUDIO_BUFFER_SIZE = 1600;
      const int NUM_CHANNELS      = 2;

      mozilla::MutexAutoLock lock(mst->mMutex);
      if (mst->mStop) {
        return;
      }

      RefPtr<mozilla::SharedBuffer> samples =
        mozilla::SharedBuffer::Create(AUDIO_BUFFER_SIZE * NUM_CHANNELS * sizeof(int16_t));
      int16_t* data = reinterpret_cast<int16_t *>(samples->Data());
      for(int i=0; i<(AUDIO_BUFFER_SIZE * NUM_CHANNELS); i++) {
        //saw tooth audio sample
        data[i] = ((mst->mCount % 8) * 4000) - (7*4000)/2;
        mst->mCount++;
      }

      mozilla::AudioSegment segment;
      AutoTArray<const int16_t *,1> channels;
      channels.AppendElement(data);
      segment.AppendFrames(samples.forget(),
                           channels,
                           AUDIO_BUFFER_SIZE,
                           PRINCIPAL_HANDLE_NONE);

      for (auto& listener: mst->mListeners) {
        listener->NotifyQueuedChanges(nullptr, 0, segment);
      }
    }
};

class TransportInfo {
 public:
  TransportInfo() :
    flow_(nullptr),
    loopback_(nullptr) {}

  static void InitAndConnect(TransportInfo &client, TransportInfo &server) {
    client.Init(true);
    server.Init(false);
    client.Connect(&server);
    server.Connect(&client);
  }

  void Init(bool client) {
    UniquePtr<TransportLayerLoopback> loopback(new TransportLayerLoopback);
    UniquePtr<TransportLayerDtls> dtls(new TransportLayerDtls);
    UniquePtr<TransportLayerSrtp> srtp(new TransportLayerSrtp(*dtls));

    std::vector<uint16_t> ciphers;
    ciphers.push_back(SRTP_AES128_CM_HMAC_SHA1_80);
    dtls->SetSrtpCiphers(ciphers);
    dtls->SetIdentity(DtlsIdentity::Generate());
    dtls->SetRole(client ? TransportLayerDtls::CLIENT :
      TransportLayerDtls::SERVER);
    dtls->SetVerificationAllowAll();

    ASSERT_EQ(NS_OK, loopback->Init());
    ASSERT_EQ(NS_OK, dtls->Init());
    ASSERT_EQ(NS_OK, srtp->Init());

    dtls->Chain(loopback.get());
    srtp->Chain(loopback.get());

    flow_ = new TransportFlow();
    loopback_ = loopback.release();
    flow_->PushLayer(loopback_);
    flow_->PushLayer(dtls.release());
    flow_->PushLayer(srtp.release());
  }

  void Connect(TransportInfo* peer) {
    MOZ_ASSERT(loopback_);
    MOZ_ASSERT(peer->loopback_);

    loopback_->Connect(peer->loopback_);
  }

  void Shutdown() {
    if (loopback_) {
      loopback_->Disconnect();
    }
    loopback_ = nullptr;
    flow_ = nullptr;
  }

  RefPtr<TransportFlow> flow_;
  TransportLayerLoopback *loopback_;
};

class TestAgent {
 public:
  TestAgent() :
      audio_config_(109, "opus", 48000, 960, 2, 64000, false),
      audio_conduit_(mozilla::AudioSessionConduit::Create()),
      audio_pipeline_(),
      use_bundle_(false) {
  }

  static void ConnectRtp(TestAgent *client, TestAgent *server) {
    TransportInfo::InitAndConnect(client->audio_rtp_transport_,
                                  server->audio_rtp_transport_);
  }

  static void ConnectRtcp(TestAgent *client, TestAgent *server) {
    TransportInfo::InitAndConnect(client->audio_rtcp_transport_,
                                  server->audio_rtcp_transport_);
  }

  static void ConnectBundle(TestAgent *client, TestAgent *server) {
    TransportInfo::InitAndConnect(client->bundle_transport_,
                                  server->bundle_transport_);
  }

  virtual void CreatePipeline(bool aIsRtcpMux) = 0;

  void Stop() {
    MOZ_MTLOG(ML_DEBUG, "Stopping");

    if (audio_pipeline_)
      audio_pipeline_->Stop();
  }

  void Shutdown_s() {
    audio_rtp_transport_.Shutdown();
    audio_rtcp_transport_.Shutdown();
    bundle_transport_.Shutdown();
  }

  void Shutdown() {
    if (audio_pipeline_)
      audio_pipeline_->Shutdown_m();
    if (audio_stream_track_)
      audio_stream_track_->Stop();

    mozilla::SyncRunnable::DispatchToThread(
      test_utils->sts_target(),
      WrapRunnable(this, &TestAgent::Shutdown_s));
  }

  uint32_t GetRemoteSSRC() {
    uint32_t res = 0;
    audio_conduit_->GetRemoteSSRC(&res);
    return res;
  }

  uint32_t GetLocalSSRC() {
    std::vector<uint32_t> res;
    res = audio_conduit_->GetLocalSSRCs();
    return res.empty() ? 0 : res[0];
  }

  int GetAudioRtpCountSent() {
    return audio_pipeline_->RtpPacketsSent();
  }

  int GetAudioRtpCountReceived() {
    return audio_pipeline_->RtpPacketsReceived();
  }

  int GetAudioRtcpCountSent() {
    return audio_pipeline_->RtcpPacketsSent();
  }

  int GetAudioRtcpCountReceived() {
    return audio_pipeline_->RtcpPacketsReceived();
  }


  void SetUsingBundle(bool use_bundle) {
    use_bundle_ = use_bundle;
  }

 protected:
  mozilla::AudioCodecConfig audio_config_;
  RefPtr<mozilla::MediaSessionConduit> audio_conduit_;
  RefPtr<FakeAudioStreamTrack> audio_stream_track_;
  // TODO(bcampen@mozilla.com): Right now this does not let us test RTCP in
  // both directions; only the sender's RTCP is sent, but the receiver should
  // be sending it too.
  RefPtr<mozilla::MediaPipeline> audio_pipeline_;
  TransportInfo audio_rtp_transport_;
  TransportInfo audio_rtcp_transport_;
  TransportInfo bundle_transport_;
  bool use_bundle_;
};

class TestAgentSend : public TestAgent {
 public:
  TestAgentSend() {
    mozilla::MediaConduitErrorCode err =
        static_cast<mozilla::AudioSessionConduit *>(audio_conduit_.get())->
        ConfigureSendMediaCodec(&audio_config_);
    EXPECT_EQ(mozilla::kMediaConduitNoError, err);

    audio_stream_track_ = new FakeAudioStreamTrack();
  }

  virtual void CreatePipeline(bool aIsRtcpMux) {

    std::string test_pc;

    if (aIsRtcpMux) {
      ASSERT_FALSE(audio_rtcp_transport_.flow_);
    }

    RefPtr<MediaPipelineTransmit> audio_pipeline =
      new mozilla::MediaPipelineTransmit(
        test_pc,
        nullptr,
        test_utils->sts_target(),
        false,
        audio_conduit_);

    audio_pipeline->SetTrack(audio_stream_track_.get());
    audio_pipeline->Start();

    audio_pipeline_ = audio_pipeline;

    RefPtr<TransportFlow> rtp(audio_rtp_transport_.flow_);
    RefPtr<TransportFlow> rtcp(audio_rtcp_transport_.flow_);

    if (use_bundle_) {
      rtp = bundle_transport_.flow_;
      rtcp = nullptr;
    }

    audio_pipeline_->UpdateTransport_m(
        rtp, rtcp, nsAutoPtr<MediaPipelineFilter>(nullptr));
  }
};


class TestAgentReceive : public TestAgent {
 public:

  TestAgentReceive() {
    std::vector<mozilla::AudioCodecConfig *> codecs;
    codecs.push_back(&audio_config_);

    mozilla::MediaConduitErrorCode err =
        static_cast<mozilla::AudioSessionConduit *>(audio_conduit_.get())->
        ConfigureRecvMediaCodecs(codecs);
    EXPECT_EQ(mozilla::kMediaConduitNoError, err);
  }

  virtual void CreatePipeline(bool aIsRtcpMux) {
    std::string test_pc;

    if (aIsRtcpMux) {
      ASSERT_FALSE(audio_rtcp_transport_.flow_);
    }

    audio_pipeline_ = new mozilla::MediaPipelineReceiveAudio(
        test_pc,
        nullptr,
        test_utils->sts_target(),
        static_cast<mozilla::AudioSessionConduit *>(audio_conduit_.get()),
        nullptr);

    audio_pipeline_->Start();

    RefPtr<TransportFlow> rtp(audio_rtp_transport_.flow_);
    RefPtr<TransportFlow> rtcp(audio_rtcp_transport_.flow_);

    if (use_bundle_) {
      rtp = bundle_transport_.flow_;
      rtcp = nullptr;
    }

    audio_pipeline_->UpdateTransport_m(rtp, rtcp, bundle_filter_);
  }

  void SetBundleFilter(nsAutoPtr<MediaPipelineFilter> filter) {
    bundle_filter_ = filter;
  }

  void UpdateFilter_s(
      nsAutoPtr<MediaPipelineFilter> filter) {
    audio_pipeline_->UpdateTransport_s(audio_rtp_transport_.flow_,
                                       audio_rtcp_transport_.flow_,
                                       filter);
  }

 private:
  nsAutoPtr<MediaPipelineFilter> bundle_filter_;
};


class MediaPipelineTest : public ::testing::Test {
 public:
  ~MediaPipelineTest() {
    p1_.Shutdown();
    p2_.Shutdown();
  }

  static void SetUpTestCase() {
    test_utils = new MtransportTestUtils();
    NSS_NoDB_Init(nullptr);
    NSS_SetDomesticPolicy();
  }

  // Setup transport.
  void InitTransports(bool aIsRtcpMux) {
    // RTP, p1_ is server, p2_ is client
    mozilla::SyncRunnable::DispatchToThread(
      test_utils->sts_target(),
      WrapRunnableNM(&TestAgent::ConnectRtp, &p2_, &p1_));

    // Create RTCP flows separately if we are not muxing them.
    if(!aIsRtcpMux) {
      // RTCP, p1_ is server, p2_ is client
      mozilla::SyncRunnable::DispatchToThread(
        test_utils->sts_target(),
        WrapRunnableNM(&TestAgent::ConnectRtcp, &p2_, &p1_));
    }

    // BUNDLE, p1_ is server, p2_ is client
    mozilla::SyncRunnable::DispatchToThread(
      test_utils->sts_target(),
      WrapRunnableNM(&TestAgent::ConnectBundle, &p2_, &p1_));
  }

  // Verify RTP and RTCP
  void TestAudioSend(bool aIsRtcpMux,
                     nsAutoPtr<MediaPipelineFilter> initialFilter =
                        nsAutoPtr<MediaPipelineFilter>(nullptr),
                     nsAutoPtr<MediaPipelineFilter> refinedFilter =
                        nsAutoPtr<MediaPipelineFilter>(nullptr),
                     unsigned int ms_until_filter_update = 500,
                     unsigned int ms_of_traffic_after_answer = 10000) {

    bool bundle = !!(initialFilter);
    // We do not support testing bundle without rtcp mux, since that doesn't
    // make any sense.
    ASSERT_FALSE(!aIsRtcpMux && bundle);

    p2_.SetBundleFilter(initialFilter);

    // Setup transport flows
    InitTransports(aIsRtcpMux);

    p1_.CreatePipeline(aIsRtcpMux);
    p2_.CreatePipeline(aIsRtcpMux);

    if (bundle) {
      PR_Sleep(ms_until_filter_update);

      // Leaving refinedFilter not set implies we want to just update with
      // the other side's SSRC
      if (!refinedFilter) {
        refinedFilter = new MediaPipelineFilter;
        // Might not be safe, strictly speaking.
        refinedFilter->AddRemoteSSRC(p1_.GetLocalSSRC());
      }

      mozilla::SyncRunnable::DispatchToThread(
          test_utils->sts_target(),
          WrapRunnable(&p2_,
                       &TestAgentReceive::UpdateFilter_s,
                       refinedFilter));
    }

    // wait for some RTP/RTCP tx and rx to happen
    PR_Sleep(ms_of_traffic_after_answer);

    p1_.Stop();
    p2_.Stop();

    // wait for any packets in flight to arrive
    PR_Sleep(100);

    p1_.Shutdown();
    p2_.Shutdown();

    if (!bundle) {
      // If we are filtering, allow the test-case to do this checking.
      ASSERT_GE(p1_.GetAudioRtpCountSent(), 40);
      ASSERT_EQ(p1_.GetAudioRtpCountReceived(), p2_.GetAudioRtpCountSent());
      ASSERT_EQ(p1_.GetAudioRtpCountSent(), p2_.GetAudioRtpCountReceived());

      // Calling ShutdownMedia_m on both pipelines does not stop the flow of
      // RTCP. So, we might be off by one here.
      ASSERT_LE(p2_.GetAudioRtcpCountReceived(), p1_.GetAudioRtcpCountSent());
      ASSERT_GE(p2_.GetAudioRtcpCountReceived() + 1, p1_.GetAudioRtcpCountSent());
    }

  }

  void TestAudioReceiverBundle(bool bundle_accepted,
      nsAutoPtr<MediaPipelineFilter> initialFilter,
      nsAutoPtr<MediaPipelineFilter> refinedFilter =
          nsAutoPtr<MediaPipelineFilter>(nullptr),
      unsigned int ms_until_answer = 500,
      unsigned int ms_of_traffic_after_answer = 10000) {
    TestAudioSend(true,
                  initialFilter,
                  refinedFilter,
                  ms_until_answer,
                  ms_of_traffic_after_answer);
  }
protected:
  TestAgentSend p1_;
  TestAgentReceive p2_;
};

class MediaPipelineFilterTest : public ::testing::Test {
  public:
    bool Filter(MediaPipelineFilter& filter,
                int32_t correlator,
                uint32_t ssrc,
                uint8_t payload_type) {

      webrtc::RTPHeader header;
      header.ssrc = ssrc;
      header.payloadType = payload_type;
      return filter.Filter(header, correlator);
    }
};

TEST_F(MediaPipelineFilterTest, TestConstruct) {
  MediaPipelineFilter filter;
}

TEST_F(MediaPipelineFilterTest, TestDefault) {
  MediaPipelineFilter filter;
  ASSERT_FALSE(Filter(filter, 0, 233, 110));
}

TEST_F(MediaPipelineFilterTest, TestSSRCFilter) {
  MediaPipelineFilter filter;
  filter.AddRemoteSSRC(555);
  ASSERT_TRUE(Filter(filter, 0, 555, 110));
  ASSERT_FALSE(Filter(filter, 0, 556, 110));
}

#define SSRC(ssrc) \
  ((ssrc >> 24) & 0xFF), \
  ((ssrc >> 16) & 0xFF), \
  ((ssrc >> 8 ) & 0xFF), \
  (ssrc         & 0xFF)

#define REPORT_FRAGMENT(ssrc) \
  SSRC(ssrc), \
  0,0,0,0, \
  0,0,0,0, \
  0,0,0,0, \
  0,0,0,0, \
  0,0,0,0

#define RTCP_TYPEINFO(num_rrs, type, size) \
  0x80 + num_rrs, type, 0, size

const unsigned char rtcp_sr_s16[] = {
  // zero rrs, size 6 words
  RTCP_TYPEINFO(0, MediaPipelineFilter::SENDER_REPORT_T, 6),
  REPORT_FRAGMENT(16)
};

const unsigned char rtcp_sr_s16_r17[] = {
  // one rr, size 12 words
  RTCP_TYPEINFO(1, MediaPipelineFilter::SENDER_REPORT_T, 12),
  REPORT_FRAGMENT(16),
  REPORT_FRAGMENT(17)
};

const unsigned char unknown_type[] = {
  RTCP_TYPEINFO(1, 222, 0)
};

TEST_F(MediaPipelineFilterTest, TestEmptyFilterReport0) {
  MediaPipelineFilter filter;
  ASSERT_FALSE(filter.FilterSenderReport(rtcp_sr_s16, sizeof(rtcp_sr_s16)));
}

TEST_F(MediaPipelineFilterTest, TestFilterReport0) {
  MediaPipelineFilter filter;
  filter.AddRemoteSSRC(16);
  ASSERT_TRUE(filter.FilterSenderReport(rtcp_sr_s16, sizeof(rtcp_sr_s16)));
}

TEST_F(MediaPipelineFilterTest, TestFilterReport0PTTruncated) {
  MediaPipelineFilter filter;
  filter.AddRemoteSSRC(16);
  const unsigned char data[] = {0x80};
  ASSERT_FALSE(filter.FilterSenderReport(data, sizeof(data)));
}

TEST_F(MediaPipelineFilterTest, TestFilterReport0CountTruncated) {
  MediaPipelineFilter filter;
  filter.AddRemoteSSRC(16);
  const unsigned char* data = {};
  ASSERT_FALSE(filter.FilterSenderReport(data, sizeof(data)));
}

TEST_F(MediaPipelineFilterTest, TestFilterReport1SSRCTruncated) {
  MediaPipelineFilter filter;
  filter.AddRemoteSSRC(16);
  const unsigned char sr[] = {
    RTCP_TYPEINFO(1, MediaPipelineFilter::SENDER_REPORT_T, 12),
    REPORT_FRAGMENT(16),
    0,0,0
  };
  ASSERT_TRUE(filter.FilterSenderReport(sr, sizeof(sr)));
}

TEST_F(MediaPipelineFilterTest, TestFilterReport1BigSSRC) {
  MediaPipelineFilter filter;
  filter.AddRemoteSSRC(0x01020304);
  const unsigned char sr[] = {
    RTCP_TYPEINFO(1, MediaPipelineFilter::SENDER_REPORT_T, 12),
    SSRC(0x01020304),
    REPORT_FRAGMENT(0x11121314)
  };
  ASSERT_TRUE(filter.FilterSenderReport(sr, sizeof(sr)));
}

TEST_F(MediaPipelineFilterTest, TestFilterReportMatch) {
  MediaPipelineFilter filter;
  filter.AddRemoteSSRC(16);
  ASSERT_TRUE(filter.FilterSenderReport(rtcp_sr_s16_r17,
                                        sizeof(rtcp_sr_s16_r17)));
}

TEST_F(MediaPipelineFilterTest, TestFilterReportNoMatch) {
  MediaPipelineFilter filter;
  filter.AddRemoteSSRC(17);
  ASSERT_FALSE(filter.FilterSenderReport(rtcp_sr_s16_r17,
                                         sizeof(rtcp_sr_s16_r17)));
}

TEST_F(MediaPipelineFilterTest, TestFilterUnknownRTCPType) {
  MediaPipelineFilter filter;
  ASSERT_FALSE(filter.FilterSenderReport(unknown_type, sizeof(unknown_type)));
}

TEST_F(MediaPipelineFilterTest, TestCorrelatorFilter) {
  MediaPipelineFilter filter;
  filter.SetCorrelator(7777);
  ASSERT_TRUE(Filter(filter, 7777, 16, 110));
  ASSERT_FALSE(Filter(filter, 7778, 17, 110));
  // This should also have resulted in the SSRC 16 being added to the filter
  ASSERT_TRUE(Filter(filter, 0, 16, 110));
  ASSERT_FALSE(Filter(filter, 0, 17, 110));

  // rtcp_sr_s16 has 16 as an SSRC
  ASSERT_TRUE(filter.FilterSenderReport(rtcp_sr_s16, sizeof(rtcp_sr_s16)));
}

TEST_F(MediaPipelineFilterTest, TestPayloadTypeFilter) {
  MediaPipelineFilter filter;
  filter.AddUniquePT(110);
  ASSERT_TRUE(Filter(filter, 0, 555, 110));
  ASSERT_FALSE(Filter(filter, 0, 556, 111));
}

TEST_F(MediaPipelineFilterTest, TestPayloadTypeFilterSSRCUpdate) {
  MediaPipelineFilter filter;
  filter.AddUniquePT(110);
  ASSERT_TRUE(Filter(filter, 0, 16, 110));

  // rtcp_sr_s16 has 16 as an SSRC
  ASSERT_TRUE(filter.FilterSenderReport(rtcp_sr_s16, sizeof(rtcp_sr_s16)));
}

TEST_F(MediaPipelineFilterTest, TestSSRCMovedWithCorrelator) {
  MediaPipelineFilter filter;
  filter.SetCorrelator(7777);
  ASSERT_TRUE(Filter(filter, 7777, 555, 110));
  ASSERT_TRUE(Filter(filter, 0, 555, 110));
  ASSERT_FALSE(Filter(filter, 7778, 555, 110));
  ASSERT_FALSE(Filter(filter, 0, 555, 110));
}

TEST_F(MediaPipelineFilterTest, TestRemoteSDPNoSSRCs) {
  // If the remote SDP doesn't have SSRCs, right now this is a no-op and
  // there is no point of even incorporating a filter, but we make the
  // behavior consistent to avoid confusion.
  MediaPipelineFilter filter;
  filter.SetCorrelator(7777);
  filter.AddUniquePT(111);
  ASSERT_TRUE(Filter(filter, 7777, 555, 110));

  MediaPipelineFilter filter2;

  filter.Update(filter2);

  // Ensure that the old SSRC still works.
  ASSERT_TRUE(Filter(filter, 0, 555, 110));
}

TEST_F(MediaPipelineTest, TestAudioSendNoMux) {
  TestAudioSend(false);
}

TEST_F(MediaPipelineTest, TestAudioSendMux) {
  TestAudioSend(true);
}

TEST_F(MediaPipelineTest, TestAudioSendBundle) {
  nsAutoPtr<MediaPipelineFilter> filter(new MediaPipelineFilter);
  // These durations have to be _extremely_ long to have any assurance that
  // some RTCP will be sent at all. This is because the first RTCP packet
  // is sometimes sent before the transports are ready, which causes it to
  // be dropped.
  TestAudioReceiverBundle(true,
                          filter,
  // We do not specify the filter for the remote description, so it will be
  // set to something sane after a short time.
                          nsAutoPtr<MediaPipelineFilter>(),
                          10000,
                          10000);

  // Some packets should have been dropped, but not all
  ASSERT_GT(p1_.GetAudioRtpCountSent(), p2_.GetAudioRtpCountReceived());
  ASSERT_GT(p2_.GetAudioRtpCountReceived(), 40);
  ASSERT_GT(p1_.GetAudioRtcpCountSent(), 1);
  ASSERT_GT(p1_.GetAudioRtcpCountSent(), p2_.GetAudioRtcpCountReceived());
  ASSERT_GT(p2_.GetAudioRtcpCountReceived(), 0);
}

TEST_F(MediaPipelineTest, TestAudioSendEmptyBundleFilter) {
  nsAutoPtr<MediaPipelineFilter> filter(new MediaPipelineFilter);
  nsAutoPtr<MediaPipelineFilter> bad_answer_filter(new MediaPipelineFilter);
  TestAudioReceiverBundle(true, filter, bad_answer_filter);
  // Filter is empty, so should drop everything.
  ASSERT_EQ(0, p2_.GetAudioRtpCountReceived());
}

}  // end namespace
