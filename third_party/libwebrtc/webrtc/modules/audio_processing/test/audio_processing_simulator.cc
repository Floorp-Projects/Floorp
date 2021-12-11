/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/test/audio_processing_simulator.h"

#include <algorithm>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "common_audio/include/audio_util.h"
#include "modules/audio_processing/aec_dump/aec_dump_factory.h"
#include "modules/audio_processing/include/audio_processing.h"
#include "modules/audio_processing/test/fake_recording_device.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/stringutils.h"

namespace webrtc {
namespace test {
namespace {

void CopyFromAudioFrame(const AudioFrame& src, ChannelBuffer<float>* dest) {
  RTC_CHECK_EQ(src.num_channels_, dest->num_channels());
  RTC_CHECK_EQ(src.samples_per_channel_, dest->num_frames());
  // Copy the data from the input buffer.
  std::vector<float> tmp(src.samples_per_channel_ * src.num_channels_);
  S16ToFloat(src.data(), tmp.size(), tmp.data());
  Deinterleave(tmp.data(), src.samples_per_channel_, src.num_channels_,
               dest->channels());
}

std::string GetIndexedOutputWavFilename(const std::string& wav_name,
                                        int counter) {
  std::stringstream ss;
  ss << wav_name.substr(0, wav_name.size() - 4) << "_" << counter
     << wav_name.substr(wav_name.size() - 4);
  return ss.str();
}

void WriteEchoLikelihoodGraphFileHeader(std::ofstream* output_file) {
  (*output_file) << "import numpy as np" << std::endl
                 << "import matplotlib.pyplot as plt" << std::endl
                 << "y = np.array([";
}

void WriteEchoLikelihoodGraphFileFooter(std::ofstream* output_file) {
  (*output_file) << "])" << std::endl
                 << "x = np.arange(len(y))*.01" << std::endl
                 << "plt.plot(x, y)" << std::endl
                 << "plt.ylabel('Echo likelihood')" << std::endl
                 << "plt.xlabel('Time (s)')" << std::endl
                 << "plt.ylim([0,1])" << std::endl
                 << "plt.show()" << std::endl;
}

}  // namespace

SimulationSettings::SimulationSettings() = default;
SimulationSettings::SimulationSettings(const SimulationSettings&) = default;
SimulationSettings::~SimulationSettings() = default;

void CopyToAudioFrame(const ChannelBuffer<float>& src, AudioFrame* dest) {
  RTC_CHECK_EQ(src.num_channels(), dest->num_channels_);
  RTC_CHECK_EQ(src.num_frames(), dest->samples_per_channel_);
  int16_t* dest_data = dest->mutable_data();
  for (size_t ch = 0; ch < dest->num_channels_; ++ch) {
    for (size_t sample = 0; sample < dest->samples_per_channel_; ++sample) {
      dest_data[sample * dest->num_channels_ + ch] =
          src.channels()[ch][sample] * 32767;
    }
  }
}

AudioProcessingSimulator::AudioProcessingSimulator(
    const SimulationSettings& settings)
    : settings_(settings),
      analog_mic_level_(settings.initial_mic_level),
      fake_recording_device_(
          settings.initial_mic_level,
          settings_.simulate_mic_gain ? *settings.simulated_mic_kind : 0),
      worker_queue_("file_writer_task_queue") {
  if (settings_.ed_graph_output_filename &&
      !settings_.ed_graph_output_filename->empty()) {
    residual_echo_likelihood_graph_writer_.open(
        *settings_.ed_graph_output_filename);
    RTC_CHECK(residual_echo_likelihood_graph_writer_.is_open());
    WriteEchoLikelihoodGraphFileHeader(&residual_echo_likelihood_graph_writer_);
  }

  if (settings_.simulate_mic_gain)
    RTC_LOG(LS_VERBOSE) << "Simulating analog mic gain";
}

AudioProcessingSimulator::~AudioProcessingSimulator() {
  if (residual_echo_likelihood_graph_writer_.is_open()) {
    WriteEchoLikelihoodGraphFileFooter(&residual_echo_likelihood_graph_writer_);
    residual_echo_likelihood_graph_writer_.close();
  }
}

AudioProcessingSimulator::ScopedTimer::~ScopedTimer() {
  int64_t interval = rtc::TimeNanos() - start_time_;
  proc_time_->sum += interval;
  proc_time_->max = std::max(proc_time_->max, interval);
  proc_time_->min = std::min(proc_time_->min, interval);
}

void AudioProcessingSimulator::ProcessStream(bool fixed_interface) {
  // Optionally use the fake recording device to simulate analog gain.
  if (settings_.simulate_mic_gain) {
    if (settings_.aec_dump_input_filename) {
      // When the analog gain is simulated and an AEC dump is used as input, set
      // the undo level to |aec_dump_mic_level_| to virtually restore the
      // unmodified microphone signal level.
      fake_recording_device_.SetUndoMicLevel(aec_dump_mic_level_);
    }

    if (fixed_interface) {
      fake_recording_device_.SimulateAnalogGain(&fwd_frame_);
    } else {
      fake_recording_device_.SimulateAnalogGain(in_buf_.get());
    }

    // Notify the current mic level to AGC.
    RTC_CHECK_EQ(AudioProcessing::kNoError,
                 ap_->gain_control()->set_stream_analog_level(
                     fake_recording_device_.MicLevel()));
  } else {
    // Notify the current mic level to AGC.
    RTC_CHECK_EQ(AudioProcessing::kNoError,
                 ap_->gain_control()->set_stream_analog_level(
                     settings_.aec_dump_input_filename ? aec_dump_mic_level_
                                                       : analog_mic_level_));
  }

  // Process the current audio frame.
  if (fixed_interface) {
    {
      const auto st = ScopedTimer(mutable_proc_time());
      RTC_CHECK_EQ(AudioProcessing::kNoError, ap_->ProcessStream(&fwd_frame_));
    }
    CopyFromAudioFrame(fwd_frame_, out_buf_.get());
  } else {
    const auto st = ScopedTimer(mutable_proc_time());
    RTC_CHECK_EQ(AudioProcessing::kNoError,
                 ap_->ProcessStream(in_buf_->channels(), in_config_,
                                    out_config_, out_buf_->channels()));
  }

  // Store the mic level suggested by AGC.
  // Note that when the analog gain is simulated and an AEC dump is used as
  // input, |analog_mic_level_| will not be used with set_stream_analog_level().
  analog_mic_level_ = ap_->gain_control()->stream_analog_level();
  if (settings_.simulate_mic_gain) {
    fake_recording_device_.SetMicLevel(analog_mic_level_);
  }

  if (buffer_writer_) {
    buffer_writer_->Write(*out_buf_);
  }

  if (residual_echo_likelihood_graph_writer_.is_open()) {
    auto stats = ap_->GetStatistics();
    residual_echo_likelihood_graph_writer_ << stats.residual_echo_likelihood
                                           << ", ";
  }

  ++num_process_stream_calls_;
}

void AudioProcessingSimulator::ProcessReverseStream(bool fixed_interface) {
  if (fixed_interface) {
    const auto st = ScopedTimer(mutable_proc_time());
    RTC_CHECK_EQ(AudioProcessing::kNoError,
                 ap_->ProcessReverseStream(&rev_frame_));
    CopyFromAudioFrame(rev_frame_, reverse_out_buf_.get());

  } else {
    const auto st = ScopedTimer(mutable_proc_time());
    RTC_CHECK_EQ(AudioProcessing::kNoError,
                 ap_->ProcessReverseStream(
                     reverse_in_buf_->channels(), reverse_in_config_,
                     reverse_out_config_, reverse_out_buf_->channels()));
  }

  if (reverse_buffer_writer_) {
    reverse_buffer_writer_->Write(*reverse_out_buf_);
  }

  ++num_reverse_process_stream_calls_;
}

void AudioProcessingSimulator::SetupBuffersConfigsOutputs(
    int input_sample_rate_hz,
    int output_sample_rate_hz,
    int reverse_input_sample_rate_hz,
    int reverse_output_sample_rate_hz,
    int input_num_channels,
    int output_num_channels,
    int reverse_input_num_channels,
    int reverse_output_num_channels) {
  in_config_ = StreamConfig(input_sample_rate_hz, input_num_channels);
  in_buf_.reset(new ChannelBuffer<float>(
      rtc::CheckedDivExact(input_sample_rate_hz, kChunksPerSecond),
      input_num_channels));

  reverse_in_config_ =
      StreamConfig(reverse_input_sample_rate_hz, reverse_input_num_channels);
  reverse_in_buf_.reset(new ChannelBuffer<float>(
      rtc::CheckedDivExact(reverse_input_sample_rate_hz, kChunksPerSecond),
      reverse_input_num_channels));

  out_config_ = StreamConfig(output_sample_rate_hz, output_num_channels);
  out_buf_.reset(new ChannelBuffer<float>(
      rtc::CheckedDivExact(output_sample_rate_hz, kChunksPerSecond),
      output_num_channels));

  reverse_out_config_ =
      StreamConfig(reverse_output_sample_rate_hz, reverse_output_num_channels);
  reverse_out_buf_.reset(new ChannelBuffer<float>(
      rtc::CheckedDivExact(reverse_output_sample_rate_hz, kChunksPerSecond),
      reverse_output_num_channels));

  fwd_frame_.sample_rate_hz_ = input_sample_rate_hz;
  fwd_frame_.samples_per_channel_ =
      rtc::CheckedDivExact(fwd_frame_.sample_rate_hz_, kChunksPerSecond);
  fwd_frame_.num_channels_ = input_num_channels;

  rev_frame_.sample_rate_hz_ = reverse_input_sample_rate_hz;
  rev_frame_.samples_per_channel_ =
      rtc::CheckedDivExact(rev_frame_.sample_rate_hz_, kChunksPerSecond);
  rev_frame_.num_channels_ = reverse_input_num_channels;

  if (settings_.use_verbose_logging) {
    rtc::LogMessage::LogToDebug(rtc::LS_VERBOSE);

    std::cout << "Sample rates:" << std::endl;
    std::cout << " Forward input: " << input_sample_rate_hz << std::endl;
    std::cout << " Forward output: " << output_sample_rate_hz << std::endl;
    std::cout << " Reverse input: " << reverse_input_sample_rate_hz
              << std::endl;
    std::cout << " Reverse output: " << reverse_output_sample_rate_hz
              << std::endl;
    std::cout << "Number of channels: " << std::endl;
    std::cout << " Forward input: " << input_num_channels << std::endl;
    std::cout << " Forward output: " << output_num_channels << std::endl;
    std::cout << " Reverse input: " << reverse_input_num_channels << std::endl;
    std::cout << " Reverse output: " << reverse_output_num_channels
              << std::endl;
  }

  SetupOutput();
}

void AudioProcessingSimulator::SetupOutput() {
  if (settings_.output_filename) {
    std::string filename;
    if (settings_.store_intermediate_output) {
      filename = GetIndexedOutputWavFilename(*settings_.output_filename,
                                             output_reset_counter_);
    } else {
      filename = *settings_.output_filename;
    }

    std::unique_ptr<WavWriter> out_file(
        new WavWriter(filename, out_config_.sample_rate_hz(),
                      static_cast<size_t>(out_config_.num_channels())));
    buffer_writer_.reset(new ChannelBufferWavWriter(std::move(out_file)));
  }

  if (settings_.reverse_output_filename) {
    std::string filename;
    if (settings_.store_intermediate_output) {
      filename = GetIndexedOutputWavFilename(*settings_.reverse_output_filename,
                                             output_reset_counter_);
    } else {
      filename = *settings_.reverse_output_filename;
    }

    std::unique_ptr<WavWriter> reverse_out_file(
        new WavWriter(filename, reverse_out_config_.sample_rate_hz(),
                      static_cast<size_t>(reverse_out_config_.num_channels())));
    reverse_buffer_writer_.reset(
        new ChannelBufferWavWriter(std::move(reverse_out_file)));
  }

  ++output_reset_counter_;
}

void AudioProcessingSimulator::DestroyAudioProcessor() {
  if (settings_.aec_dump_output_filename) {
    ap_->DetachAecDump();
  }
}

void AudioProcessingSimulator::CreateAudioProcessor() {
  Config config;
  AudioProcessing::Config apm_config;
  std::unique_ptr<EchoControlFactory> echo_control_factory;
  if (settings_.use_bf && *settings_.use_bf) {
    config.Set<Beamforming>(new Beamforming(
        true, ParseArrayGeometry(*settings_.microphone_positions),
        SphericalPointf(DegreesToRadians(settings_.target_angle_degrees), 0.f,
                        1.f)));
  }
  if (settings_.use_ts) {
    config.Set<ExperimentalNs>(new ExperimentalNs(*settings_.use_ts));
  }
  if (settings_.use_ie) {
    config.Set<Intelligibility>(new Intelligibility(*settings_.use_ie));
  }
  if (settings_.use_agc2) {
    apm_config.gain_controller2.enabled = *settings_.use_agc2;
    apm_config.gain_controller2.fixed_gain_db = settings_.agc2_fixed_gain_db;
  }
  if (settings_.use_aec3 && *settings_.use_aec3) {
    echo_control_factory.reset(new EchoCanceller3Factory());
  }
  if (settings_.use_lc) {
    apm_config.level_controller.enabled = *settings_.use_lc;
  }
  if (settings_.use_hpf) {
    apm_config.high_pass_filter.enabled = *settings_.use_hpf;
  }

  if (settings_.use_refined_adaptive_filter) {
    config.Set<RefinedAdaptiveFilter>(
        new RefinedAdaptiveFilter(*settings_.use_refined_adaptive_filter));
  }
  config.Set<ExtendedFilter>(new ExtendedFilter(
      !settings_.use_extended_filter || *settings_.use_extended_filter));
  config.Set<DelayAgnostic>(new DelayAgnostic(!settings_.use_delay_agnostic ||
                                              *settings_.use_delay_agnostic));
  config.Set<ExperimentalAgc>(new ExperimentalAgc(
      !settings_.use_experimental_agc || *settings_.use_experimental_agc));
  if (settings_.use_ed) {
    apm_config.residual_echo_detector.enabled = *settings_.use_ed;
  }

  ap_.reset(AudioProcessing::Create(config, nullptr,
                                    std::move(echo_control_factory), nullptr));
  RTC_CHECK(ap_);

  ap_->ApplyConfig(apm_config);

  if (settings_.use_aec) {
    RTC_CHECK_EQ(AudioProcessing::kNoError,
                 ap_->echo_cancellation()->Enable(*settings_.use_aec));
  }
  if (settings_.use_aecm) {
    RTC_CHECK_EQ(AudioProcessing::kNoError,
                 ap_->echo_control_mobile()->Enable(*settings_.use_aecm));
  }
  if (settings_.use_agc) {
    RTC_CHECK_EQ(AudioProcessing::kNoError,
                 ap_->gain_control()->Enable(*settings_.use_agc));
  }
  if (settings_.use_ns) {
    RTC_CHECK_EQ(AudioProcessing::kNoError,
                 ap_->noise_suppression()->Enable(*settings_.use_ns));
  }
  if (settings_.use_le) {
    RTC_CHECK_EQ(AudioProcessing::kNoError,
                 ap_->level_estimator()->Enable(*settings_.use_le));
  }
  if (settings_.use_vad) {
    RTC_CHECK_EQ(AudioProcessing::kNoError,
                 ap_->voice_detection()->Enable(*settings_.use_vad));
  }
  if (settings_.use_agc_limiter) {
    RTC_CHECK_EQ(AudioProcessing::kNoError, ap_->gain_control()->enable_limiter(
                                                *settings_.use_agc_limiter));
  }
  if (settings_.agc_target_level) {
    RTC_CHECK_EQ(AudioProcessing::kNoError,
                 ap_->gain_control()->set_target_level_dbfs(
                     *settings_.agc_target_level));
  }
  if (settings_.agc_compression_gain) {
    RTC_CHECK_EQ(AudioProcessing::kNoError,
                 ap_->gain_control()->set_compression_gain_db(
                     *settings_.agc_compression_gain));
  }
  if (settings_.agc_mode) {
    RTC_CHECK_EQ(
        AudioProcessing::kNoError,
        ap_->gain_control()->set_mode(
            static_cast<webrtc::GainControl::Mode>(*settings_.agc_mode)));
  }

  if (settings_.use_drift_compensation) {
    RTC_CHECK_EQ(AudioProcessing::kNoError,
                 ap_->echo_cancellation()->enable_drift_compensation(
                     *settings_.use_drift_compensation));
  }

  if (settings_.aec_suppression_level) {
    RTC_CHECK_EQ(AudioProcessing::kNoError,
                 ap_->echo_cancellation()->set_suppression_level(
                     static_cast<webrtc::EchoCancellation::SuppressionLevel>(
                         *settings_.aec_suppression_level)));
  }

  if (settings_.aecm_routing_mode) {
    RTC_CHECK_EQ(AudioProcessing::kNoError,
                 ap_->echo_control_mobile()->set_routing_mode(
                     static_cast<webrtc::EchoControlMobile::RoutingMode>(
                         *settings_.aecm_routing_mode)));
  }

  if (settings_.use_aecm_comfort_noise) {
    RTC_CHECK_EQ(AudioProcessing::kNoError,
                 ap_->echo_control_mobile()->enable_comfort_noise(
                     *settings_.use_aecm_comfort_noise));
  }

  if (settings_.vad_likelihood) {
    RTC_CHECK_EQ(AudioProcessing::kNoError,
                 ap_->voice_detection()->set_likelihood(
                     static_cast<webrtc::VoiceDetection::Likelihood>(
                         *settings_.vad_likelihood)));
  }
  if (settings_.ns_level) {
    RTC_CHECK_EQ(
        AudioProcessing::kNoError,
        ap_->noise_suppression()->set_level(
            static_cast<NoiseSuppression::Level>(*settings_.ns_level)));
  }

  if (settings_.use_ts) {
    ap_->set_stream_key_pressed(*settings_.use_ts);
  }

  if (settings_.aec_dump_output_filename) {
    ap_->AttachAecDump(AecDumpFactory::Create(
        *settings_.aec_dump_output_filename, -1, &worker_queue_));
  }
}

}  // namespace test
}  // namespace webrtc
