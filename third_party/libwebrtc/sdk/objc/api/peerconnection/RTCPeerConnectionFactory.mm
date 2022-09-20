/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>

#import "RTCPeerConnectionFactory+Native.h"
#import "RTCPeerConnectionFactory+Private.h"
#import "RTCPeerConnectionFactoryOptions+Private.h"

#import "RTCAudioSource+Private.h"
#import "RTCAudioTrack+Private.h"
#import "RTCMediaConstraints+Private.h"
#import "RTCMediaStream+Private.h"
#import "RTCPeerConnection+Private.h"
#import "RTCVideoSource+Private.h"
#import "RTCVideoTrack+Private.h"
#import "base/RTCLogging.h"
#import "base/RTCVideoDecoderFactory.h"
#import "base/RTCVideoEncoderFactory.h"
#import "helpers/NSString+RTCStdString.h"
#include "rtc_base/checks.h"
#include "sdk/objc/native/api/network_monitor_factory.h"
#include "sdk/objc/native/api/ssl_certificate_verifier.h"
#include "system_wrappers/include/field_trial.h"

#ifndef HAVE_NO_MEDIA
#import "components/video_codec/RTCVideoDecoderFactoryH264.h"
#import "components/video_codec/RTCVideoEncoderFactoryH264.h"
// The no-media version PeerConnectionFactory doesn't depend on these files, but the gn check tool
// is not smart enough to take the #ifdef into account.
#include "api/audio_codecs/builtin_audio_decoder_factory.h"     // nogncheck
#include "api/audio_codecs/builtin_audio_encoder_factory.h"     // nogncheck
#include "api/rtc_event_log/rtc_event_log_factory.h"
#include "api/task_queue/default_task_queue_factory.h"
#include "api/transport/field_trial_based_config.h"
#include "modules/audio_device/include/audio_device.h"          // nogncheck
#include "modules/audio_processing/include/audio_processing.h"  // nogncheck

#include "sdk/objc/native/api/video_decoder_factory.h"
#include "sdk/objc/native/api/video_encoder_factory.h"
#include "sdk/objc/native/src/objc_video_decoder_factory.h"
#include "sdk/objc/native/src/objc_video_encoder_factory.h"
#endif

#if defined(WEBRTC_IOS)
#import "sdk/objc/native/api/audio_device_module.h"
#endif

// Adding the nogncheck to disable the including header check.
// The no-media version PeerConnectionFactory doesn't depend on media related
// C++ target.
// TODO(zhihuang): Remove nogncheck once MediaEngineInterface is moved to C++
// API layer.
#include "media/engine/webrtc_media_engine.h"  // nogncheck

@implementation RTC_OBJC_TYPE (RTCPeerConnectionFactory) {
  std::unique_ptr<rtc::Thread> _networkThread;
  std::unique_ptr<rtc::Thread> _workerThread;
  std::unique_ptr<rtc::Thread> _signalingThread;
  BOOL _hasStartedAecDump;
}

@synthesize nativeFactory = _nativeFactory;

- (rtc::scoped_refptr<webrtc::AudioDeviceModule>)audioDeviceModule {
#if defined(WEBRTC_IOS)
  return webrtc::CreateAudioDeviceModule();
#else
  return nullptr;
#endif
}

- (instancetype)init {
#ifdef HAVE_NO_MEDIA
  return [self initWithNoMedia];
#else
  return [self
      initWithNativeAudioEncoderFactory:webrtc::CreateBuiltinAudioEncoderFactory()
              nativeAudioDecoderFactory:webrtc::CreateBuiltinAudioDecoderFactory()
              nativeVideoEncoderFactory:webrtc::ObjCToNativeVideoEncoderFactory([[RTC_OBJC_TYPE(
                                            RTCVideoEncoderFactoryH264) alloc] init])
              nativeVideoDecoderFactory:webrtc::ObjCToNativeVideoDecoderFactory([[RTC_OBJC_TYPE(
                                            RTCVideoDecoderFactoryH264) alloc] init])
                      audioDeviceModule:[self audioDeviceModule].get()
                  audioProcessingModule:nullptr];
#endif
}

