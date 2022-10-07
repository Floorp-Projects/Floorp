/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCVideoTrack+Private.h"

#import "RTCMediaStreamTrack+Private.h"
#import "RTCPeerConnectionFactory+Private.h"
#import "RTCVideoSource+Private.h"
#import "api/RTCVideoRendererAdapter+Private.h"
#import "helpers/NSString+StdString.h"

@implementation RTC_OBJC_TYPE (RTCVideoTrack) {
  rtc::Thread *_workerThread;
  NSMutableArray *_adapters /* accessed on _workerThread */;
}

@synthesize source = _source;

- (instancetype)initWithFactory:(RTC_OBJC_TYPE(RTCPeerConnectionFactory) *)factory
                         source:(RTC_OBJC_TYPE(RTCVideoSource) *)source
                        trackId:(NSString *)trackId {
  NSParameterAssert(factory);
  NSParameterAssert(source);
  NSParameterAssert(trackId.length);
  std::string nativeId = [NSString stdStringForString:trackId];
  rtc::scoped_refptr<webrtc::VideoTrackInterface> track =
      factory.nativeFactory->CreateVideoTrack(nativeId, source.nativeVideoSource.get());
  if (self = [self initWithFactory:factory nativeTrack:track type:RTCMediaStreamTrackTypeVideo]) {
    _source = source;
  }
  return self;
}

- (instancetype)initWithFactory:(RTC_OBJC_TYPE(RTCPeerConnectionFactory) *)factory
                    nativeTrack:
                        (rtc::scoped_refptr<webrtc::MediaStreamTrackInterface>)nativeMediaTrack
                           type:(RTCMediaStreamTrackType)type {
  NSParameterAssert(factory);
  NSParameterAssert(nativeMediaTrack);
  NSParameterAssert(type == RTCMediaStreamTrackTypeVideo);
  if (self = [super initWithFactory:factory nativeTrack:nativeMediaTrack type:type]) {
    _adapters = [NSMutableArray array];
    _workerThread = factory.workerThread;
  }
  return self;
}

- (void)dealloc {
  for (RTCVideoRendererAdapter *adapter in _adapters) {
    self.nativeVideoTrack->RemoveSink(adapter.nativeVideoRenderer);
  }
}

- (RTC_OBJC_TYPE(RTCVideoSource) *)source {
  if (!_source) {
    rtc::scoped_refptr<webrtc::VideoTrackSourceInterface> source(
        self.nativeVideoTrack->GetSource());
    if (source) {
      _source = [[RTC_OBJC_TYPE(RTCVideoSource) alloc] initWithFactory:self.factory
                                                     nativeVideoSource:source];
    }
  }
  return _source;
}

- (void)addRenderer:(id<RTC_OBJC_TYPE(RTCVideoRenderer)>)renderer {
  if (!_workerThread->IsCurrent()) {
    _workerThread->Invoke<void>(RTC_FROM_HERE, [renderer, self] { [self addRenderer:renderer]; });
    return;
  }

  // Make sure we don't have this renderer yet.
  for (RTCVideoRendererAdapter *adapter in _adapters) {
    if (adapter.videoRenderer == renderer) {
      RTC_LOG(LS_INFO) << "|renderer| is already attached to this track";
      return;
    }
  }
  // Create a wrapper that provides a native pointer for us.
  RTCVideoRendererAdapter* adapter =
      [[RTCVideoRendererAdapter alloc] initWithNativeRenderer:renderer];
  [_adapters addObject:adapter];
  self.nativeVideoTrack->AddOrUpdateSink(adapter.nativeVideoRenderer,
                                         rtc::VideoSinkWants());
}

- (void)removeRenderer:(id<RTC_OBJC_TYPE(RTCVideoRenderer)>)renderer {
  if (!_workerThread->IsCurrent()) {
    _workerThread->Invoke<void>(RTC_FROM_HERE,
                                [renderer, self] { [self removeRenderer:renderer]; });
    return;
  }
  __block NSUInteger indexToRemove = NSNotFound;
  [_adapters enumerateObjectsUsingBlock:^(RTCVideoRendererAdapter *adapter,
                                          NSUInteger idx,
                                          BOOL *stop) {
    if (adapter.videoRenderer == renderer) {
      indexToRemove = idx;
      *stop = YES;
    }
  }];
  if (indexToRemove == NSNotFound) {
    RTC_LOG(LS_INFO) << "removeRenderer called with a renderer that has not been previously added";
    return;
  }
  RTCVideoRendererAdapter *adapterToRemove =
      [_adapters objectAtIndex:indexToRemove];
  self.nativeVideoTrack->RemoveSink(adapterToRemove.nativeVideoRenderer);
  [_adapters removeObjectAtIndex:indexToRemove];
}

#pragma mark - Private

- (rtc::scoped_refptr<webrtc::VideoTrackInterface>)nativeVideoTrack {
  return rtc::scoped_refptr<webrtc::VideoTrackInterface>(
      static_cast<webrtc::VideoTrackInterface *>(self.nativeTrack.get()));
}

@end
