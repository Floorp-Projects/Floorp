/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/test/pclf/peer_configurer.h"

#include <set>

#include "absl/strings/string_view.h"
#include "api/test/peer_network_dependencies.h"
#include "modules/video_coding/svc/create_scalability_structure.h"
#include "modules/video_coding/svc/scalability_mode_util.h"
#include "rtc_base/arraysize.h"
#include "test/testsupport/file_utils.h"

namespace webrtc {
namespace webrtc_pc_e2e {
namespace {

using AudioConfig = PeerConnectionE2EQualityTestFixture::AudioConfig;
using VideoConfig = PeerConnectionE2EQualityTestFixture::VideoConfig;
using RunParams = PeerConnectionE2EQualityTestFixture::RunParams;
using VideoCodecConfig = PeerConnectionE2EQualityTestFixture::VideoCodecConfig;
using PeerConfigurer = PeerConnectionE2EQualityTestFixture::PeerConfigurer;

// List of default names of generic participants according to
// https://en.wikipedia.org/wiki/Alice_and_Bob
constexpr absl::string_view kDefaultNames[] = {"alice", "bob",  "charlie",
                                               "david", "erin", "frank"};

}  // namespace

PeerConfigurerImpl::PeerConfigurerImpl(
    const PeerNetworkDependencies& network_dependencies)
    : components_(std::make_unique<InjectableComponents>(
          network_dependencies.network_thread,
          network_dependencies.network_manager,
          network_dependencies.packet_socket_factory)),
      params_(std::make_unique<Params>()),
      configurable_params_(std::make_unique<ConfigurableParams>()) {}

PeerConfigurer* PeerConfigurerImpl::SetName(absl::string_view name) {
  params_->name = std::string(name);
  return this;
}

PeerConfigurer* PeerConfigurerImpl::SetTaskQueueFactory(
    std::unique_ptr<TaskQueueFactory> task_queue_factory) {
  components_->pcf_dependencies->task_queue_factory =
      std::move(task_queue_factory);
  return this;
}
PeerConfigurer* PeerConfigurerImpl::SetCallFactory(
    std::unique_ptr<CallFactoryInterface> call_factory) {
  components_->pcf_dependencies->call_factory = std::move(call_factory);
  return this;
}
PeerConfigurer* PeerConfigurerImpl::SetEventLogFactory(
    std::unique_ptr<RtcEventLogFactoryInterface> event_log_factory) {
  components_->pcf_dependencies->event_log_factory =
      std::move(event_log_factory);
  return this;
}
PeerConfigurer* PeerConfigurerImpl::SetFecControllerFactory(
    std::unique_ptr<FecControllerFactoryInterface> fec_controller_factory) {
  components_->pcf_dependencies->fec_controller_factory =
      std::move(fec_controller_factory);
  return this;
}
PeerConfigurer* PeerConfigurerImpl::SetNetworkControllerFactory(
    std::unique_ptr<NetworkControllerFactoryInterface>
        network_controller_factory) {
  components_->pcf_dependencies->network_controller_factory =
      std::move(network_controller_factory);
  return this;
}
PeerConfigurer* PeerConfigurerImpl::SetVideoEncoderFactory(
    std::unique_ptr<VideoEncoderFactory> video_encoder_factory) {
  components_->pcf_dependencies->video_encoder_factory =
      std::move(video_encoder_factory);
  return this;
}
PeerConfigurer* PeerConfigurerImpl::SetVideoDecoderFactory(
    std::unique_ptr<VideoDecoderFactory> video_decoder_factory) {
  components_->pcf_dependencies->video_decoder_factory =
      std::move(video_decoder_factory);
  return this;
}

PeerConfigurer* PeerConfigurerImpl::SetAsyncResolverFactory(
    std::unique_ptr<webrtc::AsyncResolverFactory> async_resolver_factory) {
  components_->pc_dependencies->async_resolver_factory =
      std::move(async_resolver_factory);
  return this;
}
PeerConfigurer* PeerConfigurerImpl::SetRTCCertificateGenerator(
    std::unique_ptr<rtc::RTCCertificateGeneratorInterface> cert_generator) {
  components_->pc_dependencies->cert_generator = std::move(cert_generator);
  return this;
}
PeerConfigurer* PeerConfigurerImpl::SetSSLCertificateVerifier(
    std::unique_ptr<rtc::SSLCertificateVerifier> tls_cert_verifier) {
  components_->pc_dependencies->tls_cert_verifier =
      std::move(tls_cert_verifier);
  return this;
}

PeerConfigurer* PeerConfigurerImpl::AddVideoConfig(
    PeerConnectionE2EQualityTestFixture::VideoConfig config) {
  video_sources_.push_back(
      CreateSquareFrameGenerator(config, /*type=*/absl::nullopt));
  configurable_params_->video_configs.push_back(std::move(config));
  return this;
}
PeerConfigurer* PeerConfigurerImpl::AddVideoConfig(
    PeerConnectionE2EQualityTestFixture::VideoConfig config,
    std::unique_ptr<test::FrameGeneratorInterface> generator) {
  configurable_params_->video_configs.push_back(std::move(config));
  video_sources_.push_back(std::move(generator));
  return this;
}
PeerConfigurer* PeerConfigurerImpl::AddVideoConfig(
    PeerConnectionE2EQualityTestFixture::VideoConfig config,
    PeerConnectionE2EQualityTestFixture::CapturingDeviceIndex index) {
  configurable_params_->video_configs.push_back(std::move(config));
  video_sources_.push_back(index);
  return this;
}
PeerConfigurer* PeerConfigurerImpl::SetVideoSubscription(
    PeerConnectionE2EQualityTestFixture::VideoSubscription subscription) {
  configurable_params_->video_subscription = std::move(subscription);
  return this;
}
PeerConfigurer* PeerConfigurerImpl::SetAudioConfig(
    PeerConnectionE2EQualityTestFixture::AudioConfig config) {
  params_->audio_config = std::move(config);
  return this;
}
PeerConfigurer* PeerConfigurerImpl::SetUseUlpFEC(bool value) {
  params_->use_ulp_fec = value;
  return this;
}
PeerConfigurer* PeerConfigurerImpl::SetUseFlexFEC(bool value) {
  params_->use_flex_fec = value;
  return this;
}
PeerConfigurer* PeerConfigurerImpl::SetVideoEncoderBitrateMultiplier(
    double multiplier) {
  params_->video_encoder_bitrate_multiplier = multiplier;
  return this;
}
PeerConfigurer* PeerConfigurerImpl::SetNetEqFactory(
    std::unique_ptr<NetEqFactory> neteq_factory) {
  components_->pcf_dependencies->neteq_factory = std::move(neteq_factory);
  return this;
}
PeerConfigurer* PeerConfigurerImpl::SetAudioProcessing(
    rtc::scoped_refptr<webrtc::AudioProcessing> audio_processing) {
  components_->pcf_dependencies->audio_processing = audio_processing;
  return this;
}
PeerConfigurer* PeerConfigurerImpl::SetAudioMixer(
    rtc::scoped_refptr<webrtc::AudioMixer> audio_mixer) {
  components_->pcf_dependencies->audio_mixer = audio_mixer;
  return this;
}

PeerConfigurer* PeerConfigurerImpl::SetUseNetworkThreadAsWorkerThread() {
  components_->worker_thread = components_->network_thread;
  return this;
}

PeerConfigurer* PeerConfigurerImpl::SetRtcEventLogPath(std::string path) {
  params_->rtc_event_log_path = std::move(path);
  return this;
}
PeerConfigurer* PeerConfigurerImpl::SetAecDumpPath(std::string path) {
  params_->aec_dump_path = std::move(path);
  return this;
}
PeerConfigurer* PeerConfigurerImpl::SetRTCConfiguration(
    PeerConnectionInterface::RTCConfiguration configuration) {
  params_->rtc_configuration = std::move(configuration);
  return this;
}
PeerConfigurer* PeerConfigurerImpl::SetRTCOfferAnswerOptions(
    PeerConnectionInterface::RTCOfferAnswerOptions options) {
  params_->rtc_offer_answer_options = std::move(options);
  return this;
}
PeerConfigurer* PeerConfigurerImpl::SetBitrateSettings(
    BitrateSettings bitrate_settings) {
  params_->bitrate_settings = bitrate_settings;
  return this;
}
PeerConfigurer* PeerConfigurerImpl::SetVideoCodecs(
    std::vector<PeerConnectionE2EQualityTestFixture::VideoCodecConfig>
        video_codecs) {
  params_->video_codecs = std::move(video_codecs);
  return this;
}

PeerConfigurer* PeerConfigurerImpl::SetIceTransportFactory(
    std::unique_ptr<IceTransportFactory> factory) {
  components_->pc_dependencies->ice_transport_factory = std::move(factory);
  return this;
}

PeerConfigurer* PeerConfigurerImpl::SetPortAllocatorExtraFlags(
    uint32_t extra_flags) {
  params_->port_allocator_extra_flags = extra_flags;
  return this;
}
std::unique_ptr<InjectableComponents> PeerConfigurerImpl::ReleaseComponents() {
  RTC_CHECK(components_);
  auto components = std::move(components_);
  components_ = nullptr;
  return components;
}

// Returns Params and transfer ownership to the caller.
// Can be called once.
std::unique_ptr<Params> PeerConfigurerImpl::ReleaseParams() {
  RTC_CHECK(params_);
  auto params = std::move(params_);
  params_ = nullptr;
  return params;
}

// Returns ConfigurableParams and transfer ownership to the caller.
// Can be called once.
std::unique_ptr<ConfigurableParams>
PeerConfigurerImpl::ReleaseConfigurableParams() {
  RTC_CHECK(configurable_params_);
  auto configurable_params = std::move(configurable_params_);
  configurable_params_ = nullptr;
  return configurable_params;
}

// Returns video sources and transfer frame generators ownership to the
// caller. Can be called once.
std::vector<PeerConfigurerImpl::VideoSource>
PeerConfigurerImpl::ReleaseVideoSources() {
  auto video_sources = std::move(video_sources_);
  video_sources_.clear();
  return video_sources;
}

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
      if (!video_config.encoding_params.empty()) {
        RTC_CHECK_EQ(video_config.simulcast_config->simulcast_streams_count,
                     video_config.encoding_params.size())
            << "|encoding_params| have to be specified for each simulcast "
            << "stream in |video_config|.";
      }
    } else {
      RTC_CHECK_LE(video_config.encoding_params.size(), 1)
          << "|encoding_params| has multiple values but simulcast is not "
             "enabled.";
    }