- (instancetype)
    initWithEncoderFactory:(nullable id<RTC_OBJC_TYPE(RTCVideoEncoderFactory)>)encoderFactory
            decoderFactory:(nullable id<RTC_OBJC_TYPE(RTCVideoDecoderFactory)>)decoderFactory {
#ifdef HAVE_NO_MEDIA
  return [self initWithNoMedia];
#else
  std::unique_ptr<webrtc::VideoEncoderFactory> native_encoder_factory;
  std::unique_ptr<webrtc::VideoDecoderFactory> native_decoder_factory;
  if (encoderFactory) {
    native_encoder_factory = webrtc::ObjCToNativeVideoEncoderFactory(encoderFactory);
  }
  if (decoderFactory) {
    native_decoder_factory = webrtc::ObjCToNativeVideoDecoderFactory(decoderFactory);
  }
  return [self initWithNativeAudioEncoderFactory:webrtc::CreateBuiltinAudioEncoderFactory()
                       nativeAudioDecoderFactory:webrtc::CreateBuiltinAudioDecoderFactory()
                       nativeVideoEncoderFactory:std::move(native_encoder_factory)
                       nativeVideoDecoderFactory:std::move(native_decoder_factory)
                               audioDeviceModule:[self audioDeviceModule].get()
                           audioProcessingModule:nullptr];
#endif
}
- (instancetype)initNative {
  if (self = [super init]) {
    _networkThread = rtc::Thread::CreateWithSocketServer();
    _networkThread->SetName("network_thread", _networkThread.get());
    BOOL result = _networkThread->Start();
    RTC_DCHECK(result) << "Failed to start network thread.";

    _workerThread = rtc::Thread::Create();
    _workerThread->SetName("worker_thread", _workerThread.get());
    result = _workerThread->Start();
    RTC_DCHECK(result) << "Failed to start worker thread.";

    _signalingThread = rtc::Thread::Create();
    _signalingThread->SetName("signaling_thread", _signalingThread.get());
    result = _signalingThread->Start();
    RTC_DCHECK(result) << "Failed to start signaling thread.";
  }
  return self;
}

- (instancetype)initWithNoMedia {
  if (self = [self initNative]) {
    webrtc::PeerConnectionFactoryDependencies dependencies;
    dependencies.network_thread = _networkThread.get();
    dependencies.worker_thread = _workerThread.get();
    dependencies.signaling_thread = _signalingThread.get();
    if (webrtc::field_trial::IsEnabled("WebRTC-Network-UseNWPathMonitor")) {
      dependencies.network_monitor_factory = webrtc::CreateNetworkMonitorFactory();
    }
    _nativeFactory = webrtc::CreateModularPeerConnectionFactory(std::move(dependencies));
    NSAssert(_nativeFactory, @"Failed to initialize PeerConnectionFactory!");
  }
  return self;
}

