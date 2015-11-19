/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <string.h>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/base/md5digest.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/base/thread_annotations.h"
#include "webrtc/modules/audio_coding/main/acm2/acm_receive_test_oldapi.h"
#include "webrtc/modules/audio_coding/main/acm2/acm_send_test_oldapi.h"
#include "webrtc/modules/audio_coding/main/interface/audio_coding_module.h"
#include "webrtc/modules/audio_coding/main/interface/audio_coding_module_typedefs.h"
#include "webrtc/modules/audio_coding/neteq/tools/audio_checksum.h"
#include "webrtc/modules/audio_coding/neteq/tools/audio_loop.h"
#include "webrtc/modules/audio_coding/neteq/tools/constant_pcm_packet_source.h"
#include "webrtc/modules/audio_coding/neteq/tools/input_audio_file.h"
#include "webrtc/modules/audio_coding/neteq/tools/output_audio_file.h"
#include "webrtc/modules/audio_coding/neteq/tools/packet.h"
#include "webrtc/modules/audio_coding/neteq/tools/rtp_file_source.h"
#include "webrtc/modules/interface/module_common_types.h"
#include "webrtc/system_wrappers/interface/clock.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/event_wrapper.h"
#include "webrtc/system_wrappers/interface/sleep.h"
#include "webrtc/system_wrappers/interface/thread_wrapper.h"
#include "webrtc/test/testsupport/fileutils.h"
#include "webrtc/test/testsupport/gtest_disable.h"

namespace webrtc {

namespace {
const int kSampleRateHz = 16000;
const int kNumSamples10ms = kSampleRateHz / 100;
const int kFrameSizeMs = 10;  // Multiple of 10.
const int kFrameSizeSamples = kFrameSizeMs / 10 * kNumSamples10ms;
const int kPayloadSizeBytes = kFrameSizeSamples * sizeof(int16_t);
const uint8_t kPayloadType = 111;
}  // namespace

class RtpUtility {
 public:
  RtpUtility(int samples_per_packet, uint8_t payload_type)
      : samples_per_packet_(samples_per_packet), payload_type_(payload_type) {}

  virtual ~RtpUtility() {}

  void Populate(WebRtcRTPHeader* rtp_header) {
    rtp_header->header.sequenceNumber = 0xABCD;
    rtp_header->header.timestamp = 0xABCDEF01;
    rtp_header->header.payloadType = payload_type_;
    rtp_header->header.markerBit = false;
    rtp_header->header.ssrc = 0x1234;
    rtp_header->header.numCSRCs = 0;
    rtp_header->frameType = kAudioFrameSpeech;

    rtp_header->header.payload_type_frequency = kSampleRateHz;
    rtp_header->type.Audio.channel = 1;
    rtp_header->type.Audio.isCNG = false;
  }

  void Forward(WebRtcRTPHeader* rtp_header) {
    ++rtp_header->header.sequenceNumber;
    rtp_header->header.timestamp += samples_per_packet_;
  }

 private:
  int samples_per_packet_;
  uint8_t payload_type_;
};

class PacketizationCallbackStubOldApi : public AudioPacketizationCallback {
 public:
  PacketizationCallbackStubOldApi()
      : num_calls_(0),
        last_frame_type_(kFrameEmpty),
        last_payload_type_(-1),
        last_timestamp_(0),
        crit_sect_(CriticalSectionWrapper::CreateCriticalSection()) {}

  int32_t SendData(FrameType frame_type,
                   uint8_t payload_type,
                   uint32_t timestamp,
                   const uint8_t* payload_data,
                   size_t payload_len_bytes,
                   const RTPFragmentationHeader* fragmentation) override {
    CriticalSectionScoped lock(crit_sect_.get());
    ++num_calls_;
    last_frame_type_ = frame_type;
    last_payload_type_ = payload_type;
    last_timestamp_ = timestamp;
    last_payload_vec_.assign(payload_data, payload_data + payload_len_bytes);
    return 0;
  }

  int num_calls() const {
    CriticalSectionScoped lock(crit_sect_.get());
    return num_calls_;
  }

  int last_payload_len_bytes() const {
    CriticalSectionScoped lock(crit_sect_.get());
    return last_payload_vec_.size();
  }

  FrameType last_frame_type() const {
    CriticalSectionScoped lock(crit_sect_.get());
    return last_frame_type_;
  }

  int last_payload_type() const {
    CriticalSectionScoped lock(crit_sect_.get());
    return last_payload_type_;
  }

  uint32_t last_timestamp() const {
    CriticalSectionScoped lock(crit_sect_.get());
    return last_timestamp_;
  }

  void SwapBuffers(std::vector<uint8_t>* payload) {
    CriticalSectionScoped lock(crit_sect_.get());
    last_payload_vec_.swap(*payload);
  }

 private:
  int num_calls_ GUARDED_BY(crit_sect_);
  FrameType last_frame_type_ GUARDED_BY(crit_sect_);
  int last_payload_type_ GUARDED_BY(crit_sect_);
  uint32_t last_timestamp_ GUARDED_BY(crit_sect_);
  std::vector<uint8_t> last_payload_vec_ GUARDED_BY(crit_sect_);
  const rtc::scoped_ptr<CriticalSectionWrapper> crit_sect_;
};

class AudioCodingModuleTestOldApi : public ::testing::Test {
 protected:
  AudioCodingModuleTestOldApi()
      : id_(1),
        rtp_utility_(new RtpUtility(kFrameSizeSamples, kPayloadType)),
        clock_(Clock::GetRealTimeClock()) {}

  ~AudioCodingModuleTestOldApi() {}

  void TearDown() {}

  void SetUp() {
    acm_.reset(AudioCodingModule::Create(id_, clock_));

    rtp_utility_->Populate(&rtp_header_);

    input_frame_.sample_rate_hz_ = kSampleRateHz;
    input_frame_.num_channels_ = 1;
    input_frame_.samples_per_channel_ = kSampleRateHz * 10 / 1000;  // 10 ms.
    static_assert(kSampleRateHz * 10 / 1000 <= AudioFrame::kMaxDataSizeSamples,
                  "audio frame too small");
    memset(input_frame_.data_,
           0,
           input_frame_.samples_per_channel_ * sizeof(input_frame_.data_[0]));

    ASSERT_EQ(0, acm_->RegisterTransportCallback(&packet_cb_));

    SetUpL16Codec();
  }

  // Set up L16 codec.
  virtual void SetUpL16Codec() {
    ASSERT_EQ(0, AudioCodingModule::Codec("L16", &codec_, kSampleRateHz, 1));
    codec_.pltype = kPayloadType;
  }

  virtual void RegisterCodec() {
    ASSERT_EQ(0, acm_->RegisterReceiveCodec(codec_));
    ASSERT_EQ(0, acm_->RegisterSendCodec(codec_));
  }

  virtual void InsertPacketAndPullAudio() {
    InsertPacket();
    PullAudio();
  }

