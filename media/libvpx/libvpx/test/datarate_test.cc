/*
 *  Copyright (c) 2012 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "./vpx_config.h"
#include "third_party/googletest/src/include/gtest/gtest.h"
#include "test/codec_factory.h"
#include "test/encode_test_driver.h"
#include "test/i420_video_source.h"
#include "test/util.h"
#include "test/y4m_video_source.h"
#include "vpx/vpx_codec.h"

namespace {

class DatarateTestLarge
    : public ::libvpx_test::EncoderTest,
      public ::libvpx_test::CodecTestWith2Params<libvpx_test::TestMode, int> {
 public:
  DatarateTestLarge() : EncoderTest(GET_PARAM(0)) {}

  virtual ~DatarateTestLarge() {}

 protected:
  virtual void SetUp() {
    InitializeConfig();
    SetMode(GET_PARAM(1));
    set_cpu_used_ = GET_PARAM(2);
    ResetModel();
  }

  virtual void ResetModel() {
    last_pts_ = 0;
    bits_in_buffer_model_ = cfg_.rc_target_bitrate * cfg_.rc_buf_initial_sz;
    frame_number_ = 0;
    first_drop_ = 0;
    bits_total_ = 0;
    duration_ = 0.0;
    denoiser_offon_test_ = 0;
    denoiser_offon_period_ = -1;
    gf_boost_ = 0;
  }

  virtual void PreEncodeFrameHook(::libvpx_test::VideoSource *video,
                                  ::libvpx_test::Encoder *encoder) {
    if (video->frame() == 0) {
      encoder->Control(VP8E_SET_NOISE_SENSITIVITY, denoiser_on_);
      encoder->Control(VP8E_SET_CPUUSED, set_cpu_used_);
      encoder->Control(VP8E_SET_GF_CBR_BOOST_PCT, gf_boost_);
    }

    if (denoiser_offon_test_) {
      ASSERT_GT(denoiser_offon_period_, 0)
          << "denoiser_offon_period_ is not positive.";
      if ((video->frame() + 1) % denoiser_offon_period_ == 0) {
        // Flip denoiser_on_ periodically
        denoiser_on_ ^= 1;
      }
      encoder->Control(VP8E_SET_NOISE_SENSITIVITY, denoiser_on_);
    }

    const vpx_rational_t tb = video->timebase();
    timebase_ = static_cast<double>(tb.num) / tb.den;
    duration_ = 0;
  }

  virtual void FramePktHook(const vpx_codec_cx_pkt_t *pkt) {
    // Time since last timestamp = duration.
    vpx_codec_pts_t duration = pkt->data.frame.pts - last_pts_;

    // TODO(jimbankoski): Remove these lines when the issue:
    // http://code.google.com/p/webm/issues/detail?id=496 is fixed.
    // For now the codec assumes buffer starts at starting buffer rate
    // plus one frame's time.
    if (last_pts_ == 0) duration = 1;

    // Add to the buffer the bits we'd expect from a constant bitrate server.
    bits_in_buffer_model_ += static_cast<int64_t>(
        duration * timebase_ * cfg_.rc_target_bitrate * 1000);

    /* Test the buffer model here before subtracting the frame. Do so because
     * the way the leaky bucket model works in libvpx is to allow the buffer to
     * empty - and then stop showing frames until we've got enough bits to
     * show one. As noted in comment below (issue 495), this does not currently
     * apply to key frames. For now exclude key frames in condition below. */
    const bool key_frame =
        (pkt->data.frame.flags & VPX_FRAME_IS_KEY) ? true : false;
    if (!key_frame) {
      ASSERT_GE(bits_in_buffer_model_, 0) << "Buffer Underrun at frame "
                                          << pkt->data.frame.pts;
    }

    const int64_t frame_size_in_bits = pkt->data.frame.sz * 8;

    // Subtract from the buffer the bits associated with a played back frame.
    bits_in_buffer_model_ -= frame_size_in_bits;

    // Update the running total of bits for end of test datarate checks.
    bits_total_ += frame_size_in_bits;

    // If first drop not set and we have a drop set it to this time.
    if (!first_drop_ && duration > 1) first_drop_ = last_pts_ + 1;

    // Update the most recent pts.
    last_pts_ = pkt->data.frame.pts;

    // We update this so that we can calculate the datarate minus the last
    // frame encoded in the file.
    bits_in_last_frame_ = frame_size_in_bits;

    ++frame_number_;
  }

  virtual void EndPassHook(void) {
    if (bits_total_) {
      const double file_size_in_kb = bits_total_ / 1000.;  // bits per kilobit

      duration_ = (last_pts_ + 1) * timebase_;

      // Effective file datarate includes the time spent prebuffering.
      effective_datarate_ = (bits_total_ - bits_in_last_frame_) / 1000.0 /
                            (cfg_.rc_buf_initial_sz / 1000.0 + duration_);

      file_datarate_ = file_size_in_kb / duration_;
    }
  }

  vpx_codec_pts_t last_pts_;
  int64_t bits_in_buffer_model_;
  double timebase_;
  int frame_number_;
  vpx_codec_pts_t first_drop_;
  int64_t bits_total_;
  double duration_;
  double file_datarate_;
  double effective_datarate_;
  int64_t bits_in_last_frame_;
  int denoiser_on_;
  int denoiser_offon_test_;
  int denoiser_offon_period_;
  int set_cpu_used_;
  int gf_boost_;
};

#if CONFIG_TEMPORAL_DENOISING
// Check basic datarate targeting, for a single bitrate, but loop over the
// various denoiser settings.
TEST_P(DatarateTestLarge, DenoiserLevels) {
  cfg_.rc_buf_initial_sz = 500;
  cfg_.rc_dropframe_thresh = 1;
  cfg_.rc_max_quantizer = 56;
  cfg_.rc_end_usage = VPX_CBR;
  ::libvpx_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352, 288,
                                       30, 1, 0, 140);
  for (int j = 1; j < 5; ++j) {
    // Run over the denoiser levels.
    // For the temporal denoiser (#if CONFIG_TEMPORAL_DENOISING) the level j
    // refers to the 4 denoiser modes: denoiserYonly, denoiserOnYUV,
    // denoiserOnAggressive, and denoiserOnAdaptive.
    denoiser_on_ = j;
    cfg_.rc_target_bitrate = 300;
    ResetModel();
    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
    ASSERT_GE(cfg_.rc_target_bitrate, effective_datarate_ * 0.95)
        << " The datarate for the file exceeds the target!";

    ASSERT_LE(cfg_.rc_target_bitrate, file_datarate_ * 1.4)
        << " The datarate for the file missed the target!";
  }
}

// Check basic datarate targeting, for a single bitrate, when denoiser is off
// and on.
TEST_P(DatarateTestLarge, DenoiserOffOn) {
  cfg_.rc_buf_initial_sz = 500;
  cfg_.rc_dropframe_thresh = 1;
  cfg_.rc_max_quantizer = 56;
  cfg_.rc_end_usage = VPX_CBR;
  ::libvpx_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352, 288,
                                       30, 1, 0, 299);
  cfg_.rc_target_bitrate = 300;
  ResetModel();
  // The denoiser is off by default.
  denoiser_on_ = 0;
  // Set the offon test flag.
  denoiser_offon_test_ = 1;
  denoiser_offon_period_ = 100;
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
  ASSERT_GE(cfg_.rc_target_bitrate, effective_datarate_ * 0.95)
      << " The datarate for the file exceeds the target!";
  ASSERT_LE(cfg_.rc_target_bitrate, file_datarate_ * 1.4)
      << " The datarate for the file missed the target!";
}
#endif  // CONFIG_TEMPORAL_DENOISING

TEST_P(DatarateTestLarge, BasicBufferModel) {
  denoiser_on_ = 0;
  cfg_.rc_buf_initial_sz = 500;
  cfg_.rc_dropframe_thresh = 1;
  cfg_.rc_max_quantizer = 56;
  cfg_.rc_end_usage = VPX_CBR;
  // 2 pass cbr datarate control has a bug hidden by the small # of
  // frames selected in this encode. The problem is that even if the buffer is
  // negative we produce a keyframe on a cutscene. Ignoring datarate
  // constraints
  // TODO(jimbankoski): ( Fix when issue
  // http://code.google.com/p/webm/issues/detail?id=495 is addressed. )
  ::libvpx_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352, 288,
                                       30, 1, 0, 140);

  // There is an issue for low bitrates in real-time mode, where the
  // effective_datarate slightly overshoots the target bitrate.
  // This is same the issue as noted about (#495).
  // TODO(jimbankoski/marpan): Update test to run for lower bitrates (< 100),
  // when the issue is resolved.
  for (int i = 100; i < 800; i += 200) {
    cfg_.rc_target_bitrate = i;
    ResetModel();
    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
    ASSERT_GE(cfg_.rc_target_bitrate, effective_datarate_ * 0.95)
        << " The datarate for the file exceeds the target!";
    ASSERT_LE(cfg_.rc_target_bitrate, file_datarate_ * 1.4)
        << " The datarate for the file missed the target!";
  }
}

