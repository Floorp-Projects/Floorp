/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Original author: ekr@rtfm.com

#include <iostream>

#include "logging.h"
#include "nss.h"

#include "AudioSegment.h"
#include "AudioStreamTrack.h"
#include "mozilla/Mutex.h"
#include "mozilla/RefPtr.h"
#include "MediaPipeline.h"
#include "MediaPipelineFilter.h"
#include "MediaTrackGraph.h"
#include "MediaTrackListener.h"
#include "MediaStreamTrack.h"
#include "transportflow.h"
#include "transportlayerloopback.h"
#include "transportlayerdtls.h"
#include "transportlayersrtp.h"
#include "mozilla/SyncRunnable.h"
#include "mtransport_test_utils.h"
#include "SharedBuffer.h"
#include "MediaTransportHandler.h"

#define GTEST_HAS_RTTI 0
#include "gtest/gtest.h"

using namespace mozilla;
MOZ_MTLOG_MODULE("mediapipeline")

static MtransportTestUtils* test_utils;

namespace {

class FakeAudioTrack : public mozilla::ProcessedMediaTrack {
 public:
  FakeAudioTrack()
      : ProcessedMediaTrack(44100, MediaSegment::AUDIO, nullptr),
        mMutex("Fake AudioTrack") {
    Resume();
  }

  void Destroy() override {
    MOZ_ASSERT(!mMainThreadDestroyed);
    mMainThreadDestroyed = true;
    Suspend();
  }

  void QueueSetAutoend(bool) override {}

  void Suspend() override {
    mozilla::MutexAutoLock lock(mMutex);
    if (mSuspended) {
      return;
    }
    mSuspended = true;
    mTimer->Cancel();
    mTimer = nullptr;
  }

  void Resume() override {
    mozilla::MutexAutoLock lock(mMutex);
    if (!mSuspended) {
      return;
    }
    mSuspended = false;
    NS_NewTimerWithFuncCallback(
        getter_AddRefs(mTimer), FakeAudioTrackGenerateData, this, 20,
        nsITimer::TYPE_REPEATING_SLACK,
        "FakeAudioTrack::FakeAudioTrackGenerateData", test_utils->sts_target());
  }

  void AddListener(MediaTrackListener* aListener) override {
    mozilla::MutexAutoLock lock(mMutex);
    MOZ_ASSERT(!mListener);
    mListener = aListener;
  }

  RefPtr<GenericPromise> RemoveListener(
      MediaTrackListener* aListener) override {
    mozilla::MutexAutoLock lock(mMutex);
    MOZ_ASSERT(mListener == aListener);
    mListener = nullptr;
    return GenericPromise::CreateAndResolve(true, __func__);
  }

  void ProcessInput(GraphTime aFrom, GraphTime aTo, uint32_t aFlags) override {}

 private:
  mozilla::Mutex mMutex;
  MediaTrackListener* mListener = nullptr;
  bool mSuspended = true;
  nsCOMPtr<nsITimer> mTimer;
  int mCount = 0;

  static void FakeAudioTrackGenerateData(nsITimer* timer, void* closure) {
    auto t = static_cast<FakeAudioTrack*>(closure);
    const int AUDIO_BUFFER_SIZE = 1600;
    const int NUM_CHANNELS = 2;

    mozilla::MutexAutoLock lock(t->mMutex);
    if (t->mSuspended) {
      return;
    }

    CheckedInt<size_t> bufferSize(sizeof(int16_t));
    bufferSize *= NUM_CHANNELS;
    bufferSize *= AUDIO_BUFFER_SIZE;
    RefPtr<mozilla::SharedBuffer> samples =
        mozilla::SharedBuffer::Create(bufferSize);
    int16_t* data = reinterpret_cast<int16_t*>(samples->Data());
    for (int i = 0; i < (AUDIO_BUFFER_SIZE * NUM_CHANNELS); i++) {
      // saw tooth audio sample
      data[i] = ((t->mCount % 8) * 4000) - (7 * 4000) / 2;
      t->mCount++;
    }

    mozilla::AudioSegment segment;
    AutoTArray<const int16_t*, 1> channels;
    channels.AppendElement(data);
    segment.AppendFrames(samples.forget(), channels, AUDIO_BUFFER_SIZE,
                         PRINCIPAL_HANDLE_NONE);

    if (t->mListener) {
      t->mListener->NotifyQueuedChanges(nullptr, 0, segment);
    }
  }
};

class LoopbackTransport : public MediaTransportHandler {
 public:
  LoopbackTransport() : MediaTransportHandler(nullptr) {
    SetState("mux", TransportLayer::TS_INIT, false);
    SetState("mux", TransportLayer::TS_INIT, true);
    SetState("non-mux", TransportLayer::TS_INIT, false);
    SetState("non-mux", TransportLayer::TS_INIT, true);
  }

