/*
 *  Copyright (c) 2016 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <climits>
#include <cstring>
#include <initializer_list>
#include <new>

#include "third_party/googletest/src/include/gtest/gtest.h"
#include "test/acm_random.h"
#include "test/codec_factory.h"
#include "test/encode_test_driver.h"
#include "test/i420_video_source.h"
#include "test/video_source.h"

#include "./vpx_config.h"
#include "vpx/vp8cx.h"
#include "vpx/vpx_codec.h"
#include "vpx/vpx_encoder.h"
#include "vpx/vpx_image.h"
#include "vpx/vpx_tpl.h"

namespace {

vpx_codec_iface_t *kCodecIfaces[] = {
#if CONFIG_VP8_ENCODER
  &vpx_codec_vp8_cx_algo,
#endif
#if CONFIG_VP9_ENCODER
  &vpx_codec_vp9_cx_algo,
#endif
};

bool IsVP9(vpx_codec_iface_t *iface) {
  static const char kVP9Name[] = "WebM Project VP9";
  return strncmp(kVP9Name, vpx_codec_iface_name(iface), sizeof(kVP9Name) - 1) ==
         0;
}

TEST(EncodeAPI, InvalidParams) {
  uint8_t buf[1] = { 0 };
  vpx_image_t img;
  vpx_codec_ctx_t enc;
  vpx_codec_enc_cfg_t cfg;

  EXPECT_EQ(&img, vpx_img_wrap(&img, VPX_IMG_FMT_I420, 1, 1, 1, buf));

  EXPECT_EQ(VPX_CODEC_INVALID_PARAM,
            vpx_codec_enc_init(nullptr, nullptr, nullptr, 0));
  EXPECT_EQ(VPX_CODEC_INVALID_PARAM,
            vpx_codec_enc_init(&enc, nullptr, nullptr, 0));
  EXPECT_EQ(VPX_CODEC_INVALID_PARAM,
            vpx_codec_encode(nullptr, nullptr, 0, 0, 0, 0));
  EXPECT_EQ(VPX_CODEC_INVALID_PARAM,
            vpx_codec_encode(nullptr, &img, 0, 0, 0, 0));
  EXPECT_EQ(VPX_CODEC_INVALID_PARAM, vpx_codec_destroy(nullptr));
  EXPECT_EQ(VPX_CODEC_INVALID_PARAM,
            vpx_codec_enc_config_default(nullptr, nullptr, 0));
  EXPECT_EQ(VPX_CODEC_INVALID_PARAM,
            vpx_codec_enc_config_default(nullptr, &cfg, 0));
  EXPECT_NE(vpx_codec_error(nullptr), nullptr);

  for (const auto *iface : kCodecIfaces) {
    SCOPED_TRACE(vpx_codec_iface_name(iface));
    EXPECT_EQ(VPX_CODEC_INVALID_PARAM,
              vpx_codec_enc_init(nullptr, iface, nullptr, 0));
    EXPECT_EQ(VPX_CODEC_INVALID_PARAM,
              vpx_codec_enc_init(&enc, iface, nullptr, 0));
    EXPECT_EQ(VPX_CODEC_INVALID_PARAM,
              vpx_codec_enc_config_default(iface, &cfg, 1));

    EXPECT_EQ(VPX_CODEC_OK, vpx_codec_enc_config_default(iface, &cfg, 0));
    EXPECT_EQ(VPX_CODEC_OK, vpx_codec_enc_init(&enc, iface, &cfg, 0));
    EXPECT_EQ(VPX_CODEC_OK, vpx_codec_encode(&enc, nullptr, 0, 0, 0, 0));

    EXPECT_EQ(VPX_CODEC_OK, vpx_codec_destroy(&enc));
  }
}

TEST(EncodeAPI, HighBitDepthCapability) {
// VP8 should not claim VP9 HBD as a capability.
#if CONFIG_VP8_ENCODER
  const vpx_codec_caps_t vp8_caps = vpx_codec_get_caps(&vpx_codec_vp8_cx_algo);
  EXPECT_EQ(vp8_caps & VPX_CODEC_CAP_HIGHBITDEPTH, 0);
#endif

#if CONFIG_VP9_ENCODER
  const vpx_codec_caps_t vp9_caps = vpx_codec_get_caps(&vpx_codec_vp9_cx_algo);
#if CONFIG_VP9_HIGHBITDEPTH
  EXPECT_EQ(vp9_caps & VPX_CODEC_CAP_HIGHBITDEPTH, VPX_CODEC_CAP_HIGHBITDEPTH);
#else
  EXPECT_EQ(vp9_caps & VPX_CODEC_CAP_HIGHBITDEPTH, 0);
#endif
#endif
}

#if CONFIG_VP8_ENCODER
TEST(EncodeAPI, ImageSizeSetting) {
  const int width = 711;
  const int height = 360;
  const int bps = 12;
  vpx_image_t img;
  vpx_codec_ctx_t enc;
  vpx_codec_enc_cfg_t cfg;
  uint8_t *img_buf = reinterpret_cast<uint8_t *>(
      calloc(width * height * bps / 8, sizeof(*img_buf)));
  vpx_codec_enc_config_default(vpx_codec_vp8_cx(), &cfg, 0);

  cfg.g_w = width;
  cfg.g_h = height;

  vpx_img_wrap(&img, VPX_IMG_FMT_I420, width, height, 1, img_buf);

  vpx_codec_enc_init(&enc, vpx_codec_vp8_cx(), &cfg, 0);

  EXPECT_EQ(VPX_CODEC_OK, vpx_codec_encode(&enc, &img, 0, 1, 0, 0));

  free(img_buf);

  vpx_codec_destroy(&enc);
}

// Verifies the fix for a float-cast-overflow in vp8_change_config().
//
// Causes cpi->framerate to become the largest possible value (10,000,000) in
// VP8 by setting cfg.g_timebase to 1/10000000 and passing a duration of 1 to
// vpx_codec_encode().
TEST(EncodeAPI, HugeFramerateVp8) {
  vpx_codec_iface_t *const iface = vpx_codec_vp8_cx();
  vpx_codec_enc_cfg_t cfg;
  ASSERT_EQ(vpx_codec_enc_config_default(iface, &cfg, 0), VPX_CODEC_OK);
  cfg.g_w = 271;
  cfg.g_h = 1080;
  cfg.g_timebase.num = 1;
  // Largest value (VP8's TICKS_PER_SEC) such that frame duration is nonzero (1
  // tick).
  cfg.g_timebase.den = 10000000;
  cfg.g_pass = VPX_RC_ONE_PASS;
  cfg.g_lag_in_frames = 0;
  cfg.rc_end_usage = VPX_CBR;

  vpx_codec_ctx_t enc;
  // Before we encode the first frame, cpi->framerate is set to a guess (the
  // reciprocal of cfg.g_timebase). If this guess doesn't seem reasonable
  // (> 180), cpi->framerate is set to 30.
  ASSERT_EQ(vpx_codec_enc_init(&enc, iface, &cfg, 0), VPX_CODEC_OK);

  ASSERT_EQ(vpx_codec_control(&enc, VP8E_SET_CPUUSED, -12), VPX_CODEC_OK);

  vpx_image_t *const image =
      vpx_img_alloc(nullptr, VPX_IMG_FMT_I420, cfg.g_w, cfg.g_h, 1);
  ASSERT_NE(image, nullptr);

  for (unsigned int i = 0; i < image->d_h; ++i) {
    memset(image->planes[0] + i * image->stride[0], 128, image->d_w);
  }
  const unsigned int uv_h = (image->d_h + 1) / 2;
  const unsigned int uv_w = (image->d_w + 1) / 2;
  for (unsigned int i = 0; i < uv_h; ++i) {
    memset(image->planes[1] + i * image->stride[1], 128, uv_w);
    memset(image->planes[2] + i * image->stride[2], 128, uv_w);
  }

  // Encode a frame.
  // Up to this point cpi->framerate is 30. Now pass a duration of only 1. This
  // causes cpi->framerate to become 10,000,000.
  ASSERT_EQ(vpx_codec_encode(&enc, image, 0, 1, 0, VPX_DL_REALTIME),
            VPX_CODEC_OK);

  // Change to the same config. Since cpi->framerate is now huge, when it is
  // used to calculate raw_target_rate (bit rate of uncompressed frames), the
  // result is likely to overflow an unsigned int.
  ASSERT_EQ(vpx_codec_enc_config_set(&enc, &cfg), VPX_CODEC_OK);

  vpx_img_free(image);
  ASSERT_EQ(vpx_codec_destroy(&enc), VPX_CODEC_OK);
}

// A test that reproduces https://crbug.com/webm/1831.
TEST(EncodeAPI, RandomPixelsVp8) {
  // Initialize libvpx encoder
  vpx_codec_iface_t *const iface = vpx_codec_vp8_cx();
  vpx_codec_enc_cfg_t cfg;
  ASSERT_EQ(vpx_codec_enc_config_default(iface, &cfg, 0), VPX_CODEC_OK);

  cfg.rc_target_bitrate = 2000;
  cfg.g_w = 1280;
  cfg.g_h = 720;

  vpx_codec_ctx_t enc;
  ASSERT_EQ(vpx_codec_enc_init(&enc, iface, &cfg, 0), VPX_CODEC_OK);

  // Generate random frame data and encode
  uint8_t img[1280 * 720 * 3 / 2];
  libvpx_test::ACMRandom rng;
  for (size_t i = 0; i < sizeof(img); ++i) {
    img[i] = rng.Rand8();
  }
  vpx_image_t img_wrapper;
  ASSERT_EQ(
      vpx_img_wrap(&img_wrapper, VPX_IMG_FMT_I420, cfg.g_w, cfg.g_h, 1, img),
      &img_wrapper);
  ASSERT_EQ(vpx_codec_encode(&enc, &img_wrapper, 0, 1, 0, VPX_DL_BEST_QUALITY),
            VPX_CODEC_OK);

  // Destroy libvpx encoder
  vpx_codec_destroy(&enc);
}
#endif

// Set up 2 spatial streams with 2 temporal layers per stream, and generate
// invalid configuration by setting the temporal layer rate allocation
// (ts_target_bitrate[]) to 0 for both layers. This should fail independent of
// CONFIG_MULTI_RES_ENCODING.
TEST(EncodeAPI, MultiResEncode) {
  const int width = 1280;
  const int height = 720;
  const int width_down = width / 2;
  const int height_down = height / 2;
  const int target_bitrate = 1000;
  const int framerate = 30;

  for (const auto *iface : kCodecIfaces) {
    vpx_codec_ctx_t enc[2];
    vpx_codec_enc_cfg_t cfg[2];
    vpx_rational_t dsf[2] = { { 2, 1 }, { 2, 1 } };

    memset(enc, 0, sizeof(enc));

    for (int i = 0; i < 2; i++) {
      vpx_codec_enc_config_default(iface, &cfg[i], 0);
    }

    /* Highest-resolution encoder settings */
    cfg[0].g_w = width;
    cfg[0].g_h = height;
    cfg[0].rc_dropframe_thresh = 0;
    cfg[0].rc_end_usage = VPX_CBR;
    cfg[0].rc_resize_allowed = 0;
    cfg[0].rc_min_quantizer = 2;
    cfg[0].rc_max_quantizer = 56;
    cfg[0].rc_undershoot_pct = 100;
    cfg[0].rc_overshoot_pct = 15;
    cfg[0].rc_buf_initial_sz = 500;
    cfg[0].rc_buf_optimal_sz = 600;
    cfg[0].rc_buf_sz = 1000;
    cfg[0].g_error_resilient = 1; /* Enable error resilient mode */
    cfg[0].g_lag_in_frames = 0;

    cfg[0].kf_mode = VPX_KF_AUTO;
    cfg[0].kf_min_dist = 3000;
    cfg[0].kf_max_dist = 3000;

    cfg[0].rc_target_bitrate = target_bitrate; /* Set target bitrate */
    cfg[0].g_timebase.num = 1;                 /* Set fps */
    cfg[0].g_timebase.den = framerate;

    memcpy(&cfg[1], &cfg[0], sizeof(cfg[0]));
    cfg[1].rc_target_bitrate = 500;
    cfg[1].g_w = width_down;
    cfg[1].g_h = height_down;

    for (int i = 0; i < 2; i++) {
      cfg[i].ts_number_layers = 2;
      cfg[i].ts_periodicity = 2;
      cfg[i].ts_rate_decimator[0] = 2;
      cfg[i].ts_rate_decimator[1] = 1;
      cfg[i].ts_layer_id[0] = 0;
      cfg[i].ts_layer_id[1] = 1;
      // Invalid parameters.
      cfg[i].ts_target_bitrate[0] = 0;
      cfg[i].ts_target_bitrate[1] = 0;
    }

    // VP9 should report incapable, VP8 invalid for all configurations.
    EXPECT_EQ(IsVP9(iface) ? VPX_CODEC_INCAPABLE : VPX_CODEC_INVALID_PARAM,
              vpx_codec_enc_init_multi(&enc[0], iface, &cfg[0], 2, 0, &dsf[0]));

    for (int i = 0; i < 2; i++) {
      vpx_codec_destroy(&enc[i]);
    }
  }
}