TEST_P(DatarateTestLarge, ChangingDropFrameThresh) {
  denoiser_on_ = 0;
  cfg_.rc_buf_initial_sz = 500;
  cfg_.rc_max_quantizer = 36;
  cfg_.rc_end_usage = VPX_CBR;
  cfg_.rc_target_bitrate = 200;
  cfg_.kf_mode = VPX_KF_DISABLED;

  const int frame_count = 40;
  ::libvpx_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352, 288,
                                       30, 1, 0, frame_count);

  // Here we check that the first dropped frame gets earlier and earlier
  // as the drop frame threshold is increased.

  const int kDropFrameThreshTestStep = 30;
  vpx_codec_pts_t last_drop = frame_count;
  for (int i = 1; i < 91; i += kDropFrameThreshTestStep) {
    cfg_.rc_dropframe_thresh = i;
    ResetModel();
    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
    ASSERT_LE(first_drop_, last_drop)
        << " The first dropped frame for drop_thresh " << i
        << " > first dropped frame for drop_thresh "
        << i - kDropFrameThreshTestStep;
    last_drop = first_drop_;
  }
}

// Disabled for tsan, see:
// https://bugs.chromium.org/p/webm/issues/detail?id=1049
#if defined(__has_feature)
#if __has_feature(thread_sanitizer)
#define BUILDING_WITH_TSAN
#endif
#endif
#ifndef BUILDING_WITH_TSAN
TEST_P(DatarateTestLarge, DropFramesMultiThreads) {
  denoiser_on_ = 0;
  cfg_.rc_buf_initial_sz = 500;
  cfg_.rc_dropframe_thresh = 30;
  cfg_.rc_max_quantizer = 56;
  cfg_.rc_end_usage = VPX_CBR;
  cfg_.g_threads = 2;

  ::libvpx_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352, 288,
                                       30, 1, 0, 140);
  cfg_.rc_target_bitrate = 200;
  ResetModel();
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
  ASSERT_GE(cfg_.rc_target_bitrate, effective_datarate_ * 0.95)
      << " The datarate for the file exceeds the target!";

  ASSERT_LE(cfg_.rc_target_bitrate, file_datarate_ * 1.4)
      << " The datarate for the file missed the target!";
}
#endif  // !BUILDING_WITH_TSAN

class DatarateTestRealTime : public DatarateTestLarge {
 public:
  virtual ~DatarateTestRealTime() {}
};

#if CONFIG_TEMPORAL_DENOISING
// Check basic datarate targeting, for a single bitrate, but loop over the
// various denoiser settings.
TEST_P(DatarateTestRealTime, DenoiserLevels) {
  cfg_.rc_buf_initial_sz = 500;
  cfg_.rc_dropframe_thresh = 1;
  cfg_.rc_max_quantizer = 56;
  cfg_.rc_end_usage = VPX_CBR;
  ::libvpx_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352, 288,
                                       30, 1, 0, 140);
  for (int j = 1; j < 5; ++j) {
    // Run over the denoiser levels.
    // For the temporal denoiser (#if CONFIG_TEMPORAL_DENOISING) the level j
    // refers to the 4 denoiser modes: denoiserYonly, denoiserOnYUV,
    // denoiserOnAggressive, and denoiserOnAdaptive.
    denoiser_on_ = j;
    cfg_.rc_target_bitrate = 300;
    ResetModel();
    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
    ASSERT_GE(cfg_.rc_target_bitrate, effective_datarate_ * 0.95)
        << " The datarate for the file exceeds the target!";
    ASSERT_LE(cfg_.rc_target_bitrate, file_datarate_ * 1.4)
        << " The datarate for the file missed the target!";
  }
}

// Check basic datarate targeting, for a single bitrate, when denoiser is off
// and on.
TEST_P(DatarateTestRealTime, DenoiserOffOn) {
  cfg_.rc_buf_initial_sz = 500;
  cfg_.rc_dropframe_thresh = 1;
  cfg_.rc_max_quantizer = 56;
  cfg_.rc_end_usage = VPX_CBR;
  ::libvpx_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352, 288,
                                       30, 1, 0, 299);
  cfg_.rc_target_bitrate = 300;
  ResetModel();
  // The denoiser is off by default.
  denoiser_on_ = 0;
  // Set the offon test flag.
  denoiser_offon_test_ = 1;
  denoiser_offon_period_ = 100;
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
  ASSERT_GE(cfg_.rc_target_bitrate, effective_datarate_ * 0.95)
      << " The datarate for the file exceeds the target!";
  ASSERT_LE(cfg_.rc_target_bitrate, file_datarate_ * 1.4)
      << " The datarate for the file missed the target!";
}
#endif  // CONFIG_TEMPORAL_DENOISING

TEST_P(DatarateTestRealTime, BasicBufferModel) {
  denoiser_on_ = 0;
  cfg_.rc_buf_initial_sz = 500;
  cfg_.rc_dropframe_thresh = 1;
  cfg_.rc_max_quantizer = 56;
  cfg_.rc_end_usage = VPX_CBR;
  // 2 pass cbr datarate control has a bug hidden by the small # of
  // frames selected in this encode. The problem is that even if the buffer is
  // negative we produce a keyframe on a cutscene, ignoring datarate
  // constraints
  // TODO(jimbankoski): Fix when issue
  // http://bugs.chromium.org/p/webm/issues/detail?id=495 is addressed.
  ::libvpx_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352, 288,
                                       30, 1, 0, 140);

  // There is an issue for low bitrates in real-time mode, where the
  // effective_datarate slightly overshoots the target bitrate.
  // This is same the issue as noted above (#495).
  // TODO(jimbankoski/marpan): Update test to run for lower bitrates (< 100),
  // when the issue is resolved.
  for (int i = 100; i <= 700; i += 200) {
    cfg_.rc_target_bitrate = i;
    ResetModel();
    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
    ASSERT_GE(cfg_.rc_target_bitrate, effective_datarate_ * 0.95)
        << " The datarate for the file exceeds the target!";
    ASSERT_LE(cfg_.rc_target_bitrate, file_datarate_ * 1.4)
        << " The datarate for the file missed the target!";
  }
}

TEST_P(DatarateTestRealTime, ChangingDropFrameThresh) {
  denoiser_on_ = 0;
  cfg_.rc_buf_initial_sz = 500;
  cfg_.rc_max_quantizer = 36;
  cfg_.rc_end_usage = VPX_CBR;
  cfg_.rc_target_bitrate = 200;
  cfg_.kf_mode = VPX_KF_DISABLED;

  const int frame_count = 40;
  ::libvpx_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352, 288,
                                       30, 1, 0, frame_count);

  // Check that the first dropped frame gets earlier and earlier
  // as the drop frame threshold is increased.

  const int kDropFrameThreshTestStep = 30;
  vpx_codec_pts_t last_drop = frame_count;
  for (int i = 1; i < 91; i += kDropFrameThreshTestStep) {
    cfg_.rc_dropframe_thresh = i;
    ResetModel();
    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
    ASSERT_LE(first_drop_, last_drop)
        << " The first dropped frame for drop_thresh " << i
        << " > first dropped frame for drop_thresh "
        << i - kDropFrameThreshTestStep;
    last_drop = first_drop_;
  }
}

// Disabled for tsan, see:
// https://bugs.chromium.org/p/webm/issues/detail?id=1049

