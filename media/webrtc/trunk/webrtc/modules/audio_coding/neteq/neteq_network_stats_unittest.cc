/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "testing/gmock/include/gmock/gmock.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/modules/audio_coding/neteq/audio_decoder_impl.h"
#include "webrtc/modules/audio_coding/neteq/tools/neteq_external_decoder_test.h"
#include "webrtc/modules/audio_coding/neteq/tools/rtp_generator.h"

namespace webrtc {
namespace test {

using ::testing::_;
using ::testing::SetArgPointee;
using ::testing::Return;


class MockAudioDecoderOpus : public AudioDecoderOpus {
 public:
  static const int kPacketDuration = 960;  // 48 kHz * 20 ms

  explicit MockAudioDecoderOpus(int num_channels)
      : AudioDecoderOpus(num_channels),
        fec_enabled_(false) {
  }
  virtual ~MockAudioDecoderOpus() { Die(); }
  MOCK_METHOD0(Die, void());

  MOCK_METHOD0(Init, int());

  int PacketDuration(const uint8_t* encoded,
                     size_t encoded_len) const override {
    return kPacketDuration;
  }

  int PacketDurationRedundant(const uint8_t* encoded,
                              size_t encoded_len) const override {
    return kPacketDuration;
  }

  bool PacketHasFec(const uint8_t* encoded, size_t encoded_len) const override {
    return fec_enabled_;
  }

  void set_fec_enabled(bool enable_fec) { fec_enabled_ = enable_fec; }

  bool fec_enabled() const { return fec_enabled_; }

 protected:
  // Override the following methods such that no actual payload is needed.
  int DecodeInternal(const uint8_t* encoded,
                     size_t encoded_len,
                     int /*sample_rate_hz*/,
                     int16_t* decoded,
                     SpeechType* speech_type) override {
    *speech_type = kSpeech;
    memset(decoded, 0, sizeof(int16_t) * kPacketDuration * Channels());
    return kPacketDuration * Channels();
  }

  int DecodeRedundantInternal(const uint8_t* encoded,
                              size_t encoded_len,
                              int sample_rate_hz,
                              int16_t* decoded,
                              SpeechType* speech_type) override {
    return DecodeInternal(encoded, encoded_len, sample_rate_hz, decoded,
                          speech_type);
  }

 private:
  bool fec_enabled_;
};

class NetEqNetworkStatsTest : public NetEqExternalDecoderTest {
 public:
  static const int kPayloadSizeByte = 30;
  static const int kFrameSizeMs = 20;  // frame size of Opus
  static const int kMaxOutputSize = 960;  // 10 ms * 48 kHz * 2 channels.

enum logic {
  IGNORE,
  EQUAL,
  SMALLER_THAN,
  LARGER_THAN,
};

struct NetEqNetworkStatsCheck {
  logic current_buffer_size_ms;
  logic preferred_buffer_size_ms;
  logic jitter_peaks_found;
  logic packet_loss_rate;
  logic packet_discard_rate;
  logic expand_rate;
  logic speech_expand_rate;
  logic preemptive_rate;
  logic accelerate_rate;
  logic secondary_decoded_rate;
  logic clockdrift_ppm;
  logic added_zero_samples;
  NetEqNetworkStatistics stats_ref;
};

  NetEqNetworkStatsTest(NetEqDecoder codec,
                        MockAudioDecoderOpus* decoder)
      : NetEqExternalDecoderTest(codec, decoder),
        external_decoder_(decoder),
        samples_per_ms_(CodecSampleRateHz(codec) / 1000),
        frame_size_samples_(kFrameSizeMs * samples_per_ms_),
        rtp_generator_(new test::RtpGenerator(samples_per_ms_)),
        last_lost_time_(0),
        packet_loss_interval_(0xffffffff) {
    Init();
  }

  bool Lost(uint32_t send_time) {
    if (send_time - last_lost_time_ >= packet_loss_interval_) {
      last_lost_time_ = send_time;
      return true;
    }
    return false;
  }

  void SetPacketLossRate(double loss_rate) {
      packet_loss_interval_ = (loss_rate >= 1e-3 ?
          static_cast<double>(kFrameSizeMs) / loss_rate : 0xffffffff);
  }

  // |stats_ref|
  // expects.x = -1, do not care
  // expects.x = 0, 'x' in current stats should equal 'x' in |stats_ref|
  // expects.x = 1, 'x' in current stats should < 'x' in |stats_ref|
  // expects.x = 2, 'x' in current stats should > 'x' in |stats_ref|
  void CheckNetworkStatistics(NetEqNetworkStatsCheck expects) {
    NetEqNetworkStatistics stats;
    neteq()->NetworkStatistics(&stats);

#define CHECK_NETEQ_NETWORK_STATS(x)\
  switch (expects.x) {\
    case EQUAL:\
      EXPECT_EQ(stats.x, expects.stats_ref.x);\
      break;\
    case SMALLER_THAN:\
      EXPECT_LT(stats.x, expects.stats_ref.x);\
      break;\
    case LARGER_THAN:\
      EXPECT_GT(stats.x, expects.stats_ref.x);\
      break;\
    default:\
      break;\
  }

    CHECK_NETEQ_NETWORK_STATS(current_buffer_size_ms);
    CHECK_NETEQ_NETWORK_STATS(preferred_buffer_size_ms);
    CHECK_NETEQ_NETWORK_STATS(jitter_peaks_found);
    CHECK_NETEQ_NETWORK_STATS(packet_loss_rate);
    CHECK_NETEQ_NETWORK_STATS(packet_discard_rate);
    CHECK_NETEQ_NETWORK_STATS(expand_rate);
    CHECK_NETEQ_NETWORK_STATS(speech_expand_rate);
    CHECK_NETEQ_NETWORK_STATS(preemptive_rate);
    CHECK_NETEQ_NETWORK_STATS(accelerate_rate);
    CHECK_NETEQ_NETWORK_STATS(secondary_decoded_rate);
    CHECK_NETEQ_NETWORK_STATS(clockdrift_ppm);
    CHECK_NETEQ_NETWORK_STATS(added_zero_samples);

#undef CHECK_NETEQ_NETWORK_STATS
  }