TEST(EncodeAPI, SetRoi) {
  static struct {
    vpx_codec_iface_t *iface;
    int ctrl_id;
  } kCodecs[] = {
#if CONFIG_VP8_ENCODER
    { &vpx_codec_vp8_cx_algo, VP8E_SET_ROI_MAP },
#endif
#if CONFIG_VP9_ENCODER
    { &vpx_codec_vp9_cx_algo, VP9E_SET_ROI_MAP },
#endif
  };
  constexpr int kWidth = 64;
  constexpr int kHeight = 64;

  for (const auto &codec : kCodecs) {
    SCOPED_TRACE(vpx_codec_iface_name(codec.iface));
    vpx_codec_ctx_t enc;
    vpx_codec_enc_cfg_t cfg;

    EXPECT_EQ(vpx_codec_enc_config_default(codec.iface, &cfg, 0), VPX_CODEC_OK);
    cfg.g_w = kWidth;
    cfg.g_h = kHeight;
    EXPECT_EQ(vpx_codec_enc_init(&enc, codec.iface, &cfg, 0), VPX_CODEC_OK);

    vpx_roi_map_t roi = {};
    uint8_t roi_map[kWidth * kHeight] = {};
    if (IsVP9(codec.iface)) {
      roi.rows = (cfg.g_w + 7) >> 3;
      roi.cols = (cfg.g_h + 7) >> 3;
    } else {
      roi.rows = (cfg.g_w + 15) >> 4;
      roi.cols = (cfg.g_h + 15) >> 4;
    }
    EXPECT_EQ(vpx_codec_control_(&enc, codec.ctrl_id, &roi), VPX_CODEC_OK);

    roi.roi_map = roi_map;
    // VP8 only. This value isn't range checked.
    roi.static_threshold[1] = 1000;
    roi.static_threshold[2] = UINT_MAX / 2 + 1;
    roi.static_threshold[3] = UINT_MAX;

    for (const auto delta : { -63, -1, 0, 1, 63 }) {
      for (int i = 0; i < 8; ++i) {
        roi.delta_q[i] = delta;
        roi.delta_lf[i] = delta;
        // VP9 only.
        roi.skip[i] ^= 1;
        roi.ref_frame[i] = (roi.ref_frame[i] + 1) % 4;
        EXPECT_EQ(vpx_codec_control_(&enc, codec.ctrl_id, &roi), VPX_CODEC_OK);
      }
    }

    vpx_codec_err_t expected_error;
    for (const auto delta : { -64, 64, INT_MIN, INT_MAX }) {
      expected_error = VPX_CODEC_INVALID_PARAM;
      for (int i = 0; i < 8; ++i) {
        roi.delta_q[i] = delta;
        // The max segment count for VP8 is 4, the remainder of the entries are
        // ignored.
        if (i >= 4 && !IsVP9(codec.iface)) expected_error = VPX_CODEC_OK;

        EXPECT_EQ(vpx_codec_control_(&enc, codec.ctrl_id, &roi), expected_error)
            << "delta_q[" << i << "]: " << delta;
        roi.delta_q[i] = 0;

        roi.delta_lf[i] = delta;
        EXPECT_EQ(vpx_codec_control_(&enc, codec.ctrl_id, &roi), expected_error)
            << "delta_lf[" << i << "]: " << delta;
        roi.delta_lf[i] = 0;
      }
    }

    // VP8 should ignore skip[] and ref_frame[] values.
    expected_error =
        IsVP9(codec.iface) ? VPX_CODEC_INVALID_PARAM : VPX_CODEC_OK;
    for (const auto skip : { -2, 2, INT_MIN, INT_MAX }) {
      for (int i = 0; i < 8; ++i) {
        roi.skip[i] = skip;
        EXPECT_EQ(vpx_codec_control_(&enc, codec.ctrl_id, &roi), expected_error)
            << "skip[" << i << "]: " << skip;
        roi.skip[i] = 0;
      }
    }

    // VP9 allows negative values to be used to disable segmentation.
    for (int ref_frame = -3; ref_frame < 0; ++ref_frame) {
      for (int i = 0; i < 8; ++i) {
        roi.ref_frame[i] = ref_frame;
        EXPECT_EQ(vpx_codec_control_(&enc, codec.ctrl_id, &roi), VPX_CODEC_OK)
            << "ref_frame[" << i << "]: " << ref_frame;
        roi.ref_frame[i] = 0;
      }
    }

    for (const auto ref_frame : { 4, INT_MIN, INT_MAX }) {
      for (int i = 0; i < 8; ++i) {
        roi.ref_frame[i] = ref_frame;
        EXPECT_EQ(vpx_codec_control_(&enc, codec.ctrl_id, &roi), expected_error)
            << "ref_frame[" << i << "]: " << ref_frame;
        roi.ref_frame[i] = 0;
      }
    }

    EXPECT_EQ(vpx_codec_destroy(&enc), VPX_CODEC_OK);
  }
}

