/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/video_render/ios/video_render_ios_channel.h"

using namespace webrtc;

VideoRenderIosChannel::VideoRenderIosChannel(VideoRenderIosView* view)
    : view_(view),
      current_frame_(new I420VideoFrame()),
      buffer_is_updated_(false) {}

VideoRenderIosChannel::~VideoRenderIosChannel() { delete current_frame_; }

int32_t VideoRenderIosChannel::RenderFrame(const uint32_t stream_id,
                                           I420VideoFrame& video_frame) {
  video_frame.set_render_time_ms(0);

  current_frame_->CopyFrame(video_frame);
  buffer_is_updated_ = true;

  return 0;
}

bool VideoRenderIosChannel::RenderOffScreenBuffer() {
  if (![view_ renderFrame:current_frame_]) {
    return false;
  }

  buffer_is_updated_ = false;

  return true;
}

bool VideoRenderIosChannel::IsUpdated() { return buffer_is_updated_; }

int VideoRenderIosChannel::SetStreamSettings(const float z_order,
                                             const float left,
                                             const float top,
                                             const float right,
                                             const float bottom) {
  if (![view_ setCoordinatesForZOrder:z_order
                                 Left:left
                                  Top:bottom
                                Right:right
                               Bottom:top]) {

    return -1;
  }

  return 0;
}