#ifndef BUILDING_WITH_TSAN
TEST_P(DatarateTestRealTime, DropFramesMultiThreads) {
  denoiser_on_ = 0;
  cfg_.rc_buf_initial_sz = 500;
  cfg_.rc_dropframe_thresh = 30;
  cfg_.rc_max_quantizer = 56;
  cfg_.rc_end_usage = VPX_CBR;
  // Encode using multiple threads.
  cfg_.g_threads = 2;

  ::libvpx_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352, 288,
                                       30, 1, 0, 140);
  cfg_.rc_target_bitrate = 200;
  ResetModel();
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
  ASSERT_GE(cfg_.rc_target_bitrate, effective_datarate_ * 0.95)
      << " The datarate for the file exceeds the target!";

  ASSERT_LE(cfg_.rc_target_bitrate, file_datarate_ * 1.4)
      << " The datarate for the file missed the target!";
}
#endif

TEST_P(DatarateTestRealTime, GFBoost) {
  denoiser_on_ = 0;
  cfg_.rc_buf_initial_sz = 500;
  cfg_.rc_dropframe_thresh = 0;
  cfg_.rc_max_quantizer = 56;
  cfg_.rc_end_usage = VPX_CBR;
  cfg_.g_error_resilient = 0;

  ::libvpx_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352, 288,
                                       30, 1, 0, 300);
  cfg_.rc_target_bitrate = 300;
  ResetModel();
  // Apply a gf boost.
  gf_boost_ = 50;

  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
  ASSERT_GE(cfg_.rc_target_bitrate, effective_datarate_ * 0.95)
      << " The datarate for the file exceeds the target!";

  ASSERT_LE(cfg_.rc_target_bitrate, file_datarate_ * 1.4)
      << " The datarate for the file missed the target!";
}

class DatarateTestVP9Large
    : public ::libvpx_test::EncoderTest,
      public ::libvpx_test::CodecTestWith2Params<libvpx_test::TestMode, int> {
 public:
  DatarateTestVP9Large() : EncoderTest(GET_PARAM(0)) {}

 protected:
  virtual ~DatarateTestVP9Large() {}

  virtual void SetUp() {
    InitializeConfig();
    SetMode(GET_PARAM(1));
    set_cpu_used_ = GET_PARAM(2);
    ResetModel();
  }

  virtual void ResetModel() {
    last_pts_ = 0;
    bits_in_buffer_model_ = cfg_.rc_target_bitrate * cfg_.rc_buf_initial_sz;
    frame_number_ = 0;
    tot_frame_number_ = 0;
    first_drop_ = 0;
    num_drops_ = 0;
    // Denoiser is off by default.
    denoiser_on_ = 0;
    // For testing up to 3 layers.
    for (int i = 0; i < 3; ++i) {
      bits_total_[i] = 0;
    }
    denoiser_offon_test_ = 0;
    denoiser_offon_period_ = -1;
  }

  //
  // Frame flags and layer id for temporal layers.
  //

  // For two layers, test pattern is:
  //   1     3
  // 0    2     .....
  // For three layers, test pattern is:
  //   1      3    5      7
  //      2           6
  // 0          4            ....
  // LAST is always update on base/layer 0, GOLDEN is updated on layer 1.
  // For this 3 layer example, the 2nd enhancement layer (layer 2) does not
  // update any reference frames.
  int SetFrameFlags(int frame_num, int num_temp_layers) {
    int frame_flags = 0;
    if (num_temp_layers == 2) {
      if (frame_num % 2 == 0) {
        // Layer 0: predict from L and ARF, update L.
        frame_flags =
            VP8_EFLAG_NO_REF_GF | VP8_EFLAG_NO_UPD_GF | VP8_EFLAG_NO_UPD_ARF;
      } else {
        // Layer 1: predict from L, G and ARF, and update G.
        frame_flags = VP8_EFLAG_NO_UPD_ARF | VP8_EFLAG_NO_UPD_LAST |
                      VP8_EFLAG_NO_UPD_ENTROPY;
      }
    } else if (num_temp_layers == 3) {
      if (frame_num % 4 == 0) {
        // Layer 0: predict from L and ARF; update L.
        frame_flags =
            VP8_EFLAG_NO_UPD_GF | VP8_EFLAG_NO_UPD_ARF | VP8_EFLAG_NO_REF_GF;
      } else if ((frame_num - 2) % 4 == 0) {
        // Layer 1: predict from L, G, ARF; update G.
        frame_flags = VP8_EFLAG_NO_UPD_ARF | VP8_EFLAG_NO_UPD_LAST;
      } else if ((frame_num - 1) % 2 == 0) {
        // Layer 2: predict from L, G, ARF; update none.
        frame_flags =
            VP8_EFLAG_NO_UPD_GF | VP8_EFLAG_NO_UPD_ARF | VP8_EFLAG_NO_UPD_LAST;
      }
    }
    return frame_flags;
  }

  int SetLayerId(int frame_num, int num_temp_layers) {
    int layer_id = 0;
    if (num_temp_layers == 2) {
      if (frame_num % 2 == 0) {
        layer_id = 0;
      } else {
        layer_id = 1;
      }
    } else if (num_temp_layers == 3) {
      if (frame_num % 4 == 0) {
        layer_id = 0;
      } else if ((frame_num - 2) % 4 == 0) {
        layer_id = 1;
      } else if ((frame_num - 1) % 2 == 0) {
        layer_id = 2;
      }
    }
    return layer_id;
  }

  virtual void PreEncodeFrameHook(::libvpx_test::VideoSource *video,
                                  ::libvpx_test::Encoder *encoder) {
    if (video->frame() == 0) encoder->Control(VP8E_SET_CPUUSED, set_cpu_used_);

    if (denoiser_offon_test_) {
      ASSERT_GT(denoiser_offon_period_, 0)
          << "denoiser_offon_period_ is not positive.";
      if ((video->frame() + 1) % denoiser_offon_period_ == 0) {
        // Flip denoiser_on_ periodically
        denoiser_on_ ^= 1;
      }
    }

    encoder->Control(VP9E_SET_NOISE_SENSITIVITY, denoiser_on_);

    if (cfg_.ts_number_layers > 1) {
      if (video->frame() == 0) {
        encoder->Control(VP9E_SET_SVC, 1);
      }
      vpx_svc_layer_id_t layer_id;
      layer_id.spatial_layer_id = 0;
      frame_flags_ = SetFrameFlags(video->frame(), cfg_.ts_number_layers);
      layer_id.temporal_layer_id =
          SetLayerId(video->frame(), cfg_.ts_number_layers);
      encoder->Control(VP9E_SET_SVC_LAYER_ID, &layer_id);
    }
    const vpx_rational_t tb = video->timebase();
    timebase_ = static_cast<double>(tb.num) / tb.den;
    duration_ = 0;
  }

  virtual void FramePktHook(const vpx_codec_cx_pkt_t *pkt) {
    // Time since last timestamp = duration.
    vpx_codec_pts_t duration = pkt->data.frame.pts - last_pts_;

    if (duration > 1) {
      // If first drop not set and we have a drop set it to this time.
      if (!first_drop_) first_drop_ = last_pts_ + 1;
      // Update the number of frame drops.
      num_drops_ += static_cast<int>(duration - 1);
      // Update counter for total number of frames (#frames input to encoder).
      // Needed for setting the proper layer_id below.
      tot_frame_number_ += static_cast<int>(duration - 1);
    }

    int layer = SetLayerId(tot_frame_number_, cfg_.ts_number_layers);

    // Add to the buffer the bits we'd expect from a constant bitrate server.
    bits_in_buffer_model_ += static_cast<int64_t>(
        duration * timebase_ * cfg_.rc_target_bitrate * 1000);

    // Buffer should not go negative.
    ASSERT_GE(bits_in_buffer_model_, 0) << "Buffer Underrun at frame "
                                        << pkt->data.frame.pts;

    const size_t frame_size_in_bits = pkt->data.frame.sz * 8;

    // Update the total encoded bits. For temporal layers, update the cumulative
    // encoded bits per layer.
    for (int i = layer; i < static_cast<int>(cfg_.ts_number_layers); ++i) {
      bits_total_[i] += frame_size_in_bits;
    }

    // Update the most recent pts.
    last_pts_ = pkt->data.frame.pts;
    ++frame_number_;
    ++tot_frame_number_;
  }

  virtual void EndPassHook(void) {
    for (int layer = 0; layer < static_cast<int>(cfg_.ts_number_layers);
         ++layer) {
      duration_ = (last_pts_ + 1) * timebase_;
      if (bits_total_[layer]) {
        // Effective file datarate:
        effective_datarate_[layer] = (bits_total_[layer] / 1000.0) / duration_;
      }
    }
  }

  vpx_codec_pts_t last_pts_;
  double timebase_;
  int frame_number_;      // Counter for number of non-dropped/encoded frames.
  int tot_frame_number_;  // Counter for total number of input frames.
  int64_t bits_total_[3];
  double duration_;
  double effective_datarate_[3];
  int set_cpu_used_;
  int64_t bits_in_buffer_model_;
  vpx_codec_pts_t first_drop_;
  int num_drops_;
  int denoiser_on_;
  int denoiser_offon_test_;
  int denoiser_offon_period_;
};