void InitCodec(vpx_codec_iface_t &iface, int width, int height,
               vpx_codec_ctx_t *enc, vpx_codec_enc_cfg_t *cfg) {
  cfg->g_w = width;
  cfg->g_h = height;
  cfg->g_lag_in_frames = 0;
  cfg->g_pass = VPX_RC_ONE_PASS;
  ASSERT_EQ(vpx_codec_enc_init(enc, &iface, cfg, 0), VPX_CODEC_OK);

  ASSERT_EQ(vpx_codec_control_(enc, VP8E_SET_CPUUSED, 2), VPX_CODEC_OK);
}

// Encodes 1 frame of size |cfg.g_w| x |cfg.g_h| setting |enc|'s configuration
// to |cfg|.
void EncodeWithConfig(const vpx_codec_enc_cfg_t &cfg, vpx_codec_ctx_t *enc) {
  libvpx_test::DummyVideoSource video;
  video.SetSize(cfg.g_w, cfg.g_h);
  video.Begin();
  EXPECT_EQ(vpx_codec_enc_config_set(enc, &cfg), VPX_CODEC_OK)
      << vpx_codec_error_detail(enc);

  EXPECT_EQ(vpx_codec_encode(enc, video.img(), video.pts(), video.duration(),
                             /*flags=*/0, VPX_DL_GOOD_QUALITY),
            VPX_CODEC_OK)
      << vpx_codec_error_detail(enc);
}

