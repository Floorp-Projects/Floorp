/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VIDEO_SEND_STATISTICS_PROXY_H_
#define WEBRTC_VIDEO_SEND_STATISTICS_PROXY_H_

#include <string>

#include "webrtc/common_types.h"
#include "webrtc/video_engine/include/vie_codec.h"
#include "webrtc/video_engine/include/vie_capture.h"
#include "webrtc/video_send_stream.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"

namespace webrtc {

class CriticalSectionWrapper;

class SendStatisticsProxy : public RtcpStatisticsCallback,
                            public StreamDataCountersCallback,
                            public BitrateStatisticsObserver,
                            public FrameCountObserver,
                            public ViEEncoderObserver,
                            public ViECaptureObserver {
 public:
  class StatsProvider {
   protected:
    StatsProvider() {}
    virtual ~StatsProvider() {}

   public:
    virtual bool GetSendSideDelay(VideoSendStream::Stats* stats) = 0;
    virtual std::string GetCName() = 0;
  };

  SendStatisticsProxy(const VideoSendStream::Config& config,
                      StatsProvider* stats_provider);
  virtual ~SendStatisticsProxy();

  VideoSendStream::Stats GetStats() const;

 protected:
  // From RtcpStatisticsCallback.
  virtual void StatisticsUpdated(const RtcpStatistics& statistics,
                                 uint32_t ssrc) OVERRIDE;
  // From StreamDataCountersCallback.
  virtual void DataCountersUpdated(const StreamDataCounters& counters,
                                   uint32_t ssrc) OVERRIDE;

  // From BitrateStatisticsObserver.
  virtual void Notify(const BitrateStatistics& stats, uint32_t ssrc) OVERRIDE;

  // From FrameCountObserver.
  virtual void FrameCountUpdated(FrameType frame_type,
                                 uint32_t frame_count,
                                 const unsigned int ssrc) OVERRIDE;

  // From ViEEncoderObserver.
  virtual void OutgoingRate(const int video_channel,
                            const unsigned int framerate,
                            const unsigned int bitrate) OVERRIDE;

  virtual void SuspendChange(int video_channel, bool is_suspended) OVERRIDE {}

  // From ViECaptureObserver.
  virtual void BrightnessAlarm(const int capture_id,
                               const Brightness brightness) OVERRIDE {}

  virtual void CapturedFrameRate(const int capture_id,
                                 const unsigned char frame_rate) OVERRIDE;

  virtual void NoPictureAlarm(const int capture_id,
                              const CaptureAlarm alarm) OVERRIDE {}

 private:
  StreamStats* GetStatsEntry(uint32_t ssrc);

  const VideoSendStream::Config config_;
  scoped_ptr<CriticalSectionWrapper> lock_;
  VideoSendStream::Stats stats_;
  StatsProvider* stats_provider_;
};

}  // namespace webrtc
#endif  // WEBRTC_VIDEO_SEND_STATISTICS_PROXY_H_
