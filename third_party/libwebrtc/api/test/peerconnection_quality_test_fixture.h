/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef API_TEST_PEERCONNECTION_QUALITY_TEST_FIXTURE_H_
#define API_TEST_PEERCONNECTION_QUALITY_TEST_FIXTURE_H_

#include <stddef.h>
#include <stdint.h>

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/memory/memory.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "api/array_view.h"
#include "api/async_resolver_factory.h"
#include "api/audio/audio_mixer.h"
#include "api/call/call_factory_interface.h"
#include "api/fec_controller.h"
#include "api/function_view.h"
#include "api/media_stream_interface.h"
#include "api/peer_connection_interface.h"
#include "api/rtc_event_log/rtc_event_log_factory_interface.h"
#include "api/rtp_parameters.h"
#include "api/task_queue/task_queue_factory.h"
#include "api/test/audio_quality_analyzer_interface.h"
#include "api/test/frame_generator_interface.h"
#include "api/test/pclf/media_configuration.h"
#include "api/test/pclf/media_quality_test_params.h"
#include "api/test/peer_network_dependencies.h"
#include "api/test/simulated_network.h"
#include "api/test/stats_observer_interface.h"
#include "api/test/track_id_stream_info_map.h"
#include "api/test/video/video_frame_writer.h"
#include "api/test/video_quality_analyzer_interface.h"
#include "api/transport/network_control.h"
#include "api/units/time_delta.h"
#include "api/video_codecs/video_decoder_factory.h"
#include "api/video_codecs/video_encoder.h"
#include "api/video_codecs/video_encoder_factory.h"
#include "media/base/media_constants.h"
#include "modules/audio_processing/include/audio_processing.h"
#include "rtc_base/checks.h"
#include "rtc_base/network.h"
#include "rtc_base/rtc_certificate_generator.h"
#include "rtc_base/ssl_certificate.h"
#include "rtc_base/thread.h"

namespace webrtc {
namespace webrtc_pc_e2e {

// API is in development. Can be changed/removed without notice.
class PeerConnectionE2EQualityTestFixture {
 public:
  using CapturingDeviceIndex = ::webrtc::webrtc_pc_e2e::CapturingDeviceIndex;
  using ScrollingParams = ::webrtc::webrtc_pc_e2e::ScrollingParams;
  using ScreenShareConfig = ::webrtc::webrtc_pc_e2e::ScreenShareConfig;
  using VideoSimulcastConfig = ::webrtc::webrtc_pc_e2e::VideoSimulcastConfig;
  using EmulatedSFUConfig = ::webrtc::webrtc_pc_e2e::EmulatedSFUConfig;
  using VideoResolution = ::webrtc::webrtc_pc_e2e::VideoResolution;
  using VideoDumpOptions = ::webrtc::webrtc_pc_e2e::VideoDumpOptions;
  using VideoConfig = ::webrtc::webrtc_pc_e2e::VideoConfig;
  using AudioConfig = ::webrtc::webrtc_pc_e2e::AudioConfig;
  using VideoCodecConfig = ::webrtc::webrtc_pc_e2e::VideoCodecConfig;
  using VideoSubscription = ::webrtc::webrtc_pc_e2e::VideoSubscription;
  using EchoEmulationConfig = ::webrtc::webrtc_pc_e2e::EchoEmulationConfig;
  using RunParams = ::webrtc::webrtc_pc_e2e::RunParams;

  // This class is used to fully configure one peer inside the call.
  class PeerConfigurer {
   public:
    virtual ~PeerConfigurer() = default;

    // Sets peer name that will be used to report metrics related to this peer.
    // If not set, some default name will be assigned. All names have to be
    // unique.
    virtual PeerConfigurer* SetName(absl::string_view name) = 0;