TEST(EncodeAPI, ConfigChangeThreadCount) {
  constexpr int kWidth = 1920;
  constexpr int kHeight = 1080;

  for (const auto *iface : kCodecIfaces) {
    SCOPED_TRACE(vpx_codec_iface_name(iface));
    for (int i = 0; i < (IsVP9(iface) ? 2 : 1); ++i) {
      vpx_codec_enc_cfg_t cfg = {};
      struct Encoder {
        ~Encoder() { EXPECT_EQ(vpx_codec_destroy(&ctx), VPX_CODEC_OK); }
        vpx_codec_ctx_t ctx = {};
      } enc;

      ASSERT_EQ(vpx_codec_enc_config_default(iface, &cfg, 0), VPX_CODEC_OK);
      EXPECT_NO_FATAL_FAILURE(
          InitCodec(*iface, kWidth, kHeight, &enc.ctx, &cfg));
      if (IsVP9(iface)) {
        EXPECT_EQ(vpx_codec_control_(&enc.ctx, VP9E_SET_TILE_COLUMNS, 6),
                  VPX_CODEC_OK);
        EXPECT_EQ(vpx_codec_control_(&enc.ctx, VP9E_SET_ROW_MT, i),
                  VPX_CODEC_OK);
      }

      for (const auto threads : { 1, 4, 8, 6, 2, 1 }) {
        cfg.g_threads = threads;
        EXPECT_NO_FATAL_FAILURE(EncodeWithConfig(cfg, &enc.ctx))
            << "iteration: " << i << " threads: " << threads;
      }
    }
  }
}