  virtual void InsertPacket() {
    const uint8_t kPayload[kPayloadSizeBytes] = {0};
    ASSERT_EQ(0,
              acm_->IncomingPacket(kPayload, kPayloadSizeBytes, rtp_header_));
    rtp_utility_->Forward(&rtp_header_);
  }

  virtual void PullAudio() {
    AudioFrame audio_frame;
    ASSERT_EQ(0, acm_->PlayoutData10Ms(-1, &audio_frame));
  }

  virtual void InsertAudio() {
    ASSERT_GE(acm_->Add10MsData(input_frame_), 0);
    input_frame_.timestamp_ += kNumSamples10ms;
  }

  virtual void VerifyEncoding() {
    int last_length = packet_cb_.last_payload_len_bytes();
    EXPECT_TRUE(last_length == 2 * codec_.pacsize || last_length == 0)
        << "Last encoded packet was " << last_length << " bytes.";
  }

  virtual void InsertAudioAndVerifyEncoding() {
    InsertAudio();
    VerifyEncoding();
  }

  const int id_;
  rtc::scoped_ptr<RtpUtility> rtp_utility_;
  rtc::scoped_ptr<AudioCodingModule> acm_;
  PacketizationCallbackStubOldApi packet_cb_;
  WebRtcRTPHeader rtp_header_;
  AudioFrame input_frame_;
  CodecInst codec_;
  Clock* clock_;
};

// Check if the statistics are initialized correctly. Before any call to ACM
// all fields have to be zero.
TEST_F(AudioCodingModuleTestOldApi, DISABLED_ON_ANDROID(InitializedToZero)) {
  RegisterCodec();
  AudioDecodingCallStats stats;
  acm_->GetDecodingCallStatistics(&stats);
  EXPECT_EQ(0, stats.calls_to_neteq);
  EXPECT_EQ(0, stats.calls_to_silence_generator);
  EXPECT_EQ(0, stats.decoded_normal);
  EXPECT_EQ(0, stats.decoded_cng);
  EXPECT_EQ(0, stats.decoded_plc);
  EXPECT_EQ(0, stats.decoded_plc_cng);
}

// Apply an initial playout delay. Calls to AudioCodingModule::PlayoutData10ms()
// should result in generating silence, check the associated field.
TEST_F(AudioCodingModuleTestOldApi,
       DISABLED_ON_ANDROID(SilenceGeneratorCalled)) {
  RegisterCodec();
  AudioDecodingCallStats stats;
  const int kInitialDelay = 100;

  acm_->SetInitialPlayoutDelay(kInitialDelay);

  int num_calls = 0;
  for (int time_ms = 0; time_ms < kInitialDelay;
       time_ms += kFrameSizeMs, ++num_calls) {
    InsertPacketAndPullAudio();
  }
  acm_->GetDecodingCallStatistics(&stats);
  EXPECT_EQ(0, stats.calls_to_neteq);
  EXPECT_EQ(num_calls, stats.calls_to_silence_generator);
  EXPECT_EQ(0, stats.decoded_normal);
  EXPECT_EQ(0, stats.decoded_cng);
  EXPECT_EQ(0, stats.decoded_plc);
  EXPECT_EQ(0, stats.decoded_plc_cng);
}

// Insert some packets and pull audio. Check statistics are valid. Then,
// simulate packet loss and check if PLC and PLC-to-CNG statistics are
// correctly updated.
TEST_F(AudioCodingModuleTestOldApi, DISABLED_ON_ANDROID(NetEqCalls)) {
  RegisterCodec();
  AudioDecodingCallStats stats;
  const int kNumNormalCalls = 10;

  for (int num_calls = 0; num_calls < kNumNormalCalls; ++num_calls) {
    InsertPacketAndPullAudio();
  }
  acm_->GetDecodingCallStatistics(&stats);
  EXPECT_EQ(kNumNormalCalls, stats.calls_to_neteq);
  EXPECT_EQ(0, stats.calls_to_silence_generator);
  EXPECT_EQ(kNumNormalCalls, stats.decoded_normal);
  EXPECT_EQ(0, stats.decoded_cng);
  EXPECT_EQ(0, stats.decoded_plc);
  EXPECT_EQ(0, stats.decoded_plc_cng);

  const int kNumPlc = 3;
  const int kNumPlcCng = 5;

  // Simulate packet-loss. NetEq first performs PLC then PLC fades to CNG.
  for (int n = 0; n < kNumPlc + kNumPlcCng; ++n) {
    PullAudio();
  }
  acm_->GetDecodingCallStatistics(&stats);
  EXPECT_EQ(kNumNormalCalls + kNumPlc + kNumPlcCng, stats.calls_to_neteq);
  EXPECT_EQ(0, stats.calls_to_silence_generator);
  EXPECT_EQ(kNumNormalCalls, stats.decoded_normal);
  EXPECT_EQ(0, stats.decoded_cng);
  EXPECT_EQ(kNumPlc, stats.decoded_plc);
  EXPECT_EQ(kNumPlcCng, stats.decoded_plc_cng);
}

TEST_F(AudioCodingModuleTestOldApi, VerifyOutputFrame) {
  AudioFrame audio_frame;
  const int kSampleRateHz = 32000;
  EXPECT_EQ(0, acm_->PlayoutData10Ms(kSampleRateHz, &audio_frame));
  EXPECT_EQ(id_, audio_frame.id_);
  EXPECT_EQ(0u, audio_frame.timestamp_);
  EXPECT_GT(audio_frame.num_channels_, 0);
  EXPECT_EQ(kSampleRateHz / 100, audio_frame.samples_per_channel_);
  EXPECT_EQ(kSampleRateHz, audio_frame.sample_rate_hz_);
}

TEST_F(AudioCodingModuleTestOldApi, FailOnZeroDesiredFrequency) {
  AudioFrame audio_frame;
  EXPECT_EQ(-1, acm_->PlayoutData10Ms(0, &audio_frame));
}

// Checks that the transport callback is invoked once for each speech packet.
// Also checks that the frame type is kAudioFrameSpeech.
TEST_F(AudioCodingModuleTestOldApi, TransportCallbackIsInvokedForEachPacket) {
  const int k10MsBlocksPerPacket = 3;
  codec_.pacsize = k10MsBlocksPerPacket * kSampleRateHz / 100;
  RegisterCodec();
  const int kLoops = 10;
  for (int i = 0; i < kLoops; ++i) {
    EXPECT_EQ(i / k10MsBlocksPerPacket, packet_cb_.num_calls());
    if (packet_cb_.num_calls() > 0)
      EXPECT_EQ(kAudioFrameSpeech, packet_cb_.last_frame_type());
    InsertAudioAndVerifyEncoding();
  }
  EXPECT_EQ(kLoops / k10MsBlocksPerPacket, packet_cb_.num_calls());
  EXPECT_EQ(kAudioFrameSpeech, packet_cb_.last_frame_type());
}

// Verifies that the RTP timestamp series is not reset when the codec is
// changed.
TEST_F(AudioCodingModuleTestOldApi, TimestampSeriesContinuesWhenCodecChanges) {
  RegisterCodec();  // This registers the default codec.
  uint32_t expected_ts = input_frame_.timestamp_;
  int blocks_per_packet = codec_.pacsize / (kSampleRateHz / 100);
  // Encode 5 packets of the first codec type.
  const int kNumPackets1 = 5;
  for (int j = 0; j < kNumPackets1; ++j) {
    for (int i = 0; i < blocks_per_packet; ++i) {
      EXPECT_EQ(j, packet_cb_.num_calls());
      InsertAudio();
    }
    EXPECT_EQ(j + 1, packet_cb_.num_calls());
    EXPECT_EQ(expected_ts, packet_cb_.last_timestamp());
    expected_ts += codec_.pacsize;
  }

  // Change codec.
  ASSERT_EQ(0, AudioCodingModule::Codec("ISAC", &codec_, kSampleRateHz, 1));
  RegisterCodec();
  blocks_per_packet = codec_.pacsize / (kSampleRateHz / 100);
  // Encode another 5 packets.
  const int kNumPackets2 = 5;
  for (int j = 0; j < kNumPackets2; ++j) {
    for (int i = 0; i < blocks_per_packet; ++i) {
      EXPECT_EQ(kNumPackets1 + j, packet_cb_.num_calls());
      InsertAudio();
    }
    EXPECT_EQ(kNumPackets1 + j + 1, packet_cb_.num_calls());
    EXPECT_EQ(expected_ts, packet_cb_.last_timestamp());
    expected_ts += codec_.pacsize;
  }
}

// Introduce this class to set different expectations on the number of encoded
// bytes. This class expects all encoded packets to be 9 bytes (matching one
// CNG SID frame) or 0 bytes. This test depends on |input_frame_| containing
// (near-)zero values. It also introduces a way to register comfort noise with
// a custom payload type.
class AudioCodingModuleTestWithComfortNoiseOldApi
    : public AudioCodingModuleTestOldApi {
 protected:
  void RegisterCngCodec(int rtp_payload_type) {
    CodecInst codec;
    AudioCodingModule::Codec("CN", &codec, kSampleRateHz, 1);
    codec.pltype = rtp_payload_type;
    ASSERT_EQ(0, acm_->RegisterReceiveCodec(codec));
    ASSERT_EQ(0, acm_->RegisterSendCodec(codec));
  }

  void VerifyEncoding() override {
    int last_length = packet_cb_.last_payload_len_bytes();
    EXPECT_TRUE(last_length == 9 || last_length == 0)
        << "Last encoded packet was " << last_length << " bytes.";
  }

  void DoTest(int blocks_per_packet, int cng_pt) {
    const int kLoops = 40;
    // This array defines the expected frame types, and when they should arrive.
    // We expect a frame to arrive each time the speech encoder would have
    // produced a packet, and once every 100 ms the frame should be non-empty,
    // that is contain comfort noise.
    const struct {
      int ix;
      FrameType type;
    } expectation[] = {{2, kAudioFrameCN},
                       {5, kFrameEmpty},
                       {8, kFrameEmpty},
                       {11, kAudioFrameCN},
                       {14, kFrameEmpty},
                       {17, kFrameEmpty},
                       {20, kAudioFrameCN},
                       {23, kFrameEmpty},
                       {26, kFrameEmpty},
                       {29, kFrameEmpty},
                       {32, kAudioFrameCN},
                       {35, kFrameEmpty},
                       {38, kFrameEmpty}};
    for (int i = 0; i < kLoops; ++i) {
      int num_calls_before = packet_cb_.num_calls();
      EXPECT_EQ(i / blocks_per_packet, num_calls_before);
      InsertAudioAndVerifyEncoding();
      int num_calls = packet_cb_.num_calls();
      if (num_calls == num_calls_before + 1) {
        EXPECT_EQ(expectation[num_calls - 1].ix, i);
        EXPECT_EQ(expectation[num_calls - 1].type, packet_cb_.last_frame_type())
            << "Wrong frame type for lap " << i;
        EXPECT_EQ(cng_pt, packet_cb_.last_payload_type());
      } else {
        EXPECT_EQ(num_calls, num_calls_before);
      }
    }
  }
};

// Checks that the transport callback is invoked once per frame period of the
// underlying speech encoder, even when comfort noise is produced.
// Also checks that the frame type is kAudioFrameCN or kFrameEmpty.
// This test and the next check the same thing, but differ in the order of
// speech codec and CNG registration.
TEST_F(AudioCodingModuleTestWithComfortNoiseOldApi,
       TransportCallbackTestForComfortNoiseRegisterCngLast) {
  const int k10MsBlocksPerPacket = 3;
  codec_.pacsize = k10MsBlocksPerPacket * kSampleRateHz / 100;
  RegisterCodec();
  const int kCngPayloadType = 105;
  RegisterCngCodec(kCngPayloadType);
  ASSERT_EQ(0, acm_->SetVAD(true, true));
  DoTest(k10MsBlocksPerPacket, kCngPayloadType);
}

TEST_F(AudioCodingModuleTestWithComfortNoiseOldApi,
       TransportCallbackTestForComfortNoiseRegisterCngFirst) {
  const int k10MsBlocksPerPacket = 3;
  codec_.pacsize = k10MsBlocksPerPacket * kSampleRateHz / 100;
  const int kCngPayloadType = 105;
  RegisterCngCodec(kCngPayloadType);
  RegisterCodec();
  ASSERT_EQ(0, acm_->SetVAD(true, true));
  DoTest(k10MsBlocksPerPacket, kCngPayloadType);
}

// A multi-threaded test for ACM. This base class is using the PCM16b 16 kHz
// codec, while the derive class AcmIsacMtTest is using iSAC.
class AudioCodingModuleMtTestOldApi : public AudioCodingModuleTestOldApi {
 protected:
  static const int kNumPackets = 500;
  static const int kNumPullCalls = 500;