    if (video_config.emulated_sfu_config) {
      if (video_config.simulcast_config &&
          video_config.emulated_sfu_config->target_layer_index) {
        RTC_CHECK_LT(*video_config.emulated_sfu_config->target_layer_index,
                     video_config.simulcast_config->simulcast_streams_count);
      }
      if (!video_config.encoding_params.empty()) {
        bool is_svc = false;
        for (const auto& encoding_param : video_config.encoding_params) {
          if (!encoding_param.scalability_mode)
            continue;

          absl::optional<ScalabilityMode> scalability_mode =
              ScalabilityModeFromString(*encoding_param.scalability_mode);
          RTC_CHECK(scalability_mode) << "Unknown scalability_mode requested";

          absl::optional<ScalableVideoController::StreamLayersConfig>
              stream_layers_config =
                  ScalabilityStructureConfig(*scalability_mode);
          is_svc |= stream_layers_config->num_spatial_layers > 1;
          RTC_CHECK(stream_layers_config->num_spatial_layers == 1 ||
                    video_config.encoding_params.size() == 1)
              << "Can't enable SVC modes with multiple spatial layers ("
              << stream_layers_config->num_spatial_layers
              << " layers) or simulcast ("
              << video_config.encoding_params.size() << " layers)";
          if (video_config.emulated_sfu_config->target_layer_index) {
            RTC_CHECK_LT(*video_config.emulated_sfu_config->target_layer_index,
                         stream_layers_config->num_spatial_layers);
          }
        }
        if (!is_svc && video_config.emulated_sfu_config->target_layer_index) {
          RTC_CHECK_LT(*video_config.emulated_sfu_config->target_layer_index,
                       video_config.encoding_params.size());
        }
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