TEST(EncodeAPI, ConfigResizeChangeThreadCount) {
  constexpr int kInitWidth = 1024;
  constexpr int kInitHeight = 1024;

  for (const auto *iface : kCodecIfaces) {
    SCOPED_TRACE(vpx_codec_iface_name(iface));
    for (int i = 0; i < (IsVP9(iface) ? 2 : 1); ++i) {
      vpx_codec_enc_cfg_t cfg = {};
      struct Encoder {
        ~Encoder() { EXPECT_EQ(vpx_codec_destroy(&ctx), VPX_CODEC_OK); }
        vpx_codec_ctx_t ctx = {};
      } enc;

      ASSERT_EQ(vpx_codec_enc_config_default(iface, &cfg, 0), VPX_CODEC_OK);
      // Start in threaded mode to ensure resolution and thread related
      // allocations are updated correctly across changes in resolution and
      // thread counts. See https://crbug.com/1486441.
      cfg.g_threads = 4;
      EXPECT_NO_FATAL_FAILURE(
          InitCodec(*iface, kInitWidth, kInitHeight, &enc.ctx, &cfg));
      if (IsVP9(iface)) {
        EXPECT_EQ(vpx_codec_control_(&enc.ctx, VP9E_SET_TILE_COLUMNS, 6),
                  VPX_CODEC_OK);
        EXPECT_EQ(vpx_codec_control_(&enc.ctx, VP9E_SET_ROW_MT, i),
                  VPX_CODEC_OK);
      }

      cfg.g_w = 1000;
      cfg.g_h = 608;
      EXPECT_EQ(vpx_codec_enc_config_set(&enc.ctx, &cfg), VPX_CODEC_OK)
          << vpx_codec_error_detail(&enc.ctx);

      cfg.g_w = 1000;
      cfg.g_h = 720;

      for (const auto threads : { 1, 4, 8, 6, 2, 1 }) {
        cfg.g_threads = threads;
        EXPECT_NO_FATAL_FAILURE(EncodeWithConfig(cfg, &enc.ctx))
            << "iteration: " << i << " threads: " << threads;
      }
    }
  }
}

#if CONFIG_VP9_ENCODER
// Frame size needed to trigger the overflow exceeds the max buffer allowed on
// 32-bit systems defined by VPX_MAX_ALLOCABLE_MEMORY
#if VPX_ARCH_X86_64 || VPX_ARCH_AARCH64
TEST(EncodeAPI, ConfigLargeTargetBitrateVp9) {
  constexpr int kWidth = 12383;
  constexpr int kHeight = 8192;
  constexpr auto *iface = &vpx_codec_vp9_cx_algo;
  SCOPED_TRACE(vpx_codec_iface_name(iface));
  vpx_codec_enc_cfg_t cfg = {};
  struct Encoder {
    ~Encoder() { EXPECT_EQ(vpx_codec_destroy(&ctx), VPX_CODEC_OK); }
    vpx_codec_ctx_t ctx = {};
  } enc;

  ASSERT_EQ(vpx_codec_enc_config_default(iface, &cfg, 0), VPX_CODEC_OK);
  // The following setting will cause avg_frame_bandwidth in rate control to be
  // larger than INT_MAX
  cfg.rc_target_bitrate = INT_MAX;
  // Framerate 0.1 (equivalent to timebase 10) is the smallest framerate allowed
  // by libvpx
  cfg.g_timebase.den = 1;
  cfg.g_timebase.num = 10;
  EXPECT_NO_FATAL_FAILURE(InitCodec(*iface, kWidth, kHeight, &enc.ctx, &cfg))
      << "target bitrate: " << cfg.rc_target_bitrate << " framerate: "
      << static_cast<double>(cfg.g_timebase.den) / cfg.g_timebase.num;
}
#endif  // VPX_ARCH_X86_64 || VPX_ARCH_AARCH64