    // The parameters of the following 9 methods will be passed to the
    // PeerConnectionFactoryInterface implementation that will be created for
    // this peer.
    virtual PeerConfigurer* SetTaskQueueFactory(
        std::unique_ptr<TaskQueueFactory> task_queue_factory) = 0;
    virtual PeerConfigurer* SetCallFactory(
        std::unique_ptr<CallFactoryInterface> call_factory) = 0;
    virtual PeerConfigurer* SetEventLogFactory(
        std::unique_ptr<RtcEventLogFactoryInterface> event_log_factory) = 0;
    virtual PeerConfigurer* SetFecControllerFactory(
        std::unique_ptr<FecControllerFactoryInterface>
            fec_controller_factory) = 0;
    virtual PeerConfigurer* SetNetworkControllerFactory(
        std::unique_ptr<NetworkControllerFactoryInterface>
            network_controller_factory) = 0;
    virtual PeerConfigurer* SetVideoEncoderFactory(
        std::unique_ptr<VideoEncoderFactory> video_encoder_factory) = 0;
    virtual PeerConfigurer* SetVideoDecoderFactory(
        std::unique_ptr<VideoDecoderFactory> video_decoder_factory) = 0;
    // Set a custom NetEqFactory to be used in the call.
    virtual PeerConfigurer* SetNetEqFactory(
        std::unique_ptr<NetEqFactory> neteq_factory) = 0;
    virtual PeerConfigurer* SetAudioProcessing(
        rtc::scoped_refptr<webrtc::AudioProcessing> audio_processing) = 0;
    virtual PeerConfigurer* SetAudioMixer(
        rtc::scoped_refptr<webrtc::AudioMixer> audio_mixer) = 0;

    // Forces the Peerconnection to use the network thread as the worker thread.
    // Ie, worker thread and the network thread is the same thread.
    virtual PeerConfigurer* SetUseNetworkThreadAsWorkerThread() = 0;

    // The parameters of the following 4 methods will be passed to the
    // PeerConnectionInterface implementation that will be created for this
    // peer.
    virtual PeerConfigurer* SetAsyncResolverFactory(
        std::unique_ptr<webrtc::AsyncResolverFactory>
            async_resolver_factory) = 0;
    virtual PeerConfigurer* SetRTCCertificateGenerator(
        std::unique_ptr<rtc::RTCCertificateGeneratorInterface>
            cert_generator) = 0;
    virtual PeerConfigurer* SetSSLCertificateVerifier(
        std::unique_ptr<rtc::SSLCertificateVerifier> tls_cert_verifier) = 0;
    virtual PeerConfigurer* SetIceTransportFactory(
        std::unique_ptr<IceTransportFactory> factory) = 0;
    // Flags to set on `cricket::PortAllocator`. These flags will be added
    // to the default ones that are presented on the port allocator.
    // For possible values check p2p/base/port_allocator.h.
    virtual PeerConfigurer* SetPortAllocatorExtraFlags(
        uint32_t extra_flags) = 0;

    // Add new video stream to the call that will be sent from this peer.
    // Default implementation of video frames generator will be used.
    virtual PeerConfigurer* AddVideoConfig(VideoConfig config) = 0;
    // Add new video stream to the call that will be sent from this peer with
    // provided own implementation of video frames generator.
    virtual PeerConfigurer* AddVideoConfig(
        VideoConfig config,
        std::unique_ptr<test::FrameGeneratorInterface> generator) = 0;
    // Add new video stream to the call that will be sent from this peer.
    // Capturing device with specified index will be used to get input video.
    virtual PeerConfigurer* AddVideoConfig(
        VideoConfig config,
        CapturingDeviceIndex capturing_device_index) = 0;
    // Sets video subscription for the peer. By default subscription will
    // include all streams with `VideoSubscription::kSameAsSendStream`
    // resolution. To override this behavior use this method.
    virtual PeerConfigurer* SetVideoSubscription(
        VideoSubscription subscription) = 0;
    // Set the list of video codecs used by the peer during the test. These
    // codecs will be negotiated in SDP during offer/answer exchange. The order
    // of these codecs during negotiation will be the same as in `video_codecs`.
    // Codecs have to be available in codecs list provided by peer connection to
    // be negotiated. If some of specified codecs won't be found, the test will
    // crash.
    virtual PeerConfigurer* SetVideoCodecs(
        std::vector<VideoCodecConfig> video_codecs) = 0;
    // Set the audio stream for the call from this peer. If this method won't
    // be invoked, this peer will send no audio.
    virtual PeerConfigurer* SetAudioConfig(AudioConfig config) = 0;

    // Set if ULP FEC should be used or not. False by default.
    virtual PeerConfigurer* SetUseUlpFEC(bool value) = 0;
    // Set if Flex FEC should be used or not. False by default.
    // Client also must enable `enable_flex_fec_support` in the `RunParams` to
    // be able to use this feature.
    virtual PeerConfigurer* SetUseFlexFEC(bool value) = 0;
    // Specifies how much video encoder target bitrate should be different than
    // target bitrate, provided by WebRTC stack. Must be greater than 0. Can be
    // used to emulate overshooting of video encoders. This multiplier will
    // be applied for all video encoder on both sides for all layers. Bitrate
    // estimated by WebRTC stack will be multiplied by this multiplier and then
    // provided into VideoEncoder::SetRates(...). 1.0 by default.
    virtual PeerConfigurer* SetVideoEncoderBitrateMultiplier(
        double multiplier) = 0;