// Check basic rate targeting for VBR mode with 0 lag.
TEST_P(DatarateTestVP9Large, BasicRateTargetingVBRLagZero) {
  cfg_.rc_min_quantizer = 0;
  cfg_.rc_max_quantizer = 63;
  cfg_.g_error_resilient = 0;
  cfg_.rc_end_usage = VPX_VBR;
  cfg_.g_lag_in_frames = 0;

  ::libvpx_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352, 288,
                                       30, 1, 0, 300);
  for (int i = 400; i <= 800; i += 400) {
    cfg_.rc_target_bitrate = i;
    ResetModel();
    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
    ASSERT_GE(effective_datarate_[0], cfg_.rc_target_bitrate * 0.75)
        << " The datarate for the file is lower than target by too much!";
    ASSERT_LE(effective_datarate_[0], cfg_.rc_target_bitrate * 1.25)
        << " The datarate for the file is greater than target by too much!";
  }
}

// Check basic rate targeting for VBR mode with non-zero lag.
TEST_P(DatarateTestVP9Large, BasicRateTargetingVBRLagNonZero) {
  cfg_.rc_min_quantizer = 0;
  cfg_.rc_max_quantizer = 63;
  cfg_.g_error_resilient = 0;
  cfg_.rc_end_usage = VPX_VBR;
  // For non-zero lag, rate control will work (be within bounds) for
  // real-time mode.
  if (deadline_ == VPX_DL_REALTIME) {
    cfg_.g_lag_in_frames = 15;
  } else {
    cfg_.g_lag_in_frames = 0;
  }

  ::libvpx_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352, 288,
                                       30, 1, 0, 300);
  for (int i = 400; i <= 800; i += 400) {
    cfg_.rc_target_bitrate = i;
    ResetModel();
    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
    ASSERT_GE(effective_datarate_[0], cfg_.rc_target_bitrate * 0.75)
        << " The datarate for the file is lower than target by too much!";
    ASSERT_LE(effective_datarate_[0], cfg_.rc_target_bitrate * 1.25)
        << " The datarate for the file is greater than target by too much!";
  }
}

// Check basic rate targeting for CBR mode.
TEST_P(DatarateTestVP9Large, BasicRateTargeting) {
  cfg_.rc_buf_initial_sz = 500;
  cfg_.rc_buf_optimal_sz = 500;
  cfg_.rc_buf_sz = 1000;
  cfg_.rc_dropframe_thresh = 1;
  cfg_.rc_min_quantizer = 0;
  cfg_.rc_max_quantizer = 63;
  cfg_.rc_end_usage = VPX_CBR;
  cfg_.g_lag_in_frames = 0;

  ::libvpx_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352, 288,
                                       30, 1, 0, 140);
  for (int i = 150; i < 800; i += 200) {
    cfg_.rc_target_bitrate = i;
    ResetModel();
    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
    ASSERT_GE(effective_datarate_[0], cfg_.rc_target_bitrate * 0.85)
        << " The datarate for the file is lower than target by too much!";
    ASSERT_LE(effective_datarate_[0], cfg_.rc_target_bitrate * 1.15)
        << " The datarate for the file is greater than target by too much!";
  }
}

// Check basic rate targeting for CBR mode, with 2 threads and dropped frames.
TEST_P(DatarateTestVP9Large, BasicRateTargetingDropFramesMultiThreads) {
  cfg_.rc_buf_initial_sz = 500;
  cfg_.rc_buf_optimal_sz = 500;
  cfg_.rc_buf_sz = 1000;
  cfg_.rc_dropframe_thresh = 30;
  cfg_.rc_min_quantizer = 0;
  cfg_.rc_max_quantizer = 63;
  cfg_.rc_end_usage = VPX_CBR;
  cfg_.g_lag_in_frames = 0;
  // Encode using multiple threads.
  cfg_.g_threads = 2;

  ::libvpx_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352, 288,
                                       30, 1, 0, 140);
  cfg_.rc_target_bitrate = 200;
  ResetModel();
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
  ASSERT_GE(effective_datarate_[0], cfg_.rc_target_bitrate * 0.85)
      << " The datarate for the file is lower than target by too much!";
  ASSERT_LE(effective_datarate_[0], cfg_.rc_target_bitrate * 1.15)
      << " The datarate for the file is greater than target by too much!";
}

// Check basic rate targeting for CBR.
TEST_P(DatarateTestVP9Large, BasicRateTargeting444) {
  ::libvpx_test::Y4mVideoSource video("rush_hour_444.y4m", 0, 140);

  cfg_.g_profile = 1;
  cfg_.g_timebase = video.timebase();

  cfg_.rc_buf_initial_sz = 500;
  cfg_.rc_buf_optimal_sz = 500;
  cfg_.rc_buf_sz = 1000;
  cfg_.rc_dropframe_thresh = 1;
  cfg_.rc_min_quantizer = 0;
  cfg_.rc_max_quantizer = 63;
  cfg_.rc_end_usage = VPX_CBR;

  for (int i = 250; i < 900; i += 200) {
    cfg_.rc_target_bitrate = i;
    ResetModel();
    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
    ASSERT_GE(static_cast<double>(cfg_.rc_target_bitrate),
              effective_datarate_[0] * 0.85)
        << " The datarate for the file exceeds the target by too much!";
    ASSERT_LE(static_cast<double>(cfg_.rc_target_bitrate),
              effective_datarate_[0] * 1.15)
        << " The datarate for the file missed the target!"
        << cfg_.rc_target_bitrate << " " << effective_datarate_;
  }
}

// Check that (1) the first dropped frame gets earlier and earlier
// as the drop frame threshold is increased, and (2) that the total number of
// frame drops does not decrease as we increase frame drop threshold.
// Use a lower qp-max to force some frame drops.
TEST_P(DatarateTestVP9Large, ChangingDropFrameThresh) {
  cfg_.rc_buf_initial_sz = 500;
  cfg_.rc_buf_optimal_sz = 500;
  cfg_.rc_buf_sz = 1000;
  cfg_.rc_undershoot_pct = 20;
  cfg_.rc_undershoot_pct = 20;
  cfg_.rc_dropframe_thresh = 10;
  cfg_.rc_min_quantizer = 0;
  cfg_.rc_max_quantizer = 50;
  cfg_.rc_end_usage = VPX_CBR;
  cfg_.rc_target_bitrate = 200;
  cfg_.g_lag_in_frames = 0;
  // TODO(marpan): Investigate datarate target failures with a smaller keyframe
  // interval (128).
  cfg_.kf_max_dist = 9999;

  ::libvpx_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352, 288,
                                       30, 1, 0, 140);

  const int kDropFrameThreshTestStep = 30;
  vpx_codec_pts_t last_drop = 140;
  int last_num_drops = 0;
  for (int i = 10; i < 100; i += kDropFrameThreshTestStep) {
    cfg_.rc_dropframe_thresh = i;
    ResetModel();
    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
    ASSERT_GE(effective_datarate_[0], cfg_.rc_target_bitrate * 0.85)
        << " The datarate for the file is lower than target by too much!";
    ASSERT_LE(effective_datarate_[0], cfg_.rc_target_bitrate * 1.15)
        << " The datarate for the file is greater than target by too much!";
    ASSERT_LE(first_drop_, last_drop)
        << " The first dropped frame for drop_thresh " << i
        << " > first dropped frame for drop_thresh "
        << i - kDropFrameThreshTestStep;
    ASSERT_GE(num_drops_, last_num_drops * 0.85)
        << " The number of dropped frames for drop_thresh " << i
        << " < number of dropped frames for drop_thresh "
        << i - kDropFrameThreshTestStep;
    last_drop = first_drop_;
    last_num_drops = num_drops_;
  }
}