  static void InitAndConnect(LoopbackTransport& client,
                             LoopbackTransport& server) {
    client.Connect(&server);
    server.Connect(&client);
  }

  void Connect(LoopbackTransport* peer) { peer_ = peer; }

  void Shutdown() { peer_ = nullptr; }

  RefPtr<IceLogPromise> GetIceLog(const nsCString& aPattern) override {
    return nullptr;
  }

  void ClearIceLog() override {}
  void EnterPrivateMode() override {}
  void ExitPrivateMode() override {}

  nsresult CreateIceCtx(const std::string& aName,
                        const nsTArray<dom::RTCIceServer>& aIceServers,
                        dom::RTCIceTransportPolicy aIcePolicy) override {
    return NS_OK;
  }

  void Destroy() override {}

  // We will probably be able to move the proxy lookup stuff into
  // this class once we move mtransport to its own process.
  void SetProxyConfig(NrSocketProxyConfig&& aProxyConfig) override {}

  void EnsureProvisionalTransport(const std::string& aTransportId,
                                  const std::string& aLocalUfrag,
                                  const std::string& aLocalPwd,
                                  size_t aComponentCount) override {}

  void SetTargetForDefaultLocalAddressLookup(const std::string& aTargetIp,
                                             uint16_t aTargetPort) override {}

  // We set default-route-only as late as possible because it depends on what
  // capture permissions have been granted on the window, which could easily
  // change between Init (ie; when the PC is created) and StartIceGathering
  // (ie; when we set the local description).
  void StartIceGathering(bool aDefaultRouteOnly, bool aObfuscateAddresses,
                         // TODO: It probably makes sense to look
                         // this up internally
                         const nsTArray<NrIceStunAddr>& aStunAddrs) override {}

  void ActivateTransport(
      const std::string& aTransportId, const std::string& aLocalUfrag,
      const std::string& aLocalPwd, size_t aComponentCount,
      const std::string& aUfrag, const std::string& aPassword,
      const nsTArray<uint8_t>& aKeyDer, const nsTArray<uint8_t>& aCertDer,
      SSLKEAType aAuthType, bool aDtlsClient, const DtlsDigestList& aDigests,
      bool aPrivacyRequested) override {}

  void RemoveTransportsExcept(
      const std::set<std::string>& aTransportIds) override {}

  void StartIceChecks(bool aIsControlling,
                      const std::vector<std::string>& aIceOptions) override {}

  void AddIceCandidate(const std::string& aTransportId,
                       const std::string& aCandidate, const std::string& aUfrag,
                       const std::string& aObfuscatedAddress) override {}

  void UpdateNetworkState(bool aOnline) override {}

  RefPtr<dom::RTCStatsPromise> GetIceStats(const std::string& aTransportId,
                                           DOMHighResTimeStamp aNow) override {
    return nullptr;
  }

  void SendPacket(const std::string& aTransportId,
                  MediaPacket&& aPacket) override {
    peer_->SignalPacketReceived(aTransportId, aPacket);
  }

