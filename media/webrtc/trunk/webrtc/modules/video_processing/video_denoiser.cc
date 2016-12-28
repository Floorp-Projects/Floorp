/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "webrtc/common_video/libyuv/include/scaler.h"
#include "webrtc/common_video/libyuv/include/webrtc_libyuv.h"
#include "webrtc/modules/video_processing/video_denoiser.h"

namespace webrtc {

VideoDenoiser::VideoDenoiser(bool runtime_cpu_detection)
    : width_(0),
      height_(0),
      filter_(DenoiserFilter::Create(runtime_cpu_detection)) {}

void VideoDenoiser::TrailingReduction(int mb_rows,
                                      int mb_cols,
                                      const uint8_t* y_src,
                                      int stride_y,
                                      uint8_t* y_dst) {
  for (int mb_row = 1; mb_row < mb_rows - 1; ++mb_row) {
    for (int mb_col = 1; mb_col < mb_cols - 1; ++mb_col) {
      int mb_index = mb_row * mb_cols + mb_col;
      uint8_t* mb_dst = y_dst + (mb_row << 4) * stride_y + (mb_col << 4);
      const uint8_t* mb_src = y_src + (mb_row << 4) * stride_y + (mb_col << 4);
      // If the number of denoised neighbors is less than a threshold,
      // do NOT denoise for the block. Set different threshold for skin MB.
      // The change of denoising status will not propagate.
      if (metrics_[mb_index].is_skin) {
        // The threshold is high (more strict) for non-skin MB where the
        // trailing usually happen.
        if (metrics_[mb_index].denoise &&
            metrics_[mb_index + 1].denoise + metrics_[mb_index - 1].denoise +
                    metrics_[mb_index + mb_cols].denoise +
                    metrics_[mb_index - mb_cols].denoise <=
                2) {
          metrics_[mb_index].denoise = 0;
          filter_->CopyMem16x16(mb_src, stride_y, mb_dst, stride_y);
        }
      } else if (metrics_[mb_index].denoise &&
                 metrics_[mb_index + 1].denoise +
                         metrics_[mb_index - 1].denoise +
                         metrics_[mb_index + mb_cols + 1].denoise +
                         metrics_[mb_index + mb_cols - 1].denoise +
                         metrics_[mb_index - mb_cols + 1].denoise +
                         metrics_[mb_index - mb_cols - 1].denoise +
                         metrics_[mb_index + mb_cols].denoise +
                         metrics_[mb_index - mb_cols].denoise <=
                     7) {
        filter_->CopyMem16x16(mb_src, stride_y, mb_dst, stride_y);
      }
    }
  }
}

void VideoDenoiser::DenoiseFrame(const VideoFrame& frame,
                                 VideoFrame* denoised_frame) {
  int stride_y = frame.stride(kYPlane);
  int stride_u = frame.stride(kUPlane);
  int stride_v = frame.stride(kVPlane);
  // If previous width and height are different from current frame's, then no
  // denoising for the current frame.
  if (width_ != frame.width() || height_ != frame.height()) {
    width_ = frame.width();
    height_ = frame.height();
    denoised_frame->CreateFrame(frame.buffer(kYPlane), frame.buffer(kUPlane),
                                frame.buffer(kVPlane), width_, height_,
                                stride_y, stride_u, stride_v);
    // Setting time parameters to the output frame.
    denoised_frame->set_timestamp(frame.timestamp());
    denoised_frame->set_render_time_ms(frame.render_time_ms());
    return;
  }
  // For 16x16 block.
  int mb_cols = width_ >> 4;
  int mb_rows = height_ >> 4;
  if (metrics_.get() == nullptr)
    metrics_.reset(new DenoiseMetrics[mb_cols * mb_rows]());
  // Denoise on Y plane.
  uint8_t* y_dst = denoised_frame->buffer(kYPlane);
  uint8_t* u_dst = denoised_frame->buffer(kUPlane);
  uint8_t* v_dst = denoised_frame->buffer(kVPlane);
  const uint8_t* y_src = frame.buffer(kYPlane);
  const uint8_t* u_src = frame.buffer(kUPlane);
  const uint8_t* v_src = frame.buffer(kVPlane);
  // Temporary buffer to store denoising result.
  uint8_t y_tmp[16 * 16] = {0};
  for (int mb_row = 0; mb_row < mb_rows; ++mb_row) {
    for (int mb_col = 0; mb_col < mb_cols; ++mb_col) {
      const uint8_t* mb_src = y_src + (mb_row << 4) * stride_y + (mb_col << 4);
      uint8_t* mb_dst = y_dst + (mb_row << 4) * stride_y + (mb_col << 4);
      int mb_index = mb_row * mb_cols + mb_col;
      // Denoise each MB at the very start and save the result to a temporary
      // buffer.
      if (filter_->MbDenoise(mb_dst, stride_y, y_tmp, 16, mb_src, stride_y, 0,
                             1) == FILTER_BLOCK) {
        uint32_t thr_var = 0;
        // Save var and sad to the buffer.
        metrics_[mb_index].var = filter_->Variance16x8(
            mb_dst, stride_y, y_tmp, 16, &metrics_[mb_index].sad);
        // Get skin map.
        metrics_[mb_index].is_skin = MbHasSkinColor(
            y_src, u_src, v_src, stride_y, stride_u, stride_v, mb_row, mb_col);
        // Variance threshold for skin/non-skin MB is different.
        // Skin MB use a small threshold to reduce blockiness.
        thr_var = metrics_[mb_index].is_skin ? 128 : 12 * 128;
        if (metrics_[mb_index].var > thr_var) {
          metrics_[mb_index].denoise = 0;
          // Use the source MB.
          filter_->CopyMem16x16(mb_src, stride_y, mb_dst, stride_y);
        } else {
          metrics_[mb_index].denoise = 1;
          // Use the denoised MB.
          filter_->CopyMem16x16(y_tmp, 16, mb_dst, stride_y);
        }
      } else {
        metrics_[mb_index].denoise = 0;
        filter_->CopyMem16x16(mb_src, stride_y, mb_dst, stride_y);
      }
      // Copy source U/V plane.
      const uint8_t* mb_src_u =
          u_src + (mb_row << 3) * stride_u + (mb_col << 3);
      const uint8_t* mb_src_v =
          v_src + (mb_row << 3) * stride_v + (mb_col << 3);
      uint8_t* mb_dst_u = u_dst + (mb_row << 3) * stride_u + (mb_col << 3);
      uint8_t* mb_dst_v = v_dst + (mb_row << 3) * stride_v + (mb_col << 3);
      filter_->CopyMem8x8(mb_src_u, stride_u, mb_dst_u, stride_u);
      filter_->CopyMem8x8(mb_src_v, stride_v, mb_dst_v, stride_v);
    }
  }
  // Second round.
  // This is to reduce the trailing artifact and blockiness by referring
  // neighbors' denoising status.
  TrailingReduction(mb_rows, mb_cols, y_src, stride_y, y_dst);

  // Setting time parameters to the output frame.
  denoised_frame->set_timestamp(frame.timestamp());
  denoised_frame->set_render_time_ms(frame.render_time_ms());
  return;
}

}  // namespace webrtc