// Check basic rate targeting for 2 temporal layers.
TEST_P(DatarateTestVP9Large, BasicRateTargeting2TemporalLayers) {
  cfg_.rc_buf_initial_sz = 500;
  cfg_.rc_buf_optimal_sz = 500;
  cfg_.rc_buf_sz = 1000;
  cfg_.rc_dropframe_thresh = 1;
  cfg_.rc_min_quantizer = 0;
  cfg_.rc_max_quantizer = 63;
  cfg_.rc_end_usage = VPX_CBR;
  cfg_.g_lag_in_frames = 0;

  // 2 Temporal layers, no spatial layers: Framerate decimation (2, 1).
  cfg_.ss_number_layers = 1;
  cfg_.ts_number_layers = 2;
  cfg_.ts_rate_decimator[0] = 2;
  cfg_.ts_rate_decimator[1] = 1;

  cfg_.temporal_layering_mode = VP9E_TEMPORAL_LAYERING_MODE_BYPASS;

  if (deadline_ == VPX_DL_REALTIME) cfg_.g_error_resilient = 1;

  ::libvpx_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352, 288,
                                       30, 1, 0, 200);
  for (int i = 200; i <= 800; i += 200) {
    cfg_.rc_target_bitrate = i;
    ResetModel();
    // 60-40 bitrate allocation for 2 temporal layers.
    cfg_.layer_target_bitrate[0] = 60 * cfg_.rc_target_bitrate / 100;
    cfg_.layer_target_bitrate[1] = cfg_.rc_target_bitrate;
    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
    for (int j = 0; j < static_cast<int>(cfg_.ts_number_layers); ++j) {
      ASSERT_GE(effective_datarate_[j], cfg_.layer_target_bitrate[j] * 0.85)
          << " The datarate for the file is lower than target by too much, "
             "for layer: "
          << j;
      ASSERT_LE(effective_datarate_[j], cfg_.layer_target_bitrate[j] * 1.15)
          << " The datarate for the file is greater than target by too much, "
             "for layer: "
          << j;
    }
  }
}

// Check basic rate targeting for 3 temporal layers.
TEST_P(DatarateTestVP9Large, BasicRateTargeting3TemporalLayers) {
  cfg_.rc_buf_initial_sz = 500;
  cfg_.rc_buf_optimal_sz = 500;
  cfg_.rc_buf_sz = 1000;
  cfg_.rc_dropframe_thresh = 1;
  cfg_.rc_min_quantizer = 0;
  cfg_.rc_max_quantizer = 63;
  cfg_.rc_end_usage = VPX_CBR;
  cfg_.g_lag_in_frames = 0;

  // 3 Temporal layers, no spatial layers: Framerate decimation (4, 2, 1).
  cfg_.ss_number_layers = 1;
  cfg_.ts_number_layers = 3;
  cfg_.ts_rate_decimator[0] = 4;
  cfg_.ts_rate_decimator[1] = 2;
  cfg_.ts_rate_decimator[2] = 1;

  cfg_.temporal_layering_mode = VP9E_TEMPORAL_LAYERING_MODE_BYPASS;

  ::libvpx_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352, 288,
                                       30, 1, 0, 200);
  for (int i = 200; i <= 800; i += 200) {
    cfg_.rc_target_bitrate = i;
    ResetModel();
    // 40-20-40 bitrate allocation for 3 temporal layers.
    cfg_.layer_target_bitrate[0] = 40 * cfg_.rc_target_bitrate / 100;
    cfg_.layer_target_bitrate[1] = 60 * cfg_.rc_target_bitrate / 100;
    cfg_.layer_target_bitrate[2] = cfg_.rc_target_bitrate;
    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
    for (int j = 0; j < static_cast<int>(cfg_.ts_number_layers); ++j) {
      // TODO(yaowu): Work out more stable rc control strategy and
      //              Adjust the thresholds to be tighter than .75.
      ASSERT_GE(effective_datarate_[j], cfg_.layer_target_bitrate[j] * 0.75)
          << " The datarate for the file is lower than target by too much, "
             "for layer: "
          << j;
      // TODO(yaowu): Work out more stable rc control strategy and
      //              Adjust the thresholds to be tighter than 1.25.
      ASSERT_LE(effective_datarate_[j], cfg_.layer_target_bitrate[j] * 1.25)
          << " The datarate for the file is greater than target by too much, "
             "for layer: "
          << j;
    }
  }
}

// Check basic rate targeting for 3 temporal layers, with frame dropping.
// Only for one (low) bitrate with lower max_quantizer, and somewhat higher
// frame drop threshold, to force frame dropping.
TEST_P(DatarateTestVP9Large, BasicRateTargeting3TemporalLayersFrameDropping) {
  cfg_.rc_buf_initial_sz = 500;
  cfg_.rc_buf_optimal_sz = 500;
  cfg_.rc_buf_sz = 1000;
  // Set frame drop threshold and rc_max_quantizer to force some frame drops.
  cfg_.rc_dropframe_thresh = 20;
  cfg_.rc_max_quantizer = 45;
  cfg_.rc_min_quantizer = 0;
  cfg_.rc_end_usage = VPX_CBR;
  cfg_.g_lag_in_frames = 0;

  // 3 Temporal layers, no spatial layers: Framerate decimation (4, 2, 1).
  cfg_.ss_number_layers = 1;
  cfg_.ts_number_layers = 3;
  cfg_.ts_rate_decimator[0] = 4;
  cfg_.ts_rate_decimator[1] = 2;
  cfg_.ts_rate_decimator[2] = 1;

  cfg_.temporal_layering_mode = VP9E_TEMPORAL_LAYERING_MODE_BYPASS;

  ::libvpx_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352, 288,
                                       30, 1, 0, 200);
  cfg_.rc_target_bitrate = 200;
  ResetModel();
  // 40-20-40 bitrate allocation for 3 temporal layers.
  cfg_.layer_target_bitrate[0] = 40 * cfg_.rc_target_bitrate / 100;
  cfg_.layer_target_bitrate[1] = 60 * cfg_.rc_target_bitrate / 100;
  cfg_.layer_target_bitrate[2] = cfg_.rc_target_bitrate;
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
  for (int j = 0; j < static_cast<int>(cfg_.ts_number_layers); ++j) {
    ASSERT_GE(effective_datarate_[j], cfg_.layer_target_bitrate[j] * 0.85)
        << " The datarate for the file is lower than target by too much, "
           "for layer: "
        << j;
    ASSERT_LE(effective_datarate_[j], cfg_.layer_target_bitrate[j] * 1.15)
        << " The datarate for the file is greater than target by too much, "
           "for layer: "
        << j;
    // Expect some frame drops in this test: for this 200 frames test,
    // expect at least 10% and not more than 60% drops.
    ASSERT_GE(num_drops_, 20);
    ASSERT_LE(num_drops_, 130);
  }
}

#if CONFIG_VP9_TEMPORAL_DENOISING
class DatarateTestVP9LargeDenoiser : public DatarateTestVP9Large {
 public:
  virtual ~DatarateTestVP9LargeDenoiser() {}
};

// Check basic datarate targeting, for a single bitrate, when denoiser is on.
TEST_P(DatarateTestVP9LargeDenoiser, LowNoise) {
  cfg_.rc_buf_initial_sz = 500;
  cfg_.rc_buf_optimal_sz = 500;
  cfg_.rc_buf_sz = 1000;
  cfg_.rc_dropframe_thresh = 1;
  cfg_.rc_min_quantizer = 2;
  cfg_.rc_max_quantizer = 56;
  cfg_.rc_end_usage = VPX_CBR;
  cfg_.g_lag_in_frames = 0;

  ::libvpx_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352, 288,
                                       30, 1, 0, 140);

  // For the temporal denoiser (#if CONFIG_VP9_TEMPORAL_DENOISING),
  // there is only one denoiser mode: denoiserYonly(which is 1),
  // but may add more modes in the future.
  cfg_.rc_target_bitrate = 300;
  ResetModel();
  // Turn on the denoiser.
  denoiser_on_ = 1;
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
  ASSERT_GE(effective_datarate_[0], cfg_.rc_target_bitrate * 0.85)
      << " The datarate for the file is lower than target by too much!";
  ASSERT_LE(effective_datarate_[0], cfg_.rc_target_bitrate * 1.15)
      << " The datarate for the file is greater than target by too much!";
}

