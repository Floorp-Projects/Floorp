/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCMediaStream+Private.h"

#import "RTCAudioTrack+Private.h"
#import "RTCMediaStreamTrack+Private.h"
#import "RTCPeerConnectionFactory+Private.h"
#import "RTCVideoTrack+Private.h"
#import "helpers/NSString+StdString.h"

@implementation RTC_OBJC_TYPE (RTCMediaStream) {
  RTC_OBJC_TYPE(RTCPeerConnectionFactory) * _factory;
  rtc::Thread *_signalingThread;
  NSMutableArray *_audioTracks /* accessed on _signalingThread */;
  NSMutableArray *_videoTracks /* accessed on _signalingThread */;
  rtc::scoped_refptr<webrtc::MediaStreamInterface> _nativeMediaStream;
}

- (instancetype)initWithFactory:(RTC_OBJC_TYPE(RTCPeerConnectionFactory) *)factory
                       streamId:(NSString *)streamId {
  NSParameterAssert(factory);
  NSParameterAssert(streamId.length);
  std::string nativeId = [NSString stdStringForString:streamId];
  rtc::scoped_refptr<webrtc::MediaStreamInterface> stream =
      factory.nativeFactory->CreateLocalMediaStream(nativeId);
  return [self initWithFactory:factory nativeMediaStream:stream];
}

- (NSArray<RTC_OBJC_TYPE(RTCAudioTrack) *> *)audioTracks {
  if (!_signalingThread->IsCurrent()) {
    return _signalingThread->Invoke<NSArray<RTC_OBJC_TYPE(RTCAudioTrack) *> *>(
        RTC_FROM_HERE, [self]() { return self.audioTracks; });
  }
  return [_audioTracks copy];
}

- (NSArray<RTC_OBJC_TYPE(RTCVideoTrack) *> *)videoTracks {
  if (!_signalingThread->IsCurrent()) {
    return _signalingThread->Invoke<NSArray<RTC_OBJC_TYPE(RTCVideoTrack) *> *>(
        RTC_FROM_HERE, [self]() { return self.videoTracks; });
  }
  return [_videoTracks copy];
}

- (NSString *)streamId {
  return [NSString stringForStdString:_nativeMediaStream->id()];
}

- (void)addAudioTrack:(RTC_OBJC_TYPE(RTCAudioTrack) *)audioTrack {
  if (!_signalingThread->IsCurrent()) {
    return _signalingThread->Invoke<void>(
        RTC_FROM_HERE, [audioTrack, self]() { return [self addAudioTrack:audioTrack]; });
  }
  if (_nativeMediaStream->AddTrack(audioTrack.nativeAudioTrack)) {
    [_audioTracks addObject:audioTrack];
  }
}

- (void)addVideoTrack:(RTC_OBJC_TYPE(RTCVideoTrack) *)videoTrack {
  if (!_signalingThread->IsCurrent()) {
    return _signalingThread->Invoke<void>(
        RTC_FROM_HERE, [videoTrack, self]() { return [self addVideoTrack:videoTrack]; });
  }
  if (_nativeMediaStream->AddTrack(videoTrack.nativeVideoTrack)) {
    [_videoTracks addObject:videoTrack];
  }
}

- (void)removeAudioTrack:(RTC_OBJC_TYPE(RTCAudioTrack) *)audioTrack {
  if (!_signalingThread->IsCurrent()) {
    return _signalingThread->Invoke<void>(
        RTC_FROM_HERE, [audioTrack, self]() { return [self removeAudioTrack:audioTrack]; });
  }
  NSUInteger index = [_audioTracks indexOfObjectIdenticalTo:audioTrack];
  if (index == NSNotFound) {
    RTC_LOG(LS_INFO) << "|removeAudioTrack| called on unexpected RTC_OBJC_TYPE(RTCAudioTrack)";
    return;
  }
  if (_nativeMediaStream->RemoveTrack(audioTrack.nativeAudioTrack)) {
    [_audioTracks removeObjectAtIndex:index];
  }
}

- (void)removeVideoTrack:(RTC_OBJC_TYPE(RTCVideoTrack) *)videoTrack {
  if (!_signalingThread->IsCurrent()) {
    return _signalingThread->Invoke<void>(
        RTC_FROM_HERE, [videoTrack, self]() { return [self removeVideoTrack:videoTrack]; });
  }
  NSUInteger index = [_videoTracks indexOfObjectIdenticalTo:videoTrack];
  if (index == NSNotFound) {
    RTC_LOG(LS_INFO) << "|removeVideoTrack| called on unexpected RTC_OBJC_TYPE(RTCVideoTrack)";
    return;
  }

  if (_nativeMediaStream->RemoveTrack(videoTrack.nativeVideoTrack)) {
    [_videoTracks removeObjectAtIndex:index];
  }
}

- (NSString *)description {
  return [NSString stringWithFormat:@"RTC_OBJC_TYPE(RTCMediaStream):\n%@\nA=%lu\nV=%lu",
                                    self.streamId,
                                    (unsigned long)self.audioTracks.count,
                                    (unsigned long)self.videoTracks.count];
}

#pragma mark - Private

- (rtc::scoped_refptr<webrtc::MediaStreamInterface>)nativeMediaStream {
  return _nativeMediaStream;
}

- (instancetype)initWithFactory:(RTC_OBJC_TYPE(RTCPeerConnectionFactory) *)factory
              nativeMediaStream:
                  (rtc::scoped_refptr<webrtc::MediaStreamInterface>)nativeMediaStream {
  NSParameterAssert(nativeMediaStream);
  if (self = [super init]) {
    _factory = factory;
    _signalingThread = factory.signalingThread;

    webrtc::AudioTrackVector audioTracks = nativeMediaStream->GetAudioTracks();
    webrtc::VideoTrackVector videoTracks = nativeMediaStream->GetVideoTracks();

    _audioTracks = [NSMutableArray arrayWithCapacity:audioTracks.size()];
    _videoTracks = [NSMutableArray arrayWithCapacity:videoTracks.size()];
    _nativeMediaStream = nativeMediaStream;

    for (auto &track : audioTracks) {
      RTCMediaStreamTrackType type = RTCMediaStreamTrackTypeAudio;
      RTC_OBJC_TYPE(RTCAudioTrack) *audioTrack =
          [[RTC_OBJC_TYPE(RTCAudioTrack) alloc] initWithFactory:_factory
                                                    nativeTrack:track
                                                           type:type];
      [_audioTracks addObject:audioTrack];
    }

    for (auto &track : videoTracks) {
      RTCMediaStreamTrackType type = RTCMediaStreamTrackTypeVideo;
      RTC_OBJC_TYPE(RTCVideoTrack) *videoTrack =
          [[RTC_OBJC_TYPE(RTCVideoTrack) alloc] initWithFactory:_factory
                                                    nativeTrack:track
                                                           type:type];
      [_videoTracks addObject:videoTrack];
    }
  }
  return self;
}

@end