  AudioCodingModuleMtTestOldApi()
      : AudioCodingModuleTestOldApi(),
        send_thread_(ThreadWrapper::CreateThread(CbSendThread, this, "send")),
        insert_packet_thread_(ThreadWrapper::CreateThread(
            CbInsertPacketThread, this, "insert_packet")),
        pull_audio_thread_(ThreadWrapper::CreateThread(
            CbPullAudioThread, this, "pull_audio")),
        test_complete_(EventWrapper::Create()),
        send_count_(0),
        insert_packet_count_(0),
        pull_audio_count_(0),
        crit_sect_(CriticalSectionWrapper::CreateCriticalSection()),
        next_insert_packet_time_ms_(0),
        fake_clock_(new SimulatedClock(0)) {
    clock_ = fake_clock_.get();
  }

  void SetUp() {
    AudioCodingModuleTestOldApi::SetUp();
    RegisterCodec();  // Must be called before the threads start below.
    StartThreads();
  }

  void StartThreads() {
    ASSERT_TRUE(send_thread_->Start());
    send_thread_->SetPriority(kRealtimePriority);
    ASSERT_TRUE(insert_packet_thread_->Start());
    insert_packet_thread_->SetPriority(kRealtimePriority);
    ASSERT_TRUE(pull_audio_thread_->Start());
    pull_audio_thread_->SetPriority(kRealtimePriority);
  }

