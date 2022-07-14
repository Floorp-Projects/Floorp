/*
 *  Copyright (c) 2020 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "vp9/ratectrl_rtc.h"

#include <fstream>  // NOLINT
#include <string>

#include "./vpx_config.h"
#include "third_party/googletest/src/include/gtest/gtest.h"
#include "test/codec_factory.h"
#include "test/encode_test_driver.h"
#include "test/i420_video_source.h"
#include "test/util.h"
#include "test/video_source.h"
#include "vpx/vpx_codec.h"
#include "vpx_ports/bitops.h"

namespace {

const size_t kNumFrames = 300;

const int kTemporalId[4] = { 0, 2, 1, 2 };

class RcInterfaceTest
    : public ::libvpx_test::EncoderTest,
      public ::libvpx_test::CodecTestWith2Params<int, vpx_rc_mode> {
 public:
  RcInterfaceTest()
      : EncoderTest(GET_PARAM(0)), aq_mode_(GET_PARAM(1)), key_interval_(3000),
        encoder_exit_(false) {}

  virtual ~RcInterfaceTest() {}

 protected:
  virtual void SetUp() {
    InitializeConfig();
    SetMode(::libvpx_test::kRealTime);
  }

  virtual void PreEncodeFrameHook(libvpx_test::VideoSource *video,
                                  libvpx_test::Encoder *encoder) {
    if (video->frame() == 0) {
      encoder->Control(VP8E_SET_CPUUSED, 7);
      encoder->Control(VP9E_SET_AQ_MODE, aq_mode_);
      encoder->Control(VP9E_SET_TUNE_CONTENT, 0);
      encoder->Control(VP8E_SET_MAX_INTRA_BITRATE_PCT, 1000);
      encoder->Control(VP9E_SET_RTC_EXTERNAL_RATECTRL, 1);
    }
    frame_params_.frame_type =
        video->frame() % key_interval_ == 0 ? KEY_FRAME : INTER_FRAME;
    if (rc_cfg_.rc_mode == VPX_CBR && frame_params_.frame_type == INTER_FRAME) {
      // Disable golden frame update.
      frame_flags_ |= VP8_EFLAG_NO_UPD_GF;
      frame_flags_ |= VP8_EFLAG_NO_UPD_ARF;
    }
    encoder_exit_ = video->frame() == kNumFrames;
  }

  virtual void PostEncodeFrameHook(::libvpx_test::Encoder *encoder) {
    if (encoder_exit_) {
      return;
    }
    int loopfilter_level, qp;
    encoder->Control(VP9E_GET_LOOPFILTER_LEVEL, &loopfilter_level);
    encoder->Control(VP8E_GET_LAST_QUANTIZER, &qp);
    rc_api_->ComputeQP(frame_params_);
    ASSERT_EQ(rc_api_->GetQP(), qp);
    ASSERT_EQ(rc_api_->GetLoopfilterLevel(), loopfilter_level);
  }

  virtual void FramePktHook(const vpx_codec_cx_pkt_t *pkt) {
    rc_api_->PostEncodeUpdate(pkt->data.frame.sz);
  }

  void RunOneLayer() {
    SetConfig(GET_PARAM(2));
    rc_api_ = libvpx::VP9RateControlRTC::Create(rc_cfg_);
    frame_params_.spatial_layer_id = 0;
    frame_params_.temporal_layer_id = 0;

    ::libvpx_test::I420VideoSource video("desktop_office1.1280_720-020.yuv",
                                         1280, 720, 30, 1, 0, kNumFrames);

    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
  }

  void RunOneLayerVBRPeriodicKey() {
    if (GET_PARAM(2) != VPX_VBR) return;
    key_interval_ = 100;
    SetConfig(VPX_VBR);
    rc_api_ = libvpx::VP9RateControlRTC::Create(rc_cfg_);
    frame_params_.spatial_layer_id = 0;
    frame_params_.temporal_layer_id = 0;

    ::libvpx_test::I420VideoSource video("desktop_office1.1280_720-020.yuv",
                                         1280, 720, 30, 1, 0, kNumFrames);

    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
  }

 private:
  void SetConfig(vpx_rc_mode rc_mode) {
    rc_cfg_.width = 1280;
    rc_cfg_.height = 720;
    rc_cfg_.max_quantizer = 52;
    rc_cfg_.min_quantizer = 2;
    rc_cfg_.target_bandwidth = 1000;
    rc_cfg_.buf_initial_sz = 600;
    rc_cfg_.buf_optimal_sz = 600;
    rc_cfg_.buf_sz = 1000;
    rc_cfg_.undershoot_pct = 50;
    rc_cfg_.overshoot_pct = 50;
    rc_cfg_.max_intra_bitrate_pct = 1000;
    rc_cfg_.framerate = 30.0;
    rc_cfg_.ss_number_layers = 1;
    rc_cfg_.ts_number_layers = 1;
    rc_cfg_.scaling_factor_num[0] = 1;
    rc_cfg_.scaling_factor_den[0] = 1;
    rc_cfg_.layer_target_bitrate[0] = 1000;
    rc_cfg_.max_quantizers[0] = 52;
    rc_cfg_.min_quantizers[0] = 2;
    rc_cfg_.rc_mode = rc_mode;
    rc_cfg_.aq_mode = aq_mode_;

    // Encoder settings for ground truth.
    cfg_.g_w = 1280;
    cfg_.g_h = 720;
    cfg_.rc_undershoot_pct = 50;
    cfg_.rc_overshoot_pct = 50;
    cfg_.rc_buf_initial_sz = 600;
    cfg_.rc_buf_optimal_sz = 600;
    cfg_.rc_buf_sz = 1000;
    cfg_.rc_dropframe_thresh = 0;
    cfg_.rc_min_quantizer = 2;
    cfg_.rc_max_quantizer = 52;
    cfg_.rc_end_usage = rc_mode;
    cfg_.g_lag_in_frames = 0;
    cfg_.g_error_resilient = 0;
    cfg_.rc_target_bitrate = 1000;
    cfg_.kf_min_dist = key_interval_;
    cfg_.kf_max_dist = key_interval_;
  }

  std::unique_ptr<libvpx::VP9RateControlRTC> rc_api_;
  libvpx::VP9RateControlRtcConfig rc_cfg_;
  int aq_mode_;
  int key_interval_;
  libvpx::VP9FrameParamsQpRTC frame_params_;
  bool encoder_exit_;
};

class RcInterfaceSvcTest : public ::libvpx_test::EncoderTest,
                           public ::libvpx_test::CodecTestWithParam<int> {
 public:
  RcInterfaceSvcTest() : EncoderTest(GET_PARAM(0)), aq_mode_(GET_PARAM(1)) {}
  virtual ~RcInterfaceSvcTest() {}

 protected:
  virtual void SetUp() {
    InitializeConfig();
    SetMode(::libvpx_test::kRealTime);
  }

  virtual void PreEncodeFrameHook(libvpx_test::VideoSource *video,
                                  ::libvpx_test::Encoder *encoder) {
    if (video->frame() == 0) {
      encoder->Control(VP8E_SET_CPUUSED, 7);
      encoder->Control(VP9E_SET_AQ_MODE, aq_mode_);
      encoder->Control(VP9E_SET_TUNE_CONTENT, 0);
      encoder->Control(VP8E_SET_MAX_INTRA_BITRATE_PCT, 900);
      encoder->Control(VP9E_SET_RTC_EXTERNAL_RATECTRL, 1);
      encoder->Control(VP9E_SET_SVC, 1);
      encoder->Control(VP9E_SET_SVC_PARAMETERS, &svc_params_);
    }

    frame_params_.frame_type = video->frame() == 0 ? KEY_FRAME : INTER_FRAME;
    if (rc_cfg_.rc_mode == VPX_CBR && frame_params_.frame_type == INTER_FRAME) {
      // Disable golden frame update.
      frame_flags_ |= VP8_EFLAG_NO_UPD_GF;
      frame_flags_ |= VP8_EFLAG_NO_UPD_ARF;
    }
    encoder_exit_ = video->frame() == kNumFrames;
    current_superframe_ = video->frame();
  }

  virtual void PostEncodeFrameHook(::libvpx_test::Encoder *encoder) {
    ::libvpx_test::CxDataIterator iter = encoder->GetCxData();
    while (const vpx_codec_cx_pkt_t *pkt = iter.Next()) {
      ParseSuperframeSizes(static_cast<const uint8_t *>(pkt->data.frame.buf),
                           pkt->data.frame.sz);
      for (int sl = 0; sl < rc_cfg_.ss_number_layers; sl++) {
        frame_params_.spatial_layer_id = sl;
        frame_params_.temporal_layer_id = kTemporalId[current_superframe_ % 4];
        rc_api_->ComputeQP(frame_params_);
        frame_params_.frame_type = INTER_FRAME;
        rc_api_->PostEncodeUpdate(sizes_[sl]);
      }
    }
    if (!encoder_exit_) {
      int loopfilter_level, qp;
      encoder->Control(VP9E_GET_LOOPFILTER_LEVEL, &loopfilter_level);
      encoder->Control(VP8E_GET_LAST_QUANTIZER, &qp);
      ASSERT_EQ(rc_api_->GetQP(), qp);
      ASSERT_EQ(rc_api_->GetLoopfilterLevel(), loopfilter_level);
    }
  }
  // This method needs to be overridden because non-reference frames are
  // expected to be mismatched frames as the encoder will avoid loopfilter on
  // these frames.
  virtual void MismatchHook(const vpx_image_t * /*img1*/,
                            const vpx_image_t * /*img2*/) {}

  void RunSvc() {
    SetConfigSvc();
    rc_api_ = libvpx::VP9RateControlRTC::Create(rc_cfg_);
    SetEncoderSvc();

    ::libvpx_test::I420VideoSource video("desktop_office1.1280_720-020.yuv",
                                         1280, 720, 30, 1, 0, kNumFrames);

    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
  }

 private:
  vpx_codec_err_t ParseSuperframeSizes(const uint8_t *data, size_t data_sz) {
    uint8_t marker = *(data + data_sz - 1);
    if ((marker & 0xe0) == 0xc0) {
      const uint32_t frames = (marker & 0x7) + 1;
      const uint32_t mag = ((marker >> 3) & 0x3) + 1;
      const size_t index_sz = 2 + mag * frames;
      // This chunk is marked as having a superframe index but doesn't have
      // enough data for it, thus it's an invalid superframe index.
      if (data_sz < index_sz) return VPX_CODEC_CORRUPT_FRAME;
      {
        const uint8_t marker2 = *(data + data_sz - index_sz);
        // This chunk is marked as having a superframe index but doesn't have
        // the matching marker byte at the front of the index therefore it's an
        // invalid chunk.
        if (marker != marker2) return VPX_CODEC_CORRUPT_FRAME;
      }
      const uint8_t *x = &data[data_sz - index_sz + 1];
      for (uint32_t i = 0; i < frames; ++i) {
        uint32_t this_sz = 0;

        for (uint32_t j = 0; j < mag; ++j) this_sz |= (*x++) << (j * 8);
        sizes_[i] = this_sz;
      }
    }
    return VPX_CODEC_OK;
  }

  void SetEncoderSvc() {
    cfg_.ss_number_layers = 3;
    cfg_.ts_number_layers = 3;
    cfg_.g_timebase.num = 1;
    cfg_.g_timebase.den = 30;
    svc_params_.scaling_factor_num[0] = 72;
    svc_params_.scaling_factor_den[0] = 288;
    svc_params_.scaling_factor_num[1] = 144;
    svc_params_.scaling_factor_den[1] = 288;
    svc_params_.scaling_factor_num[2] = 288;
    svc_params_.scaling_factor_den[2] = 288;
    for (int i = 0; i < VPX_MAX_LAYERS; ++i) {
      svc_params_.max_quantizers[i] = 56;
      svc_params_.min_quantizers[i] = 2;
      svc_params_.speed_per_layer[i] = 7;
    }
    cfg_.rc_end_usage = VPX_CBR;
    cfg_.g_lag_in_frames = 0;
    cfg_.g_error_resilient = 0;
    // 3 temporal layers
    cfg_.ts_rate_decimator[0] = 4;
    cfg_.ts_rate_decimator[1] = 2;
    cfg_.ts_rate_decimator[2] = 1;
    cfg_.temporal_layering_mode = 3;

    cfg_.rc_buf_initial_sz = 500;
    cfg_.rc_buf_optimal_sz = 600;
    cfg_.rc_buf_sz = 1000;
    cfg_.rc_min_quantizer = 2;
    cfg_.rc_max_quantizer = 56;
    cfg_.g_threads = 1;
    cfg_.kf_max_dist = 9999;
    cfg_.rc_target_bitrate = 1600;
    cfg_.rc_overshoot_pct = 50;
    cfg_.rc_undershoot_pct = 50;

    cfg_.layer_target_bitrate[0] = 100;
    cfg_.layer_target_bitrate[1] = 140;
    cfg_.layer_target_bitrate[2] = 200;
    cfg_.layer_target_bitrate[3] = 250;
    cfg_.layer_target_bitrate[4] = 350;
    cfg_.layer_target_bitrate[5] = 500;
    cfg_.layer_target_bitrate[6] = 450;
    cfg_.layer_target_bitrate[7] = 630;
    cfg_.layer_target_bitrate[8] = 900;
  }

  void SetConfigSvc() {
    rc_cfg_.width = 1280;
    rc_cfg_.height = 720;
    rc_cfg_.max_quantizer = 56;
    rc_cfg_.min_quantizer = 2;
    rc_cfg_.target_bandwidth = 1600;
    rc_cfg_.buf_initial_sz = 500;
    rc_cfg_.buf_optimal_sz = 600;
    rc_cfg_.buf_sz = 1000;
    rc_cfg_.undershoot_pct = 50;
    rc_cfg_.overshoot_pct = 50;
    rc_cfg_.max_intra_bitrate_pct = 900;
    rc_cfg_.framerate = 30.0;
    rc_cfg_.ss_number_layers = 3;
    rc_cfg_.ts_number_layers = 3;
    rc_cfg_.rc_mode = VPX_CBR;
    rc_cfg_.aq_mode = aq_mode_;

    rc_cfg_.scaling_factor_num[0] = 1;
    rc_cfg_.scaling_factor_den[0] = 4;
    rc_cfg_.scaling_factor_num[1] = 2;
    rc_cfg_.scaling_factor_den[1] = 4;
    rc_cfg_.scaling_factor_num[2] = 4;
    rc_cfg_.scaling_factor_den[2] = 4;

    rc_cfg_.ts_rate_decimator[0] = 4;
    rc_cfg_.ts_rate_decimator[1] = 2;
    rc_cfg_.ts_rate_decimator[2] = 1;

    rc_cfg_.layer_target_bitrate[0] = 100;
    rc_cfg_.layer_target_bitrate[1] = 140;
    rc_cfg_.layer_target_bitrate[2] = 200;
    rc_cfg_.layer_target_bitrate[3] = 250;
    rc_cfg_.layer_target_bitrate[4] = 350;
    rc_cfg_.layer_target_bitrate[5] = 500;
    rc_cfg_.layer_target_bitrate[6] = 450;
    rc_cfg_.layer_target_bitrate[7] = 630;
    rc_cfg_.layer_target_bitrate[8] = 900;

    for (int sl = 0; sl < rc_cfg_.ss_number_layers; ++sl) {
      for (int tl = 0; tl < rc_cfg_.ts_number_layers; ++tl) {
        const int i = sl * rc_cfg_.ts_number_layers + tl;
        rc_cfg_.max_quantizers[i] = 56;
        rc_cfg_.min_quantizers[i] = 2;
      }
    }
  }

  int aq_mode_;
  std::unique_ptr<libvpx::VP9RateControlRTC> rc_api_;
  libvpx::VP9RateControlRtcConfig rc_cfg_;
  vpx_svc_extra_cfg_t svc_params_;
  libvpx::VP9FrameParamsQpRTC frame_params_;
  bool encoder_exit_;
  int current_superframe_;
  uint32_t sizes_[8];
};

TEST_P(RcInterfaceTest, OneLayer) { RunOneLayer(); }

TEST_P(RcInterfaceTest, OneLayerVBRPeriodicKey) { RunOneLayerVBRPeriodicKey(); }

TEST_P(RcInterfaceSvcTest, Svc) { RunSvc(); }

VP9_INSTANTIATE_TEST_SUITE(RcInterfaceTest, ::testing::Values(0, 3),
                           ::testing::Values(VPX_CBR, VPX_VBR));
VP9_INSTANTIATE_TEST_SUITE(RcInterfaceSvcTest, ::testing::Values(0, 3));
}  // namespace
