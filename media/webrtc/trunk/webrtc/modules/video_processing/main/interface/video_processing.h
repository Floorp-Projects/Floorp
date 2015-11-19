/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 * video_processing.h
 * This header file contains the API required for the video
 * processing module class.
 */


#ifndef WEBRTC_MODULES_INTERFACE_VIDEO_PROCESSING_H
#define WEBRTC_MODULES_INTERFACE_VIDEO_PROCESSING_H

#include "webrtc/common_video/interface/i420_video_frame.h"
#include "webrtc/modules/interface/module.h"
#include "webrtc/modules/interface/module_common_types.h"
#include "webrtc/modules/video_processing/main/interface/video_processing_defines.h"

/**
   The module is largely intended to process video streams, except functionality
   provided by static functions which operate independent of previous frames. It
   is recommended, but not required that a unique instance be used for each
   concurrently processed stream. Similarly, it is recommended to call Reset()
   before switching to a new stream, but this is not absolutely required.

   The module provides basic thread safety by permitting only a single function
   to execute concurrently.
*/

namespace webrtc {

class VideoProcessingModule : public Module {
 public:
  /**
     Structure to hold frame statistics. Populate it with GetFrameStats().
  */
  struct FrameStats {
      FrameStats() :
          mean(0),
          sum(0),
          num_pixels(0),
          subSamplWidth(0),
          subSamplHeight(0) {
    memset(hist, 0, sizeof(hist));
  }

  uint32_t hist[256];       // FRame histogram.
  uint32_t mean;            // Frame Mean value.
  uint32_t sum;             // Sum of frame.
  uint32_t num_pixels;       // Number of pixels.
  uint8_t  subSamplWidth;   // Subsampling rate of width in powers of 2.
  uint8_t  subSamplHeight;  // Subsampling rate of height in powers of 2.
};

  /**
     Specifies the warning types returned by BrightnessDetection().
  */
  enum BrightnessWarning {
    kNoWarning,     // Frame has acceptable brightness.
    kDarkWarning,   // Frame is too dark.
    kBrightWarning  // Frame is too bright.
  };

  /*
     Creates a VPM object.

     \param[in] id
         Unique identifier of this object.

     \return Pointer to a VPM object.
  */
  static VideoProcessingModule* Create(int32_t id);

  /**
     Destroys a VPM object.

     \param[in] module
         Pointer to the VPM object to destroy.
  */
  static void Destroy(VideoProcessingModule* module);

  /**
     Not supported.
  */
  int64_t TimeUntilNextProcess() override { return -1; }

  /**
     Not supported.
  */
  int32_t Process() override { return -1; }

  /**
     Resets all processing components to their initial states. This should be
     called whenever a new video stream is started.
  */
  virtual void Reset() = 0;

  /**
     Retrieves statistics for the input frame. This function must be used to
     prepare a FrameStats struct for use in certain VPM functions.

     \param[out] stats
         The frame statistics will be stored here on return.

     \param[in]  frame
         Reference to the video frame.

     \return 0 on success, -1 on failure.
  */
  static int32_t GetFrameStats(FrameStats* stats,
                               const I420VideoFrame& frame);

  /**
     Checks the validity of a FrameStats struct. Currently, valid implies only
     that is had changed from its initialized state.

     \param[in] stats
         Frame statistics.

     \return True on valid stats, false on invalid stats.
  */
  static bool ValidFrameStats(const FrameStats& stats);

  /**
     Returns a FrameStats struct to its intialized state.

     \param[in,out] stats
         Frame statistics.
  */
  static void ClearFrameStats(FrameStats* stats);

  /**
     Enhances the color of an image through a constant mapping. Only the
     chrominance is altered. Has a fixed-point implementation.

     \param[in,out] frame
         Pointer to the video frame.
  */
  static int32_t ColorEnhancement(I420VideoFrame* frame);

  /**
     Increases/decreases the luminance value.

     \param[in,out] frame
         Pointer to the video frame.

    \param[in] delta
         The amount to change the chrominance value of every single pixel.
         Can be < 0 also.

     \return 0 on success, -1 on failure.
  */
  static int32_t Brighten(I420VideoFrame* frame, int delta);

  /**
     Detects and removes camera flicker from a video stream. Every frame from
     the stream must be passed in. A frame will only be altered if flicker has
     been detected. Has a fixed-point implementation.

     \param[in,out] frame
         Pointer to the video frame.

     \param[in,out] stats
         Frame statistics provided by GetFrameStats(). On return the stats will
         be reset to zero if the frame was altered. Call GetFrameStats() again
         if the statistics for the altered frame are required.

     \return 0 on success, -1 on failure.
  */
  virtual int32_t Deflickering(I420VideoFrame* frame, FrameStats* stats) = 0;

  /**
     Detects if a video frame is excessively bright or dark. Returns a
     warning if this is the case. Multiple frames should be passed in before
     expecting a warning. Has a floating-point implementation.

     \param[in] frame
         Pointer to the video frame.

     \param[in] stats
         Frame statistics provided by GetFrameStats().

     \return A member of BrightnessWarning on success, -1 on error
  */
  virtual int32_t BrightnessDetection(const I420VideoFrame& frame,
                                      const FrameStats& stats) = 0;

  /**
  The following functions refer to the pre-processor unit within VPM. The
  pre-processor perfoms spatial/temporal decimation and content analysis on
  the frames prior to encoding.
  */

  /**
  Enable/disable temporal decimation

  \param[in] enable when true, temporal decimation is enabled
  */
  virtual void EnableTemporalDecimation(bool enable) = 0;

  /**
 Set target resolution

 \param[in] width
 Target width

 \param[in] height
 Target height

  \param[in] frame_rate
  Target frame_rate

  \return VPM_OK on success, a negative value on error (see error codes)

  */
  virtual int32_t SetTargetResolution(uint32_t width,
                                      uint32_t height,
                                      uint32_t frame_rate) = 0;

  /**
  Get decimated(target) frame rate
  */
  virtual uint32_t Decimatedframe_rate() = 0;

  /**
  Get decimated(target) frame width
  */
  virtual uint32_t DecimatedWidth() const = 0;

  /**
  Get decimated(target) frame height
  */
  virtual uint32_t DecimatedHeight() const = 0 ;

  /**
  Set the spatial resampling settings of the VPM: The resampler may either be
  disabled or one of the following:
  scaling to a close to target dimension followed by crop/pad

  \param[in] resampling_mode
  Set resampling mode (a member of VideoFrameResampling)
  */
  virtual void SetInputFrameResampleMode(VideoFrameResampling
                                         resampling_mode) = 0;

  /**
  Get Processed (decimated) frame

  \param[in] frame pointer to the video frame.
  \param[in] processed_frame pointer (double) to the processed frame. If no
             processing is required, processed_frame will be NULL.

  \return VPM_OK on success, a negative value on error (see error codes)
  */
  virtual int32_t PreprocessFrame(const I420VideoFrame& frame,
                                  I420VideoFrame** processed_frame) = 0;

  /**
  Return content metrics for the last processed frame
  */
  virtual VideoContentMetrics* ContentMetrics() const = 0 ;

  /**
  Enable content analysis
  */
  virtual void EnableContentAnalysis(bool enable) = 0;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_INTERFACE_VIDEO_PROCESSING_H
