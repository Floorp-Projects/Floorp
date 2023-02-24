/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef API_TEST_PCLF_PEER_CONFIGURER_H_
#define API_TEST_PCLF_PEER_CONFIGURER_H_

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "api/async_resolver_factory.h"
#include "api/audio/audio_mixer.h"
#include "api/call/call_factory_interface.h"
#include "api/fec_controller.h"
#include "api/rtc_event_log/rtc_event_log_factory_interface.h"
#include "api/task_queue/task_queue_factory.h"
#include "api/test/create_peer_connection_quality_test_frame_generator.h"
#include "api/test/pclf/media_quality_test_params.h"
#include "api/test/peer_network_dependencies.h"
#include "api/test/peerconnection_quality_test_fixture.h"
#include "api/transport/network_control.h"
#include "api/video_codecs/video_decoder_factory.h"
#include "api/video_codecs/video_encoder_factory.h"
#include "modules/audio_processing/include/audio_processing.h"
#include "rtc_base/network.h"
#include "rtc_base/rtc_certificate_generator.h"
#include "rtc_base/ssl_certificate.h"
#include "rtc_base/thread.h"

namespace webrtc {
namespace webrtc_pc_e2e {

// This class is used to fully configure one peer inside a call.
class PeerConfigurerImpl final
    : public PeerConnectionE2EQualityTestFixture::PeerConfigurer {
 public:
  using VideoSource =
      absl::variant<std::unique_ptr<test::FrameGeneratorInterface>,
                    PeerConnectionE2EQualityTestFixture::CapturingDeviceIndex>;

  explicit PeerConfigurerImpl(
      const PeerNetworkDependencies& network_dependencies);

  PeerConfigurerImpl(rtc::Thread* network_thread,
                     rtc::NetworkManager* network_manager,
                     rtc::PacketSocketFactory* packet_socket_factory)
      : components_(
            std::make_unique<InjectableComponents>(network_thread,
                                                   network_manager,
                                                   packet_socket_factory)),
        params_(std::make_unique<Params>()),
        configurable_params_(std::make_unique<ConfigurableParams>()) {}

  // Sets peer name that will be used to report metrics related to this peer.
  // If not set, some default name will be assigned. All names have to be
  // unique.
  PeerConfigurer* SetName(absl::string_view name) override;

  // The parameters of the following 9 methods will be passed to the
  // PeerConnectionFactoryInterface implementation that will be created for
  // this peer.
  PeerConfigurer* SetTaskQueueFactory(
      std::unique_ptr<TaskQueueFactory> task_queue_factory) override;
  PeerConfigurer* SetCallFactory(
      std::unique_ptr<CallFactoryInterface> call_factory) override;
  PeerConfigurer* SetEventLogFactory(
      std::unique_ptr<RtcEventLogFactoryInterface> event_log_factory) override;
  PeerConfigurer* SetFecControllerFactory(
      std::unique_ptr<FecControllerFactoryInterface> fec_controller_factory)
      override;
  PeerConfigurer* SetNetworkControllerFactory(
      std::unique_ptr<NetworkControllerFactoryInterface>
          network_controller_factory) override;
  PeerConfigurer* SetVideoEncoderFactory(
      std::unique_ptr<VideoEncoderFactory> video_encoder_factory) override;
  PeerConfigurer* SetVideoDecoderFactory(
      std::unique_ptr<VideoDecoderFactory> video_decoder_factory) override;
  // Set a custom NetEqFactory to be used in the call.
  PeerConfigurer* SetNetEqFactory(
      std::unique_ptr<NetEqFactory> neteq_factory) override;
  PeerConfigurer* SetAudioProcessing(
      rtc::scoped_refptr<webrtc::AudioProcessing> audio_processing) override;
  PeerConfigurer* SetAudioMixer(
      rtc::scoped_refptr<webrtc::AudioMixer> audio_mixer) override;

  // Forces the Peerconnection to use the network thread as the worker thread.
  // Ie, worker thread and the network thread is the same thread.
  PeerConfigurer* SetUseNetworkThreadAsWorkerThread() override;

  // The parameters of the following 4 methods will be passed to the
  // PeerConnectionInterface implementation that will be created for this
  // peer.
  PeerConfigurer* SetAsyncResolverFactory(
      std::unique_ptr<webrtc::AsyncResolverFactory> async_resolver_factory)
      override;
  PeerConfigurer* SetRTCCertificateGenerator(
      std::unique_ptr<rtc::RTCCertificateGeneratorInterface> cert_generator)
      override;
  PeerConfigurer* SetSSLCertificateVerifier(
      std::unique_ptr<rtc::SSLCertificateVerifier> tls_cert_verifier) override;
  PeerConfigurer* SetIceTransportFactory(
      std::unique_ptr<IceTransportFactory> factory) override;
  // Flags to set on `cricket::PortAllocator`. These flags will be added
  // to the default ones that are presented on the port allocator.
  // For possible values check p2p/base/port_allocator.h.
  PeerConfigurer* SetPortAllocatorExtraFlags(uint32_t extra_flags) override;

  // Add new video stream to the call that will be sent from this peer.
  // Default implementation of video frames generator will be used.
  PeerConfigurer* AddVideoConfig(
      PeerConnectionE2EQualityTestFixture::VideoConfig config) override;
  // Add new video stream to the call that will be sent from this peer with
  // provided own implementation of video frames generator.
  PeerConfigurer* AddVideoConfig(
      PeerConnectionE2EQualityTestFixture::VideoConfig config,
      std::unique_ptr<test::FrameGeneratorInterface> generator) override;
  // Add new video stream to the call that will be sent from this peer.
  // Capturing device with specified index will be used to get input video.
  PeerConfigurer* AddVideoConfig(
      PeerConnectionE2EQualityTestFixture::VideoConfig config,
      PeerConnectionE2EQualityTestFixture::CapturingDeviceIndex
          capturing_device_index) override;
  // Sets video subscription for the peer. By default subscription will
  // include all streams with `VideoSubscription::kSameAsSendStream`
  // resolution. To override this behavior use this method.
  PeerConfigurer* SetVideoSubscription(
      PeerConnectionE2EQualityTestFixture::VideoSubscription subscription)
      override;
  // Set the list of video codecs used by the peer during the test. These
  // codecs will be negotiated in SDP during offer/answer exchange. The order
  // of these codecs during negotiation will be the same as in `video_codecs`.
  // Codecs have to be available in codecs list provided by peer connection to
  // be negotiated. If some of specified codecs won't be found, the test will
  // crash.
  PeerConfigurer* SetVideoCodecs(
      std::vector<PeerConnectionE2EQualityTestFixture::VideoCodecConfig>
          video_codecs) override;
  // Set the audio stream for the call from this peer. If this method won't
  // be invoked, this peer will send no audio.
  PeerConfigurer* SetAudioConfig(
      PeerConnectionE2EQualityTestFixture::AudioConfig config) override;

  // Set if ULP FEC should be used or not. False by default.
  PeerConfigurer* SetUseUlpFEC(bool value) override;
  // Set if Flex FEC should be used or not. False by default.
  // Client also must enable `enable_flex_fec_support` in the `RunParams` to
  // be able to use this feature.
  PeerConfigurer* SetUseFlexFEC(bool value) override;
  // Specifies how much video encoder target bitrate should be different than
  // target bitrate, provided by WebRTC stack. Must be greater than 0. Can be
  // used to emulate overshooting of video encoders. This multiplier will
  // be applied for all video encoder on both sides for all layers. Bitrate
  // estimated by WebRTC stack will be multiplied by this multiplier and then
  // provided into VideoEncoder::SetRates(...). 1.0 by default.
  PeerConfigurer* SetVideoEncoderBitrateMultiplier(double multiplier) override;

  // If is set, an RTCEventLog will be saved in that location and it will be
  // available for further analysis.
  PeerConfigurer* SetRtcEventLogPath(std::string path) override;
  // If is set, an AEC dump will be saved in that location and it will be
  // available for further analysis.
  PeerConfigurer* SetAecDumpPath(std::string path) override;
  PeerConfigurer* SetRTCConfiguration(
      PeerConnectionInterface::RTCConfiguration configuration) override;
  PeerConfigurer* SetRTCOfferAnswerOptions(
      PeerConnectionInterface::RTCOfferAnswerOptions options) override;
  // Set bitrate parameters on PeerConnection. This constraints will be
  // applied to all summed RTP streams for this peer.
  PeerConfigurer* SetBitrateSettings(BitrateSettings bitrate_settings) override;

  // Returns InjectableComponents and transfer ownership to the caller.
  // Can be called once.
  std::unique_ptr<InjectableComponents> ReleaseComponents();

  // Returns Params and transfer ownership to the caller.
  // Can be called once.
  std::unique_ptr<Params> ReleaseParams();

  // Returns ConfigurableParams and transfer ownership to the caller.
  // Can be called once.
  std::unique_ptr<ConfigurableParams> ReleaseConfigurableParams();

  // Returns video sources and transfer frame generators ownership to the
  // caller. Can be called once.
  std::vector<VideoSource> ReleaseVideoSources();

  InjectableComponents* components() { return components_.get(); }
  Params* params() { return params_.get(); }
  ConfigurableParams* configurable_params() {
    return configurable_params_.get();
  }
  const Params& params() const { return *params_; }
  const ConfigurableParams& configurable_params() const {
    return *configurable_params_;
  }
  std::vector<VideoSource>* video_sources() { return &video_sources_; }

 private:
  std::unique_ptr<InjectableComponents> components_;
  std::unique_ptr<Params> params_;
  std::unique_ptr<ConfigurableParams> configurable_params_;
  std::vector<VideoSource> video_sources_;
};

class DefaultNamesProvider {
 public:
  // Caller have to ensure that default names array will outlive names provider
  // instance.
  explicit DefaultNamesProvider(
      absl::string_view prefix,
      rtc::ArrayView<const absl::string_view> default_names = {});

  void MaybeSetName(absl::optional<std::string>& name);

 private:
  std::string GenerateName();

  std::string GenerateNameInternal();

  const std::string prefix_;
  const rtc::ArrayView<const absl::string_view> default_names_;

  std::set<std::string> known_names_;
  size_t counter_ = 0;
};

class PeerParamsPreprocessor {
 public:
  PeerParamsPreprocessor();

  // Set missing params to default values if it is required:
  //  * Generate video stream labels if some of them are missing
  //  * Generate audio stream labels if some of them are missing
  //  * Set video source generation mode if it is not specified
  //  * Video codecs under test
  void SetDefaultValuesForMissingParams(PeerConfigurerImpl& peer);

  // Validate peer's parameters, also ensure uniqueness of all video stream
  // labels.
  void ValidateParams(const PeerConfigurerImpl& peer);

 private:
  DefaultNamesProvider peer_names_provider_;

  std::set<std::string> peer_names_;
  std::set<std::string> video_labels_;
  std::set<std::string> audio_labels_;
  std::set<std::string> video_sync_groups_;
  std::set<std::string> audio_sync_groups_;
};

}  // namespace webrtc_pc_e2e
}  // namespace webrtc

#endif  // API_TEST_PCLF_PEER_CONFIGURER_H_
