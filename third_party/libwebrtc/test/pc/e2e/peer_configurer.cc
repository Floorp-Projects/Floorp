/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/pc/e2e/peer_configurer.h"

#include <set>

#include "absl/strings/string_view.h"
#include "rtc_base/arraysize.h"
#include "test/testsupport/file_utils.h"

namespace webrtc {
namespace webrtc_pc_e2e {
namespace {

using AudioConfig = PeerConnectionE2EQualityTestFixture::AudioConfig;
using VideoConfig = PeerConnectionE2EQualityTestFixture::VideoConfig;
using RunParams = PeerConnectionE2EQualityTestFixture::RunParams;
using VideoCodecConfig = PeerConnectionE2EQualityTestFixture::VideoCodecConfig;

// List of default names of generic participants according to
// https://en.wikipedia.org/wiki/Alice_and_Bob
constexpr absl::string_view kDefaultNames[] = {"alice", "bob",  "charlie",
                                               "david", "erin", "frank"};

}  // namespace

DefaultNamesProvider::DefaultNamesProvider(
    absl::string_view prefix,
    rtc::ArrayView<const absl::string_view> default_names)
    : prefix_(prefix), default_names_(default_names) {}

void DefaultNamesProvider::MaybeSetName(absl::optional<std::string>& name) {
  if (name.has_value()) {
    known_names_.insert(name.value());
  } else {
    name = GenerateName();
  }
}

std::string DefaultNamesProvider::GenerateName() {
  std::string name;
  do {
    name = GenerateNameInternal();
  } while (!known_names_.insert(name).second);
  return name;
}

std::string DefaultNamesProvider::GenerateNameInternal() {
  if (counter_ < default_names_.size()) {
    return std::string(default_names_[counter_++]);
  }
  return prefix_ + std::to_string(counter_++);
}

PeerParamsPreprocessor::PeerParamsPreprocessor()
    : peer_names_provider_("peer_", kDefaultNames) {}

void PeerParamsPreprocessor::SetDefaultValuesForMissingParams(
    PeerConfigurerImpl& peer) {
  Params* params = peer.params();
  ConfigurableParams* configurable_params = peer.configurable_params();
  peer_names_provider_.MaybeSetName(params->name);
  DefaultNamesProvider video_stream_names_provider(*params->name +
                                                   "_auto_video_stream_label_");
  for (VideoConfig& config : configurable_params->video_configs) {
    video_stream_names_provider.MaybeSetName(config.stream_label);
  }
  if (params->audio_config) {
    DefaultNamesProvider audio_stream_names_provider(
        *params->name + "_auto_audio_stream_label_");
    audio_stream_names_provider.MaybeSetName(
        params->audio_config->stream_label);
  }

  if (params->video_codecs.empty()) {
    params->video_codecs.push_back(
        PeerConnectionE2EQualityTestFixture::VideoCodecConfig(
            cricket::kVp8CodecName));
  }
}

void PeerParamsPreprocessor::ValidateParams(const PeerConfigurerImpl& peer) {
  const Params& p = peer.params();
  RTC_CHECK_GT(p.video_encoder_bitrate_multiplier, 0.0);
  // Each peer should at least support 1 video codec.
  RTC_CHECK_GE(p.video_codecs.size(), 1);

  {
    RTC_CHECK(p.name);
    bool inserted = peer_names_.insert(p.name.value()).second;
    RTC_CHECK(inserted) << "Duplicate name=" << p.name.value();
  }

  // Validate that all video stream labels are unique and sync groups are
  // valid.
  for (const VideoConfig& video_config :
       peer.configurable_params().video_configs) {
    RTC_CHECK(video_config.stream_label);
    bool inserted =
        video_labels_.insert(video_config.stream_label.value()).second;
    RTC_CHECK(inserted) << "Duplicate video_config.stream_label="
                        << video_config.stream_label.value();

    if (video_config.input_dump_file_name.has_value()) {
      RTC_CHECK_GT(video_config.input_dump_sampling_modulo, 0)
          << "video_config.input_dump_sampling_modulo must be greater than 0";
    }
    if (video_config.output_dump_file_name.has_value()) {
      RTC_CHECK_GT(video_config.output_dump_sampling_modulo, 0)
          << "video_config.input_dump_sampling_modulo must be greater than 0";
    }

    // TODO(bugs.webrtc.org/4762): remove this check after synchronization of
    // more than two streams is supported.
    if (video_config.sync_group.has_value()) {
      bool sync_group_inserted =
          video_sync_groups_.insert(video_config.sync_group.value()).second;
      RTC_CHECK(sync_group_inserted)
          << "Sync group shouldn't consist of more than two streams (one "
             "video and one audio). Duplicate video_config.sync_group="
          << video_config.sync_group.value();
    }

    if (video_config.simulcast_config) {
      if (video_config.simulcast_config->target_spatial_index) {
        RTC_CHECK_GE(*video_config.simulcast_config->target_spatial_index, 0);
        RTC_CHECK_LT(*video_config.simulcast_config->target_spatial_index,
                     video_config.simulcast_config->simulcast_streams_count);
      }
      RTC_CHECK(!video_config.max_encode_bitrate_bps)
          << "Setting max encode bitrate is not implemented for simulcast.";
      RTC_CHECK(!video_config.min_encode_bitrate_bps)
          << "Setting min encode bitrate is not implemented for simulcast.";
      if (p.video_codecs[0].name == cricket::kVp8CodecName &&
          !video_config.simulcast_config->encoding_params.empty()) {
        RTC_CHECK_EQ(video_config.simulcast_config->simulcast_streams_count,
                     video_config.simulcast_config->encoding_params.size())
            << "|encoding_params| have to be specified for each simulcast "
            << "stream in |simulcast_config|.";
      }
    }
  }
  if (p.audio_config) {
    bool inserted =
        audio_labels_.insert(p.audio_config->stream_label.value()).second;
    RTC_CHECK(inserted) << "Duplicate audio_config.stream_label="
                        << p.audio_config->stream_label.value();
    // TODO(bugs.webrtc.org/4762): remove this check after synchronization of
    // more than two streams is supported.
    if (p.audio_config->sync_group.has_value()) {
      bool sync_group_inserted =
          audio_sync_groups_.insert(p.audio_config->sync_group.value()).second;
      RTC_CHECK(sync_group_inserted)
          << "Sync group shouldn't consist of more than two streams (one "
             "video and one audio). Duplicate audio_config.sync_group="
          << p.audio_config->sync_group.value();
    }
    // Check that if mode input file name specified only if mode is kFile.
    if (p.audio_config.value().mode == AudioConfig::Mode::kGenerated) {
      RTC_CHECK(!p.audio_config.value().input_file_name);
    }
    if (p.audio_config.value().mode == AudioConfig::Mode::kFile) {
      RTC_CHECK(p.audio_config.value().input_file_name);
      RTC_CHECK(
          test::FileExists(p.audio_config.value().input_file_name.value()))
          << p.audio_config.value().input_file_name.value() << " doesn't exist";
    }
  }
}

}  // namespace webrtc_pc_e2e
}  // namespace webrtc