vpx_image_t *CreateImage(const unsigned int width, const unsigned int height) {
  vpx_image_t *image =
      vpx_img_alloc(nullptr, VPX_IMG_FMT_I420, width, height, 1);
  if (!image) return image;

  for (unsigned int i = 0; i < image->d_h; ++i) {
    memset(image->planes[0] + i * image->stride[0], 128, image->d_w);
  }
  const unsigned int uv_h = (image->d_h + 1) / 2;
  const unsigned int uv_w = (image->d_w + 1) / 2;
  for (unsigned int i = 0; i < uv_h; ++i) {
    memset(image->planes[1] + i * image->stride[1], 128, uv_w);
    memset(image->planes[2] + i * image->stride[2], 128, uv_w);
  }

  return image;
}

// Emulates the WebCodecs VideoEncoder interface.
class VP9Encoder {
 public:
  explicit VP9Encoder(int speed) : speed_(speed) {}
  ~VP9Encoder();

  void Configure(unsigned int threads, unsigned int width, unsigned int height,
                 vpx_rc_mode end_usage, vpx_enc_deadline_t deadline);
  void Encode(bool key_frame);

 private:
  const int speed_;
  bool initialized_ = false;
  vpx_codec_enc_cfg_t cfg_;
  vpx_codec_ctx_t enc_;
  int frame_index_ = 0;
  vpx_enc_deadline_t deadline_ = 0;
};

VP9Encoder::~VP9Encoder() {
  if (initialized_) {
    EXPECT_EQ(vpx_codec_destroy(&enc_), VPX_CODEC_OK);
  }
}

void VP9Encoder::Configure(unsigned int threads, unsigned int width,
                           unsigned int height, vpx_rc_mode end_usage,
                           vpx_enc_deadline_t deadline) {
  deadline_ = deadline;

  if (!initialized_) {
    vpx_codec_iface_t *const iface = vpx_codec_vp9_cx();
    ASSERT_EQ(vpx_codec_enc_config_default(iface, &cfg_, /*usage=*/0),
              VPX_CODEC_OK);
    cfg_.g_threads = threads;
    cfg_.g_w = width;
    cfg_.g_h = height;
    cfg_.g_timebase.num = 1;
    cfg_.g_timebase.den = 1000 * 1000;  // microseconds
    cfg_.g_pass = VPX_RC_ONE_PASS;
    cfg_.g_lag_in_frames = 0;
    cfg_.rc_end_usage = end_usage;
    cfg_.rc_min_quantizer = 2;
    cfg_.rc_max_quantizer = 58;
    ASSERT_EQ(vpx_codec_enc_init(&enc_, iface, &cfg_, 0), VPX_CODEC_OK);
    ASSERT_EQ(vpx_codec_control(&enc_, VP8E_SET_CPUUSED, speed_), VPX_CODEC_OK);
    initialized_ = true;
    return;
  }

  cfg_.g_threads = threads;
  cfg_.g_w = width;
  cfg_.g_h = height;
  cfg_.rc_end_usage = end_usage;
  ASSERT_EQ(vpx_codec_enc_config_set(&enc_, &cfg_), VPX_CODEC_OK)
      << vpx_codec_error_detail(&enc_);
}

void VP9Encoder::Encode(bool key_frame) {
  const vpx_codec_cx_pkt_t *pkt;
  vpx_image_t *image = CreateImage(cfg_.g_w, cfg_.g_h);
  ASSERT_NE(image, nullptr);
  const vpx_enc_frame_flags_t frame_flags = key_frame ? VPX_EFLAG_FORCE_KF : 0;
  ASSERT_EQ(
      vpx_codec_encode(&enc_, image, frame_index_, 1, frame_flags, deadline_),
      VPX_CODEC_OK);
  frame_index_++;
  vpx_codec_iter_t iter = nullptr;
  while ((pkt = vpx_codec_get_cx_data(&enc_, &iter)) != nullptr) {
    ASSERT_EQ(pkt->kind, VPX_CODEC_CX_FRAME_PKT);
  }
  vpx_img_free(image);
}

// This is a test case from clusterfuzz.
TEST(EncodeAPI, PrevMiCheckNullptr) {
  VP9Encoder encoder(0);
  encoder.Configure(0, 1554, 644, VPX_VBR, VPX_DL_REALTIME);

  // First step: encode, without forcing KF.
  encoder.Encode(false);
  // Second step: change config
  encoder.Configure(0, 1131, 644, VPX_CBR, VPX_DL_GOOD_QUALITY);
  // Third step: encode, without forcing KF
  encoder.Encode(false);
}

// This is a test case from clusterfuzz: based on b/310477034.
// Encode a few frames with multiple change config calls
// with different frame sizes.
TEST(EncodeAPI, MultipleChangeConfigResize) {
  VP9Encoder encoder(3);

  // Set initial config.
  encoder.Configure(3, 41, 1, VPX_VBR, VPX_DL_REALTIME);

  // Encode first frame.
  encoder.Encode(true);

  // Change config.
  encoder.Configure(16, 31, 1, VPX_VBR, VPX_DL_GOOD_QUALITY);

  // Change config again.
  encoder.Configure(0, 17, 1, VPX_CBR, VPX_DL_REALTIME);

  // Encode 2nd frame with new config, set delta frame.
  encoder.Encode(false);

  // Encode 3rd frame with same config, set delta frame.
  encoder.Encode(false);
}