  void RunTest(int num_loops, NetEqNetworkStatsCheck expects) {
    NetEqOutputType output_type;
    uint32_t time_now;
    uint32_t next_send_time;

    // Initiate |last_lost_time_|.
    time_now = next_send_time = last_lost_time_ =
        rtp_generator_->GetRtpHeader(kPayloadType, frame_size_samples_,
                                     &rtp_header_);
    for (int k = 0; k < num_loops; ++k) {
      // Delay by one frame such that the FEC can come in.
      while (time_now + kFrameSizeMs >= next_send_time) {
        next_send_time = rtp_generator_->GetRtpHeader(kPayloadType,
                                                      frame_size_samples_,
                                                      &rtp_header_);
        if (!Lost(next_send_time)) {
          InsertPacket(rtp_header_, payload_, kPayloadSizeByte,
                       next_send_time);
        }
      }
      GetOutputAudio(kMaxOutputSize, output_, &output_type);
      time_now += kOutputLengthMs;
    }
    CheckNetworkStatistics(expects);
    neteq()->FlushBuffers();
  }

  void DecodeFecTest() {
    external_decoder_->set_fec_enabled(false);
    NetEqNetworkStatsCheck expects = {
      IGNORE,  // current_buffer_size_ms
      IGNORE,  // preferred_buffer_size_ms
      IGNORE,  // jitter_peaks_found
      EQUAL,  // packet_loss_rate
      EQUAL,  // packet_discard_rate
      EQUAL,  // expand_rate
      EQUAL,  // voice_expand_rate
      IGNORE,  // preemptive_rate
      EQUAL,  // accelerate_rate
      EQUAL,  // decoded_fec_rate
      IGNORE,  // clockdrift_ppm
      EQUAL,  // added_zero_samples
      {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
    };
    RunTest(50, expects);

    // Next we introduce packet losses.
    SetPacketLossRate(0.1);
    expects.stats_ref.packet_loss_rate = 1337;
    expects.stats_ref.expand_rate = expects.stats_ref.speech_expand_rate = 1065;
    RunTest(50, expects);

    // Next we enable Opus FEC.
    external_decoder_->set_fec_enabled(true);
    // If FEC fills in the lost packets, no packet loss will be counted.
    expects.stats_ref.packet_loss_rate = 0;
    expects.stats_ref.expand_rate = expects.stats_ref.speech_expand_rate = 0;
    expects.stats_ref.secondary_decoded_rate = 2006;
    RunTest(50, expects);
  }

  void NoiseExpansionTest() {
    NetEqNetworkStatsCheck expects = {
      IGNORE,  // current_buffer_size_ms
      IGNORE,  // preferred_buffer_size_ms
      IGNORE,  // jitter_peaks_found
      EQUAL,  // packet_loss_rate
      EQUAL,  // packet_discard_rate
      EQUAL,  // expand_rate
      EQUAL,  // speech_expand_rate
      IGNORE,  // preemptive_rate
      EQUAL,  // accelerate_rate
      EQUAL,  // decoded_fec_rate
      IGNORE,  // clockdrift_ppm
      EQUAL,  // added_zero_samples
      {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
    };
    RunTest(50, expects);

    SetPacketLossRate(1);
    expects.stats_ref.expand_rate = 16384;
    expects.stats_ref.speech_expand_rate = 5324;
    RunTest(10, expects);  // Lost 10 * 20ms in a row.
  }

 private:
  MockAudioDecoderOpus* external_decoder_;
  const int samples_per_ms_;
  const size_t frame_size_samples_;
  rtc::scoped_ptr<test::RtpGenerator> rtp_generator_;
  WebRtcRTPHeader rtp_header_;
  uint32_t last_lost_time_;
  uint32_t packet_loss_interval_;
  uint8_t payload_[kPayloadSizeByte];
  int16_t output_[kMaxOutputSize];
};

TEST(NetEqNetworkStatsTest, OpusDecodeFec) {
  MockAudioDecoderOpus decoder(1);
  EXPECT_CALL(decoder, Init());
  NetEqNetworkStatsTest test(kDecoderOpus, &decoder);
  test.DecodeFecTest();
  EXPECT_CALL(decoder, Die()).Times(1);
}

TEST(NetEqNetworkStatsTest, StereoOpusDecodeFec) {
  MockAudioDecoderOpus decoder(2);
  EXPECT_CALL(decoder, Init());
  NetEqNetworkStatsTest test(kDecoderOpus, &decoder);
  test.DecodeFecTest();
  EXPECT_CALL(decoder, Die()).Times(1);
}

TEST(NetEqNetworkStatsTest, NoiseExpansionTest) {
  MockAudioDecoderOpus decoder(1);
  EXPECT_CALL(decoder, Init());
  NetEqNetworkStatsTest test(kDecoderOpus, &decoder);
  test.NoiseExpansionTest();
  EXPECT_CALL(decoder, Die()).Times(1);
}

}  // namespace test
}  // namespace webrtc