// Check basic datarate targeting, for a single bitrate, when denoiser is on,
// for clip with high noise level.
TEST_P(DatarateTestVP9LargeDenoiser, HighNoise) {
  cfg_.rc_buf_initial_sz = 500;
  cfg_.rc_buf_optimal_sz = 500;
  cfg_.rc_buf_sz = 1000;
  cfg_.rc_dropframe_thresh = 1;
  cfg_.rc_min_quantizer = 2;
  cfg_.rc_max_quantizer = 56;
  cfg_.rc_end_usage = VPX_CBR;
  cfg_.g_lag_in_frames = 0;

  ::libvpx_test::Y4mVideoSource video("noisy_clip_640_360.y4m", 0, 200);

  // For the temporal denoiser (#if CONFIG_VP9_TEMPORAL_DENOISING),
  // there is only one denoiser mode: denoiserYonly(which is 1),
  // but may add more modes in the future.
  cfg_.rc_target_bitrate = 1000;
  ResetModel();
  // Turn on the denoiser.
  denoiser_on_ = 1;
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
  ASSERT_GE(effective_datarate_[0], cfg_.rc_target_bitrate * 0.85)
      << " The datarate for the file is lower than target by too much!";
  ASSERT_LE(effective_datarate_[0], cfg_.rc_target_bitrate * 1.15)
      << " The datarate for the file is greater than target by too much!";
}

// Check basic datarate targeting, for a single bitrate, when denoiser is off
// and on.
TEST_P(DatarateTestVP9LargeDenoiser, DenoiserOffOn) {
  cfg_.rc_buf_initial_sz = 500;
  cfg_.rc_buf_optimal_sz = 500;
  cfg_.rc_buf_sz = 1000;
  cfg_.rc_dropframe_thresh = 1;
  cfg_.rc_min_quantizer = 2;
  cfg_.rc_max_quantizer = 56;
  cfg_.rc_end_usage = VPX_CBR;
  cfg_.g_lag_in_frames = 0;

  ::libvpx_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352, 288,
                                       30, 1, 0, 299);

  // For the temporal denoiser (#if CONFIG_VP9_TEMPORAL_DENOISING),
  // there is only one denoiser mode: denoiserYonly(which is 1),
  // but may add more modes in the future.
  cfg_.rc_target_bitrate = 300;
  ResetModel();
  // The denoiser is off by default.
  denoiser_on_ = 0;
  // Set the offon test flag.
  denoiser_offon_test_ = 1;
  denoiser_offon_period_ = 100;
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
  ASSERT_GE(effective_datarate_[0], cfg_.rc_target_bitrate * 0.85)
      << " The datarate for the file is lower than target by too much!";
  ASSERT_LE(effective_datarate_[0], cfg_.rc_target_bitrate * 1.15)
      << " The datarate for the file is greater than target by too much!";
}
#endif  // CONFIG_VP9_TEMPORAL_DENOISING

class DatarateOnePassCbrSvc
    : public ::libvpx_test::EncoderTest,
      public ::libvpx_test::CodecTestWith2Params<libvpx_test::TestMode, int> {
 public:
  DatarateOnePassCbrSvc() : EncoderTest(GET_PARAM(0)) {
    memset(&svc_params_, 0, sizeof(svc_params_));
  }
  virtual ~DatarateOnePassCbrSvc() {}

 protected:
  virtual void SetUp() {
    InitializeConfig();
    SetMode(GET_PARAM(1));
    speed_setting_ = GET_PARAM(2);
    ResetModel();
  }
  virtual void ResetModel() {
    last_pts_ = 0;
    bits_in_buffer_model_ = cfg_.rc_target_bitrate * cfg_.rc_buf_initial_sz;
    frame_number_ = 0;
    first_drop_ = 0;
    bits_total_ = 0;
    duration_ = 0.0;
    mismatch_psnr_ = 0.0;
    mismatch_nframes_ = 0;
  }
  virtual void BeginPassHook(unsigned int /*pass*/) {}
  virtual void PreEncodeFrameHook(::libvpx_test::VideoSource *video,
                                  ::libvpx_test::Encoder *encoder) {
    if (video->frame() == 0) {
      int i;
      for (i = 0; i < VPX_MAX_LAYERS; ++i) {
        svc_params_.max_quantizers[i] = 63;
        svc_params_.min_quantizers[i] = 0;
      }
      svc_params_.speed_per_layer[0] = 5;
      for (i = 1; i < VPX_SS_MAX_LAYERS; ++i) {
        svc_params_.speed_per_layer[i] = speed_setting_;
      }
      encoder->Control(VP9E_SET_SVC, 1);
      encoder->Control(VP9E_SET_SVC_PARAMETERS, &svc_params_);
      encoder->Control(VP8E_SET_CPUUSED, speed_setting_);
      encoder->Control(VP9E_SET_TILE_COLUMNS, 0);
      encoder->Control(VP8E_SET_MAX_INTRA_BITRATE_PCT, 300);
      encoder->Control(VP9E_SET_TILE_COLUMNS, (cfg_.g_threads >> 1));
      encoder->Control(VP8E_SET_STATIC_THRESHOLD, 1);
    }
    const vpx_rational_t tb = video->timebase();
    timebase_ = static_cast<double>(tb.num) / tb.den;
    duration_ = 0;
  }
  virtual void FramePktHook(const vpx_codec_cx_pkt_t *pkt) {
    vpx_codec_pts_t duration = pkt->data.frame.pts - last_pts_;
    if (last_pts_ == 0) duration = 1;
    bits_in_buffer_model_ += static_cast<int64_t>(
        duration * timebase_ * cfg_.rc_target_bitrate * 1000);
    const bool key_frame =
        (pkt->data.frame.flags & VPX_FRAME_IS_KEY) ? true : false;
    if (!key_frame) {
      // TODO(marpan): This check currently fails for some of the SVC tests,
      // re-enable when issue (webm:1350) is resolved.
      //  ASSERT_GE(bits_in_buffer_model_, 0) << "Buffer Underrun at frame "
      //                                      << pkt->data.frame.pts;
    }
    const size_t frame_size_in_bits = pkt->data.frame.sz * 8;
    bits_in_buffer_model_ -= static_cast<int64_t>(frame_size_in_bits);
    bits_total_ += frame_size_in_bits;
    if (!first_drop_ && duration > 1) first_drop_ = last_pts_ + 1;
    last_pts_ = pkt->data.frame.pts;
    bits_in_last_frame_ = frame_size_in_bits;
    ++frame_number_;
  }
  virtual void EndPassHook(void) {
    if (bits_total_) {
      const double file_size_in_kb = bits_total_ / 1000.;  // bits per kilobit
      duration_ = (last_pts_ + 1) * timebase_;
      file_datarate_ = file_size_in_kb / duration_;
    }
  }

  virtual void MismatchHook(const vpx_image_t *img1, const vpx_image_t *img2) {
    double mismatch_psnr = compute_psnr(img1, img2);
    mismatch_psnr_ += mismatch_psnr;
    ++mismatch_nframes_;
  }

  unsigned int GetMismatchFrames() { return mismatch_nframes_; }

  vpx_codec_pts_t last_pts_;
  int64_t bits_in_buffer_model_;
  double timebase_;
  int frame_number_;
  vpx_codec_pts_t first_drop_;
  int64_t bits_total_;
  double duration_;
  double file_datarate_;
  size_t bits_in_last_frame_;
  vpx_svc_extra_cfg_t svc_params_;
  int speed_setting_;
  double mismatch_psnr_;
  int mismatch_nframes_;
};
static void assign_layer_bitrates(vpx_codec_enc_cfg_t *const enc_cfg,
                                  const vpx_svc_extra_cfg_t *svc_params,
                                  int spatial_layers, int temporal_layers,
                                  int temporal_layering_mode) {
  int sl, spatial_layer_target;
  float total = 0;
  float alloc_ratio[VPX_MAX_LAYERS] = { 0 };
  for (sl = 0; sl < spatial_layers; ++sl) {
    if (svc_params->scaling_factor_den[sl] > 0) {
      alloc_ratio[sl] = (float)(svc_params->scaling_factor_num[sl] * 1.0 /
                                svc_params->scaling_factor_den[sl]);
      total += alloc_ratio[sl];
    }
  }
  for (sl = 0; sl < spatial_layers; ++sl) {
    enc_cfg->ss_target_bitrate[sl] = spatial_layer_target =
        (unsigned int)(enc_cfg->rc_target_bitrate * alloc_ratio[sl] / total);
    const int index = sl * temporal_layers;
    if (temporal_layering_mode == 3) {
      enc_cfg->layer_target_bitrate[index] = spatial_layer_target >> 1;
      enc_cfg->layer_target_bitrate[index + 1] =
          (spatial_layer_target >> 1) + (spatial_layer_target >> 2);
      enc_cfg->layer_target_bitrate[index + 2] = spatial_layer_target;
    } else if (temporal_layering_mode == 2) {
      enc_cfg->layer_target_bitrate[index] = spatial_layer_target * 2 / 3;
      enc_cfg->layer_target_bitrate[index + 1] = spatial_layer_target;
    }
  }
}