- (instancetype)initWithNativeAudioEncoderFactory:
                    (rtc::scoped_refptr<webrtc::AudioEncoderFactory>)audioEncoderFactory
                        nativeAudioDecoderFactory:
                            (rtc::scoped_refptr<webrtc::AudioDecoderFactory>)audioDecoderFactory
                        nativeVideoEncoderFactory:
                            (std::unique_ptr<webrtc::VideoEncoderFactory>)videoEncoderFactory
                        nativeVideoDecoderFactory:
                            (std::unique_ptr<webrtc::VideoDecoderFactory>)videoDecoderFactory
                                audioDeviceModule:(webrtc::AudioDeviceModule *)audioDeviceModule
                            audioProcessingModule:
                                (rtc::scoped_refptr<webrtc::AudioProcessing>)audioProcessingModule {
  return [self initWithNativeAudioEncoderFactory:audioEncoderFactory
                       nativeAudioDecoderFactory:audioDecoderFactory
                       nativeVideoEncoderFactory:std::move(videoEncoderFactory)
                       nativeVideoDecoderFactory:std::move(videoDecoderFactory)
                               audioDeviceModule:audioDeviceModule
                           audioProcessingModule:audioProcessingModule
                        networkControllerFactory:nullptr];
}
- (instancetype)initWithNativeAudioEncoderFactory:
                    (rtc::scoped_refptr<webrtc::AudioEncoderFactory>)audioEncoderFactory
                        nativeAudioDecoderFactory:
                            (rtc::scoped_refptr<webrtc::AudioDecoderFactory>)audioDecoderFactory
                        nativeVideoEncoderFactory:
                            (std::unique_ptr<webrtc::VideoEncoderFactory>)videoEncoderFactory
                        nativeVideoDecoderFactory:
                            (std::unique_ptr<webrtc::VideoDecoderFactory>)videoDecoderFactory
                                audioDeviceModule:(webrtc::AudioDeviceModule *)audioDeviceModule
                            audioProcessingModule:
                                (rtc::scoped_refptr<webrtc::AudioProcessing>)audioProcessingModule
                         networkControllerFactory:
                             (std::unique_ptr<webrtc::NetworkControllerFactoryInterface>)
                                 networkControllerFactory {
  if (self = [self initNative]) {
    webrtc::PeerConnectionFactoryDependencies dependencies;
    dependencies.network_thread = _networkThread.get();
    dependencies.worker_thread = _workerThread.get();
    dependencies.signaling_thread = _signalingThread.get();
    if (webrtc::field_trial::IsEnabled("WebRTC-Network-UseNWPathMonitor")) {
      dependencies.network_monitor_factory = webrtc::CreateNetworkMonitorFactory();
    }
#ifndef HAVE_NO_MEDIA
    dependencies.task_queue_factory = webrtc::CreateDefaultTaskQueueFactory();
    dependencies.trials = std::make_unique<webrtc::FieldTrialBasedConfig>();
    cricket::MediaEngineDependencies media_deps;
    media_deps.adm = std::move(audioDeviceModule);
    media_deps.task_queue_factory = dependencies.task_queue_factory.get();
    media_deps.audio_encoder_factory = std::move(audioEncoderFactory);
    media_deps.audio_decoder_factory = std::move(audioDecoderFactory);
    media_deps.video_encoder_factory = std::move(videoEncoderFactory);
    media_deps.video_decoder_factory = std::move(videoDecoderFactory);
    if (audioProcessingModule) {
      media_deps.audio_processing = std::move(audioProcessingModule);
    } else {
      media_deps.audio_processing = webrtc::AudioProcessingBuilder().Create();
    }
    media_deps.trials = dependencies.trials.get();
    dependencies.media_engine = cricket::CreateMediaEngine(std::move(media_deps));
    dependencies.call_factory = webrtc::CreateCallFactory();
    dependencies.event_log_factory =
        std::make_unique<webrtc::RtcEventLogFactory>(dependencies.task_queue_factory.get());
    dependencies.network_controller_factory = std::move(networkControllerFactory);
#endif
    _nativeFactory = webrtc::CreateModularPeerConnectionFactory(std::move(dependencies));
    NSAssert(_nativeFactory, @"Failed to initialize PeerConnectionFactory!");
  }
  return self;
}

- (RTC_OBJC_TYPE(RTCAudioSource) *)audioSourceWithConstraints:
    (nullable RTC_OBJC_TYPE(RTCMediaConstraints) *)constraints {
  std::unique_ptr<webrtc::MediaConstraints> nativeConstraints;
  if (constraints) {
    nativeConstraints = constraints.nativeConstraints;
  }
  cricket::AudioOptions options;
  CopyConstraintsIntoAudioOptions(nativeConstraints.get(), &options);

  rtc::scoped_refptr<webrtc::AudioSourceInterface> source =
      _nativeFactory->CreateAudioSource(options);
  return [[RTC_OBJC_TYPE(RTCAudioSource) alloc] initWithFactory:self nativeAudioSource:source];
}

- (RTC_OBJC_TYPE(RTCAudioTrack) *)audioTrackWithTrackId:(NSString *)trackId {
  RTC_OBJC_TYPE(RTCAudioSource) *audioSource = [self audioSourceWithConstraints:nil];
  return [self audioTrackWithSource:audioSource trackId:trackId];
}

- (RTC_OBJC_TYPE(RTCAudioTrack) *)audioTrackWithSource:(RTC_OBJC_TYPE(RTCAudioSource) *)source
                                               trackId:(NSString *)trackId {
  return [[RTC_OBJC_TYPE(RTCAudioTrack) alloc] initWithFactory:self source:source trackId:trackId];
}

- (RTC_OBJC_TYPE(RTCVideoSource) *)videoSource {
  return [[RTC_OBJC_TYPE(RTCVideoSource) alloc] initWithFactory:self
                                                signalingThread:_signalingThread.get()
                                                   workerThread:_workerThread.get()];
}

