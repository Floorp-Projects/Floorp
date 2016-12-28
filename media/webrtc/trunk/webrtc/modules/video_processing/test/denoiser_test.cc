/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <string.h>

#include "webrtc/common_video/libyuv/include/webrtc_libyuv.h"
#include "webrtc/modules/video_processing/include/video_processing.h"
#include "webrtc/modules/video_processing/test/video_processing_unittest.h"
#include "webrtc/modules/video_processing/video_denoiser.h"

namespace webrtc {

TEST_F(VideoProcessingTest, CopyMem) {
  rtc::scoped_ptr<DenoiserFilter> df_c(DenoiserFilter::Create(false));
  rtc::scoped_ptr<DenoiserFilter> df_sse_neon(DenoiserFilter::Create(true));
  uint8_t src[16 * 16], dst[16 * 16];
  for (int i = 0; i < 16; ++i) {
    for (int j = 0; j < 16; ++j) {
      src[i * 16 + j] = i * 16 + j;
    }
  }

  memset(dst, 0, 8 * 8);
  df_c->CopyMem8x8(src, 8, dst, 8);
  EXPECT_EQ(0, memcmp(src, dst, 8 * 8));

  memset(dst, 0, 16 * 16);
  df_c->CopyMem16x16(src, 16, dst, 16);
  EXPECT_EQ(0, memcmp(src, dst, 16 * 16));

  memset(dst, 0, 8 * 8);
  df_sse_neon->CopyMem16x16(src, 8, dst, 8);
  EXPECT_EQ(0, memcmp(src, dst, 8 * 8));

  memset(dst, 0, 16 * 16);
  df_sse_neon->CopyMem16x16(src, 16, dst, 16);
  EXPECT_EQ(0, memcmp(src, dst, 16 * 16));
}

TEST_F(VideoProcessingTest, Variance) {
  rtc::scoped_ptr<DenoiserFilter> df_c(DenoiserFilter::Create(false));
  rtc::scoped_ptr<DenoiserFilter> df_sse_neon(DenoiserFilter::Create(true));
  uint8_t src[16 * 16], dst[16 * 16];
  uint32_t sum = 0, sse = 0, var;
  for (int i = 0; i < 16; ++i) {
    for (int j = 0; j < 16; ++j) {
      src[i * 16 + j] = i * 16 + j;
    }
  }
  // Compute the 16x8 variance of the 16x16 block.
  for (int i = 0; i < 8; ++i) {
    for (int j = 0; j < 16; ++j) {
      sum += (i * 32 + j);
      sse += (i * 32 + j) * (i * 32 + j);
    }
  }
  var = sse - ((sum * sum) >> 7);
  memset(dst, 0, 16 * 16);
  EXPECT_EQ(var, df_c->Variance16x8(src, 16, dst, 16, &sse));
  EXPECT_EQ(var, df_sse_neon->Variance16x8(src, 16, dst, 16, &sse));
}

TEST_F(VideoProcessingTest, MbDenoise) {
  rtc::scoped_ptr<DenoiserFilter> df_c(DenoiserFilter::Create(false));
  rtc::scoped_ptr<DenoiserFilter> df_sse_neon(DenoiserFilter::Create(true));
  uint8_t running_src[16 * 16], src[16 * 16], dst[16 * 16], dst_ref[16 * 16];

  // Test case: |diff| <= |3 + shift_inc1|
  for (int i = 0; i < 16; ++i) {
    for (int j = 0; j < 16; ++j) {
      running_src[i * 16 + j] = i * 11 + j;
      src[i * 16 + j] = i * 11 + j + 2;
      dst_ref[i * 16 + j] = running_src[i * 16 + j];
    }
  }
  memset(dst, 0, 16 * 16);
  df_c->MbDenoise(running_src, 16, dst, 16, src, 16, 0, 1);
  EXPECT_EQ(0, memcmp(dst, dst_ref, 16 * 16));

  // Test case: |diff| >= |4 + shift_inc1|
  for (int i = 0; i < 16; ++i) {
    for (int j = 0; j < 16; ++j) {
      running_src[i * 16 + j] = i * 11 + j;
      src[i * 16 + j] = i * 11 + j + 5;
      dst_ref[i * 16 + j] = src[i * 16 + j] - 2;
    }
  }
  memset(dst, 0, 16 * 16);
  df_c->MbDenoise(running_src, 16, dst, 16, src, 16, 0, 1);
  EXPECT_EQ(0, memcmp(dst, dst_ref, 16 * 16));
  memset(dst, 0, 16 * 16);
  df_sse_neon->MbDenoise(running_src, 16, dst, 16, src, 16, 0, 1);
  EXPECT_EQ(0, memcmp(dst, dst_ref, 16 * 16));

  // Test case: |diff| >= 8
  for (int i = 0; i < 16; ++i) {
    for (int j = 0; j < 16; ++j) {
      running_src[i * 16 + j] = i * 11 + j;
      src[i * 16 + j] = i * 11 + j + 8;
      dst_ref[i * 16 + j] = src[i * 16 + j] - 6;
    }
  }
  memset(dst, 0, 16 * 16);
  df_c->MbDenoise(running_src, 16, dst, 16, src, 16, 0, 1);
  EXPECT_EQ(0, memcmp(dst, dst_ref, 16 * 16));
  memset(dst, 0, 16 * 16);
  df_sse_neon->MbDenoise(running_src, 16, dst, 16, src, 16, 0, 1);
  EXPECT_EQ(0, memcmp(dst, dst_ref, 16 * 16));

  // Test case: |diff| > 15
  for (int i = 0; i < 16; ++i) {
    for (int j = 0; j < 16; ++j) {
      running_src[i * 16 + j] = i * 11 + j;
      src[i * 16 + j] = i * 11 + j + 16;
    }
  }
  memset(dst, 0, 16 * 16);
  DenoiserDecision decision =
      df_c->MbDenoise(running_src, 16, dst, 16, src, 16, 0, 1);
  EXPECT_EQ(COPY_BLOCK, decision);
  decision = df_sse_neon->MbDenoise(running_src, 16, dst, 16, src, 16, 0, 1);
  EXPECT_EQ(COPY_BLOCK, decision);
}

TEST_F(VideoProcessingTest, Denoiser) {
  // Create pure C denoiser.
  VideoDenoiser denoiser_c(false);
  // Create SSE or NEON denoiser.
  VideoDenoiser denoiser_sse_neon(true);
  VideoFrame denoised_frame_c;
  VideoFrame denoised_frame_sse_neon;

  rtc::scoped_ptr<uint8_t[]> video_buffer(new uint8_t[frame_length_]);
  while (fread(video_buffer.get(), 1, frame_length_, source_file_) ==
         frame_length_) {
    // Using ConvertToI420 to add stride to the image.
    EXPECT_EQ(0, ConvertToI420(kI420, video_buffer.get(), 0, 0, width_, height_,
                               0, kVideoRotation_0, &video_frame_));

    denoiser_c.DenoiseFrame(video_frame_, &denoised_frame_c);
    denoiser_sse_neon.DenoiseFrame(video_frame_, &denoised_frame_sse_neon);

    // Denoising results should be the same for C and SSE/NEON denoiser.
    ASSERT_EQ(true, denoised_frame_c.EqualsFrame(denoised_frame_sse_neon));
  }
  ASSERT_NE(0, feof(source_file_)) << "Error reading source file";
}

}  // namespace webrtc