  void TearDown() {
    AudioCodingModuleTestOldApi::TearDown();
    pull_audio_thread_->Stop();
    send_thread_->Stop();
    insert_packet_thread_->Stop();
  }

  EventTypeWrapper RunTest() {
    return test_complete_->Wait(10 * 60 * 1000);  // 10 minutes' timeout.
  }

  virtual bool TestDone() {
    if (packet_cb_.num_calls() > kNumPackets) {
      CriticalSectionScoped lock(crit_sect_.get());
      if (pull_audio_count_ > kNumPullCalls) {
        // Both conditions for completion are met. End the test.
        return true;
      }
    }
    return false;
  }

  static bool CbSendThread(void* context) {
    return reinterpret_cast<AudioCodingModuleMtTestOldApi*>(context)
        ->CbSendImpl();
  }

  // The send thread doesn't have to care about the current simulated time,
  // since only the AcmReceiver is using the clock.
  bool CbSendImpl() {
    SleepMs(1);
    if (HasFatalFailure()) {
      // End the test early if a fatal failure (ASSERT_*) has occurred.
      test_complete_->Set();
    }
    ++send_count_;
    InsertAudioAndVerifyEncoding();
    if (TestDone()) {
      test_complete_->Set();
    }
    return true;
  }

  static bool CbInsertPacketThread(void* context) {
    return reinterpret_cast<AudioCodingModuleMtTestOldApi*>(context)
        ->CbInsertPacketImpl();
  }

  bool CbInsertPacketImpl() {
    SleepMs(1);
    {
      CriticalSectionScoped lock(crit_sect_.get());
      if (clock_->TimeInMilliseconds() < next_insert_packet_time_ms_) {
        return true;
      }
      next_insert_packet_time_ms_ += 10;
    }
    // Now we're not holding the crit sect when calling ACM.
    ++insert_packet_count_;
    InsertPacket();
    return true;
  }

  static bool CbPullAudioThread(void* context) {
    return reinterpret_cast<AudioCodingModuleMtTestOldApi*>(context)
        ->CbPullAudioImpl();
  }

  bool CbPullAudioImpl() {
    SleepMs(1);
    {
      CriticalSectionScoped lock(crit_sect_.get());
      // Don't let the insert thread fall behind.
      if (next_insert_packet_time_ms_ < clock_->TimeInMilliseconds()) {
        return true;
      }
      ++pull_audio_count_;
    }
    // Now we're not holding the crit sect when calling ACM.
    PullAudio();
    fake_clock_->AdvanceTimeMilliseconds(10);
    return true;
  }

  rtc::scoped_ptr<ThreadWrapper> send_thread_;
  rtc::scoped_ptr<ThreadWrapper> insert_packet_thread_;
  rtc::scoped_ptr<ThreadWrapper> pull_audio_thread_;
  const rtc::scoped_ptr<EventWrapper> test_complete_;
  int send_count_;
  int insert_packet_count_;
  int pull_audio_count_ GUARDED_BY(crit_sect_);
  const rtc::scoped_ptr<CriticalSectionWrapper> crit_sect_;
  int64_t next_insert_packet_time_ms_ GUARDED_BY(crit_sect_);
  rtc::scoped_ptr<SimulatedClock> fake_clock_;
};

TEST_F(AudioCodingModuleMtTestOldApi, DoTest) {
  EXPECT_EQ(kEventSignaled, RunTest());
}

// This is a multi-threaded ACM test using iSAC. The test encodes audio
// from a PCM file. The most recent encoded frame is used as input to the
// receiving part. Depending on timing, it may happen that the same RTP packet
// is inserted into the receiver multiple times, but this is a valid use-case,
// and simplifies the test code a lot.
class AcmIsacMtTestOldApi : public AudioCodingModuleMtTestOldApi {
 protected:
  static const int kNumPackets = 500;
  static const int kNumPullCalls = 500;

  AcmIsacMtTestOldApi()
      : AudioCodingModuleMtTestOldApi(), last_packet_number_(0) {}

  ~AcmIsacMtTestOldApi() {}

  void SetUp() {
    AudioCodingModuleTestOldApi::SetUp();
    RegisterCodec();  // Must be called before the threads start below.

    // Set up input audio source to read from specified file, loop after 5
    // seconds, and deliver blocks of 10 ms.
    const std::string input_file_name =
        webrtc::test::ResourcePath("audio_coding/speech_mono_16kHz", "pcm");
    audio_loop_.Init(input_file_name, 5 * kSampleRateHz, kNumSamples10ms);

    // Generate one packet to have something to insert.
    int loop_counter = 0;
    while (packet_cb_.last_payload_len_bytes() == 0) {
      InsertAudio();
      ASSERT_LT(loop_counter++, 10);
    }
    // Set |last_packet_number_| to one less that |num_calls| so that the packet
    // will be fetched in the next InsertPacket() call.
    last_packet_number_ = packet_cb_.num_calls() - 1;

    StartThreads();
  }

  void RegisterCodec() override {
    static_assert(kSampleRateHz == 16000, "test designed for iSAC 16 kHz");
    AudioCodingModule::Codec("ISAC", &codec_, kSampleRateHz, 1);
    codec_.pltype = kPayloadType;

    // Register iSAC codec in ACM, effectively unregistering the PCM16B codec
    // registered in AudioCodingModuleTestOldApi::SetUp();
    ASSERT_EQ(0, acm_->RegisterReceiveCodec(codec_));
    ASSERT_EQ(0, acm_->RegisterSendCodec(codec_));
  }

  void InsertPacket() {
    int num_calls = packet_cb_.num_calls();  // Store locally for thread safety.
    if (num_calls > last_packet_number_) {
      // Get the new payload out from the callback handler.
      // Note that since we swap buffers here instead of directly inserting
      // a pointer to the data in |packet_cb_|, we avoid locking the callback
      // for the duration of the IncomingPacket() call.
      packet_cb_.SwapBuffers(&last_payload_vec_);
      ASSERT_GT(last_payload_vec_.size(), 0u);
      rtp_utility_->Forward(&rtp_header_);
      last_packet_number_ = num_calls;
    }
    ASSERT_GT(last_payload_vec_.size(), 0u);
    ASSERT_EQ(
        0,
        acm_->IncomingPacket(
            &last_payload_vec_[0], last_payload_vec_.size(), rtp_header_));
  }

  void InsertAudio() {
    memcpy(input_frame_.data_, audio_loop_.GetNextBlock(), kNumSamples10ms);
    AudioCodingModuleTestOldApi::InsertAudio();
  }

  // Override the verification function with no-op, since iSAC produces variable
  // payload sizes.
  void VerifyEncoding() override {}