  void SetState(const std::string& aTransportId, TransportLayer::State aState,
                bool aRtcp) {
    if (aRtcp) {
      MediaTransportHandler::OnRtcpStateChange(aTransportId, aState);
    } else {
      MediaTransportHandler::OnStateChange(aTransportId, aState);
    }
  }

 private:
  RefPtr<MediaTransportHandler> peer_;
};

class TestAgent {
 public:
  TestAgent()
      : audio_config_(109, "opus", 48000, 2, false),
        audio_conduit_(mozilla::AudioSessionConduit::Create(
            WebRtcCallWrapper::Create(dom::RTCStatsTimestampMaker()),
            test_utils->sts_target())),
        audio_pipeline_(),
        transport_(new LoopbackTransport) {}

  static void Connect(TestAgent* client, TestAgent* server) {
    LoopbackTransport::InitAndConnect(*client->transport_, *server->transport_);
  }

  virtual void CreatePipeline(const std::string& aTransportId) = 0;

  void SetState(const std::string& aTransportId, TransportLayer::State aState,
                bool aRtcp) {
    test_utils->sts_target()->Dispatch(
        WrapRunnable(transport_, &LoopbackTransport::SetState, aTransportId,
                     aState, aRtcp),
        nsISerialEventTarget::DISPATCH_SYNC);
  }

  void UpdateTransport(const std::string& aTransportId,
                       UniquePtr<MediaPipelineFilter>&& aFilter) {
    test_utils->sts_target()->Dispatch(
        NS_NewRunnableFunction(__func__,
                               [pipeline = audio_pipeline_, aTransportId,
                                filter = std::move(aFilter)]() mutable {
                                 pipeline->UpdateTransport_s(aTransportId,
                                                             std::move(filter));
                               }),
        nsISerialEventTarget::DISPATCH_SYNC);
  }

  void Stop() {
    MOZ_MTLOG(ML_DEBUG, "Stopping");

    if (audio_pipeline_) audio_pipeline_->Stop();
  }

  void Shutdown_s() { transport_->Shutdown(); }

  void Shutdown() {
    if (audio_pipeline_) audio_pipeline_->Shutdown_m();

    test_utils->sts_target()->Dispatch(
        WrapRunnable(this, &TestAgent::Shutdown_s),
        nsISerialEventTarget::DISPATCH_SYNC);
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

  int GetAudioRtpCountSent() { return audio_pipeline_->RtpPacketsSent(); }

  int GetAudioRtpCountReceived() {
    return audio_pipeline_->RtpPacketsReceived();
  }

  int GetAudioRtcpCountSent() { return audio_pipeline_->RtcpPacketsSent(); }

  int GetAudioRtcpCountReceived() {
    return audio_pipeline_->RtcpPacketsReceived();
  }

 protected:
  mozilla::AudioCodecConfig audio_config_;
  RefPtr<mozilla::MediaSessionConduit> audio_conduit_;
  RefPtr<FakeAudioTrack> audio_track_;
  // TODO(bcampen@mozilla.com): Right now this does not let us test RTCP in
  // both directions; only the sender's RTCP is sent, but the receiver should
  // be sending it too.
  RefPtr<mozilla::MediaPipeline> audio_pipeline_;
  RefPtr<LoopbackTransport> transport_;
};

class TestAgentSend : public TestAgent {
 public:
  TestAgentSend() {
    mozilla::MediaConduitErrorCode err =
        static_cast<mozilla::AudioSessionConduit*>(audio_conduit_.get())
            ->ConfigureSendMediaCodec(&audio_config_);
    EXPECT_EQ(mozilla::kMediaConduitNoError, err);

    audio_track_ = new FakeAudioTrack();
  }

