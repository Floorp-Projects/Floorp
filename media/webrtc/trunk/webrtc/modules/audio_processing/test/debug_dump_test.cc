/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stddef.h>  // size_t
#include <string>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/audio_processing/debug.pb.h"
#include "webrtc/base/checks.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/common_audio/channel_buffer.h"
#include "webrtc/modules/audio_coding/neteq/tools/resample_input_audio_file.h"
#include "webrtc/modules/audio_processing/include/audio_processing.h"
#include "webrtc/modules/audio_processing/test/protobuf_utils.h"
#include "webrtc/modules/audio_processing/test/test_utils.h"
#include "webrtc/test/testsupport/fileutils.h"

namespace webrtc {
namespace test {

namespace {

void MaybeResetBuffer(rtc::scoped_ptr<ChannelBuffer<float>>* buffer,
                      const StreamConfig& config) {
  auto& buffer_ref = *buffer;
  if (!buffer_ref.get() || buffer_ref->num_frames() != config.num_frames() ||
      buffer_ref->num_channels() != config.num_channels()) {
    buffer_ref.reset(new ChannelBuffer<float>(config.num_frames(),
                                             config.num_channels()));
  }
}

class DebugDumpGenerator {
 public:
  DebugDumpGenerator(const std::string& input_file_name,
                     int input_file_rate_hz,
                     int input_channels,
                     const std::string& reverse_file_name,
                     int reverse_file_rate_hz,
                     int reverse_channels,
                     const Config& config,
                     const std::string& dump_file_name);

  // Constructor that uses default input files.
  explicit DebugDumpGenerator(const Config& config);

  ~DebugDumpGenerator();

  // Changes the sample rate of the input audio to the APM.
  void SetInputRate(int rate_hz);

  // Sets if converts stereo input signal to mono by discarding other channels.
  void ForceInputMono(bool mono);

  // Changes the sample rate of the reverse audio to the APM.
  void SetReverseRate(int rate_hz);

  // Sets if converts stereo reverse signal to mono by discarding other
  // channels.
  void ForceReverseMono(bool mono);

  // Sets the required sample rate of the APM output.
  void SetOutputRate(int rate_hz);

  // Sets the required channels of the APM output.
  void SetOutputChannels(int channels);

  std::string dump_file_name() const { return dump_file_name_; }

  void StartRecording();
  void Process(size_t num_blocks);
  void StopRecording();
  AudioProcessing* apm() const { return apm_.get(); }

 private:
  static void ReadAndDeinterleave(ResampleInputAudioFile* audio, int channels,
                                  const StreamConfig& config,
                                  float* const* buffer);

  // APM input/output settings.
  StreamConfig input_config_;
  StreamConfig reverse_config_;
  StreamConfig output_config_;

  // Input file format.
  const std::string input_file_name_;
  ResampleInputAudioFile input_audio_;
  const int input_file_channels_;

  // Reverse file format.
  const std::string reverse_file_name_;
  ResampleInputAudioFile reverse_audio_;
  const int reverse_file_channels_;

  // Buffer for APM input/output.
  rtc::scoped_ptr<ChannelBuffer<float>> input_;
  rtc::scoped_ptr<ChannelBuffer<float>> reverse_;
  rtc::scoped_ptr<ChannelBuffer<float>> output_;

  rtc::scoped_ptr<AudioProcessing> apm_;