// This is a test case from clusterfuzz: based on b/310663186.
// Encode set of frames while varying the deadline on the fly from
// good to realtime to best and back to realtime.
TEST(EncodeAPI, DynamicDeadlineChange) {
  // Use realtime speed: 5 to 9.
  VP9Encoder encoder(5);

  // Set initial config, in particular set deadline to GOOD mode.
  encoder.Configure(0, 1, 1, VPX_VBR, VPX_DL_GOOD_QUALITY);

  // Encode 1st frame.
  encoder.Encode(true);

  // Encode 2nd frame, delta frame.
  encoder.Encode(false);

  // Change config: change deadline to REALTIME.
  encoder.Configure(0, 1, 1, VPX_VBR, VPX_DL_REALTIME);

  // Encode 3rd frame with new config, set key frame.
  encoder.Encode(true);

  // Encode 4th frame with same config, delta frame.
  encoder.Encode(false);

  // Encode 5th frame with same config, key frame.
  encoder.Encode(true);

  // Change config: change deadline to BEST.
  encoder.Configure(0, 1, 1, VPX_VBR, VPX_DL_BEST_QUALITY);

  // Encode 6th frame with new config, set delta frame.
  encoder.Encode(false);

  // Change config: change deadline to REALTIME.
  encoder.Configure(0, 1, 1, VPX_VBR, VPX_DL_REALTIME);

  // Encode 7th frame with new config, set delta frame.
  encoder.Encode(false);

  // Encode 8th frame with new config, set key frame.
  encoder.Encode(true);

  // Encode 9th frame with new config, set delta frame.
  encoder.Encode(false);
}

TEST(EncodeAPI, Buganizer310340241) {
  VP9Encoder encoder(-6);

  // Set initial config, in particular set deadline to GOOD mode.
  encoder.Configure(0, 1, 1, VPX_VBR, VPX_DL_GOOD_QUALITY);

  // Encode 1st frame.
  encoder.Encode(true);

  // Encode 2nd frame, delta frame.
  encoder.Encode(false);

  // Change config: change deadline to REALTIME.
  encoder.Configure(0, 1, 1, VPX_VBR, VPX_DL_REALTIME);

  // Encode 3rd frame with new config, set key frame.
  encoder.Encode(true);
}

// This is a test case from clusterfuzz: based on b/312517065.
TEST(EncodeAPI, Buganizer312517065) {
  VP9Encoder encoder(4);
  encoder.Configure(0, 1060, 437, VPX_CBR, VPX_DL_REALTIME);
  encoder.Encode(true);
  encoder.Configure(10, 33, 437, VPX_VBR, VPX_DL_GOOD_QUALITY);
  encoder.Encode(false);
  encoder.Configure(6, 327, 269, VPX_VBR, VPX_DL_GOOD_QUALITY);
  encoder.Configure(15, 1060, 437, VPX_CBR, VPX_DL_REALTIME);
  encoder.Encode(false);
}

// This is a test case from clusterfuzz: based on b/311489136.
// Encode a few frames with multiple change config calls
// with different frame sizes.
TEST(EncodeAPI, Buganizer311489136) {
  VP9Encoder encoder(1);

  // Set initial config.
  encoder.Configure(12, 1678, 620, VPX_VBR, VPX_DL_GOOD_QUALITY);

  // Encode first frame.
  encoder.Encode(true);

  // Change config.
  encoder.Configure(3, 1678, 202, VPX_CBR, VPX_DL_GOOD_QUALITY);

  // Encode 2nd frame with new config, set delta frame.
  encoder.Encode(false);

  // Change config again.
  encoder.Configure(8, 1037, 476, VPX_CBR, VPX_DL_REALTIME);

  // Encode 3rd frame with new config, set delta frame.
  encoder.Encode(false);

  // Change config again.
  encoder.Configure(0, 580, 620, VPX_CBR, VPX_DL_GOOD_QUALITY);

  // Encode 4th frame with same config, set delta frame.
  encoder.Encode(false);
}

// This is a test case from clusterfuzz: based on b/312656387.
// Encode a few frames with multiple change config calls
// with different frame sizes.
TEST(EncodeAPI, Buganizer312656387) {
  VP9Encoder encoder(1);

  // Set initial config.
  encoder.Configure(16, 1, 1024, VPX_CBR, VPX_DL_REALTIME);

  // Change config.
  encoder.Configure(15, 1, 1024, VPX_VBR, VPX_DL_REALTIME);

  // Encode first frame.
  encoder.Encode(true);

  // Change config again.
  encoder.Configure(14, 1, 595, VPX_VBR, VPX_DL_GOOD_QUALITY);

  // Encode 2nd frame with new config.
  encoder.Encode(true);

  // Change config again.
  encoder.Configure(2, 1, 1024, VPX_VBR, VPX_DL_GOOD_QUALITY);

  // Encode 3rd frame with new config, set delta frame.
  encoder.Encode(false);
}

