/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import <Foundation/Foundation.h>

#import "RTCMacros.h"
#import "RTCMediaStreamTrack.h"
#import "RTCRtpParameters.h"

NS_ASSUME_NONNULL_BEGIN

/** Represents the media type of the RtpReceiver. */
typedef NS_ENUM(NSInteger, RTCRtpMediaType) {
  RTCRtpMediaTypeAudio,
  RTCRtpMediaTypeVideo,
  RTCRtpMediaTypeData,
  RTCRtpMediaTypeUnsupported,
};

@class RTC_OBJC_TYPE(RTCRtpReceiver);

RTC_OBJC_EXPORT
@protocol RTC_OBJC_TYPE
(RTCRtpReceiverDelegate)<NSObject>

    /** Called when the first RTP packet is received.
     *
     *  Note: Currently if there are multiple RtpReceivers of the same media type,
     *  they will all call OnFirstPacketReceived at once.
     *
     *  For example, if we create three audio receivers, A/B/C, they will listen to
     *  the same signal from the underneath network layer. Whenever the first audio packet
     *  is received, the underneath signal will be fired. All the receivers A/B/C will be
     *  notified and the callback of the receiver's delegate will be called.
     *
     *  The process is the same for video receivers.
     */
    - (void)rtpReceiver
    : (RTC_OBJC_TYPE(RTCRtpReceiver) *)rtpReceiver didReceiveFirstPacketForMediaType
    : (RTCRtpMediaType)mediaType;

@end

RTC_OBJC_EXPORT
@protocol RTC_OBJC_TYPE
(RTCRtpReceiver)<NSObject>

    /** A unique identifier for this receiver. */
    @property(nonatomic, readonly) NSString *receiverId;

/** The currently active RTCRtpParameters, as defined in
 *  https://www.w3.org/TR/webrtc/#idl-def-RTCRtpParameters.
 *
 *  The WebRTC specification only defines RTCRtpParameters in terms of senders,
 *  but this API also applies them to receivers, similar to ORTC:
 *  http://ortc.org/wp-content/uploads/2016/03/ortc.html#rtcrtpparameters*.
 */
@property(nonatomic, readonly) RTC_OBJC_TYPE(RTCRtpParameters) * parameters;

/** The RTCMediaStreamTrack associated with the receiver.
 *  Note: reading this property returns a new instance of
 *  RTCMediaStreamTrack. Use isEqual: instead of == to compare
 *  RTCMediaStreamTrack instances.
 */
@property(nonatomic, readonly, nullable) RTC_OBJC_TYPE(RTCMediaStreamTrack) * track;

/** The delegate for this RtpReceiver. */
@property(nonatomic, weak) id<RTC_OBJC_TYPE(RTCRtpReceiverDelegate)> delegate;

@end

RTC_OBJC_EXPORT
@interface RTC_OBJC_TYPE (RTCRtpReceiver) : NSObject <RTC_OBJC_TYPE(RTCRtpReceiver)>

- (instancetype)init NS_UNAVAILABLE;

@end

NS_ASSUME_NONNULL_END
