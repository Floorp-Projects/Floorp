/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_PROCESSING_INCLUDE_VIDEO_PROCESSING_H_
#define WEBRTC_MODULES_VIDEO_PROCESSING_INCLUDE_VIDEO_PROCESSING_H_

#include "webrtc/modules/include/module_common_types.h"
#include "webrtc/modules/video_processing/include/video_processing_defines.h"
#include "webrtc/video_frame.h"

// The module is largely intended to process video streams, except functionality
// provided by static functions which operate independent of previous frames. It
// is recommended, but not required that a unique instance be used for each
// concurrently processed stream. Similarly, it is recommended to call Reset()
// before switching to a new stream, but this is not absolutely required.
//
// The module provides basic thread safety by permitting only a single function
// to execute concurrently.

namespace webrtc {

class VideoProcessing {
 public:
  struct FrameStats {
    uint32_t hist[256];  // Frame histogram.
    uint32_t mean;
    uint32_t sum;
    uint32_t num_pixels;
    uint32_t sub_sampling_factor;  // Sub-sampling factor, in powers of 2.
  };

  enum BrightnessWarning { kNoWarning, kDarkWarning, kBrightWarning };

  static VideoProcessing* Create();
  virtual ~VideoProcessing() {}

  // Retrieves statistics for the input frame. This function must be used to
  // prepare a FrameStats struct for use in certain VPM functions.
  static void GetFrameStats(const VideoFrame& frame, FrameStats* stats);

  // Checks the validity of a FrameStats struct. Currently, valid implies only
  // that is had changed from its initialized state.
  static bool ValidFrameStats(const FrameStats& stats);

  static void ClearFrameStats(FrameStats* stats);

  // Increases/decreases the luminance value. 'delta' can be in the range {}
  static void Brighten(int delta, VideoFrame* frame);

  // Detects and removes camera flicker from a video stream. Every frame from
  // the stream must be passed in. A frame will only be altered if flicker has
  // been detected. Has a fixed-point implementation.
  // Frame statistics provided by GetFrameStats(). On return the stats will
  // be reset to zero if the frame was altered. Call GetFrameStats() again
  // if the statistics for the altered frame are required.
  virtual int32_t Deflickering(VideoFrame* frame, FrameStats* stats) = 0;

  // Detects if a video frame is excessively bright or dark. Returns a
  // warning if this is the case. Multiple frames should be passed in before
  // expecting a warning. Has a floating-point implementation.
  virtual int32_t BrightnessDetection(const VideoFrame& frame,
                                      const FrameStats& stats) = 0;

  // The following functions refer to the pre-processor unit within VPM. The
  // pre-processor perfoms spatial/temporal decimation and content analysis on
  // the frames prior to encoding.

  // Enable/disable temporal decimation
  virtual void EnableTemporalDecimation(bool enable) = 0;

  virtual int32_t SetTargetResolution(uint32_t width,
                                      uint32_t height,
                                      uint32_t frame_rate) = 0;

  virtual void SetTargetFramerate(int frame_rate) = 0;

  virtual uint32_t GetDecimatedFrameRate() = 0;
  virtual uint32_t GetDecimatedWidth() const = 0;
  virtual uint32_t GetDecimatedHeight() const = 0;

  // Set the spatial resampling settings of the VPM according to
  // VideoFrameResampling.
  virtual void SetInputFrameResampleMode(
      VideoFrameResampling resampling_mode) = 0;

  virtual void EnableDenosing(bool enable) = 0;
  virtual const VideoFrame* PreprocessFrame(const VideoFrame& frame) = 0;

  virtual VideoContentMetrics* GetContentMetrics() const = 0;
  virtual void EnableContentAnalysis(bool enable) = 0;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_VIDEO_PROCESSING_INCLUDE_VIDEO_PROCESSING_H_