  // This method is the same as AudioCodingModuleMtTestOldApi::TestDone(), but
  // here it is using the constants defined in this class (i.e., shorter test
  // run).
  virtual bool TestDone() {
    if (packet_cb_.num_calls() > kNumPackets) {
      CriticalSectionScoped lock(crit_sect_.get());
      if (pull_audio_count_ > kNumPullCalls) {
        // Both conditions for completion are met. End the test.
        return true;
      }
    }
    return false;
  }

  int last_packet_number_;
  std::vector<uint8_t> last_payload_vec_;
  test::AudioLoop audio_loop_;
};

TEST_F(AcmIsacMtTestOldApi, DoTest) {
  EXPECT_EQ(kEventSignaled, RunTest());
}

class AcmReceiverBitExactnessOldApi : public ::testing::Test {
 public:
  static std::string PlatformChecksum(std::string win64,
                                      std::string android,
                                      std::string others) {
#if defined(_WIN32) && defined(WEBRTC_ARCH_64_BITS)
    return win64;
#elif defined(WEBRTC_ANDROID)
    return android;
#else
    return others;
#endif
  }

 protected:
  void Run(int output_freq_hz, const std::string& checksum_ref) {
    const std::string input_file_name =
        webrtc::test::ResourcePath("audio_coding/neteq_universal_new", "rtp");
    rtc::scoped_ptr<test::RtpFileSource> packet_source(
        test::RtpFileSource::Create(input_file_name));
#ifdef WEBRTC_ANDROID
    // Filter out iLBC and iSAC-swb since they are not supported on Android.
    packet_source->FilterOutPayloadType(102);  // iLBC.
    packet_source->FilterOutPayloadType(104);  // iSAC-swb.
#endif

    test::AudioChecksum checksum;
    const std::string output_file_name =
        webrtc::test::OutputPath() +
        ::testing::UnitTest::GetInstance()
            ->current_test_info()
            ->test_case_name() +
        "_" + ::testing::UnitTest::GetInstance()->current_test_info()->name() +
        "_output.pcm";
    test::OutputAudioFile output_file(output_file_name);
    test::AudioSinkFork output(&checksum, &output_file);

    test::AcmReceiveTestOldApi test(
        packet_source.get(),
        &output,
        output_freq_hz,
        test::AcmReceiveTestOldApi::kArbitraryChannels);
    ASSERT_NO_FATAL_FAILURE(test.RegisterNetEqTestCodecs());
    test.Run();

    std::string checksum_string = checksum.Finish();
    EXPECT_EQ(checksum_ref, checksum_string);
  }
};

// Fails Android ARM64. https://code.google.com/p/webrtc/issues/detail?id=4199
#if defined(WEBRTC_ANDROID) && defined(__aarch64__)
#define MAYBE_8kHzOutput DISABLED_8kHzOutput
#else
#define MAYBE_8kHzOutput 8kHzOutput
#endif
TEST_F(AcmReceiverBitExactnessOldApi, MAYBE_8kHzOutput) {
  Run(8000,
      PlatformChecksum("dcee98c623b147ebe1b40dd30efa896e",
                       "adc92e173f908f93b96ba5844209815a",
                       "908002dc01fc4eb1d2be24eb1d3f354b"));
}

// Fails Android ARM64. https://code.google.com/p/webrtc/issues/detail?id=4199
#if defined(WEBRTC_ANDROID) && defined(__aarch64__)
#define MAYBE_16kHzOutput DISABLED_16kHzOutput
#else
#define MAYBE_16kHzOutput 16kHzOutput
#endif
TEST_F(AcmReceiverBitExactnessOldApi, MAYBE_16kHzOutput) {
  Run(16000,
      PlatformChecksum("f790e7a8cce4e2c8b7bb5e0e4c5dac0d",
                       "8cffa6abcb3e18e33b9d857666dff66a",
                       "a909560b5ca49fa472b17b7b277195e9"));
}

// Fails Android ARM64. https://code.google.com/p/webrtc/issues/detail?id=4199
#if defined(WEBRTC_ANDROID) && defined(__aarch64__)
#define MAYBE_32kHzOutput DISABLED_32kHzOutput
#else
#define MAYBE_32kHzOutput 32kHzOutput
#endif
TEST_F(AcmReceiverBitExactnessOldApi, MAYBE_32kHzOutput) {
  Run(32000,
      PlatformChecksum("306e0d990ee6e92de3fbecc0123ece37",
                       "3e126fe894720c3f85edadcc91964ba5",
                       "441aab4b347fb3db4e9244337aca8d8e"));
}

// Fails Android ARM64. https://code.google.com/p/webrtc/issues/detail?id=4199
#if defined(WEBRTC_ANDROID) && defined(__aarch64__)
#define MAYBE_48kHzOutput DISABLED_48kHzOutput
#else
#define MAYBE_48kHzOutput 48kHzOutput
#endif
TEST_F(AcmReceiverBitExactnessOldApi, MAYBE_48kHzOutput) {
  Run(48000,
      PlatformChecksum("aa7c232f63a67b2a72703593bdd172e0",
                       "0155665e93067c4e89256b944dd11999",
                       "4ee2730fa1daae755e8a8fd3abd779ec"));
}

// This test verifies bit exactness for the send-side of ACM. The test setup is
// a chain of three different test classes:
//
// test::AcmSendTest -> AcmSenderBitExactness -> test::AcmReceiveTest
//
// The receiver side is driving the test by requesting new packets from
// AcmSenderBitExactness::NextPacket(). This method, in turn, asks for the
// packet from test::AcmSendTest::NextPacket, which inserts audio from the
// input file until one packet is produced. (The input file loops indefinitely.)
// Before passing the packet to the receiver, this test class verifies the
// packet header and updates a payload checksum with the new payload. The
// decoded output from the receiver is also verified with a (separate) checksum.
class AcmSenderBitExactnessOldApi : public ::testing::Test,
                                    public test::PacketSource {
 protected:
  static const int kTestDurationMs = 1000;

  AcmSenderBitExactnessOldApi()
      : frame_size_rtp_timestamps_(0),
        packet_count_(0),
        payload_type_(0),
        last_sequence_number_(0),
        last_timestamp_(0) {}

  // Sets up the test::AcmSendTest object. Returns true on success, otherwise
  // false.
  bool SetUpSender() {
    const std::string input_file_name =
        webrtc::test::ResourcePath("audio_coding/testfile32kHz", "pcm");
    // Note that |audio_source_| will loop forever. The test duration is set
    // explicitly by |kTestDurationMs|.
    audio_source_.reset(new test::InputAudioFile(input_file_name));
    static const int kSourceRateHz = 32000;
    send_test_.reset(new test::AcmSendTestOldApi(
        audio_source_.get(), kSourceRateHz, kTestDurationMs));
    return send_test_.get() != NULL;
  }

  // Registers a send codec in the test::AcmSendTest object. Returns true on
  // success, false on failure.
  bool RegisterSendCodec(const char* payload_name,
                         int sampling_freq_hz,
                         int channels,
                         int payload_type,
                         int frame_size_samples,
                         int frame_size_rtp_timestamps) {
    payload_type_ = payload_type;
    frame_size_rtp_timestamps_ = frame_size_rtp_timestamps;
    return send_test_->RegisterCodec(payload_name,
                                     sampling_freq_hz,
                                     channels,
                                     payload_type,
                                     frame_size_samples);
  }

  // Runs the test. SetUpSender() and RegisterSendCodec() must have been called
  // before calling this method.
  void Run(const std::string& audio_checksum_ref,
           const std::string& payload_checksum_ref,
           int expected_packets,
           test::AcmReceiveTestOldApi::NumOutputChannels expected_channels) {
    // Set up the receiver used to decode the packets and verify the decoded
    // output.
    test::AudioChecksum audio_checksum;
    const std::string output_file_name =
        webrtc::test::OutputPath() +
        ::testing::UnitTest::GetInstance()
            ->current_test_info()
            ->test_case_name() +
        "_" + ::testing::UnitTest::GetInstance()->current_test_info()->name() +
        "_output.pcm";
    test::OutputAudioFile output_file(output_file_name);
    // Have the output audio sent both to file and to the checksum calculator.
    test::AudioSinkFork output(&audio_checksum, &output_file);
    const int kOutputFreqHz = 8000;
    test::AcmReceiveTestOldApi receive_test(
        this, &output, kOutputFreqHz, expected_channels);
    ASSERT_NO_FATAL_FAILURE(receive_test.RegisterDefaultCodecs());

    // This is where the actual test is executed.
    receive_test.Run();

    // Extract and verify the audio checksum.
    std::string checksum_string = audio_checksum.Finish();
    EXPECT_EQ(audio_checksum_ref, checksum_string);

    // Extract and verify the payload checksum.
    char checksum_result[rtc::Md5Digest::kSize];
    payload_checksum_.Finish(checksum_result, rtc::Md5Digest::kSize);
    checksum_string = rtc::hex_encode(checksum_result, rtc::Md5Digest::kSize);
    EXPECT_EQ(payload_checksum_ref, checksum_string);

    // Verify number of packets produced.
    EXPECT_EQ(expected_packets, packet_count_);
  }

  // Returns a pointer to the next packet. Returns NULL if the source is
  // depleted (i.e., the test duration is exceeded), or if an error occurred.
  // Inherited from test::PacketSource.
  test::Packet* NextPacket() override {
    // Get the next packet from AcmSendTest. Ownership of |packet| is
    // transferred to this method.
    test::Packet* packet = send_test_->NextPacket();
    if (!packet)
      return NULL;

    VerifyPacket(packet);
    // TODO(henrik.lundin) Save the packet to file as well.

    // Pass it on to the caller. The caller becomes the owner of |packet|.
    return packet;
  }

  // Verifies the packet.
  void VerifyPacket(const test::Packet* packet) {
    EXPECT_TRUE(packet->valid_header());
    // (We can check the header fields even if valid_header() is false.)
    EXPECT_EQ(payload_type_, packet->header().payloadType);
    if (packet_count_ > 0) {
      // This is not the first packet.
      uint16_t sequence_number_diff =
          packet->header().sequenceNumber - last_sequence_number_;
      EXPECT_EQ(1, sequence_number_diff);
      uint32_t timestamp_diff = packet->header().timestamp - last_timestamp_;
      EXPECT_EQ(frame_size_rtp_timestamps_, timestamp_diff);
    }
    ++packet_count_;
    last_sequence_number_ = packet->header().sequenceNumber;
    last_timestamp_ = packet->header().timestamp;
    // Update the checksum.
    payload_checksum_.Update(packet->payload(), packet->payload_length_bytes());
  }

  void SetUpTest(const char* codec_name,
                 int codec_sample_rate_hz,
                 int channels,
                 int payload_type,
                 int codec_frame_size_samples,
                 int codec_frame_size_rtp_timestamps) {
    ASSERT_TRUE(SetUpSender());
    ASSERT_TRUE(RegisterSendCodec(codec_name,
                                  codec_sample_rate_hz,
                                  channels,
                                  payload_type,
                                  codec_frame_size_samples,
                                  codec_frame_size_rtp_timestamps));
  }

  rtc::scoped_ptr<test::AcmSendTestOldApi> send_test_;
  rtc::scoped_ptr<test::InputAudioFile> audio_source_;
  uint32_t frame_size_rtp_timestamps_;
  int packet_count_;
  uint8_t payload_type_;
  uint16_t last_sequence_number_;
  uint32_t last_timestamp_;
  rtc::Md5Digest payload_checksum_;
};

// Fails Android ARM64. https://code.google.com/p/webrtc/issues/detail?id=4199
#if defined(WEBRTC_ANDROID) && defined(__aarch64__)
#define MAYBE_IsacWb30ms DISABLED_IsacWb30ms
#else
#define MAYBE_IsacWb30ms IsacWb30ms
#endif
TEST_F(AcmSenderBitExactnessOldApi, MAYBE_IsacWb30ms) {
  ASSERT_NO_FATAL_FAILURE(SetUpTest("ISAC", 16000, 1, 103, 480, 480));
  Run(AcmReceiverBitExactnessOldApi::PlatformChecksum(
          "c7e5bdadfa2871df95639fcc297cf23d",
          "0499ca260390769b3172136faad925b9",
          "0b58f9eeee43d5891f5f6c75e77984a3"),
      AcmReceiverBitExactnessOldApi::PlatformChecksum(
          "d42cb5195463da26c8129bbfe73a22e6",
          "83de248aea9c3c2bd680b6952401b4ca",
          "3c79f16f34218271f3dca4e2b1dfe1bb"),
      33,
      test::AcmReceiveTestOldApi::kMonoOutput);
}

// Fails Android ARM64. https://code.google.com/p/webrtc/issues/detail?id=4199
#if defined(WEBRTC_ANDROID) && defined(__aarch64__)
#define MAYBE_IsacWb60ms DISABLED_IsacWb60ms
#else
#define MAYBE_IsacWb60ms IsacWb60ms
#endif
TEST_F(AcmSenderBitExactnessOldApi, MAYBE_IsacWb60ms) {
  ASSERT_NO_FATAL_FAILURE(SetUpTest("ISAC", 16000, 1, 103, 960, 960));
  Run(AcmReceiverBitExactnessOldApi::PlatformChecksum(
          "14d63c5f08127d280e722e3191b73bdd",
          "8da003e16c5371af2dc2be79a50f9076",
          "1ad29139a04782a33daad8c2b9b35875"),
      AcmReceiverBitExactnessOldApi::PlatformChecksum(
          "ebe04a819d3a9d83a83a17f271e1139a",
          "97aeef98553b5a4b5a68f8b716e8eaf0",
          "9e0a0ab743ad987b55b8e14802769c56"),
      16,
      test::AcmReceiveTestOldApi::kMonoOutput);
}

TEST_F(AcmSenderBitExactnessOldApi, DISABLED_ON_ANDROID(IsacSwb30ms)) {
  ASSERT_NO_FATAL_FAILURE(SetUpTest("ISAC", 32000, 1, 104, 960, 960));
  Run(AcmReceiverBitExactnessOldApi::PlatformChecksum(
          "2b3c387d06f00b7b7aad4c9be56fb83d",
          "",
          "5683b58da0fbf2063c7adc2e6bfb3fb8"),
      AcmReceiverBitExactnessOldApi::PlatformChecksum(
          "bcc2041e7744c7ebd9f701866856849c",
          "",
          "ce86106a93419aefb063097108ec94ab"),
      33, test::AcmReceiveTestOldApi::kMonoOutput);
}

TEST_F(AcmSenderBitExactnessOldApi, Pcm16_8000khz_10ms) {
  ASSERT_NO_FATAL_FAILURE(SetUpTest("L16", 8000, 1, 107, 80, 80));
  Run("de4a98e1406f8b798d99cd0704e862e2",
      "c1edd36339ce0326cc4550041ad719a0",
      100,
      test::AcmReceiveTestOldApi::kMonoOutput);
}

TEST_F(AcmSenderBitExactnessOldApi, Pcm16_16000khz_10ms) {
  ASSERT_NO_FATAL_FAILURE(SetUpTest("L16", 16000, 1, 108, 160, 160));
  Run("ae646d7b68384a1269cc080dd4501916",
      "ad786526383178b08d80d6eee06e9bad",
      100,
      test::AcmReceiveTestOldApi::kMonoOutput);
}

TEST_F(AcmSenderBitExactnessOldApi, Pcm16_32000khz_10ms) {
  ASSERT_NO_FATAL_FAILURE(SetUpTest("L16", 32000, 1, 109, 320, 320));
  Run("7fe325e8fbaf755e3c5df0b11a4774fb",
      "5ef82ea885e922263606c6fdbc49f651",
      100,
      test::AcmReceiveTestOldApi::kMonoOutput);
}

TEST_F(AcmSenderBitExactnessOldApi, Pcm16_stereo_8000khz_10ms) {
  ASSERT_NO_FATAL_FAILURE(SetUpTest("L16", 8000, 2, 111, 80, 80));
  Run("fb263b74e7ac3de915474d77e4744ceb",
      "62ce5adb0d4965d0a52ec98ae7f98974",
      100,
      test::AcmReceiveTestOldApi::kStereoOutput);
}

TEST_F(AcmSenderBitExactnessOldApi, Pcm16_stereo_16000khz_10ms) {
  ASSERT_NO_FATAL_FAILURE(SetUpTest("L16", 16000, 2, 112, 160, 160));
  Run("d09e9239553649d7ac93e19d304281fd",
      "41ca8edac4b8c71cd54fd9f25ec14870",
      100,
      test::AcmReceiveTestOldApi::kStereoOutput);
}

TEST_F(AcmSenderBitExactnessOldApi, Pcm16_stereo_32000khz_10ms) {
  ASSERT_NO_FATAL_FAILURE(SetUpTest("L16", 32000, 2, 113, 320, 320));
  Run("5f025d4f390982cc26b3d92fe02e3044",
      "50e58502fb04421bf5b857dda4c96879",
      100,
      test::AcmReceiveTestOldApi::kStereoOutput);
}

TEST_F(AcmSenderBitExactnessOldApi, Pcmu_20ms) {
  ASSERT_NO_FATAL_FAILURE(SetUpTest("PCMU", 8000, 1, 0, 160, 160));
  Run("81a9d4c0bb72e9becc43aef124c981e9",
      "8f9b8750bd80fe26b6cbf6659b89f0f9",
      50,
      test::AcmReceiveTestOldApi::kMonoOutput);
}

TEST_F(AcmSenderBitExactnessOldApi, Pcma_20ms) {
  ASSERT_NO_FATAL_FAILURE(SetUpTest("PCMA", 8000, 1, 8, 160, 160));
  Run("39611f798969053925a49dc06d08de29",
      "6ad745e55aa48981bfc790d0eeef2dd1",
      50,
      test::AcmReceiveTestOldApi::kMonoOutput);
}

TEST_F(AcmSenderBitExactnessOldApi, Pcmu_stereo_20ms) {
  ASSERT_NO_FATAL_FAILURE(SetUpTest("PCMU", 8000, 2, 110, 160, 160));
  Run("437bec032fdc5cbaa0d5175430af7b18",
      "60b6f25e8d1e74cb679cfe756dd9bca5",
      50,
      test::AcmReceiveTestOldApi::kStereoOutput);
}

TEST_F(AcmSenderBitExactnessOldApi, Pcma_stereo_20ms) {
  ASSERT_NO_FATAL_FAILURE(SetUpTest("PCMA", 8000, 2, 118, 160, 160));
  Run("a5c6d83c5b7cedbeff734238220a4b0c",
      "92b282c83efd20e7eeef52ba40842cf7",
      50,
      test::AcmReceiveTestOldApi::kStereoOutput);
}

TEST_F(AcmSenderBitExactnessOldApi, DISABLED_ON_ANDROID(Ilbc_30ms)) {
  ASSERT_NO_FATAL_FAILURE(SetUpTest("ILBC", 8000, 1, 102, 240, 240));
  Run(AcmReceiverBitExactnessOldApi::PlatformChecksum(
          "7b6ec10910debd9af08011d3ed5249f7",
          "android_audio",
          "7b6ec10910debd9af08011d3ed5249f7"),
      AcmReceiverBitExactnessOldApi::PlatformChecksum(
          "cfae2e9f6aba96e145f2bcdd5050ce78",
          "android_payload",
          "cfae2e9f6aba96e145f2bcdd5050ce78"),
      33,
      test::AcmReceiveTestOldApi::kMonoOutput);
}

TEST_F(AcmSenderBitExactnessOldApi, DISABLED_ON_ANDROID(G722_20ms)) {
  ASSERT_NO_FATAL_FAILURE(SetUpTest("G722", 16000, 1, 9, 320, 160));
  Run(AcmReceiverBitExactnessOldApi::PlatformChecksum(
          "7d759436f2533582950d148b5161a36c",
          "android_audio",
          "7d759436f2533582950d148b5161a36c"),
      AcmReceiverBitExactnessOldApi::PlatformChecksum(
          "fc68a87e1380614e658087cb35d5ca10",
          "android_payload",
          "fc68a87e1380614e658087cb35d5ca10"),
      50,
      test::AcmReceiveTestOldApi::kMonoOutput);
}

TEST_F(AcmSenderBitExactnessOldApi, DISABLED_ON_ANDROID(G722_stereo_20ms)) {
  ASSERT_NO_FATAL_FAILURE(SetUpTest("G722", 16000, 2, 119, 320, 160));
  Run(AcmReceiverBitExactnessOldApi::PlatformChecksum(
          "7190ee718ab3d80eca181e5f7140c210",
          "android_audio",
          "7190ee718ab3d80eca181e5f7140c210"),
      AcmReceiverBitExactnessOldApi::PlatformChecksum(
          "66516152eeaa1e650ad94ff85f668dac",
          "android_payload",
          "66516152eeaa1e650ad94ff85f668dac"),
      50,
      test::AcmReceiveTestOldApi::kStereoOutput);
}

// Fails Android ARM64. https://code.google.com/p/webrtc/issues/detail?id=4199
#if defined(WEBRTC_ANDROID) && defined(__aarch64__)
#define MAYBE_Opus_stereo_20ms DISABLED_Opus_stereo_20ms
#else
#define MAYBE_Opus_stereo_20ms Opus_stereo_20ms
#endif
TEST_F(AcmSenderBitExactnessOldApi, MAYBE_Opus_stereo_20ms) {
  ASSERT_NO_FATAL_FAILURE(SetUpTest("opus", 48000, 2, 120, 960, 960));
  Run(AcmReceiverBitExactnessOldApi::PlatformChecksum(
          "855041f2490b887302bce9d544731849",
          "1e1a0fce893fef2d66886a7f09e2ebce",
          "855041f2490b887302bce9d544731849"),
      AcmReceiverBitExactnessOldApi::PlatformChecksum(
          "d781cce1ab986b618d0da87226cdde30",
          "1a1fe04dd12e755949987c8d729fb3e0",
          "d781cce1ab986b618d0da87226cdde30"),
      50,
      test::AcmReceiveTestOldApi::kStereoOutput);
}

// Fails Android ARM64. https://code.google.com/p/webrtc/issues/detail?id=4199
#if defined(WEBRTC_ANDROID) && defined(__aarch64__)
#define MAYBE_Opus_stereo_20ms_voip DISABLED_Opus_stereo_20ms_voip
#else
#define MAYBE_Opus_stereo_20ms_voip Opus_stereo_20ms_voip
#endif
TEST_F(AcmSenderBitExactnessOldApi, MAYBE_Opus_stereo_20ms_voip) {
  ASSERT_NO_FATAL_FAILURE(SetUpTest("opus", 48000, 2, 120, 960, 960));
  // If not set, default will be kAudio in case of stereo.
  EXPECT_EQ(0, send_test_->acm()->SetOpusApplication(kVoip, false));
  Run(AcmReceiverBitExactnessOldApi::PlatformChecksum(
          "9b9e12bc3cc793740966e11cbfa8b35b",
          "57412a4b5771d19ff03ec35deffe7067",
          "9b9e12bc3cc793740966e11cbfa8b35b"),
      AcmReceiverBitExactnessOldApi::PlatformChecksum(
          "c7340b1189652ab6b5e80dade7390cb4",
          "cdfe85939c411d12b61701c566e22d26",
          "c7340b1189652ab6b5e80dade7390cb4"),
      50,
      test::AcmReceiveTestOldApi::kStereoOutput);
}

// This test fixture is implemented to run ACM and change the desired output
// frequency during the call. The input packets are simply PCM16b-wb encoded
// payloads with a constant value of |kSampleValue|. The test fixture itself
// acts as PacketSource in between the receive test class and the constant-
// payload packet source class. The output is both written to file, and analyzed
// in this test fixture.
class AcmSwitchingOutputFrequencyOldApi : public ::testing::Test,
                                          public test::PacketSource,
                                          public test::AudioSink {
 protected:
  static const size_t kTestNumPackets = 50;
  static const int kEncodedSampleRateHz = 16000;
  static const size_t kPayloadLenSamples = 30 * kEncodedSampleRateHz / 1000;
  static const int kPayloadType = 108;  // Default payload type for PCM16b-wb.

  AcmSwitchingOutputFrequencyOldApi()
      : first_output_(true),
        num_packets_(0),
        packet_source_(kPayloadLenSamples,
                       kSampleValue,
                       kEncodedSampleRateHz,
                       kPayloadType),
        output_freq_2_(0),
        has_toggled_(false) {}

  void Run(int output_freq_1, int output_freq_2, int toggle_period_ms) {
    // Set up the receiver used to decode the packets and verify the decoded
    // output.
    const std::string output_file_name =
        webrtc::test::OutputPath() +
        ::testing::UnitTest::GetInstance()
            ->current_test_info()
            ->test_case_name() +
        "_" + ::testing::UnitTest::GetInstance()->current_test_info()->name() +
        "_output.pcm";
    test::OutputAudioFile output_file(output_file_name);
    // Have the output audio sent both to file and to the WriteArray method in
    // this class.
    test::AudioSinkFork output(this, &output_file);
    test::AcmReceiveTestToggleOutputFreqOldApi receive_test(
        this,
        &output,
        output_freq_1,
        output_freq_2,
        toggle_period_ms,
        test::AcmReceiveTestOldApi::kMonoOutput);
    ASSERT_NO_FATAL_FAILURE(receive_test.RegisterDefaultCodecs());
    output_freq_2_ = output_freq_2;

    // This is where the actual test is executed.
    receive_test.Run();
  }

  // Inherited from test::PacketSource.
  test::Packet* NextPacket() override {
    // Check if it is time to terminate the test. The packet source is of type
    // ConstantPcmPacketSource, which is infinite, so we must end the test
    // "manually".
    if (num_packets_++ > kTestNumPackets) {
      EXPECT_TRUE(has_toggled_);
      return NULL;  // Test ended.
    }

    // Get the next packet from the source.
    return packet_source_.NextPacket();
  }

  // Inherited from test::AudioSink.
  bool WriteArray(const int16_t* audio, size_t num_samples) {
    // Skip checking the first output frame, since it has a number of zeros
    // due to how NetEq is initialized.
    if (first_output_) {
      first_output_ = false;
      return true;
    }
    for (size_t i = 0; i < num_samples; ++i) {
      EXPECT_EQ(kSampleValue, audio[i]);
    }
    if (num_samples ==
        static_cast<size_t>(output_freq_2_ / 100))  // Size of 10 ms frame.
      has_toggled_ = true;
    // The return value does not say if the values match the expectation, just
    // that the method could process the samples.
    return true;
  }

  const int16_t kSampleValue = 1000;
  bool first_output_;
  size_t num_packets_;
  test::ConstantPcmPacketSource packet_source_;
  int output_freq_2_;
  bool has_toggled_;
};

TEST_F(AcmSwitchingOutputFrequencyOldApi, TestWithoutToggling) {
  Run(16000, 16000, 1000);
}

TEST_F(AcmSwitchingOutputFrequencyOldApi, Toggle16KhzTo32Khz) {
  Run(16000, 32000, 1000);
}

TEST_F(AcmSwitchingOutputFrequencyOldApi, Toggle32KhzTo16Khz) {
  Run(32000, 16000, 1000);
}

TEST_F(AcmSwitchingOutputFrequencyOldApi, Toggle16KhzTo8Khz) {
  Run(16000, 8000, 1000);
}

TEST_F(AcmSwitchingOutputFrequencyOldApi, Toggle8KhzTo16Khz) {
  Run(8000, 16000, 1000);
}
}  // namespace webrtc