  virtual void CreatePipeline(const std::string& aTransportId) {
    std::string test_pc;

    RefPtr<MediaPipelineTransmit> audio_pipeline =
        new mozilla::MediaPipelineTransmit(test_pc, transport_, nullptr,
                                           test_utils->sts_target(), false,
                                           audio_conduit_);

    audio_pipeline->SetSendTrack(audio_track_);
    audio_pipeline->Start();

    audio_pipeline_ = audio_pipeline;

    audio_pipeline_->UpdateTransport_m(aTransportId, nullptr);
  }
};

class TestAgentReceive : public TestAgent {
 public:
  TestAgentReceive() {
    std::vector<UniquePtr<mozilla::AudioCodecConfig>> codecs;
    codecs.emplace_back(new AudioCodecConfig(audio_config_));

    mozilla::MediaConduitErrorCode err =
        static_cast<mozilla::AudioSessionConduit*>(audio_conduit_.get())
            ->ConfigureRecvMediaCodecs(codecs);
    EXPECT_EQ(mozilla::kMediaConduitNoError, err);
  }

  virtual void CreatePipeline(const std::string& aTransportId) {
    std::string test_pc;

    audio_pipeline_ = new mozilla::MediaPipelineReceiveAudio(
        test_pc, transport_, nullptr, test_utils->sts_target(),
        static_cast<mozilla::AudioSessionConduit*>(audio_conduit_.get()),
        nullptr, PRINCIPAL_HANDLE_NONE);

    audio_pipeline_->Start();

    audio_pipeline_->UpdateTransport_m(aTransportId, std::move(bundle_filter_));
  }

  void SetBundleFilter(UniquePtr<MediaPipelineFilter>&& filter) {
    bundle_filter_ = std::move(filter);
  }

  void UpdateTransport_s(const std::string& aTransportId,
                         UniquePtr<MediaPipelineFilter>&& filter) {
    audio_pipeline_->UpdateTransport_s(aTransportId, std::move(filter));
  }

 private:
  UniquePtr<MediaPipelineFilter> bundle_filter_;
};

void WaitFor(TimeDuration aDuration) {
  bool done = false;
  NS_DelayedDispatchToCurrentThread(
      NS_NewRunnableFunction(__func__, [&] { done = true; }),
      aDuration.ToMilliseconds());
  SpinEventLoopUntil<ProcessFailureBehavior::IgnoreAndContinue>(
      [&] { return done; });
}

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
  void InitTransports() {
    test_utils->sts_target()->Dispatch(
        WrapRunnableNM(&TestAgent::Connect, &p2_, &p1_),
        nsISerialEventTarget::DISPATCH_SYNC);
  }

