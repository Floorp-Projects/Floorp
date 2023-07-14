/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCPeerConnection.h"

#include "api/peer_connection_interface.h"

NS_ASSUME_NONNULL_BEGIN

namespace webrtc {

/**
 * These objects are created by RTCPeerConnectionFactory to wrap an
 * id<RTCPeerConnectionDelegate> and call methods on that interface.
 */
class PeerConnectionDelegateAdapter : public PeerConnectionObserver {
 public:
  PeerConnectionDelegateAdapter(RTC_OBJC_TYPE(RTCPeerConnection) * peerConnection);
  ~PeerConnectionDelegateAdapter() override;

  void OnSignalingChange(PeerConnectionInterface::SignalingState new_state) override;

  void OnAddStream(rtc::scoped_refptr<MediaStreamInterface> stream) override;

  void OnRemoveStream(rtc::scoped_refptr<MediaStreamInterface> stream) override;

  void OnTrack(rtc::scoped_refptr<RtpTransceiverInterface> transceiver) override;

  void OnDataChannel(rtc::scoped_refptr<DataChannelInterface> data_channel) override;

  void OnRenegotiationNeeded() override;

  void OnIceConnectionChange(PeerConnectionInterface::IceConnectionState new_state) override;

  void OnStandardizedIceConnectionChange(
      PeerConnectionInterface::IceConnectionState new_state) override;

  void OnConnectionChange(PeerConnectionInterface::PeerConnectionState new_state) override;

  void OnIceGatheringChange(PeerConnectionInterface::IceGatheringState new_state) override;

  void OnIceCandidate(const IceCandidateInterface *candidate) override;

  void OnIceCandidateError(const std::string &address,
                           int port,
                           const std::string &url,
                           int error_code,
                           const std::string &error_text) override;

  void OnIceCandidatesRemoved(const std::vector<cricket::Candidate> &candidates) override;

  void OnIceSelectedCandidatePairChanged(const cricket::CandidatePairChangeEvent &event) override;

  void OnAddTrack(rtc::scoped_refptr<RtpReceiverInterface> receiver,
                  const std::vector<rtc::scoped_refptr<MediaStreamInterface>> &streams) override;

  void OnRemoveTrack(rtc::scoped_refptr<RtpReceiverInterface> receiver) override;

 private:
  __weak RTC_OBJC_TYPE(RTCPeerConnection) * peer_connection_;
};

}  // namespace webrtc
@protocol RTC_OBJC_TYPE
(RTCSSLCertificateVerifier);

@interface RTC_OBJC_TYPE (RTCPeerConnection)
()

    /** The factory used to create this RTCPeerConnection */
    @property(nonatomic, readonly) RTC_OBJC_TYPE(RTCPeerConnectionFactory) *
    factory;

/** The native PeerConnectionInterface created during construction. */
@property(nonatomic, readonly) rtc::scoped_refptr<webrtc::PeerConnectionInterface>
    nativePeerConnection;

/** Initialize an RTCPeerConnection with a configuration, constraints, and
 *  delegate.
 */
- (nullable instancetype)
        initWithFactory:(RTC_OBJC_TYPE(RTCPeerConnectionFactory) *)factory
          configuration:(RTC_OBJC_TYPE(RTCConfiguration) *)configuration
            constraints:(RTC_OBJC_TYPE(RTCMediaConstraints) *)constraints
    certificateVerifier:(nullable id<RTC_OBJC_TYPE(RTCSSLCertificateVerifier)>)certificateVerifier
               delegate:(nullable id<RTC_OBJC_TYPE(RTCPeerConnectionDelegate)>)delegate;

/** Initialize an RTCPeerConnection with a configuration, constraints,
 *  delegate and PeerConnectionDependencies.
 */
- (nullable instancetype)
    initWithDependencies:(RTC_OBJC_TYPE(RTCPeerConnectionFactory) *)factory
           configuration:(RTC_OBJC_TYPE(RTCConfiguration) *)configuration
             constraints:(RTC_OBJC_TYPE(RTCMediaConstraints) *)constraints
            dependencies:(std::unique_ptr<webrtc::PeerConnectionDependencies>)dependencies
                delegate:(nullable id<RTC_OBJC_TYPE(RTCPeerConnectionDelegate)>)delegate
    NS_DESIGNATED_INITIALIZER;

+ (webrtc::PeerConnectionInterface::SignalingState)nativeSignalingStateForState:
    (RTCSignalingState)state;

+ (RTCSignalingState)signalingStateForNativeState:
    (webrtc::PeerConnectionInterface::SignalingState)nativeState;

+ (NSString *)stringForSignalingState:(RTCSignalingState)state;

+ (webrtc::PeerConnectionInterface::IceConnectionState)nativeIceConnectionStateForState:
    (RTCIceConnectionState)state;

+ (webrtc::PeerConnectionInterface::PeerConnectionState)nativeConnectionStateForState:
    (RTCPeerConnectionState)state;

+ (RTCIceConnectionState)iceConnectionStateForNativeState:
    (webrtc::PeerConnectionInterface::IceConnectionState)nativeState;

+ (RTCPeerConnectionState)connectionStateForNativeState:
    (webrtc::PeerConnectionInterface::PeerConnectionState)nativeState;

+ (NSString *)stringForIceConnectionState:(RTCIceConnectionState)state;

+ (NSString *)stringForConnectionState:(RTCPeerConnectionState)state;

+ (webrtc::PeerConnectionInterface::IceGatheringState)nativeIceGatheringStateForState:
    (RTCIceGatheringState)state;

+ (RTCIceGatheringState)iceGatheringStateForNativeState:
    (webrtc::PeerConnectionInterface::IceGatheringState)nativeState;

+ (NSString *)stringForIceGatheringState:(RTCIceGatheringState)state;

+ (webrtc::PeerConnectionInterface::StatsOutputLevel)nativeStatsOutputLevelForLevel:
    (RTCStatsOutputLevel)level;

@end

NS_ASSUME_NONNULL_END