// Check basic rate targeting for 1 pass CBR SVC: 2 spatial layers and
// 3 temporal layers. Run CIF clip with 1 thread.
TEST_P(DatarateOnePassCbrSvc, OnePassCbrSvc2SpatialLayers) {
  cfg_.rc_buf_initial_sz = 500;
  cfg_.rc_buf_optimal_sz = 500;
  cfg_.rc_buf_sz = 1000;
  cfg_.rc_min_quantizer = 0;
  cfg_.rc_max_quantizer = 63;
  cfg_.rc_end_usage = VPX_CBR;
  cfg_.g_lag_in_frames = 0;
  cfg_.ss_number_layers = 2;
  cfg_.ts_number_layers = 3;
  cfg_.ts_rate_decimator[0] = 4;
  cfg_.ts_rate_decimator[1] = 2;
  cfg_.ts_rate_decimator[2] = 1;
  cfg_.g_error_resilient = 1;
  cfg_.g_threads = 1;
  cfg_.temporal_layering_mode = 3;
  svc_params_.scaling_factor_num[0] = 144;
  svc_params_.scaling_factor_den[0] = 288;
  svc_params_.scaling_factor_num[1] = 288;
  svc_params_.scaling_factor_den[1] = 288;
  cfg_.rc_dropframe_thresh = 10;
  cfg_.kf_max_dist = 9999;
  ::libvpx_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352, 288,
                                       30, 1, 0, 200);
  // TODO(marpan): Check that effective_datarate for each layer hits the
  // layer target_bitrate.
  for (int i = 200; i <= 800; i += 200) {
    cfg_.rc_target_bitrate = i;
    ResetModel();
    assign_layer_bitrates(&cfg_, &svc_params_, cfg_.ss_number_layers,
                          cfg_.ts_number_layers, cfg_.temporal_layering_mode);
    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
    ASSERT_GE(cfg_.rc_target_bitrate, file_datarate_ * 0.85)
        << " The datarate for the file exceeds the target by too much!";
    ASSERT_LE(cfg_.rc_target_bitrate, file_datarate_ * 1.15)
        << " The datarate for the file is lower than the target by too much!";
    EXPECT_EQ(static_cast<unsigned int>(0), GetMismatchFrames());
  }
}

// Check basic rate targeting for 1 pass CBR SVC: 2 spatial layers and 3
// temporal layers. Run CIF clip with 1 thread, and few short key frame periods.
TEST_P(DatarateOnePassCbrSvc, OnePassCbrSvc2SpatialLayersSmallKf) {
  cfg_.rc_buf_initial_sz = 500;
  cfg_.rc_buf_optimal_sz = 500;
  cfg_.rc_buf_sz = 1000;
  cfg_.rc_min_quantizer = 0;
  cfg_.rc_max_quantizer = 63;
  cfg_.rc_end_usage = VPX_CBR;
  cfg_.g_lag_in_frames = 0;
  cfg_.ss_number_layers = 2;
  cfg_.ts_number_layers = 3;
  cfg_.ts_rate_decimator[0] = 4;
  cfg_.ts_rate_decimator[1] = 2;
  cfg_.ts_rate_decimator[2] = 1;
  cfg_.g_error_resilient = 1;
  cfg_.g_threads = 1;
  cfg_.temporal_layering_mode = 3;
  svc_params_.scaling_factor_num[0] = 144;
  svc_params_.scaling_factor_den[0] = 288;
  svc_params_.scaling_factor_num[1] = 288;
  svc_params_.scaling_factor_den[1] = 288;
  cfg_.rc_dropframe_thresh = 10;
  ::libvpx_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352, 288,
                                       30, 1, 0, 200);
  cfg_.rc_target_bitrate = 400;
  // For this 3 temporal layer case, pattern repeats every 4 frames, so choose
  // 4 key neighboring key frame periods (so key frame will land on 0-2-1-2).
  for (int j = 64; j <= 67; j++) {
    cfg_.kf_max_dist = j;
    ResetModel();
    assign_layer_bitrates(&cfg_, &svc_params_, cfg_.ss_number_layers,
                          cfg_.ts_number_layers, cfg_.temporal_layering_mode);
    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
    ASSERT_GE(cfg_.rc_target_bitrate, file_datarate_ * 0.85)
        << " The datarate for the file exceeds the target by too much!";
    ASSERT_LE(cfg_.rc_target_bitrate, file_datarate_ * 1.15)
        << " The datarate for the file is lower than the target by too much!";
    EXPECT_EQ(static_cast<unsigned int>(0), GetMismatchFrames());
  }
}

// Check basic rate targeting for 1 pass CBR SVC: 2 spatial layers and
// 3 temporal layers. Run HD clip with 4 threads.
TEST_P(DatarateOnePassCbrSvc, OnePassCbrSvc2SpatialLayers4threads) {
  cfg_.rc_buf_initial_sz = 500;
  cfg_.rc_buf_optimal_sz = 500;
  cfg_.rc_buf_sz = 1000;
  cfg_.rc_min_quantizer = 0;
  cfg_.rc_max_quantizer = 63;
  cfg_.rc_end_usage = VPX_CBR;
  cfg_.g_lag_in_frames = 0;
  cfg_.ss_number_layers = 2;
  cfg_.ts_number_layers = 3;
  cfg_.ts_rate_decimator[0] = 4;
  cfg_.ts_rate_decimator[1] = 2;
  cfg_.ts_rate_decimator[2] = 1;
  cfg_.g_error_resilient = 1;
  cfg_.g_threads = 4;
  cfg_.temporal_layering_mode = 3;
  svc_params_.scaling_factor_num[0] = 144;
  svc_params_.scaling_factor_den[0] = 288;
  svc_params_.scaling_factor_num[1] = 288;
  svc_params_.scaling_factor_den[1] = 288;
  cfg_.rc_dropframe_thresh = 10;
  cfg_.kf_max_dist = 9999;
  ::libvpx_test::Y4mVideoSource video("niklas_1280_720_30.y4m", 0, 300);
  cfg_.rc_target_bitrate = 800;
  ResetModel();
  assign_layer_bitrates(&cfg_, &svc_params_, cfg_.ss_number_layers,
                        cfg_.ts_number_layers, cfg_.temporal_layering_mode);
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
  ASSERT_GE(cfg_.rc_target_bitrate, file_datarate_ * 0.85)
      << " The datarate for the file exceeds the target by too much!";
  ASSERT_LE(cfg_.rc_target_bitrate, file_datarate_ * 1.15)
      << " The datarate for the file is lower than the target by too much!";
  EXPECT_EQ(static_cast<unsigned int>(0), GetMismatchFrames());
}

// Check basic rate targeting for 1 pass CBR SVC: 3 spatial layers and
// 3 temporal layers. Run CIF clip with 1 thread.
TEST_P(DatarateOnePassCbrSvc, OnePassCbrSvc3SpatialLayers) {
  cfg_.rc_buf_initial_sz = 500;
  cfg_.rc_buf_optimal_sz = 500;
  cfg_.rc_buf_sz = 1000;
  cfg_.rc_min_quantizer = 0;
  cfg_.rc_max_quantizer = 63;
  cfg_.rc_end_usage = VPX_CBR;
  cfg_.g_lag_in_frames = 0;
  cfg_.ss_number_layers = 3;
  cfg_.ts_number_layers = 3;
  cfg_.ts_rate_decimator[0] = 4;
  cfg_.ts_rate_decimator[1] = 2;
  cfg_.ts_rate_decimator[2] = 1;
  cfg_.g_error_resilient = 1;
  cfg_.g_threads = 1;
  cfg_.temporal_layering_mode = 3;
  svc_params_.scaling_factor_num[0] = 72;
  svc_params_.scaling_factor_den[0] = 288;
  svc_params_.scaling_factor_num[1] = 144;
  svc_params_.scaling_factor_den[1] = 288;
  svc_params_.scaling_factor_num[2] = 288;
  svc_params_.scaling_factor_den[2] = 288;
  cfg_.rc_dropframe_thresh = 10;
  cfg_.kf_max_dist = 9999;
  ::libvpx_test::Y4mVideoSource video("niklas_1280_720_30.y4m", 0, 300);
  cfg_.rc_target_bitrate = 800;
  ResetModel();
  assign_layer_bitrates(&cfg_, &svc_params_, cfg_.ss_number_layers,
                        cfg_.ts_number_layers, cfg_.temporal_layering_mode);
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
  ASSERT_GE(cfg_.rc_target_bitrate, file_datarate_ * 0.85)
      << " The datarate for the file exceeds the target by too much!";
  ASSERT_LE(cfg_.rc_target_bitrate, file_datarate_ * 1.22)
      << " The datarate for the file is lower than the target by too much!";
  EXPECT_EQ(static_cast<unsigned int>(0), GetMismatchFrames());
}