  // Verify RTP and RTCP
  void TestAudioSend(bool aIsRtcpMux,
                     UniquePtr<MediaPipelineFilter>&& initialFilter = nullptr,
                     UniquePtr<MediaPipelineFilter>&& refinedFilter = nullptr,
                     unsigned int ms_until_filter_update = 500,
                     unsigned int ms_of_traffic_after_answer = 10000) {
    bool bundle = !!(initialFilter);
    // We do not support testing bundle without rtcp mux, since that doesn't
    // make any sense.
    ASSERT_FALSE(!aIsRtcpMux && bundle);

    p2_.SetBundleFilter(std::move(initialFilter));

    // Setup transport flows
    InitTransports();

    std::string transportId = aIsRtcpMux ? "mux" : "non-mux";
    p1_.CreatePipeline(transportId);
    p2_.CreatePipeline(transportId);

    // Set state of transports to CONNECTING. MediaPipeline doesn't really care
    // about this transition, but we're trying to simluate what happens in a
    // real case.
    p1_.SetState(transportId, TransportLayer::TS_CONNECTING, false);
    p1_.SetState(transportId, TransportLayer::TS_CONNECTING, true);
    p2_.SetState(transportId, TransportLayer::TS_CONNECTING, false);
    p2_.SetState(transportId, TransportLayer::TS_CONNECTING, true);

    WaitFor(TimeDuration::FromMilliseconds(10));

    // Set state of transports to OPEN (ie; connected). This should result in
    // media flowing.
    p1_.SetState(transportId, TransportLayer::TS_OPEN, false);
    p1_.SetState(transportId, TransportLayer::TS_OPEN, true);
    p2_.SetState(transportId, TransportLayer::TS_OPEN, false);
    p2_.SetState(transportId, TransportLayer::TS_OPEN, true);

    if (bundle) {
      WaitFor(TimeDuration::FromMilliseconds(ms_until_filter_update));

      // Leaving refinedFilter not set implies we want to just update with
      // the other side's SSRC
      if (!refinedFilter) {
        refinedFilter = MakeUnique<MediaPipelineFilter>();
        // Might not be safe, strictly speaking.
        refinedFilter->AddRemoteSSRC(p1_.GetLocalSSRC());
      }

      p2_.UpdateTransport(transportId, std::move(refinedFilter));
    }

    // wait for some RTP/RTCP tx and rx to happen
    WaitFor(TimeDuration::FromMilliseconds(ms_of_traffic_after_answer));

    p1_.Stop();
    p2_.Stop();

    // wait for any packets in flight to arrive
    WaitFor(TimeDuration::FromMilliseconds(200));

    p1_.Shutdown();
    p2_.Shutdown();

    if (!bundle) {
      // If we are filtering, allow the test-case to do this checking.
      ASSERT_GE(p1_.GetAudioRtpCountSent(), 40);
      ASSERT_EQ(p1_.GetAudioRtpCountReceived(), p2_.GetAudioRtpCountSent());
      ASSERT_EQ(p1_.GetAudioRtpCountSent(), p2_.GetAudioRtpCountReceived());
    }

    // No RTCP packets should have been dropped, because we do not filter them.
    // Calling ShutdownMedia_m on both pipelines does not stop the flow of
    // RTCP. So, we might be off by one here.
    ASSERT_LE(p2_.GetAudioRtcpCountReceived(), p1_.GetAudioRtcpCountSent());
    ASSERT_GE(p2_.GetAudioRtcpCountReceived() + 1, p1_.GetAudioRtcpCountSent());
  }

  void TestAudioReceiverBundle(
      bool bundle_accepted, UniquePtr<MediaPipelineFilter>&& initialFilter,
      UniquePtr<MediaPipelineFilter>&& refinedFilter = nullptr,
      unsigned int ms_until_answer = 500,
      unsigned int ms_of_traffic_after_answer = 10000) {
    TestAudioSend(true, std::move(initialFilter), std::move(refinedFilter),
                  ms_until_answer, ms_of_traffic_after_answer);
  }

