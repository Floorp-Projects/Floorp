/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCPeerConnection+Private.h"

#import "RTCLegacyStatsReport+Private.h"
#import "RTCMediaStreamTrack+Private.h"
#import "RTCRtpReceiver+Private.h"
#import "RTCRtpSender+Private.h"
#import "RTCStatisticsReport+Private.h"
#import "helpers/NSString+StdString.h"

#include "rtc_base/checks.h"

namespace webrtc {

class StatsCollectorCallbackAdapter : public RTCStatsCollectorCallback {
 public:
  StatsCollectorCallbackAdapter(RTCStatisticsCompletionHandler completion_handler)
      : completion_handler_(completion_handler) {}

  void OnStatsDelivered(const rtc::scoped_refptr<const RTCStatsReport> &report) override {
    RTC_DCHECK(completion_handler_);
    RTC_OBJC_TYPE(RTCStatisticsReport) *statisticsReport =
        [[RTC_OBJC_TYPE(RTCStatisticsReport) alloc] initWithReport:*report];
    completion_handler_(statisticsReport);
    completion_handler_ = nil;
  }

 private:
  RTCStatisticsCompletionHandler completion_handler_;
};

class StatsObserverAdapter : public StatsObserver {
 public:
  StatsObserverAdapter(
      void (^completionHandler)(NSArray<RTC_OBJC_TYPE(RTCLegacyStatsReport) *> *stats)) {
    completion_handler_ = completionHandler;
  }

  ~StatsObserverAdapter() override { completion_handler_ = nil; }

  void OnComplete(const StatsReports& reports) override {
    RTC_DCHECK(completion_handler_);
    NSMutableArray *stats = [NSMutableArray arrayWithCapacity:reports.size()];
    for (const auto* report : reports) {
      RTC_OBJC_TYPE(RTCLegacyStatsReport) *statsReport =
          [[RTC_OBJC_TYPE(RTCLegacyStatsReport) alloc] initWithNativeReport:*report];
      [stats addObject:statsReport];
    }
    completion_handler_(stats);
    completion_handler_ = nil;
  }

 private:
  void (^completion_handler_)(NSArray<RTC_OBJC_TYPE(RTCLegacyStatsReport) *> *stats);
};
}  // namespace webrtc

@implementation RTC_OBJC_TYPE (RTCPeerConnection)
(Stats)

    - (void)statisticsForSender : (RTC_OBJC_TYPE(RTCRtpSender) *)sender completionHandler
    : (RTCStatisticsCompletionHandler)completionHandler {
  rtc::scoped_refptr<webrtc::StatsCollectorCallbackAdapter> collector =
      rtc::make_ref_counted<webrtc::StatsCollectorCallbackAdapter>(completionHandler);
  self.nativePeerConnection->GetStats(sender.nativeRtpSender, collector);
}

- (void)statisticsForReceiver:(RTC_OBJC_TYPE(RTCRtpReceiver) *)receiver
            completionHandler:(RTCStatisticsCompletionHandler)completionHandler {
  rtc::scoped_refptr<webrtc::StatsCollectorCallbackAdapter> collector =
      rtc::make_ref_counted<webrtc::StatsCollectorCallbackAdapter>(completionHandler);
  self.nativePeerConnection->GetStats(receiver.nativeRtpReceiver, collector);
}

- (void)statisticsWithCompletionHandler:(RTCStatisticsCompletionHandler)completionHandler {
  rtc::scoped_refptr<webrtc::StatsCollectorCallbackAdapter> collector =
      rtc::make_ref_counted<webrtc::StatsCollectorCallbackAdapter>(completionHandler);
  self.nativePeerConnection->GetStats(collector);
}

- (void)statsForTrack:(RTC_OBJC_TYPE(RTCMediaStreamTrack) *)mediaStreamTrack
     statsOutputLevel:(RTCStatsOutputLevel)statsOutputLevel
    completionHandler:
        (void (^)(NSArray<RTC_OBJC_TYPE(RTCLegacyStatsReport) *> *stats))completionHandler {
  rtc::scoped_refptr<webrtc::StatsObserverAdapter> observer =
      rtc::make_ref_counted<webrtc::StatsObserverAdapter>(completionHandler);
  webrtc::PeerConnectionInterface::StatsOutputLevel nativeOutputLevel =
      [[self class] nativeStatsOutputLevelForLevel:statsOutputLevel];
  self.nativePeerConnection->GetStats(
      observer, mediaStreamTrack.nativeTrack, nativeOutputLevel);
}

@end