  const std::string dump_file_name_;
};

DebugDumpGenerator::DebugDumpGenerator(const std::string& input_file_name,
                                       int input_rate_hz,
                                       int input_channels,
                                       const std::string& reverse_file_name,
                                       int reverse_rate_hz,
                                       int reverse_channels,
                                       const Config& config,
                                       const std::string& dump_file_name)
    : input_config_(input_rate_hz, input_channels),
      reverse_config_(reverse_rate_hz, reverse_channels),
      output_config_(input_rate_hz, input_channels),
      input_audio_(input_file_name, input_rate_hz, input_rate_hz),
      input_file_channels_(input_channels),
      reverse_audio_(reverse_file_name, reverse_rate_hz, reverse_rate_hz),
      reverse_file_channels_(reverse_channels),
      input_(new ChannelBuffer<float>(input_config_.num_frames(),
                                      input_config_.num_channels())),
      reverse_(new ChannelBuffer<float>(reverse_config_.num_frames(),
                                        reverse_config_.num_channels())),
      output_(new ChannelBuffer<float>(output_config_.num_frames(),
                                       output_config_.num_channels())),
      apm_(AudioProcessing::Create(config)),
      dump_file_name_(dump_file_name) {
}

DebugDumpGenerator::DebugDumpGenerator(const Config& config)
  : DebugDumpGenerator(ResourcePath("near32_stereo", "pcm"), 32000, 2,
                       ResourcePath("far32_stereo", "pcm"), 32000, 2,
                       config,
                       TempFilename(OutputPath(), "debug_aec")) {
}

DebugDumpGenerator::~DebugDumpGenerator() {
  remove(dump_file_name_.c_str());
}

void DebugDumpGenerator::SetInputRate(int rate_hz) {
  input_audio_.set_output_rate_hz(rate_hz);
  input_config_.set_sample_rate_hz(rate_hz);
  MaybeResetBuffer(&input_, input_config_);
}

void DebugDumpGenerator::ForceInputMono(bool mono) {
  const int channels = mono ? 1 : input_file_channels_;
  input_config_.set_num_channels(channels);
  MaybeResetBuffer(&input_, input_config_);
}

void DebugDumpGenerator::SetReverseRate(int rate_hz) {
  reverse_audio_.set_output_rate_hz(rate_hz);
  reverse_config_.set_sample_rate_hz(rate_hz);
  MaybeResetBuffer(&reverse_, reverse_config_);
}

void DebugDumpGenerator::ForceReverseMono(bool mono) {
  const int channels = mono ? 1 : reverse_file_channels_;
  reverse_config_.set_num_channels(channels);
  MaybeResetBuffer(&reverse_, reverse_config_);
}

void DebugDumpGenerator::SetOutputRate(int rate_hz) {
  output_config_.set_sample_rate_hz(rate_hz);
  MaybeResetBuffer(&output_, output_config_);
}

void DebugDumpGenerator::SetOutputChannels(int channels) {
  output_config_.set_num_channels(channels);
  MaybeResetBuffer(&output_, output_config_);
}

void DebugDumpGenerator::StartRecording() {
  apm_->StartDebugRecording(dump_file_name_.c_str());
}

void DebugDumpGenerator::Process(size_t num_blocks) {
  for (size_t i = 0; i < num_blocks; ++i) {
    ReadAndDeinterleave(&reverse_audio_, reverse_file_channels_,
                        reverse_config_, reverse_->channels());
    ReadAndDeinterleave(&input_audio_, input_file_channels_, input_config_,
                        input_->channels());
    RTC_CHECK_EQ(AudioProcessing::kNoError, apm_->set_stream_delay_ms(100));
    apm_->set_stream_key_pressed(i % 10 == 9);
    RTC_CHECK_EQ(AudioProcessing::kNoError,
                 apm_->ProcessStream(input_->channels(), input_config_,
                                     output_config_, output_->channels()));

    RTC_CHECK_EQ(AudioProcessing::kNoError,
                 apm_->ProcessReverseStream(reverse_->channels(),
                                            reverse_config_,
                                            reverse_config_,
                                            reverse_->channels()));
  }
}

void DebugDumpGenerator::StopRecording() {
  apm_->StopDebugRecording();
}

void DebugDumpGenerator::ReadAndDeinterleave(ResampleInputAudioFile* audio,
                                             int channels,
                                             const StreamConfig& config,
                                             float* const* buffer) {
  const size_t num_frames = config.num_frames();
  const int out_channels = config.num_channels();

  std::vector<int16_t> signal(channels * num_frames);

  audio->Read(num_frames * channels, &signal[0]);

  // We only allow reducing number of channels by discarding some channels.
  RTC_CHECK_LE(out_channels, channels);
  for (int channel = 0; channel < out_channels; ++channel) {
    for (size_t i = 0; i < num_frames; ++i) {
      buffer[channel][i] = S16ToFloat(signal[i * channels + channel]);
    }
  }
}

}  // namespace

class DebugDumpTest : public ::testing::Test {
 public:
  DebugDumpTest();

