/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/codecs/test/video_codec_analyzer.h"

#include <memory>

#include "api/task_queue/default_task_queue_factory.h"
#include "api/test/video_codec_tester.h"
#include "api/video/i420_buffer.h"
#include "api/video/video_codec_constants.h"
#include "api/video/video_frame.h"
#include "rtc_base/checks.h"
#include "rtc_base/event.h"
#include "rtc_base/time_utils.h"
#include "third_party/libyuv/include/libyuv/compare.h"

namespace webrtc {
namespace test {

namespace {

struct Psnr {
  double y;
  double u;
  double v;
  double yuv;
};

Psnr CalcPsnr(const I420BufferInterface& ref_buffer,
              const I420BufferInterface& dec_buffer) {
  RTC_CHECK_EQ(ref_buffer.width(), dec_buffer.width());
  RTC_CHECK_EQ(ref_buffer.height(), dec_buffer.height());

  uint64_t sse_y = libyuv::ComputeSumSquareErrorPlane(
      dec_buffer.DataY(), dec_buffer.StrideY(), ref_buffer.DataY(),
      ref_buffer.StrideY(), dec_buffer.width(), dec_buffer.height());

  uint64_t sse_u = libyuv::ComputeSumSquareErrorPlane(
      dec_buffer.DataU(), dec_buffer.StrideU(), ref_buffer.DataU(),
      ref_buffer.StrideU(), dec_buffer.width() / 2, dec_buffer.height() / 2);

  uint64_t sse_v = libyuv::ComputeSumSquareErrorPlane(
      dec_buffer.DataV(), dec_buffer.StrideV(), ref_buffer.DataV(),
      ref_buffer.StrideV(), dec_buffer.width() / 2, dec_buffer.height() / 2);

  int num_y_samples = dec_buffer.width() * dec_buffer.height();
  Psnr psnr;
  psnr.y = libyuv::SumSquareErrorToPsnr(sse_y, num_y_samples);
  psnr.u = libyuv::SumSquareErrorToPsnr(sse_u, num_y_samples / 4);
  psnr.v = libyuv::SumSquareErrorToPsnr(sse_v, num_y_samples / 4);
  psnr.yuv = libyuv::SumSquareErrorToPsnr(sse_y + sse_u + sse_v,
                                          num_y_samples + num_y_samples / 2);
  return psnr;
}

}  // namespace

VideoCodecAnalyzer::VideoCodecAnalyzer(
    rtc::TaskQueue& task_queue,
    ReferenceVideoSource* reference_video_source)
    : task_queue_(task_queue), reference_video_source_(reference_video_source) {
  sequence_checker_.Detach();
}

void VideoCodecAnalyzer::StartEncode(const VideoFrame& input_frame) {
  int64_t encode_started_ns = rtc::TimeNanos();
  task_queue_.PostTask(
      [this, timestamp_rtp = input_frame.timestamp(), encode_started_ns]() {
        RTC_DCHECK_RUN_ON(&sequence_checker_);
        VideoCodecTestStats::FrameStatistics* fs =
            stats_.GetOrAddFrame(timestamp_rtp, /*spatial_idx=*/0);
        fs->encode_start_ns = encode_started_ns;
      });
}

void VideoCodecAnalyzer::FinishEncode(const EncodedImage& frame) {
  int64_t encode_finished_ns = rtc::TimeNanos();

  task_queue_.PostTask([this, timestamp_rtp = frame.Timestamp(),
                        spatial_idx = frame.SpatialIndex().value_or(0),
                        temporal_idx = frame.TemporalIndex().value_or(0),
                        frame_type = frame._frameType, qp = frame.qp_,
                        frame_size_bytes = frame.size(), encode_finished_ns]() {
    RTC_DCHECK_RUN_ON(&sequence_checker_);
    VideoCodecTestStats::FrameStatistics* fs =
        stats_.GetOrAddFrame(timestamp_rtp, spatial_idx);
    VideoCodecTestStats::FrameStatistics* fs_base =
        stats_.GetOrAddFrame(timestamp_rtp, 0);

    fs->encode_start_ns = fs_base->encode_start_ns;
    fs->spatial_idx = spatial_idx;
    fs->temporal_idx = temporal_idx;
    fs->frame_type = frame_type;
    fs->qp = qp;

    fs->encode_time_us = (encode_finished_ns - fs->encode_start_ns) /
                         rtc::kNumNanosecsPerMicrosec;
    fs->length_bytes = frame_size_bytes;

    fs->encoding_successful = true;
  });
}

void VideoCodecAnalyzer::StartDecode(const EncodedImage& frame) {
  int64_t decode_start_ns = rtc::TimeNanos();
  task_queue_.PostTask([this, timestamp_rtp = frame.Timestamp(),
                        spatial_idx = frame.SpatialIndex().value_or(0),
                        frame_size_bytes = frame.size(), decode_start_ns]() {
    RTC_DCHECK_RUN_ON(&sequence_checker_);
    VideoCodecTestStats::FrameStatistics* fs =
        stats_.GetOrAddFrame(timestamp_rtp, spatial_idx);
    if (fs->length_bytes == 0) {
      // In encode-decode test the frame size is set in EncodeFinished. In
      // decode-only test set it here.
      fs->length_bytes = frame_size_bytes;
    }
    fs->decode_start_ns = decode_start_ns;
  });
}

void VideoCodecAnalyzer::FinishDecode(const VideoFrame& frame,
                                      int spatial_idx) {
  int64_t decode_finished_ns = rtc::TimeNanos();
  task_queue_.PostTask([this, timestamp_rtp = frame.timestamp(), spatial_idx,
                        width = frame.width(), height = frame.height(),
                        decode_finished_ns]() {
    RTC_DCHECK_RUN_ON(&sequence_checker_);
    VideoCodecTestStats::FrameStatistics* fs =
        stats_.GetFrameWithTimestamp(timestamp_rtp, spatial_idx);
    fs->decode_time_us = (decode_finished_ns - fs->decode_start_ns) /
                         rtc::kNumNanosecsPerMicrosec;
    fs->decoded_width = width;
    fs->decoded_height = height;
    fs->decoding_successful = true;
  });

  if (reference_video_source_ != nullptr) {
    // Copy hardware-backed frame into main memory to release output buffers
    // which number may be limited in hardware decoders.
    rtc::scoped_refptr<I420BufferInterface> decoded_buffer =
        frame.video_frame_buffer()->ToI420();

    task_queue_.PostTask([this, decoded_buffer,
                          timestamp_rtp = frame.timestamp(), spatial_idx]() {
      RTC_DCHECK_RUN_ON(&sequence_checker_);
      VideoFrame ref_frame = reference_video_source_->GetFrame(
          timestamp_rtp, {.width = decoded_buffer->width(),
                          .height = decoded_buffer->height()});
      rtc::scoped_refptr<I420BufferInterface> ref_buffer =
          ref_frame.video_frame_buffer()->ToI420();

      Psnr psnr = CalcPsnr(*decoded_buffer, *ref_buffer);
      VideoCodecTestStats::FrameStatistics* fs =
          this->stats_.GetFrameWithTimestamp(timestamp_rtp, spatial_idx);
      fs->psnr_y = static_cast<float>(psnr.y);
      fs->psnr_u = static_cast<float>(psnr.u);
      fs->psnr_v = static_cast<float>(psnr.v);
      fs->psnr = static_cast<float>(psnr.yuv);

      fs->quality_analysis_successful = true;
    });
  }
}

std::unique_ptr<VideoCodecTestStats> VideoCodecAnalyzer::GetStats() {
  std::unique_ptr<VideoCodecTestStats> stats;
  rtc::Event ready;
  task_queue_.PostTask([this, &stats, &ready]() mutable {
    RTC_DCHECK_RUN_ON(&sequence_checker_);
    stats.reset(new VideoCodecTestStatsImpl(stats_));
    ready.Set();
  });
  ready.Wait(rtc::Event::kForever);
  return stats;
}

}  // namespace test
}  // namespace webrtc
