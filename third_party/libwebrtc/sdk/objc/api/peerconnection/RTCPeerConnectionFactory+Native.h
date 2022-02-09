/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCPeerConnectionFactory.h"

#include "api/scoped_refptr.h"

namespace webrtc {

class AudioDeviceModule;
class AudioEncoderFactory;
class AudioDecoderFactory;
class NetworkControllerFactoryInterface;
class VideoEncoderFactory;
class VideoDecoderFactory;
class AudioProcessing;
struct PeerConnectionDependencies;

}  // namespace webrtc

NS_ASSUME_NONNULL_BEGIN

/**
 * This class extension exposes methods that work directly with injectable C++ components.
 */
@interface RTC_OBJC_TYPE (RTCPeerConnectionFactory)
()

    - (instancetype)initNative NS_DESIGNATED_INITIALIZER;

/* Initializer used when WebRTC is compiled with no media support */
- (instancetype)initWithNoMedia;

/* Initialize object with injectable native audio/video encoder/decoder factories */
- (instancetype)initWithNativeAudioEncoderFactory:
                    (rtc::scoped_refptr<webrtc::AudioEncoderFactory>)audioEncoderFactory
                        nativeAudioDecoderFactory:
                            (rtc::scoped_refptr<webrtc::AudioDecoderFactory>)audioDecoderFactory
                        nativeVideoEncoderFactory:
                            (std::unique_ptr<webrtc::VideoEncoderFactory>)videoEncoderFactory
                        nativeVideoDecoderFactory:
                            (std::unique_ptr<webrtc::VideoDecoderFactory>)videoDecoderFactory
                                audioDeviceModule:
                                    (nullable webrtc::AudioDeviceModule *)audioDeviceModule
                            audioProcessingModule:
                                (rtc::scoped_refptr<webrtc::AudioProcessing>)audioProcessingModule;

- (instancetype)
    initWithNativeAudioEncoderFactory:
        (rtc::scoped_refptr<webrtc::AudioEncoderFactory>)audioEncoderFactory
            nativeAudioDecoderFactory:
                (rtc::scoped_refptr<webrtc::AudioDecoderFactory>)audioDecoderFactory
            nativeVideoEncoderFactory:
                (std::unique_ptr<webrtc::VideoEncoderFactory>)videoEncoderFactory
            nativeVideoDecoderFactory:
                (std::unique_ptr<webrtc::VideoDecoderFactory>)videoDecoderFactory
                    audioDeviceModule:(nullable webrtc::AudioDeviceModule *)audioDeviceModule
                audioProcessingModule:
                    (rtc::scoped_refptr<webrtc::AudioProcessing>)audioProcessingModule
             networkControllerFactory:(std::unique_ptr<webrtc::NetworkControllerFactoryInterface>)
                                          networkControllerFactory;

- (instancetype)
    initWithEncoderFactory:(nullable id<RTC_OBJC_TYPE(RTCVideoEncoderFactory)>)encoderFactory
            decoderFactory:(nullable id<RTC_OBJC_TYPE(RTCVideoDecoderFactory)>)decoderFactory;

/** Initialize an RTCPeerConnection with a configuration, constraints, and
 *  dependencies.
 */
- (RTC_OBJC_TYPE(RTCPeerConnection) *)
    peerConnectionWithDependencies:(RTC_OBJC_TYPE(RTCConfiguration) *)configuration
                       constraints:(RTC_OBJC_TYPE(RTCMediaConstraints) *)constraints
                      dependencies:(std::unique_ptr<webrtc::PeerConnectionDependencies>)dependencies
                          delegate:(nullable id<RTC_OBJC_TYPE(RTCPeerConnectionDelegate)>)delegate;

@end

NS_ASSUME_NONNULL_END
