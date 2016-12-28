/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VIDEO_FRAME_H_
#define WEBRTC_VIDEO_FRAME_H_

#include "webrtc/base/scoped_ref_ptr.h"
#include "webrtc/common_types.h"
#include "webrtc/common_video/include/video_frame_buffer.h"
#include "webrtc/common_video/rotation.h"
#include "webrtc/typedefs.h"

namespace webrtc {

class VideoFrame {
 public:
  VideoFrame();
  VideoFrame(const rtc::scoped_refptr<webrtc::VideoFrameBuffer>& buffer,
             uint32_t timestamp,
             int64_t render_time_ms,
             VideoRotation rotation);

  // TODO(pbos): Make all create/copy functions void, they should not be able to
  // fail (which should be RTC_DCHECK/CHECKed instead).

  // CreateEmptyFrame: Sets frame dimensions and allocates buffers based
  // on set dimensions - height and plane stride.
  // If required size is bigger than the allocated one, new buffers of adequate
  // size will be allocated.
  // Return value: 0 on success, -1 on error.
  int CreateEmptyFrame(int width,
                       int height,
                       int stride_y,
                       int stride_u,
                       int stride_v);

  // CreateFrame: Sets the frame's members and buffers. If required size is
  // bigger than allocated one, new buffers of adequate size will be allocated.
  // Return value: 0 on success, -1 on error.
  int CreateFrame(const uint8_t* buffer_y,
                  const uint8_t* buffer_u,
                  const uint8_t* buffer_v,
                  int width,
                  int height,
                  int stride_y,
                  int stride_u,
                  int stride_v);

  // TODO(guoweis): remove the previous CreateFrame when chromium has this code.
  int CreateFrame(const uint8_t* buffer_y,
                  const uint8_t* buffer_u,
                  const uint8_t* buffer_v,
                  int width,
                  int height,
                  int stride_y,
                  int stride_u,
                  int stride_v,
                  VideoRotation rotation);

  // CreateFrame: Sets the frame's members and buffers. If required size is
  // bigger than allocated one, new buffers of adequate size will be allocated.
  // |buffer| must be a packed I420 buffer.
  // Return value: 0 on success, -1 on error.
  int CreateFrame(const uint8_t* buffer,
                  int width,
                  int height,
                  VideoRotation rotation);

  // Deep copy frame: If required size is bigger than allocated one, new
  // buffers of adequate size will be allocated.
  // Return value: 0 on success, -1 on error.
  int CopyFrame(const VideoFrame& videoFrame);

  // Creates a shallow copy of |videoFrame|, i.e, the this object will retain a
  // reference to the video buffer also retained by |videoFrame|.
  void ShallowCopy(const VideoFrame& videoFrame);

  // Release frame buffer and reset time stamps.
  void Reset();

  // Get pointer to buffer per plane.
  uint8_t* buffer(PlaneType type);
  // Overloading with const.
  const uint8_t* buffer(PlaneType type) const;

  // Get allocated size per plane.
  int allocated_size(PlaneType type) const;

  // Get allocated stride per plane.
  int stride(PlaneType type) const;

  // Get frame width.
  int width() const;

  // Get frame height.
  int height() const;

  // Set frame timestamp (90kHz).
  void set_timestamp(uint32_t timestamp) { timestamp_ = timestamp; }

  // Get frame timestamp (90kHz).
  uint32_t timestamp() const { return timestamp_; }

  // Set capture ntp time in miliseconds.
  void set_ntp_time_ms(int64_t ntp_time_ms) {
    ntp_time_ms_ = ntp_time_ms;
  }

  // Get capture ntp time in miliseconds.
  int64_t ntp_time_ms() const { return ntp_time_ms_; }

  // Naming convention for Coordination of Video Orientation. Please see
  // http://www.etsi.org/deliver/etsi_ts/126100_126199/126114/12.07.00_60/ts_126114v120700p.pdf
  //
  // "pending rotation" or "pending" = a frame that has a VideoRotation > 0.
  //
  // "not pending" = a frame that has a VideoRotation == 0.
  //
  // "apply rotation" = modify a frame from being "pending" to being "not
  //                    pending" rotation (a no-op for "unrotated").
  //
  VideoRotation rotation() const { return rotation_; }
  void set_rotation(VideoRotation rotation) {
    rotation_ = rotation;
  }

  // Set render time in miliseconds.
  void set_render_time_ms(int64_t render_time_ms) {
    render_time_ms_ = render_time_ms;
  }

  // Get render time in miliseconds.
  int64_t render_time_ms() const { return render_time_ms_; }

  // Return true if underlying plane buffers are of zero size, false if not.
  bool IsZeroSize() const;

  // Return the handle of the underlying video frame. This is used when the
  // frame is backed by a texture. The object should be destroyed when it is no
  // longer in use, so the underlying resource can be freed.
  void* native_handle() const;

  // Return the underlying buffer.
  rtc::scoped_refptr<webrtc::VideoFrameBuffer> video_frame_buffer() const;

  // Set the underlying buffer.
  void set_video_frame_buffer(
      const rtc::scoped_refptr<webrtc::VideoFrameBuffer>& buffer);

  // Convert native-handle frame to memory-backed I420 frame. Should not be
  // called on a non-native-handle frame.
  VideoFrame ConvertNativeToI420Frame() const;

  bool EqualsFrame(const VideoFrame& frame) const;

 private:
  // An opaque reference counted handle that stores the pixel data.
  rtc::scoped_refptr<webrtc::VideoFrameBuffer> video_frame_buffer_;
  uint32_t timestamp_;
  int64_t ntp_time_ms_;
  int64_t render_time_ms_;
  VideoRotation rotation_;
};


// TODO(pbos): Rename EncodedFrame and reformat this class' members.
class EncodedImage {
 public:
  EncodedImage() : EncodedImage(nullptr, 0, 0) {}
  EncodedImage(uint8_t* buffer, size_t length, size_t size)
      : _buffer(buffer), _length(length), _size(size) {}

  struct AdaptReason {
    AdaptReason()
        : quality_resolution_downscales(-1),
          bw_resolutions_disabled(-1) {}

    int quality_resolution_downscales;  // Number of times this frame is down
                                        // scaled in resolution due to quality.
                                        // Or -1 if information is not provided.
    int bw_resolutions_disabled;  // Number of resolutions that are not sent
                                  // due to bandwidth for this frame.
                                  // Or -1 if information is not provided.
  };
  uint32_t _encodedWidth = 0;
  uint32_t _encodedHeight = 0;
  uint32_t _timeStamp = 0;
  // NTP time of the capture time in local timebase in milliseconds.
  int64_t ntp_time_ms_ = 0;
  int64_t capture_time_ms_ = 0;
  FrameType _frameType = kVideoFrameDelta;
  uint8_t* _buffer;
  size_t _length;
  size_t _size;
  bool _completeFrame = false;
  AdaptReason adapt_reason_;
  int qp_ = -1;  // Quantizer value.
};

}  // namespace webrtc
#endif  // WEBRTC_VIDEO_FRAME_H_