// This is a test case from clusterfuzz: based on b/310329177.
// Encode a few frames with multiple change config calls
// with different frame sizes.
TEST(EncodeAPI, Buganizer310329177) {
  VP9Encoder encoder(6);

  // Set initial config.
  encoder.Configure(10, 41, 1, VPX_VBR, VPX_DL_REALTIME);

  // Encode first frame.
  encoder.Encode(true);

  // Change config.
  encoder.Configure(16, 1, 1, VPX_VBR, VPX_DL_REALTIME);

  // Encode 2nd frame with new config, set delta frame.
  encoder.Encode(false);
}

// This is a test case from clusterfuzz: based on b/311394513.
// Encode a few frames with multiple change config calls
// with different frame sizes.
TEST(EncodeAPI, Buganizer311394513) {
  VP9Encoder encoder(-7);

  // Set initial config.
  encoder.Configure(0, 5, 9, VPX_VBR, VPX_DL_REALTIME);

  // Encode first frame.
  encoder.Encode(false);

  // Change config.
  encoder.Configure(5, 2, 1, VPX_VBR, VPX_DL_REALTIME);

  // Encode 2nd frame with new config.
  encoder.Encode(true);
}

TEST(EncodeAPI, Buganizer311985118) {
  VP9Encoder encoder(0);

  // Set initial config, in particular set deadline to GOOD mode.
  encoder.Configure(12, 1678, 620, VPX_VBR, VPX_DL_GOOD_QUALITY);

  // Encode 1st frame.
  encoder.Encode(false);

  // Change config: change threads and width.
  encoder.Configure(0, 1574, 620, VPX_VBR, VPX_DL_GOOD_QUALITY);

  // Change config: change threads, width and height.
  encoder.Configure(16, 837, 432, VPX_VBR, VPX_DL_GOOD_QUALITY);

  // Encode 2nd frame.
  encoder.Encode(false);
}

// This is a test case from clusterfuzz: based on b/314857577.
// Encode a few frames with multiple change config calls
// with different frame sizes.
TEST(EncodeAPI, Buganizer314857577) {
  VP9Encoder encoder(4);

  // Set initial config.
  encoder.Configure(12, 1060, 437, VPX_VBR, VPX_DL_REALTIME);

  // Encode first frame.
  encoder.Encode(false);

  // Change config.
  encoder.Configure(16, 1060, 1, VPX_CBR, VPX_DL_REALTIME);

  // Encode 2nd frame with new config.
  encoder.Encode(false);

  // Encode 3rd frame with new config.
  encoder.Encode(true);

  // Change config.
  encoder.Configure(15, 33, 437, VPX_VBR, VPX_DL_GOOD_QUALITY);

  // Encode 4th frame with new config.
  encoder.Encode(true);

  // Encode 5th frame with new config.
  encoder.Encode(false);

  // Change config.
  encoder.Configure(5, 327, 269, VPX_VBR, VPX_DL_REALTIME);

  // Change config.
  encoder.Configure(15, 1060, 437, VPX_CBR, VPX_DL_REALTIME);

  // Encode 6th frame with new config.
  encoder.Encode(false);

  // Encode 7th frame with new config.
  encoder.Encode(false);

  // Change config.
  encoder.Configure(4, 1060, 437, VPX_VBR, VPX_DL_REALTIME);

  // Encode 8th frame with new config.
  encoder.Encode(false);
}

TEST(EncodeAPI, Buganizer312875957PredBufferStride) {
  VP9Encoder encoder(-1);

  encoder.Configure(12, 1678, 620, VPX_VBR, VPX_DL_REALTIME);
  encoder.Encode(true);
  encoder.Encode(false);
  encoder.Configure(0, 456, 486, VPX_VBR, VPX_DL_REALTIME);
  encoder.Encode(true);
  encoder.Configure(0, 1678, 620, VPX_CBR, 1000000);
  encoder.Encode(false);
  encoder.Encode(false);
}

// This is a test case from clusterfuzz: based on b/311294795
// Encode a few frames with multiple change config calls
// with different frame sizes.
TEST(EncodeAPI, Buganizer311294795) {
  VP9Encoder encoder(1);

  // Set initial config.
  encoder.Configure(12, 1678, 620, VPX_VBR, VPX_DL_REALTIME);

  // Encode first frame.
  encoder.Encode(false);

  // Change config.
  encoder.Configure(16, 632, 620, VPX_VBR, VPX_DL_GOOD_QUALITY);

  // Encode 2nd frame with new config
  encoder.Encode(true);

  // Change config.
  encoder.Configure(16, 1678, 342, VPX_VBR, VPX_DL_GOOD_QUALITY);

  // Encode 3rd frame with new config.
  encoder.Encode(false);

  // Change config.
  encoder.Configure(0, 1574, 618, VPX_VBR, VPX_DL_REALTIME);
  // Encode more frames with new config.
  encoder.Encode(false);
  encoder.Encode(false);
}
#endif  // CONFIG_VP9_ENCODER

}  // namespace