- (RTC_OBJC_TYPE(RTCVideoSource) *)videoSourceForScreenCast:(BOOL)forScreenCast {
  return [[RTC_OBJC_TYPE(RTCVideoSource) alloc] initWithFactory:self
                                                signalingThread:_signalingThread.get()
                                                   workerThread:_workerThread.get()
                                                   isScreenCast:forScreenCast];
}

- (RTC_OBJC_TYPE(RTCVideoTrack) *)videoTrackWithSource:(RTC_OBJC_TYPE(RTCVideoSource) *)source
                                               trackId:(NSString *)trackId {
  return [[RTC_OBJC_TYPE(RTCVideoTrack) alloc] initWithFactory:self source:source trackId:trackId];
}

- (RTC_OBJC_TYPE(RTCMediaStream) *)mediaStreamWithStreamId:(NSString *)streamId {
  return [[RTC_OBJC_TYPE(RTCMediaStream) alloc] initWithFactory:self streamId:streamId];
}

- (nullable RTC_OBJC_TYPE(RTCPeerConnection) *)
    peerConnectionWithConfiguration:(RTC_OBJC_TYPE(RTCConfiguration) *)configuration
                        constraints:(RTC_OBJC_TYPE(RTCMediaConstraints) *)constraints
                           delegate:
                               (nullable id<RTC_OBJC_TYPE(RTCPeerConnectionDelegate)>)delegate {
  return [[RTC_OBJC_TYPE(RTCPeerConnection) alloc] initWithFactory:self
                                                     configuration:configuration
                                                       constraints:constraints
                                               certificateVerifier:nil
                                                          delegate:delegate];
}

- (nullable RTC_OBJC_TYPE(RTCPeerConnection) *)
    peerConnectionWithConfiguration:(RTC_OBJC_TYPE(RTCConfiguration) *)configuration
                        constraints:(RTC_OBJC_TYPE(RTCMediaConstraints) *)constraints
                certificateVerifier:
                    (id<RTC_OBJC_TYPE(RTCSSLCertificateVerifier)>)certificateVerifier
                           delegate:
                               (nullable id<RTC_OBJC_TYPE(RTCPeerConnectionDelegate)>)delegate {
  return [[RTC_OBJC_TYPE(RTCPeerConnection) alloc] initWithFactory:self
                                                     configuration:configuration
                                                       constraints:constraints
                                               certificateVerifier:certificateVerifier
                                                          delegate:delegate];
}

- (nullable RTC_OBJC_TYPE(RTCPeerConnection) *)
    peerConnectionWithDependencies:(RTC_OBJC_TYPE(RTCConfiguration) *)configuration
                       constraints:(RTC_OBJC_TYPE(RTCMediaConstraints) *)constraints
                      dependencies:(std::unique_ptr<webrtc::PeerConnectionDependencies>)dependencies
                          delegate:(id<RTC_OBJC_TYPE(RTCPeerConnectionDelegate)>)delegate {
  return [[RTC_OBJC_TYPE(RTCPeerConnection) alloc] initWithDependencies:self
                                                          configuration:configuration
                                                            constraints:constraints
                                                           dependencies:std::move(dependencies)
                                                               delegate:delegate];
}

- (void)setOptions:(nonnull RTC_OBJC_TYPE(RTCPeerConnectionFactoryOptions) *)options {
  RTC_DCHECK(options != nil);
  _nativeFactory->SetOptions(options.nativeOptions);
}

- (BOOL)startAecDumpWithFilePath:(NSString *)filePath
                  maxSizeInBytes:(int64_t)maxSizeInBytes {
  RTC_DCHECK(filePath.length);
  RTC_DCHECK_GT(maxSizeInBytes, 0);

  if (_hasStartedAecDump) {
    RTCLogError(@"Aec dump already started.");
    return NO;
  }
  FILE *f = fopen(filePath.UTF8String, "wb");
  if (!f) {
    RTCLogError(@"Error opening file: %@. Error: %s", filePath, strerror(errno));
    return NO;
  }
  _hasStartedAecDump = _nativeFactory->StartAecDump(f, maxSizeInBytes);
  return _hasStartedAecDump;
}

- (void)stopAecDump {
  _nativeFactory->StopAecDump();
  _hasStartedAecDump = NO;
}

@end