 protected:
  TestAgentSend p1_;
  TestAgentReceive p2_;
};

class MediaPipelineFilterTest : public ::testing::Test {
 public:
  bool Filter(MediaPipelineFilter& filter, uint32_t ssrc, uint8_t payload_type,
              const mozilla::Maybe<std::string>& mid = mozilla::Nothing()) {
    webrtc::RTPHeader header;
    header.ssrc = ssrc;
    header.payloadType = payload_type;
    mid.apply([&](const auto& mid) {
      header.extension.mid.Set(mid.c_str(), mid.length());
    });
    return filter.Filter(header);
  }
};

TEST_F(MediaPipelineFilterTest, TestConstruct) { MediaPipelineFilter filter; }

TEST_F(MediaPipelineFilterTest, TestDefault) {
  MediaPipelineFilter filter;
  EXPECT_FALSE(Filter(filter, 233, 110));
}

TEST_F(MediaPipelineFilterTest, TestSSRCFilter) {
  MediaPipelineFilter filter;
  filter.AddRemoteSSRC(555);
  EXPECT_TRUE(Filter(filter, 555, 110));
  EXPECT_FALSE(Filter(filter, 556, 110));
}

#define SSRC(ssrc)                                                    \
  ((ssrc >> 24) & 0xFF), ((ssrc >> 16) & 0xFF), ((ssrc >> 8) & 0xFF), \
      (ssrc & 0xFF)
#define REPORT_FRAGMENT(ssrc) \
  SSRC(ssrc), 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0

#define RTCP_TYPEINFO(num_rrs, type, size) 0x80 + num_rrs, type, 0, size

TEST_F(MediaPipelineFilterTest, TestMidFilter) {
  MediaPipelineFilter filter;
  const auto mid = Some(std::string("mid0"));
  filter.SetRemoteMediaStreamId(mid);

  EXPECT_FALSE(Filter(filter, 16, 110));
  EXPECT_TRUE(Filter(filter, 16, 110, mid));
  EXPECT_TRUE(Filter(filter, 16, 110));
  EXPECT_FALSE(Filter(filter, 17, 110));
}

TEST_F(MediaPipelineFilterTest, TestPayloadTypeFilter) {
  MediaPipelineFilter filter;
  filter.AddUniquePT(110);
  EXPECT_TRUE(Filter(filter, 555, 110));
  EXPECT_FALSE(Filter(filter, 556, 111));
}

TEST_F(MediaPipelineFilterTest, TestSSRCMovedWithMid) {
  MediaPipelineFilter filter;
  const auto mid0 = Some(std::string("mid0"));
  const auto mid1 = Some(std::string("mid1"));
  filter.SetRemoteMediaStreamId(mid0);
  ASSERT_TRUE(Filter(filter, 555, 110, mid0));
  ASSERT_TRUE(Filter(filter, 555, 110));
  // Present a new MID binding
  ASSERT_FALSE(Filter(filter, 555, 110, mid1));
  ASSERT_FALSE(Filter(filter, 555, 110));
}

TEST_F(MediaPipelineFilterTest, TestRemoteSDPNoSSRCs) {
  // If the remote SDP doesn't have SSRCs, right now this is a no-op and
  // there is no point of even incorporating a filter, but we make the
  // behavior consistent to avoid confusion.
  MediaPipelineFilter filter;
  const auto mid = Some(std::string("mid0"));
  filter.SetRemoteMediaStreamId(mid);
  filter.AddUniquePT(111);
  EXPECT_TRUE(Filter(filter, 555, 110, mid));
  EXPECT_TRUE(Filter(filter, 555, 110));

  // Update but remember binding./
  MediaPipelineFilter filter2;

  filter.Update(filter2);

  // Ensure that the old SSRC still works.
  EXPECT_TRUE(Filter(filter, 555, 110));

  // Forget the previous binding
  MediaPipelineFilter filter3;
  filter3.SetRemoteMediaStreamId(Some(std::string("mid1")));
  filter.Update(filter3);

  ASSERT_FALSE(Filter(filter, 555, 110));
}

TEST_F(MediaPipelineTest, TestAudioSendNoMux) { TestAudioSend(false); }

TEST_F(MediaPipelineTest, TestAudioSendMux) { TestAudioSend(true); }

TEST_F(MediaPipelineTest, TestAudioSendBundle) {
  auto filter = MakeUnique<MediaPipelineFilter>();
  // These durations have to be _extremely_ long to have any assurance that
  // some RTCP will be sent at all. This is because the first RTCP packet
  // is sometimes sent before the transports are ready, which causes it to
  // be dropped.
  TestAudioReceiverBundle(
      true, std::move(filter),
      // We do not specify the filter for the remote description, so it will be
      // set to something sane after a short time.
      nullptr, 10000, 10000);

  // Some packets should have been dropped, but not all
  ASSERT_GT(p1_.GetAudioRtpCountSent(), p2_.GetAudioRtpCountReceived());
  ASSERT_GT(p2_.GetAudioRtpCountReceived(), 40);
  ASSERT_GT(p1_.GetAudioRtcpCountSent(), 1);
}

TEST_F(MediaPipelineTest, TestAudioSendEmptyBundleFilter) {
  auto filter = MakeUnique<MediaPipelineFilter>();
  auto bad_answer_filter = MakeUnique<MediaPipelineFilter>();
  TestAudioReceiverBundle(true, std::move(filter),
                          std::move(bad_answer_filter));
  // Filter is empty, so should drop everything.
  ASSERT_EQ(0, p2_.GetAudioRtpCountReceived());
}

}  // end namespace