  // VerifyDebugDump replays a debug dump using APM and verifies that the result
  // is bit-exact-identical to the output channel in the dump. This is only
  // guaranteed if the debug dump is started on the first frame.
  void VerifyDebugDump(const std::string& dump_file_name);

 private:
  // Following functions are facilities for replaying debug dumps.
  void OnInitEvent(const audioproc::Init& msg);
  void OnStreamEvent(const audioproc::Stream& msg);
  void OnReverseStreamEvent(const audioproc::ReverseStream& msg);
  void OnConfigEvent(const audioproc::Config& msg);

  void MaybeRecreateApm(const audioproc::Config& msg);
  void ConfigureApm(const audioproc::Config& msg);

  // Buffer for APM input/output.
  rtc::scoped_ptr<ChannelBuffer<float>> input_;
  rtc::scoped_ptr<ChannelBuffer<float>> reverse_;
  rtc::scoped_ptr<ChannelBuffer<float>> output_;

  rtc::scoped_ptr<AudioProcessing> apm_;

  StreamConfig input_config_;
  StreamConfig reverse_config_;
  StreamConfig output_config_;
};

DebugDumpTest::DebugDumpTest()
    : input_(nullptr),  // will be created upon usage.
      reverse_(nullptr),
      output_(nullptr),
      apm_(nullptr) {
}

void DebugDumpTest::VerifyDebugDump(const std::string& in_filename) {
  FILE* in_file = fopen(in_filename.c_str(), "rb");
  ASSERT_TRUE(in_file);
  audioproc::Event event_msg;

  while (ReadMessageFromFile(in_file, &event_msg)) {
    switch (event_msg.type()) {
      case audioproc::Event::INIT:
        OnInitEvent(event_msg.init());
        break;
      case audioproc::Event::STREAM:
        OnStreamEvent(event_msg.stream());
        break;
      case audioproc::Event::REVERSE_STREAM:
        OnReverseStreamEvent(event_msg.reverse_stream());
        break;
      case audioproc::Event::CONFIG:
        OnConfigEvent(event_msg.config());
        break;
      case audioproc::Event::UNKNOWN_EVENT:
        // We do not expect receive UNKNOWN event currently.
        FAIL();
    }
  }
  fclose(in_file);
}

// OnInitEvent reset the input/output/reserve channel format.
void DebugDumpTest::OnInitEvent(const audioproc::Init& msg) {
  ASSERT_TRUE(msg.has_num_input_channels());
  ASSERT_TRUE(msg.has_output_sample_rate());
  ASSERT_TRUE(msg.has_num_output_channels());
  ASSERT_TRUE(msg.has_reverse_sample_rate());
  ASSERT_TRUE(msg.has_num_reverse_channels());

  input_config_ = StreamConfig(msg.sample_rate(), msg.num_input_channels());
  output_config_ =
      StreamConfig(msg.output_sample_rate(), msg.num_output_channels());
  reverse_config_ =
      StreamConfig(msg.reverse_sample_rate(), msg.num_reverse_channels());

  MaybeResetBuffer(&input_, input_config_);
  MaybeResetBuffer(&output_, output_config_);
  MaybeResetBuffer(&reverse_, reverse_config_);
}

// OnStreamEvent replays an input signal and verifies the output.
void DebugDumpTest::OnStreamEvent(const audioproc::Stream& msg) {
  // APM should have been created.
  ASSERT_TRUE(apm_.get());

  EXPECT_NOERR(apm_->gain_control()->set_stream_analog_level(msg.level()));
  EXPECT_NOERR(apm_->set_stream_delay_ms(msg.delay()));
  apm_->echo_cancellation()->set_stream_drift_samples(msg.drift());
  if (msg.has_keypress())
    apm_->set_stream_key_pressed(msg.keypress());
  else
    apm_->set_stream_key_pressed(true);

  ASSERT_EQ(input_config_.num_channels(),
            static_cast<size_t>(msg.input_channel_size()));
  ASSERT_EQ(input_config_.num_frames() * sizeof(float),
            msg.input_channel(0).size());

  for (int i = 0; i < msg.input_channel_size(); ++i) {
     memcpy(input_->channels()[i], msg.input_channel(i).data(),
            msg.input_channel(i).size());
  }

  ASSERT_EQ(AudioProcessing::kNoError,
            apm_->ProcessStream(input_->channels(), input_config_,
                                output_config_, output_->channels()));

  // Check that output of APM is bit-exact to the output in the dump.
  ASSERT_EQ(output_config_.num_channels(),
            static_cast<size_t>(msg.output_channel_size()));
  ASSERT_EQ(output_config_.num_frames() * sizeof(float),
            msg.output_channel(0).size());
  for (int i = 0; i < msg.output_channel_size(); ++i) {
    ASSERT_EQ(0, memcmp(output_->channels()[i], msg.output_channel(i).data(),
                        msg.output_channel(i).size()));
  }
}

void DebugDumpTest::OnReverseStreamEvent(const audioproc::ReverseStream& msg) {
  // APM should have been created.
  ASSERT_TRUE(apm_.get());

  ASSERT_GT(msg.channel_size(), 0);
  ASSERT_EQ(reverse_config_.num_channels(),
            static_cast<size_t>(msg.channel_size()));
  ASSERT_EQ(reverse_config_.num_frames() * sizeof(float),
            msg.channel(0).size());

  for (int i = 0; i < msg.channel_size(); ++i) {
     memcpy(reverse_->channels()[i], msg.channel(i).data(),
            msg.channel(i).size());
  }

  ASSERT_EQ(AudioProcessing::kNoError,
            apm_->ProcessReverseStream(reverse_->channels(),
                                       reverse_config_,
                                       reverse_config_,
                                       reverse_->channels()));
}

void DebugDumpTest::OnConfigEvent(const audioproc::Config& msg) {
  MaybeRecreateApm(msg);
  ConfigureApm(msg);
}

void DebugDumpTest::MaybeRecreateApm(const audioproc::Config& msg) {
  // These configurations cannot be changed on the fly.
  Config config;
  ASSERT_TRUE(msg.has_aec_delay_agnostic_enabled());
  config.Set<DelayAgnostic>(
      new DelayAgnostic(msg.aec_delay_agnostic_enabled()));

  ASSERT_TRUE(msg.has_noise_robust_agc_enabled());
  config.Set<ExperimentalAgc>(
       new ExperimentalAgc(msg.noise_robust_agc_enabled()));

  ASSERT_TRUE(msg.has_transient_suppression_enabled());
  config.Set<ExperimentalNs>(
      new ExperimentalNs(msg.transient_suppression_enabled()));

  ASSERT_TRUE(msg.has_aec_extended_filter_enabled());
  config.Set<ExtendedFilter>(new ExtendedFilter(
      msg.aec_extended_filter_enabled()));

  // We only create APM once, since changes on these fields should not
  // happen in current implementation.
  if (!apm_.get()) {
    apm_.reset(AudioProcessing::Create(config));
  }
}

void DebugDumpTest::ConfigureApm(const audioproc::Config& msg) {
  // AEC configs.
  ASSERT_TRUE(msg.has_aec_enabled());
  EXPECT_EQ(AudioProcessing::kNoError,
            apm_->echo_cancellation()->Enable(msg.aec_enabled()));

  ASSERT_TRUE(msg.has_aec_drift_compensation_enabled());
  EXPECT_EQ(AudioProcessing::kNoError,
            apm_->echo_cancellation()->enable_drift_compensation(
                msg.aec_drift_compensation_enabled()));

  ASSERT_TRUE(msg.has_aec_suppression_level());
  EXPECT_EQ(AudioProcessing::kNoError,
            apm_->echo_cancellation()->set_suppression_level(
                static_cast<EchoCancellation::SuppressionLevel>(
                    msg.aec_suppression_level())));

  // AECM configs.
  ASSERT_TRUE(msg.has_aecm_enabled());
  EXPECT_EQ(AudioProcessing::kNoError,
            apm_->echo_control_mobile()->Enable(msg.aecm_enabled()));

  ASSERT_TRUE(msg.has_aecm_comfort_noise_enabled());
  EXPECT_EQ(AudioProcessing::kNoError,
            apm_->echo_control_mobile()->enable_comfort_noise(
                msg.aecm_comfort_noise_enabled()));

  ASSERT_TRUE(msg.has_aecm_routing_mode());
  EXPECT_EQ(AudioProcessing::kNoError,
            apm_->echo_control_mobile()->set_routing_mode(
                static_cast<EchoControlMobile::RoutingMode>(
                    msg.aecm_routing_mode())));

  // AGC configs.
  ASSERT_TRUE(msg.has_agc_enabled());
  EXPECT_EQ(AudioProcessing::kNoError,
            apm_->gain_control()->Enable(msg.agc_enabled()));

  ASSERT_TRUE(msg.has_agc_mode());
  EXPECT_EQ(AudioProcessing::kNoError,
            apm_->gain_control()->set_mode(
                static_cast<GainControl::Mode>(msg.agc_mode())));

  ASSERT_TRUE(msg.has_agc_limiter_enabled());
  EXPECT_EQ(AudioProcessing::kNoError,
            apm_->gain_control()->enable_limiter(msg.agc_limiter_enabled()));

  // HPF configs.
  ASSERT_TRUE(msg.has_hpf_enabled());
  EXPECT_EQ(AudioProcessing::kNoError,
            apm_->high_pass_filter()->Enable(msg.hpf_enabled()));

  // NS configs.
  ASSERT_TRUE(msg.has_ns_enabled());
  EXPECT_EQ(AudioProcessing::kNoError,
            apm_->noise_suppression()->Enable(msg.ns_enabled()));

  ASSERT_TRUE(msg.has_ns_level());
  EXPECT_EQ(AudioProcessing::kNoError,
            apm_->noise_suppression()->set_level(
                static_cast<NoiseSuppression::Level>(msg.ns_level())));
}

TEST_F(DebugDumpTest, SimpleCase) {
  Config config;
  DebugDumpGenerator generator(config);
  generator.StartRecording();
  generator.Process(100);
  generator.StopRecording();
  VerifyDebugDump(generator.dump_file_name());
}

TEST_F(DebugDumpTest, ChangeInputFormat) {
  Config config;
  DebugDumpGenerator generator(config);
  generator.StartRecording();
  generator.Process(100);
  generator.SetInputRate(48000);

  generator.ForceInputMono(true);
  // Number of output channel should not be larger than that of input. APM will
  // fail otherwise.
  generator.SetOutputChannels(1);

  generator.Process(100);
  generator.StopRecording();
  VerifyDebugDump(generator.dump_file_name());
}

TEST_F(DebugDumpTest, ChangeReverseFormat) {
  Config config;
  DebugDumpGenerator generator(config);
  generator.StartRecording();
  generator.Process(100);
  generator.SetReverseRate(48000);
  generator.ForceReverseMono(true);
  generator.Process(100);
  generator.StopRecording();
  VerifyDebugDump(generator.dump_file_name());
}

TEST_F(DebugDumpTest, ChangeOutputFormat) {
  Config config;
  DebugDumpGenerator generator(config);
  generator.StartRecording();
  generator.Process(100);
  generator.SetOutputRate(48000);
  generator.SetOutputChannels(1);
  generator.Process(100);
  generator.StopRecording();
  VerifyDebugDump(generator.dump_file_name());
}

TEST_F(DebugDumpTest, ToggleAec) {
  Config config;
  DebugDumpGenerator generator(config);
  generator.StartRecording();
  generator.Process(100);

  EchoCancellation* aec = generator.apm()->echo_cancellation();
  EXPECT_EQ(AudioProcessing::kNoError, aec->Enable(!aec->is_enabled()));

  generator.Process(100);
  generator.StopRecording();
  VerifyDebugDump(generator.dump_file_name());
}

TEST_F(DebugDumpTest, ToggleDelayAgnosticAec) {
  Config config;
  config.Set<DelayAgnostic>(new DelayAgnostic(true));
  DebugDumpGenerator generator(config);
  generator.StartRecording();
  generator.Process(100);

  EchoCancellation* aec = generator.apm()->echo_cancellation();
  EXPECT_EQ(AudioProcessing::kNoError, aec->Enable(!aec->is_enabled()));

  generator.Process(100);
  generator.StopRecording();
  VerifyDebugDump(generator.dump_file_name());
}

TEST_F(DebugDumpTest, ToggleAecLevel) {
  Config config;
  DebugDumpGenerator generator(config);
  EchoCancellation* aec = generator.apm()->echo_cancellation();
  EXPECT_EQ(AudioProcessing::kNoError, aec->Enable(true));
  EXPECT_EQ(AudioProcessing::kNoError,
            aec->set_suppression_level(EchoCancellation::kLowSuppression));
  generator.StartRecording();
  generator.Process(100);

  EXPECT_EQ(AudioProcessing::kNoError,
            aec->set_suppression_level(EchoCancellation::kHighSuppression));
  generator.Process(100);
  generator.StopRecording();
  VerifyDebugDump(generator.dump_file_name());
}

#if defined(WEBRTC_ANDROID)
// AGC may not be supported on Android.
#define MAYBE_ToggleAgc DISABLED_ToggleAgc
#else
#define MAYBE_ToggleAgc ToggleAgc
#endif
TEST_F(DebugDumpTest, MAYBE_ToggleAgc) {
  Config config;
  DebugDumpGenerator generator(config);
  generator.StartRecording();
  generator.Process(100);

  GainControl* agc = generator.apm()->gain_control();
  EXPECT_EQ(AudioProcessing::kNoError, agc->Enable(!agc->is_enabled()));

  generator.Process(100);
  generator.StopRecording();
  VerifyDebugDump(generator.dump_file_name());
}

TEST_F(DebugDumpTest, ToggleNs) {
  Config config;
  DebugDumpGenerator generator(config);
  generator.StartRecording();
  generator.Process(100);

  NoiseSuppression* ns = generator.apm()->noise_suppression();
  EXPECT_EQ(AudioProcessing::kNoError, ns->Enable(!ns->is_enabled()));

  generator.Process(100);
  generator.StopRecording();
  VerifyDebugDump(generator.dump_file_name());
}

TEST_F(DebugDumpTest, TransientSuppressionOn) {
  Config config;
  config.Set<ExperimentalNs>(new ExperimentalNs(true));
  DebugDumpGenerator generator(config);
  generator.StartRecording();
  generator.Process(100);
  generator.StopRecording();
  VerifyDebugDump(generator.dump_file_name());
}

}  // namespace test
}  // namespace webrtc