    // If is set, an RTCEventLog will be saved in that location and it will be
    // available for further analysis.
    virtual PeerConfigurer* SetRtcEventLogPath(std::string path) = 0;
    // If is set, an AEC dump will be saved in that location and it will be
    // available for further analysis.
    virtual PeerConfigurer* SetAecDumpPath(std::string path) = 0;
    virtual PeerConfigurer* SetRTCConfiguration(
        PeerConnectionInterface::RTCConfiguration configuration) = 0;
    virtual PeerConfigurer* SetRTCOfferAnswerOptions(
        PeerConnectionInterface::RTCOfferAnswerOptions options) = 0;
    // Set bitrate parameters on PeerConnection. This constraints will be
    // applied to all summed RTP streams for this peer.
    virtual PeerConfigurer* SetBitrateSettings(
        BitrateSettings bitrate_settings) = 0;
  };

  // Represent an entity that will report quality metrics after test.
  class QualityMetricsReporter : public StatsObserverInterface {
   public:
    virtual ~QualityMetricsReporter() = default;

    // Invoked by framework after peer connection factory and peer connection
    // itself will be created but before offer/answer exchange will be started.
    // `test_case_name` is name of test case, that should be used to report all
    // metrics.
    // `reporter_helper` is a pointer to a class that will allow track_id to
    // stream_id matching. The caller is responsible for ensuring the
    // TrackIdStreamInfoMap will be valid from Start() to
    // StopAndReportResults().
    virtual void Start(absl::string_view test_case_name,
                       const TrackIdStreamInfoMap* reporter_helper) = 0;

    // Invoked by framework after call is ended and peer connection factory and
    // peer connection are destroyed.
    virtual void StopAndReportResults() = 0;
  };

  // Represents single participant in call and can be used to perform different
  // in-call actions. Might be extended in future.
  class PeerHandle {
   public:
    virtual ~PeerHandle() = default;
  };

  virtual ~PeerConnectionE2EQualityTestFixture() = default;

  // Add activity that will be executed on the best effort at least after
  // `target_time_since_start` after call will be set up (after offer/answer
  // exchange, ICE gathering will be done and ICE candidates will passed to
  // remote side). `func` param is amount of time spent from the call set up.
  virtual void ExecuteAt(TimeDelta target_time_since_start,
                         std::function<void(TimeDelta)> func) = 0;
  // Add activity that will be executed every `interval` with first execution
  // on the best effort at least after `initial_delay_since_start` after call
  // will be set up (after all participants will be connected). `func` param is
  // amount of time spent from the call set up.
  virtual void ExecuteEvery(TimeDelta initial_delay_since_start,
                            TimeDelta interval,
                            std::function<void(TimeDelta)> func) = 0;

  // Add stats reporter entity to observe the test.
  virtual void AddQualityMetricsReporter(
      std::unique_ptr<QualityMetricsReporter> quality_metrics_reporter) = 0;

  // Add a new peer to the call and return an object through which caller
  // can configure peer's behavior.
  // `network_dependencies` are used to provide networking for peer's peer
  // connection. Members must be non-null.
  // `configurer` function will be used to configure peer in the call.
  virtual PeerHandle* AddPeer(
      const PeerNetworkDependencies& network_dependencies,
      rtc::FunctionView<void(PeerConfigurer*)> configurer) = 0;

  // Runs the media quality test, which includes setting up the call with
  // configured participants, running it according to provided `run_params` and
  // terminating it properly at the end. During call duration media quality
  // metrics are gathered, which are then reported to stdout and (if configured)
  // to the json/protobuf output file through the WebRTC perf test results
  // reporting system.
  virtual void Run(RunParams run_params) = 0;

  // Returns real test duration - the time of test execution measured during
  // test. Client must call this method only after test is finished (after
  // Run(...) method returned). Test execution time is time from end of call
  // setup (offer/answer, ICE candidates exchange done and ICE connected) to
  // start of call tear down (PeerConnection closed).
  virtual TimeDelta GetRealTestDuration() const = 0;
};

}  // namespace webrtc_pc_e2e
}  // namespace webrtc

#endif  // API_TEST_PEERCONNECTION_QUALITY_TEST_FIXTURE_H_