// Check basic rate targeting for 1 pass CBR SVC: 3 spatial layers and 3
// temporal layers. Run CIF clip with 1 thread, and few short key frame periods.
TEST_P(DatarateOnePassCbrSvc, OnePassCbrSvc3SpatialLayersSmallKf) {
  cfg_.rc_buf_initial_sz = 500;
  cfg_.rc_buf_optimal_sz = 500;
  cfg_.rc_buf_sz = 1000;
  cfg_.rc_min_quantizer = 0;
  cfg_.rc_max_quantizer = 63;
  cfg_.rc_end_usage = VPX_CBR;
  cfg_.g_lag_in_frames = 0;
  cfg_.ss_number_layers = 3;
  cfg_.ts_number_layers = 3;
  cfg_.ts_rate_decimator[0] = 4;
  cfg_.ts_rate_decimator[1] = 2;
  cfg_.ts_rate_decimator[2] = 1;
  cfg_.g_error_resilient = 1;
  cfg_.g_threads = 1;
  cfg_.temporal_layering_mode = 3;
  svc_params_.scaling_factor_num[0] = 72;
  svc_params_.scaling_factor_den[0] = 288;
  svc_params_.scaling_factor_num[1] = 144;
  svc_params_.scaling_factor_den[1] = 288;
  svc_params_.scaling_factor_num[2] = 288;
  svc_params_.scaling_factor_den[2] = 288;
  cfg_.rc_dropframe_thresh = 10;
  ::libvpx_test::Y4mVideoSource video("niklas_1280_720_30.y4m", 0, 300);
  cfg_.rc_target_bitrate = 800;
  // For this 3 temporal layer case, pattern repeats every 4 frames, so choose
  // 4 key neighboring key frame periods (so key frame will land on 0-2-1-2).
  for (int j = 32; j <= 35; j++) {
    cfg_.kf_max_dist = j;
    ResetModel();
    assign_layer_bitrates(&cfg_, &svc_params_, cfg_.ss_number_layers,
                          cfg_.ts_number_layers, cfg_.temporal_layering_mode);
    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
    ASSERT_GE(cfg_.rc_target_bitrate, file_datarate_ * 0.85)
        << " The datarate for the file exceeds the target by too much!";
    ASSERT_LE(cfg_.rc_target_bitrate, file_datarate_ * 1.30)
        << " The datarate for the file is lower than the target by too much!";
    EXPECT_EQ(static_cast<unsigned int>(0), GetMismatchFrames());
  }
}

// Check basic rate targeting for 1 pass CBR SVC: 3 spatial layers and
// 3 temporal layers. Run HD clip with 4 threads.
TEST_P(DatarateOnePassCbrSvc, OnePassCbrSvc3SpatialLayers4threads) {
  cfg_.rc_buf_initial_sz = 500;
  cfg_.rc_buf_optimal_sz = 500;
  cfg_.rc_buf_sz = 1000;
  cfg_.rc_min_quantizer = 0;
  cfg_.rc_max_quantizer = 63;
  cfg_.rc_end_usage = VPX_CBR;
  cfg_.g_lag_in_frames = 0;
  cfg_.ss_number_layers = 3;
  cfg_.ts_number_layers = 3;
  cfg_.ts_rate_decimator[0] = 4;
  cfg_.ts_rate_decimator[1] = 2;
  cfg_.ts_rate_decimator[2] = 1;
  cfg_.g_error_resilient = 1;
  cfg_.g_threads = 4;
  cfg_.temporal_layering_mode = 3;
  svc_params_.scaling_factor_num[0] = 72;
  svc_params_.scaling_factor_den[0] = 288;
  svc_params_.scaling_factor_num[1] = 144;
  svc_params_.scaling_factor_den[1] = 288;
  svc_params_.scaling_factor_num[2] = 288;
  svc_params_.scaling_factor_den[2] = 288;
  cfg_.rc_dropframe_thresh = 10;
  cfg_.kf_max_dist = 9999;
  ::libvpx_test::Y4mVideoSource video("niklas_1280_720_30.y4m", 0, 300);
  cfg_.rc_target_bitrate = 800;
  ResetModel();
  assign_layer_bitrates(&cfg_, &svc_params_, cfg_.ss_number_layers,
                        cfg_.ts_number_layers, cfg_.temporal_layering_mode);
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
  ASSERT_GE(cfg_.rc_target_bitrate, file_datarate_ * 0.85)
      << " The datarate for the file exceeds the target by too much!";
  ASSERT_LE(cfg_.rc_target_bitrate, file_datarate_ * 1.22)
      << " The datarate for the file is lower than the target by too much!";
  EXPECT_EQ(static_cast<unsigned int>(0), GetMismatchFrames());
}

// Run SVC encoder for 1 temporal layer, 2 spatial layers, with spatial
// downscale 5x5.
TEST_P(DatarateOnePassCbrSvc, OnePassCbrSvc2SpatialLayers5x5MultipleRuns) {
  cfg_.rc_buf_initial_sz = 500;
  cfg_.rc_buf_optimal_sz = 500;
  cfg_.rc_buf_sz = 1000;
  cfg_.rc_min_quantizer = 0;
  cfg_.rc_max_quantizer = 63;
  cfg_.rc_end_usage = VPX_CBR;
  cfg_.g_lag_in_frames = 0;
  cfg_.ss_number_layers = 2;
  cfg_.ts_number_layers = 1;
  cfg_.ts_rate_decimator[0] = 1;
  cfg_.g_error_resilient = 1;
  cfg_.g_threads = 3;
  cfg_.temporal_layering_mode = 0;
  svc_params_.scaling_factor_num[0] = 256;
  svc_params_.scaling_factor_den[0] = 1280;
  svc_params_.scaling_factor_num[1] = 1280;
  svc_params_.scaling_factor_den[1] = 1280;
  cfg_.rc_dropframe_thresh = 0;
  cfg_.kf_max_dist = 999999;
  cfg_.kf_min_dist = 0;
  cfg_.ss_target_bitrate[0] = 300;
  cfg_.ss_target_bitrate[1] = 1400;
  cfg_.layer_target_bitrate[0] = 300;
  cfg_.layer_target_bitrate[1] = 1400;
  cfg_.rc_target_bitrate = 1700;
  ::libvpx_test::Y4mVideoSource video("niklas_1280_720_30.y4m", 0, 300);
  ResetModel();
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
  EXPECT_EQ(static_cast<unsigned int>(0), GetMismatchFrames());
}

VP8_INSTANTIATE_TEST_CASE(DatarateTestLarge, ALL_TEST_MODES,
                          ::testing::Values(0));
VP8_INSTANTIATE_TEST_CASE(DatarateTestRealTime,
                          ::testing::Values(::libvpx_test::kRealTime),
                          ::testing::Values(-6, -12));
VP9_INSTANTIATE_TEST_CASE(DatarateTestVP9Large,
                          ::testing::Values(::libvpx_test::kOnePassGood,
                                            ::libvpx_test::kRealTime),
                          ::testing::Range(2, 9));
#if CONFIG_VP9_TEMPORAL_DENOISING
VP9_INSTANTIATE_TEST_CASE(DatarateTestVP9LargeDenoiser,
                          ::testing::Values(::libvpx_test::kRealTime),
                          ::testing::Range(5, 9));
#endif
VP9_INSTANTIATE_TEST_CASE(DatarateOnePassCbrSvc,
                          ::testing::Values(::libvpx_test::kRealTime),
                          ::testing::Range(5, 9));
}  // namespace
