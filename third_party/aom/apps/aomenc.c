/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include "apps/aomenc.h"

#include "config/aom_config.h"

#include <assert.h>
#include <limits.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if CONFIG_AV1_DECODER
#include "aom/aom_decoder.h"
#include "aom/aomdx.h"
#endif

#include "aom/aom_encoder.h"
#include "aom/aom_integer.h"
#include "aom/aomcx.h"
#include "aom_dsp/aom_dsp_common.h"
#include "aom_ports/aom_timer.h"
#include "aom_ports/mem_ops.h"
#include "common/args.h"
#include "common/ivfenc.h"
#include "common/tools_common.h"
#include "common/warnings.h"

#if CONFIG_WEBM_IO
#include "common/webmenc.h"
#endif

#include "common/y4minput.h"
#include "examples/encoder_util.h"
#include "stats/aomstats.h"
#include "stats/rate_hist.h"

#if CONFIG_LIBYUV
#include "third_party/libyuv/include/libyuv/scale.h"
#endif

/* Swallow warnings about unused results of fread/fwrite */
static size_t wrap_fread(void *ptr, size_t size, size_t nmemb, FILE *stream) {
  return fread(ptr, size, nmemb, stream);
}
#define fread wrap_fread

static size_t wrap_fwrite(const void *ptr, size_t size, size_t nmemb,
                          FILE *stream) {
  return fwrite(ptr, size, nmemb, stream);
}
#define fwrite wrap_fwrite

static const char *exec_name;

static void warn_or_exit_on_errorv(aom_codec_ctx_t *ctx, int fatal,
                                   const char *s, va_list ap) {
  if (ctx->err) {
    const char *detail = aom_codec_error_detail(ctx);

    vfprintf(stderr, s, ap);
    fprintf(stderr, ": %s\n", aom_codec_error(ctx));

    if (detail) fprintf(stderr, "    %s\n", detail);

    if (fatal) exit(EXIT_FAILURE);
  }
}

static void ctx_exit_on_error(aom_codec_ctx_t *ctx, const char *s, ...) {
  va_list ap;

  va_start(ap, s);
  warn_or_exit_on_errorv(ctx, 1, s, ap);
  va_end(ap);
}

static void warn_or_exit_on_error(aom_codec_ctx_t *ctx, int fatal,
                                  const char *s, ...) {
  va_list ap;

  va_start(ap, s);
  warn_or_exit_on_errorv(ctx, fatal, s, ap);
  va_end(ap);
}

static int read_frame(struct AvxInputContext *input_ctx, aom_image_t *img) {
  FILE *f = input_ctx->file;
  y4m_input *y4m = &input_ctx->y4m;
  int shortread = 0;

  if (input_ctx->file_type == FILE_TYPE_Y4M) {
    if (y4m_input_fetch_frame(y4m, f, img) < 1) return 0;
  } else {
    shortread = read_yuv_frame(input_ctx, img);
  }

  return !shortread;
}

static int file_is_y4m(const char detect[4]) {
  if (memcmp(detect, "YUV4", 4) == 0) {
    return 1;
  }
  return 0;
}

static int fourcc_is_ivf(const char detect[4]) {
  if (memcmp(detect, "DKIF", 4) == 0) {
    return 1;
  }
  return 0;
}

static const arg_def_t help =
    ARG_DEF(NULL, "help", 0, "Show usage options and exit");
static const arg_def_t debugmode =
    ARG_DEF("D", "debug", 0, "Debug mode (makes output deterministic)");
static const arg_def_t outputfile =
    ARG_DEF("o", "output", 1, "Output filename");
static const arg_def_t use_yv12 =
    ARG_DEF(NULL, "yv12", 0, "Input file is YV12 ");
static const arg_def_t use_i420 =
    ARG_DEF(NULL, "i420", 0, "Input file is I420 (default)");
static const arg_def_t use_i422 =
    ARG_DEF(NULL, "i422", 0, "Input file is I422");
static const arg_def_t use_i444 =
    ARG_DEF(NULL, "i444", 0, "Input file is I444");
static const arg_def_t codecarg = ARG_DEF(NULL, "codec", 1, "Codec to use");
static const arg_def_t passes =
    ARG_DEF("p", "passes", 1, "Number of passes (1/2)");
static const arg_def_t pass_arg =
    ARG_DEF(NULL, "pass", 1, "Pass to execute (1/2)");
static const arg_def_t fpf_name =
    ARG_DEF(NULL, "fpf", 1, "First pass statistics file name");
#if CONFIG_FP_MB_STATS
static const arg_def_t fpmbf_name =
    ARG_DEF(NULL, "fpmbf", 1, "First pass block statistics file name");
#endif
static const arg_def_t limit =
    ARG_DEF(NULL, "limit", 1, "Stop encoding after n input frames");
static const arg_def_t skip =
    ARG_DEF(NULL, "skip", 1, "Skip the first n input frames");
static const arg_def_t good_dl =
    ARG_DEF(NULL, "good", 0, "Use Good Quality Deadline");
static const arg_def_t quietarg =
    ARG_DEF("q", "quiet", 0, "Do not print encode progress");
static const arg_def_t verbosearg =
    ARG_DEF("v", "verbose", 0, "Show encoder parameters");
static const arg_def_t psnrarg =
    ARG_DEF(NULL, "psnr", 0, "Show PSNR in status line");
#if CONFIG_FILEOPTIONS
static const arg_def_t use_cfg = ARG_DEF("c", "cfg", 1, "Config file to use");
static const arg_def_t ext_partition =
    ARG_DEF(NULL, "ext-partition", 1, "corresponds to extended partitions");
#endif

static const struct arg_enum_list test_decode_enum[] = {
  { "off", TEST_DECODE_OFF },
  { "fatal", TEST_DECODE_FATAL },
  { "warn", TEST_DECODE_WARN },
  { NULL, 0 }
};
static const arg_def_t recontest = ARG_DEF_ENUM(
    NULL, "test-decode", 1, "Test encode/decode mismatch", test_decode_enum);
static const arg_def_t framerate =
    ARG_DEF(NULL, "fps", 1, "Stream frame rate (rate/scale)");
static const arg_def_t use_webm =
    ARG_DEF(NULL, "webm", 0, "Output WebM (default when WebM IO is enabled)");
static const arg_def_t use_ivf = ARG_DEF(NULL, "ivf", 0, "Output IVF");
static const arg_def_t use_obu = ARG_DEF(NULL, "obu", 0, "Output OBU");
static const arg_def_t out_part =
    ARG_DEF("P", "output-partitions", 0,
            "Makes encoder output partitions. Requires IVF output!");
static const arg_def_t q_hist_n =
    ARG_DEF(NULL, "q-hist", 1, "Show quantizer histogram (n-buckets)");
static const arg_def_t rate_hist_n =
    ARG_DEF(NULL, "rate-hist", 1, "Show rate histogram (n-buckets)");
static const arg_def_t disable_warnings =
    ARG_DEF(NULL, "disable-warnings", 0,
            "Disable warnings about potentially incorrect encode settings.");
static const arg_def_t disable_warning_prompt =
    ARG_DEF("y", "disable-warning-prompt", 0,
            "Display warnings, but do not prompt user to continue.");
static const struct arg_enum_list bitdepth_enum[] = {
  { "8", AOM_BITS_8 }, { "10", AOM_BITS_10 }, { "12", AOM_BITS_12 }, { NULL, 0 }
};

static const arg_def_t bitdeptharg = ARG_DEF_ENUM(
    "b", "bit-depth", 1,
    "Bit depth for codec (8 for version <=1, 10 or 12 for version 2)",
    bitdepth_enum);
static const arg_def_t inbitdeptharg =
    ARG_DEF(NULL, "input-bit-depth", 1, "Bit depth of input");
static const arg_def_t *main_args[] = { &help,
#if CONFIG_FILEOPTIONS
                                        &use_cfg,
#endif
                                        &debugmode,
                                        &outputfile,
                                        &codecarg,
                                        &passes,
                                        &pass_arg,
                                        &fpf_name,
                                        &limit,
                                        &skip,
                                        &good_dl,
                                        &quietarg,
                                        &verbosearg,
                                        &psnrarg,
                                        &use_webm,
                                        &use_ivf,
                                        &use_obu,
                                        &out_part,
                                        &q_hist_n,
                                        &rate_hist_n,
                                        &disable_warnings,
                                        &disable_warning_prompt,
                                        &recontest,
                                        NULL };

static const arg_def_t usage =
    ARG_DEF("u", "usage", 1, "Usage profile number to use");
static const arg_def_t threads =
    ARG_DEF("t", "threads", 1, "Max number of threads to use");
static const arg_def_t profile =
    ARG_DEF(NULL, "profile", 1, "Bitstream profile number to use");
static const arg_def_t width = ARG_DEF("w", "width", 1, "Frame width");
static const arg_def_t height = ARG_DEF("h", "height", 1, "Frame height");
static const arg_def_t forced_max_frame_width = ARG_DEF(
    NULL, "forced_max_frame_width", 0, "Maximum frame width value to force");
static const arg_def_t forced_max_frame_height = ARG_DEF(
    NULL, "forced_max_frame_height", 0, "Maximum frame height value to force");
#if CONFIG_WEBM_IO
static const struct arg_enum_list stereo_mode_enum[] = {
  { "mono", STEREO_FORMAT_MONO },
  { "left-right", STEREO_FORMAT_LEFT_RIGHT },
  { "bottom-top", STEREO_FORMAT_BOTTOM_TOP },
  { "top-bottom", STEREO_FORMAT_TOP_BOTTOM },
  { "right-left", STEREO_FORMAT_RIGHT_LEFT },
  { NULL, 0 }
};
static const arg_def_t stereo_mode = ARG_DEF_ENUM(
    NULL, "stereo-mode", 1, "Stereo 3D video format", stereo_mode_enum);
#endif
static const arg_def_t timebase = ARG_DEF(
    NULL, "timebase", 1, "Output timestamp precision (fractional seconds)");
static const arg_def_t global_error_resilient =
    ARG_DEF(NULL, "global-error-resilient", 1,
            "Enable global error resiliency features");
static const arg_def_t lag_in_frames =
    ARG_DEF(NULL, "lag-in-frames", 1, "Max number of frames to lag");
static const arg_def_t large_scale_tile =
    ARG_DEF(NULL, "large-scale-tile", 1,
            "Large scale tile coding (0: off (default), 1: on)");
static const arg_def_t monochrome =
    ARG_DEF(NULL, "monochrome", 0, "Monochrome video (no chroma planes)");
static const arg_def_t full_still_picture_hdr = ARG_DEF(
    NULL, "full-still-picture-hdr", 0, "Use full header for still picture");

static const arg_def_t *global_args[] = { &use_yv12,
                                          &use_i420,
                                          &use_i422,
                                          &use_i444,
                                          &usage,
                                          &threads,
                                          &profile,
                                          &width,
                                          &height,
                                          &forced_max_frame_width,
                                          &forced_max_frame_height,
#if CONFIG_WEBM_IO
                                          &stereo_mode,
#endif
                                          &timebase,
                                          &framerate,
                                          &global_error_resilient,
                                          &bitdeptharg,
                                          &lag_in_frames,
                                          &large_scale_tile,
                                          &monochrome,
                                          &full_still_picture_hdr,
                                          NULL };

static const arg_def_t dropframe_thresh =
    ARG_DEF(NULL, "drop-frame", 1, "Temporal resampling threshold (buf %)");
static const arg_def_t resize_mode =
    ARG_DEF(NULL, "resize-mode", 1, "Frame resize mode");
static const arg_def_t resize_denominator =
    ARG_DEF(NULL, "resize-denominator", 1, "Frame resize denominator");
static const arg_def_t resize_kf_denominator = ARG_DEF(
    NULL, "resize-kf-denominator", 1, "Frame resize keyframe denominator");
static const arg_def_t superres_mode =
    ARG_DEF(NULL, "superres-mode", 1, "Frame super-resolution mode");
static const arg_def_t superres_denominator = ARG_DEF(
    NULL, "superres-denominator", 1, "Frame super-resolution denominator");
static const arg_def_t superres_kf_denominator =
    ARG_DEF(NULL, "superres-kf-denominator", 1,
            "Frame super-resolution keyframe denominator");
static const arg_def_t superres_qthresh = ARG_DEF(
    NULL, "superres-qthresh", 1, "Frame super-resolution qindex threshold");
static const arg_def_t superres_kf_qthresh =
    ARG_DEF(NULL, "superres-kf-qthresh", 1,
            "Frame super-resolution keyframe qindex threshold");
static const struct arg_enum_list end_usage_enum[] = { { "vbr", AOM_VBR },
                                                       { "cbr", AOM_CBR },
                                                       { "cq", AOM_CQ },
                                                       { "q", AOM_Q },
                                                       { NULL, 0 } };
static const arg_def_t end_usage =
    ARG_DEF_ENUM(NULL, "end-usage", 1, "Rate control mode", end_usage_enum);
static const arg_def_t target_bitrate =
    ARG_DEF(NULL, "target-bitrate", 1, "Bitrate (kbps)");
static const arg_def_t min_quantizer =
    ARG_DEF(NULL, "min-q", 1, "Minimum (best) quantizer");
static const arg_def_t max_quantizer =
    ARG_DEF(NULL, "max-q", 1, "Maximum (worst) quantizer");
static const arg_def_t undershoot_pct =
    ARG_DEF(NULL, "undershoot-pct", 1, "Datarate undershoot (min) target (%)");
static const arg_def_t overshoot_pct =
    ARG_DEF(NULL, "overshoot-pct", 1, "Datarate overshoot (max) target (%)");
static const arg_def_t buf_sz =
    ARG_DEF(NULL, "buf-sz", 1, "Client buffer size (ms)");
static const arg_def_t buf_initial_sz =
    ARG_DEF(NULL, "buf-initial-sz", 1, "Client initial buffer size (ms)");
static const arg_def_t buf_optimal_sz =
    ARG_DEF(NULL, "buf-optimal-sz", 1, "Client optimal buffer size (ms)");
static const arg_def_t *rc_args[] = { &dropframe_thresh,
                                      &resize_mode,
                                      &resize_denominator,
                                      &resize_kf_denominator,
                                      &superres_mode,
                                      &superres_denominator,
                                      &superres_kf_denominator,
                                      &superres_qthresh,
                                      &superres_kf_qthresh,
                                      &end_usage,
                                      &target_bitrate,
                                      &min_quantizer,
                                      &max_quantizer,
                                      &undershoot_pct,
                                      &overshoot_pct,
                                      &buf_sz,
                                      &buf_initial_sz,
                                      &buf_optimal_sz,
                                      NULL };

static const arg_def_t bias_pct =
    ARG_DEF(NULL, "bias-pct", 1, "CBR/VBR bias (0=CBR, 100=VBR)");
static const arg_def_t minsection_pct =
    ARG_DEF(NULL, "minsection-pct", 1, "GOP min bitrate (% of target)");
static const arg_def_t maxsection_pct =
    ARG_DEF(NULL, "maxsection-pct", 1, "GOP max bitrate (% of target)");
static const arg_def_t *rc_twopass_args[] = { &bias_pct, &minsection_pct,
                                              &maxsection_pct, NULL };
static const arg_def_t fwd_kf_enabled =
    ARG_DEF(NULL, "enable-fwd-kf", 1, "Enable forward reference keyframes");
static const arg_def_t kf_min_dist =
    ARG_DEF(NULL, "kf-min-dist", 1, "Minimum keyframe interval (frames)");
static const arg_def_t kf_max_dist =
    ARG_DEF(NULL, "kf-max-dist", 1, "Maximum keyframe interval (frames)");
static const arg_def_t kf_disabled =
    ARG_DEF(NULL, "disable-kf", 0, "Disable keyframe placement");
static const arg_def_t *kf_args[] = { &fwd_kf_enabled, &kf_min_dist,
                                      &kf_max_dist, &kf_disabled, NULL };
static const arg_def_t sframe_dist =
    ARG_DEF(NULL, "sframe-dist", 1, "S-Frame interval (frames)");
static const arg_def_t sframe_mode =
    ARG_DEF(NULL, "sframe-mode", 1, "S-Frame insertion mode (1..2)");
static const arg_def_t save_as_annexb =
    ARG_DEF(NULL, "annexb", 1, "Save as Annex-B");
static const arg_def_t noise_sens =
    ARG_DEF(NULL, "noise-sensitivity", 1, "Noise sensitivity (frames to blur)");
static const arg_def_t sharpness =
    ARG_DEF(NULL, "sharpness", 1, "Loop filter sharpness (0..7)");
static const arg_def_t static_thresh =
    ARG_DEF(NULL, "static-thresh", 1, "Motion detection threshold");
static const arg_def_t auto_altref =
    ARG_DEF(NULL, "auto-alt-ref", 1, "Enable automatic alt reference frames");
static const arg_def_t arnr_maxframes =
    ARG_DEF(NULL, "arnr-maxframes", 1, "AltRef max frames (0..15)");
static const arg_def_t arnr_strength =
    ARG_DEF(NULL, "arnr-strength", 1, "AltRef filter strength (0..6)");
static const struct arg_enum_list tuning_enum[] = {
  { "psnr", AOM_TUNE_PSNR },
  { "ssim", AOM_TUNE_SSIM },
#ifdef CONFIG_DIST_8X8
  { "cdef-dist", AOM_TUNE_CDEF_DIST },
  { "daala-dist", AOM_TUNE_DAALA_DIST },
#endif
  { NULL, 0 }
};
static const arg_def_t tune_metric =
    ARG_DEF_ENUM(NULL, "tune", 1, "Distortion metric tuned with", tuning_enum);
static const arg_def_t cq_level =
    ARG_DEF(NULL, "cq-level", 1, "Constant/Constrained Quality level");
static const arg_def_t max_intra_rate_pct =
    ARG_DEF(NULL, "max-intra-rate", 1, "Max I-frame bitrate (pct)");

#if CONFIG_AV1_ENCODER
static const arg_def_t cpu_used_av1 =
    ARG_DEF(NULL, "cpu-used", 1, "CPU Used (0..8)");
static const arg_def_t dev_sf_av1 =
    ARG_DEF(NULL, "dev-sf", 1, "Dev Speed (0..255)");
static const arg_def_t single_tile_decoding =
    ARG_DEF(NULL, "single-tile-decoding", 1,
            "Single tile decoding (0: off (default), 1: on)");
static const arg_def_t tile_cols =
    ARG_DEF(NULL, "tile-columns", 1, "Number of tile columns to use, log2");
static const arg_def_t tile_rows =
    ARG_DEF(NULL, "tile-rows", 1,
            "Number of tile rows to use, log2 (set to 0 while threads > 1)");
static const arg_def_t tile_width =
    ARG_DEF(NULL, "tile-width", 1, "Tile widths (comma separated)");
static const arg_def_t tile_height =
    ARG_DEF(NULL, "tile-height", 1, "Tile heights (command separated)");
static const arg_def_t lossless =
    ARG_DEF(NULL, "lossless", 1, "Lossless mode (0: false (default), 1: true)");
static const arg_def_t enable_cdef =
    ARG_DEF(NULL, "enable-cdef", 1,
            "Enable the constrained directional enhancement filter (0: false, "
            "1: true (default))");
static const arg_def_t enable_restoration =
    ARG_DEF(NULL, "enable-restoration", 1,
            "Enable the loop restoration filter (0: false, "
            "1: true (default))");
static const arg_def_t disable_trellis_quant =
    ARG_DEF(NULL, "disable-trellis-quant", 1,
            "Disable trellis optimization of quantized coefficients (0: false ("
            "default) 1: true)");
static const arg_def_t enable_qm =
    ARG_DEF(NULL, "enable-qm", 1,
            "Enable quantisation matrices (0: false (default), 1: true)");
static const arg_def_t qm_min = ARG_DEF(
    NULL, "qm-min", 1, "Min quant matrix flatness (0..15), default is 8");
static const arg_def_t qm_max = ARG_DEF(
    NULL, "qm-max", 1, "Max quant matrix flatness (0..15), default is 15");
#if CONFIG_DIST_8X8
static const arg_def_t enable_dist_8x8 =
    ARG_DEF(NULL, "enable-dist-8x8", 1,
            "Enable dist-8x8 (0: false (default), 1: true)");
#endif  // CONFIG_DIST_8X8
static const arg_def_t num_tg = ARG_DEF(
    NULL, "num-tile-groups", 1, "Maximum number of tile groups, default is 1");
static const arg_def_t mtu_size =
    ARG_DEF(NULL, "mtu-size", 1,
            "MTU size for a tile group, default is 0 (no MTU targeting), "
            "overrides maximum number of tile groups");
static const struct arg_enum_list timing_info_enum[] = {
  { "unspecified", AOM_TIMING_UNSPECIFIED },
  { "constant", AOM_TIMING_EQUAL },
  { "model", AOM_TIMING_DEC_MODEL },
  { NULL, 0 }
};
static const arg_def_t timing_info =
    ARG_DEF_ENUM(NULL, "timing-info", 1,
                 "Signal timing info in the bitstream (model unly works for no "
                 "hidden frames, no super-res yet):",
                 timing_info_enum);
static const arg_def_t film_grain_test =
    ARG_DEF(NULL, "film-grain-test", 1,
            "Film grain test vectors (0: none (default), 1: test-1  2: test-2, "
            "... 16: test-16)");
static const arg_def_t film_grain_table =
    ARG_DEF(NULL, "film-grain-table", 1,
            "Path to file containing film grain parameters");
static const arg_def_t enable_ref_frame_mvs =
    ARG_DEF(NULL, "enable-ref-frame-mvs", 1,
            "Enable temporal mv prediction (default is 1)");
static const arg_def_t frame_parallel_decoding =
    ARG_DEF(NULL, "frame-parallel", 1,
            "Enable frame parallel decodability features "
            "(0: false (default), 1: true)");
static const arg_def_t error_resilient_mode =
    ARG_DEF(NULL, "error-resilient", 1,
            "Enable error resilient features "
            "(0: false (default), 1: true)");
static const arg_def_t aq_mode = ARG_DEF(
    NULL, "aq-mode", 1,
    "Adaptive quantization mode (0: off (default), 1: variance 2: complexity, "
    "3: cyclic refresh)");
static const arg_def_t deltaq_mode = ARG_DEF(
    NULL, "deltaq-mode", 1,
    "Delta qindex mode (0: off (default), 1: deltaq 2: deltaq + deltalf)");
static const arg_def_t frame_periodic_boost =
    ARG_DEF(NULL, "frame-boost", 1,
            "Enable frame periodic boost (0: off (default), 1: on)");
static const arg_def_t gf_cbr_boost_pct = ARG_DEF(
    NULL, "gf-cbr-boost", 1, "Boost for Golden Frame in CBR mode (pct)");
static const arg_def_t max_inter_rate_pct =
    ARG_DEF(NULL, "max-inter-rate", 1, "Max P-frame bitrate (pct)");
static const arg_def_t min_gf_interval = ARG_DEF(
    NULL, "min-gf-interval", 1,
    "min gf/arf frame interval (default 0, indicating in-built behavior)");
static const arg_def_t max_gf_interval = ARG_DEF(
    NULL, "max-gf-interval", 1,
    "max gf/arf frame interval (default 0, indicating in-built behavior)");

static const struct arg_enum_list color_primaries_enum[] = {
  { "bt709", AOM_CICP_CP_BT_709 },
  { "unspecified", AOM_CICP_CP_UNSPECIFIED },
  { "bt601", AOM_CICP_CP_BT_601 },
  { "bt470m", AOM_CICP_CP_BT_470_M },
  { "bt470bg", AOM_CICP_CP_BT_470_B_G },
  { "smpte240", AOM_CICP_CP_SMPTE_240 },
  { "film", AOM_CICP_CP_GENERIC_FILM },
  { "bt2020", AOM_CICP_CP_BT_2020 },
  { "xyz", AOM_CICP_CP_XYZ },
  { "smpte431", AOM_CICP_CP_SMPTE_431 },
  { "smpte432", AOM_CICP_CP_SMPTE_432 },
  { "ebu3213", AOM_CICP_CP_EBU_3213 },
  { NULL, 0 }
};

static const arg_def_t input_color_primaries = ARG_DEF_ENUM(
    NULL, "color-primaries", 1,
    "Color primaries (CICP) of input content:", color_primaries_enum);

static const struct arg_enum_list transfer_characteristics_enum[] = {
  { "unspecified", AOM_CICP_CP_UNSPECIFIED },
  { "bt709", AOM_CICP_TC_BT_709 },
  { "bt470m", AOM_CICP_TC_BT_470_M },
  { "bt470bg", AOM_CICP_TC_BT_470_B_G },
  { "bt601", AOM_CICP_TC_BT_601 },
  { "smpte240", AOM_CICP_TC_SMPTE_240 },
  { "lin", AOM_CICP_TC_LINEAR },
  { "log100", AOM_CICP_TC_LOG_100 },
  { "log100sq10", AOM_CICP_TC_LOG_100_SQRT10 },
  { "iec61966", AOM_CICP_TC_IEC_61966 },
  { "bt1361", AOM_CICP_TC_BT_1361 },
  { "srgb", AOM_CICP_TC_SRGB },
  { "bt2020-10bit", AOM_CICP_TC_BT_2020_10_BIT },
  { "bt2020-12bit", AOM_CICP_TC_BT_2020_12_BIT },
  { "smpte2084", AOM_CICP_TC_SMPTE_2084 },
  { "hlg", AOM_CICP_TC_HLG },
  { "smpte428", AOM_CICP_TC_SMPTE_428 },
  { NULL, 0 }
};

static const arg_def_t input_transfer_characteristics =
    ARG_DEF_ENUM(NULL, "transfer-characteristics", 1,
                 "Transfer characteristics (CICP) of input content:",
                 transfer_characteristics_enum);

static const struct arg_enum_list matrix_coefficients_enum[] = {
  { "identity", AOM_CICP_MC_IDENTITY },
  { "bt709", AOM_CICP_MC_BT_709 },
  { "unspecified", AOM_CICP_MC_UNSPECIFIED },
  { "fcc73", AOM_CICP_MC_FCC },
  { "bt470bg", AOM_CICP_MC_BT_470_B_G },
  { "bt601", AOM_CICP_MC_BT_601 },
  { "smpte240", AOM_CICP_CP_SMPTE_240 },
  { "ycgco", AOM_CICP_MC_SMPTE_YCGCO },
  { "bt2020ncl", AOM_CICP_MC_BT_2020_NCL },
  { "bt2020cl", AOM_CICP_MC_BT_2020_CL },
  { "smpte2085", AOM_CICP_MC_SMPTE_2085 },
  { "chromncl", AOM_CICP_MC_CHROMAT_NCL },
  { "chromcl", AOM_CICP_MC_CHROMAT_CL },
  { "ictcp", AOM_CICP_MC_ICTCP },
  { NULL, 0 }
};

static const arg_def_t input_matrix_coefficients = ARG_DEF_ENUM(
    NULL, "matrix-coefficients", 1,
    "Matrix coefficients (CICP) of input content:", matrix_coefficients_enum);

static const struct arg_enum_list chroma_sample_position_enum[] = {
  { "unknown", AOM_CSP_UNKNOWN },
  { "vertical", AOM_CSP_VERTICAL },
  { "colocated", AOM_CSP_COLOCATED },
  { NULL, 0 }
};

static const arg_def_t input_chroma_sample_position =
    ARG_DEF_ENUM(NULL, "chroma-sample-position", 1,
                 "The chroma sample position when chroma 4:2:0 is signaled:",
                 chroma_sample_position_enum);

static const struct arg_enum_list tune_content_enum[] = {
  { "default", AOM_CONTENT_DEFAULT },
  { "screen", AOM_CONTENT_SCREEN },
  { NULL, 0 }
};

static const arg_def_t tune_content = ARG_DEF_ENUM(
    NULL, "tune-content", 1, "Tune content type", tune_content_enum);

static const arg_def_t cdf_update_mode =
    ARG_DEF(NULL, "cdf-update-mode", 1,
            "CDF update mode for entropy coding "
            "(0: no CDF update; 1: update CDF on all frames(default); "
            "2: selectively update CDF on some frames");

static const struct arg_enum_list superblock_size_enum[] = {
  { "dynamic", AOM_SUPERBLOCK_SIZE_DYNAMIC },
  { "64", AOM_SUPERBLOCK_SIZE_64X64 },
  { "128", AOM_SUPERBLOCK_SIZE_128X128 },
  { NULL, 0 }
};
static const arg_def_t superblock_size = ARG_DEF_ENUM(
    NULL, "sb-size", 1, "Superblock size to use", superblock_size_enum);

static const arg_def_t *av1_args[] = { &cpu_used_av1,
                                       &dev_sf_av1,
                                       &auto_altref,
                                       &sharpness,
                                       &static_thresh,
                                       &single_tile_decoding,
                                       &tile_cols,
                                       &tile_rows,
                                       &arnr_maxframes,
                                       &arnr_strength,
                                       &tune_metric,
                                       &cq_level,
                                       &max_intra_rate_pct,
                                       &max_inter_rate_pct,
                                       &gf_cbr_boost_pct,
                                       &lossless,
                                       &enable_cdef,
                                       &enable_restoration,
                                       &disable_trellis_quant,
                                       &enable_qm,
                                       &qm_min,
                                       &qm_max,
#if CONFIG_DIST_8X8
                                       &enable_dist_8x8,
#endif
                                       &frame_parallel_decoding,
                                       &error_resilient_mode,
                                       &aq_mode,
                                       &deltaq_mode,
                                       &frame_periodic_boost,
                                       &noise_sens,
                                       &tune_content,
                                       &cdf_update_mode,
                                       &input_color_primaries,
                                       &input_transfer_characteristics,
                                       &input_matrix_coefficients,
                                       &input_chroma_sample_position,
                                       &min_gf_interval,
                                       &max_gf_interval,
                                       &superblock_size,
                                       &num_tg,
                                       &mtu_size,
                                       &timing_info,
                                       &film_grain_test,
                                       &film_grain_table,
                                       &enable_ref_frame_mvs,
                                       &bitdeptharg,
                                       &inbitdeptharg,
                                       &sframe_dist,
                                       &sframe_mode,
                                       &save_as_annexb,
                                       NULL };
static const int av1_arg_ctrl_map[] = { AOME_SET_CPUUSED,
                                        AOME_SET_DEVSF,
                                        AOME_SET_ENABLEAUTOALTREF,
                                        AOME_SET_SHARPNESS,
                                        AOME_SET_STATIC_THRESHOLD,
                                        AV1E_SET_SINGLE_TILE_DECODING,
                                        AV1E_SET_TILE_COLUMNS,
                                        AV1E_SET_TILE_ROWS,
                                        AOME_SET_ARNR_MAXFRAMES,
                                        AOME_SET_ARNR_STRENGTH,
                                        AOME_SET_TUNING,
                                        AOME_SET_CQ_LEVEL,
                                        AOME_SET_MAX_INTRA_BITRATE_PCT,
                                        AV1E_SET_MAX_INTER_BITRATE_PCT,
                                        AV1E_SET_GF_CBR_BOOST_PCT,
                                        AV1E_SET_LOSSLESS,
                                        AV1E_SET_ENABLE_CDEF,
                                        AV1E_SET_ENABLE_RESTORATION,
                                        AV1E_SET_DISABLE_TRELLIS_QUANT,
                                        AV1E_SET_ENABLE_QM,
                                        AV1E_SET_QM_MIN,
                                        AV1E_SET_QM_MAX,
#if CONFIG_DIST_8X8
                                        AV1E_SET_ENABLE_DIST_8X8,
#endif
                                        AV1E_SET_FRAME_PARALLEL_DECODING,
                                        AV1E_SET_ERROR_RESILIENT_MODE,
                                        AV1E_SET_AQ_MODE,
                                        AV1E_SET_DELTAQ_MODE,
                                        AV1E_SET_FRAME_PERIODIC_BOOST,
                                        AV1E_SET_NOISE_SENSITIVITY,
                                        AV1E_SET_TUNE_CONTENT,
                                        AV1E_SET_CDF_UPDATE_MODE,
                                        AV1E_SET_COLOR_PRIMARIES,
                                        AV1E_SET_TRANSFER_CHARACTERISTICS,
                                        AV1E_SET_MATRIX_COEFFICIENTS,
                                        AV1E_SET_CHROMA_SAMPLE_POSITION,
                                        AV1E_SET_MIN_GF_INTERVAL,
                                        AV1E_SET_MAX_GF_INTERVAL,
                                        AV1E_SET_SUPERBLOCK_SIZE,
                                        AV1E_SET_NUM_TG,
                                        AV1E_SET_MTU,
                                        AV1E_SET_TIMING_INFO_TYPE,
                                        AV1E_SET_FILM_GRAIN_TEST_VECTOR,
                                        AV1E_SET_FILM_GRAIN_TABLE,
                                        AV1E_SET_ENABLE_REF_FRAME_MVS,
                                        AV1E_SET_ENABLE_DF,
                                        AV1E_SET_ENABLE_ORDER_HINT,
                                        AV1E_SET_ENABLE_JNT_COMP,
                                        AV1E_SET_ENABLE_SUPERRES,
                                        0 };
#endif  // CONFIG_AV1_ENCODER

static const arg_def_t *no_args[] = { NULL };

void show_help(FILE *fout, int shorthelp) {
  fprintf(fout, "Usage: %s <options> -o dst_filename src_filename \n",
          exec_name);

  if (shorthelp) {
    fprintf(fout, "Use --help to see the full list of options.\n");
    return;
  }

  fprintf(fout, "\nOptions:\n");
  arg_show_usage(fout, main_args);
  fprintf(fout, "\nEncoder Global Options:\n");
  arg_show_usage(fout, global_args);
  fprintf(fout, "\nRate Control Options:\n");
  arg_show_usage(fout, rc_args);
  fprintf(fout, "\nTwopass Rate Control Options:\n");
  arg_show_usage(fout, rc_twopass_args);
  fprintf(fout, "\nKeyframe Placement Options:\n");
  arg_show_usage(fout, kf_args);
#if CONFIG_AV1_ENCODER
  fprintf(fout, "\nAV1 Specific Options:\n");
  arg_show_usage(fout, av1_args);
#endif
  fprintf(fout,
          "\nStream timebase (--timebase):\n"
          "  The desired precision of timestamps in the output, expressed\n"
          "  in fractional seconds. Default is 1/1000.\n");
  fprintf(fout, "\nIncluded encoders:\n\n");

  const int num_encoder = get_aom_encoder_count();
  for (int i = 0; i < num_encoder; ++i) {
    const AvxInterface *const encoder = get_aom_encoder_by_index(i);
    const char *defstr = (i == (num_encoder - 1)) ? "(default)" : "";
    fprintf(fout, "    %-6s - %s %s\n", encoder->name,
            aom_codec_iface_name(encoder->codec_interface()), defstr);
  }
  fprintf(fout, "\n        ");
  fprintf(fout, "Use --codec to switch to a non-default encoder.\n\n");
}

void usage_exit(void) {
  show_help(stderr, 1);
  exit(EXIT_FAILURE);
}

#if CONFIG_AV1_ENCODER
#define ARG_CTRL_CNT_MAX NELEMENTS(av1_arg_ctrl_map)
#endif

#if !CONFIG_WEBM_IO
typedef int stereo_format_t;
struct WebmOutputContext {
  int debug;
};
#endif

/* Per-stream configuration */
struct stream_config {
  struct aom_codec_enc_cfg cfg;
  const char *out_fn;
  const char *stats_fn;
#if CONFIG_FP_MB_STATS
  const char *fpmb_stats_fn;
#endif
  stereo_format_t stereo_fmt;
  int arg_ctrls[ARG_CTRL_CNT_MAX][2];
  int arg_ctrl_cnt;
  int write_webm;
  const char *film_grain_filename;
  int write_ivf;
  // whether to use 16bit internal buffers
  int use_16bit_internal;
};

struct stream_state {
  int index;
  struct stream_state *next;
  struct stream_config config;
  FILE *file;
  struct rate_hist *rate_hist;
  struct WebmOutputContext webm_ctx;
  uint64_t psnr_sse_total;
  uint64_t psnr_samples_total;
  double psnr_totals[4];
  int psnr_count;
  int counts[64];
  aom_codec_ctx_t encoder;
  unsigned int frames_out;
  uint64_t cx_time;
  size_t nbytes;
  stats_io_t stats;
#if CONFIG_FP_MB_STATS
  stats_io_t fpmb_stats;
#endif
  struct aom_image *img;
  aom_codec_ctx_t decoder;
  int mismatch_seen;
};

static void validate_positive_rational(const char *msg,
                                       struct aom_rational *rat) {
  if (rat->den < 0) {
    rat->num *= -1;
    rat->den *= -1;
  }

  if (rat->num < 0) die("Error: %s must be positive\n", msg);

  if (!rat->den) die("Error: %s has zero denominator\n", msg);
}

static void parse_global_config(struct AvxEncoderConfig *global, int *argc,
                                char ***argv) {
  char **argi, **argj;
  struct arg arg;
  const int num_encoder = get_aom_encoder_count();
  char **argv_local = (char **)*argv;
#if CONFIG_FILEOPTIONS
  int argc_local = *argc;
#endif
  if (num_encoder < 1) die("Error: no valid encoder available\n");

  /* Initialize default parameters */
  memset(global, 0, sizeof(*global));
  global->codec = get_aom_encoder_by_index(num_encoder - 1);
  global->passes = 0;
  global->color_type = I420;

#if CONFIG_FILEOPTIONS
  const char *cfg = NULL;
  int cfg_included = 0;
#endif
  for (argi = argj = argv_local; (*argj = *argi); argi += arg.argv_step) {
    arg.argv_step = 1;

#if CONFIG_FILEOPTIONS
    if (arg_match(&arg, &use_cfg, argi)) {
      if (cfg_included) continue;
      cfg = arg.val;

      arg_cfg(&argc_local, &argv_local, cfg);

      *argj = *argi = *argv_local;
      argj = argi = argv_local;
      *argv = argv_local;
      cfg_included = 1;
      continue;
    }
#endif
    if (arg_match(&arg, &help, argi)) {
      show_help(stdout, 0);
      exit(EXIT_SUCCESS);
    } else if (arg_match(&arg, &codecarg, argi)) {
      global->codec = get_aom_encoder_by_name(arg.val);
      if (!global->codec)
        die("Error: Unrecognized argument (%s) to --codec\n", arg.val);
    } else if (arg_match(&arg, &passes, argi)) {
      global->passes = arg_parse_uint(&arg);

      if (global->passes < 1 || global->passes > 2)
        die("Error: Invalid number of passes (%d)\n", global->passes);
    } else if (arg_match(&arg, &pass_arg, argi)) {
      global->pass = arg_parse_uint(&arg);

      if (global->pass < 1 || global->pass > 2)
        die("Error: Invalid pass selected (%d)\n", global->pass);
    } else if (arg_match(&arg, &usage, argi))
      global->usage = arg_parse_uint(&arg);
    else if (arg_match(&arg, &good_dl, argi))
      warn("Deprecated --good option! Ignoring\n");
    else if (arg_match(&arg, &use_yv12, argi))
      global->color_type = YV12;
    else if (arg_match(&arg, &use_i420, argi))
      global->color_type = I420;
    else if (arg_match(&arg, &use_i422, argi))
      global->color_type = I422;
    else if (arg_match(&arg, &use_i444, argi))
      global->color_type = I444;
    else if (arg_match(&arg, &quietarg, argi))
      global->quiet = 1;
    else if (arg_match(&arg, &verbosearg, argi))
      global->verbose = 1;
    else if (arg_match(&arg, &limit, argi))
      global->limit = arg_parse_uint(&arg);
    else if (arg_match(&arg, &skip, argi))
      global->skip_frames = arg_parse_uint(&arg);
    else if (arg_match(&arg, &psnrarg, argi))
      global->show_psnr = 1;
    else if (arg_match(&arg, &recontest, argi))
      global->test_decode = arg_parse_enum_or_int(&arg);
    else if (arg_match(&arg, &framerate, argi)) {
      global->framerate = arg_parse_rational(&arg);
      validate_positive_rational(arg.name, &global->framerate);
      global->have_framerate = 1;
    } else if (arg_match(&arg, &out_part, argi))
      global->out_part = 1;
    else if (arg_match(&arg, &debugmode, argi))
      global->debug = 1;
    else if (arg_match(&arg, &q_hist_n, argi))
      global->show_q_hist_buckets = arg_parse_uint(&arg);
    else if (arg_match(&arg, &rate_hist_n, argi))
      global->show_rate_hist_buckets = arg_parse_uint(&arg);
    else if (arg_match(&arg, &disable_warnings, argi))
      global->disable_warnings = 1;
    else if (arg_match(&arg, &disable_warning_prompt, argi))
      global->disable_warning_prompt = 1;
    else
      argj++;
  }

  if (global->pass) {
    /* DWIM: Assume the user meant passes=2 if pass=2 is specified */
    if (global->pass > global->passes) {
      warn("Assuming --pass=%d implies --passes=%d\n", global->pass,
           global->pass);
      global->passes = global->pass;
    }
  }
  /* Validate global config */
  if (global->passes == 0) {
#if CONFIG_AV1_ENCODER
    // Make default AV1 passes = 2 until there is a better quality 1-pass
    // encoder
    if (global->codec != NULL && global->codec->name != NULL)
      global->passes = (strcmp(global->codec->name, "av1") == 0) ? 2 : 1;
#else
    global->passes = 1;
#endif
  }
}

static void open_input_file(struct AvxInputContext *input) {
  /* Parse certain options from the input file, if possible */
  input->file = strcmp(input->filename, "-") ? fopen(input->filename, "rb")
                                             : set_binary_mode(stdin);

  if (!input->file) fatal("Failed to open input file");

  if (!fseeko(input->file, 0, SEEK_END)) {
    /* Input file is seekable. Figure out how long it is, so we can get
     * progress info.
     */
    input->length = ftello(input->file);
    rewind(input->file);
  }

  /* Default to 1:1 pixel aspect ratio. */
  input->pixel_aspect_ratio.numerator = 1;
  input->pixel_aspect_ratio.denominator = 1;

  /* For RAW input sources, these bytes will applied on the first frame
   *  in read_frame().
   */
  input->detect.buf_read = fread(input->detect.buf, 1, 4, input->file);
  input->detect.position = 0;

  if (input->detect.buf_read == 4 && file_is_y4m(input->detect.buf)) {
    if (y4m_input_open(&input->y4m, input->file, input->detect.buf, 4,
                       input->only_i420) >= 0) {
      input->file_type = FILE_TYPE_Y4M;
      input->width = input->y4m.pic_w;
      input->height = input->y4m.pic_h;
      input->pixel_aspect_ratio.numerator = input->y4m.par_n;
      input->pixel_aspect_ratio.denominator = input->y4m.par_d;
      input->framerate.numerator = input->y4m.fps_n;
      input->framerate.denominator = input->y4m.fps_d;
      input->fmt = input->y4m.aom_fmt;
      input->bit_depth = input->y4m.bit_depth;
    } else
      fatal("Unsupported Y4M stream.");
  } else if (input->detect.buf_read == 4 && fourcc_is_ivf(input->detect.buf)) {
    fatal("IVF is not supported as input.");
  } else {
    input->file_type = FILE_TYPE_RAW;
  }
}

static void close_input_file(struct AvxInputContext *input) {
  fclose(input->file);
  if (input->file_type == FILE_TYPE_Y4M) y4m_input_close(&input->y4m);
}

static struct stream_state *new_stream(struct AvxEncoderConfig *global,
                                       struct stream_state *prev) {
  struct stream_state *stream;

  stream = calloc(1, sizeof(*stream));
  if (stream == NULL) {
    fatal("Failed to allocate new stream.");
  }

  if (prev) {
    memcpy(stream, prev, sizeof(*stream));
    stream->index++;
    prev->next = stream;
  } else {
    aom_codec_err_t res;

    /* Populate encoder configuration */
    res = aom_codec_enc_config_default(global->codec->codec_interface(),
                                       &stream->config.cfg, global->usage);
    if (res) fatal("Failed to get config: %s\n", aom_codec_err_to_string(res));

    /* Change the default timebase to a high enough value so that the
     * encoder will always create strictly increasing timestamps.
     */
    stream->config.cfg.g_timebase.den = 1000;

    /* Never use the library's default resolution, require it be parsed
     * from the file or set on the command line.
     */
    stream->config.cfg.g_w = 0;
    stream->config.cfg.g_h = 0;

    /* Initialize remaining stream parameters */
    stream->config.write_webm = 1;
    stream->config.write_ivf = 0;

#if CONFIG_WEBM_IO
    stream->config.stereo_fmt = STEREO_FORMAT_MONO;
    stream->webm_ctx.last_pts_ns = -1;
    stream->webm_ctx.writer = NULL;
    stream->webm_ctx.segment = NULL;
#endif

    /* Allows removal of the application version from the EBML tags */
    stream->webm_ctx.debug = global->debug;
  }

  /* Output files must be specified for each stream */
  stream->config.out_fn = NULL;

  stream->next = NULL;
  return stream;
}

static void set_config_arg_ctrls(struct stream_config *config, int key,
                                 const struct arg *arg) {
  int j;
  if (key == AV1E_SET_FILM_GRAIN_TABLE) {
    config->film_grain_filename = arg->val;
    return;
  }

  /* Point either to the next free element or the first instance of this
   * control.
   */
  for (j = 0; j < config->arg_ctrl_cnt; j++)
    if (config->arg_ctrls[j][0] == key) break;

  /* Update/insert */
  assert(j < (int)ARG_CTRL_CNT_MAX);
  config->arg_ctrls[j][0] = key;
  config->arg_ctrls[j][1] = arg_parse_enum_or_int(arg);
  if (j == config->arg_ctrl_cnt) config->arg_ctrl_cnt++;
}

static int parse_stream_params(struct AvxEncoderConfig *global,
                               struct stream_state *stream, char **argv) {
  char **argi, **argj;
  struct arg arg;
  static const arg_def_t **ctrl_args = no_args;
  static const int *ctrl_args_map = NULL;
  struct stream_config *config = &stream->config;
  int eos_mark_found = 0;
  int webm_forced = 0;

  // Handle codec specific options
  if (0) {
#if CONFIG_AV1_ENCODER
  } else if (strcmp(global->codec->name, "av1") == 0) {
    // TODO(jingning): Reuse AV1 specific encoder configuration parameters.
    // Consider to expand this set for AV1 encoder control.
    ctrl_args = av1_args;
    ctrl_args_map = av1_arg_ctrl_map;
#endif
  }

  for (argi = argj = argv; (*argj = *argi); argi += arg.argv_step) {
    arg.argv_step = 1;

    /* Once we've found an end-of-stream marker (--) we want to continue
     * shifting arguments but not consuming them.
     */
    if (eos_mark_found) {
      argj++;
      continue;
    } else if (!strcmp(*argj, "--")) {
      eos_mark_found = 1;
      continue;
    }

    if (arg_match(&arg, &outputfile, argi)) {
      config->out_fn = arg.val;
      if (!webm_forced) {
        const size_t out_fn_len = strlen(config->out_fn);
        if (out_fn_len >= 4 &&
            !strcmp(config->out_fn + out_fn_len - 4, ".ivf")) {
          config->write_webm = 0;
          config->write_ivf = 1;
        } else if (out_fn_len >= 4 &&
                   !strcmp(config->out_fn + out_fn_len - 4, ".obu")) {
          config->write_webm = 0;
          config->write_ivf = 0;
        }
      }
    } else if (arg_match(&arg, &fpf_name, argi)) {
      config->stats_fn = arg.val;
#if CONFIG_FP_MB_STATS
    } else if (arg_match(&arg, &fpmbf_name, argi)) {
      config->fpmb_stats_fn = arg.val;
#endif
    } else if (arg_match(&arg, &use_webm, argi)) {
#if CONFIG_WEBM_IO
      config->write_webm = 1;
      webm_forced = 1;
#else
      die("Error: --webm specified but webm is disabled.");
#endif
    } else if (arg_match(&arg, &use_ivf, argi)) {
      config->write_webm = 0;
      config->write_ivf = 1;
    } else if (arg_match(&arg, &use_obu, argi)) {
      config->write_webm = 0;
      config->write_ivf = 0;
    } else if (arg_match(&arg, &threads, argi)) {
      config->cfg.g_threads = arg_parse_uint(&arg);
    } else if (arg_match(&arg, &profile, argi)) {
      config->cfg.g_profile = arg_parse_uint(&arg);
    } else if (arg_match(&arg, &width, argi)) {
      config->cfg.g_w = arg_parse_uint(&arg);
    } else if (arg_match(&arg, &height, argi)) {
      config->cfg.g_h = arg_parse_uint(&arg);
    } else if (arg_match(&arg, &forced_max_frame_width, argi)) {
      config->cfg.g_forced_max_frame_width = arg_parse_uint(&arg);
    } else if (arg_match(&arg, &forced_max_frame_height, argi)) {
      config->cfg.g_forced_max_frame_height = arg_parse_uint(&arg);
    } else if (arg_match(&arg, &bitdeptharg, argi)) {
      config->cfg.g_bit_depth = arg_parse_enum_or_int(&arg);
    } else if (arg_match(&arg, &inbitdeptharg, argi)) {
      config->cfg.g_input_bit_depth = arg_parse_uint(&arg);
#if CONFIG_WEBM_IO
    } else if (arg_match(&arg, &stereo_mode, argi)) {
      config->stereo_fmt = arg_parse_enum_or_int(&arg);
#endif
    } else if (arg_match(&arg, &timebase, argi)) {
      config->cfg.g_timebase = arg_parse_rational(&arg);
      validate_positive_rational(arg.name, &config->cfg.g_timebase);
    } else if (arg_match(&arg, &global_error_resilient, argi)) {
      config->cfg.g_error_resilient = arg_parse_uint(&arg);
    } else if (arg_match(&arg, &lag_in_frames, argi)) {
      config->cfg.g_lag_in_frames = arg_parse_uint(&arg);
    } else if (arg_match(&arg, &large_scale_tile, argi)) {
      config->cfg.large_scale_tile = arg_parse_uint(&arg);
    } else if (arg_match(&arg, &monochrome, argi)) {
      config->cfg.monochrome = 1;
    } else if (arg_match(&arg, &full_still_picture_hdr, argi)) {
      config->cfg.full_still_picture_hdr = 1;
    } else if (arg_match(&arg, &dropframe_thresh, argi)) {
      config->cfg.rc_dropframe_thresh = arg_parse_uint(&arg);
    } else if (arg_match(&arg, &resize_mode, argi)) {
      config->cfg.rc_resize_mode = arg_parse_uint(&arg);
    } else if (arg_match(&arg, &resize_denominator, argi)) {
      config->cfg.rc_resize_denominator = arg_parse_uint(&arg);
    } else if (arg_match(&arg, &resize_kf_denominator, argi)) {
      config->cfg.rc_resize_kf_denominator = arg_parse_uint(&arg);
    } else if (arg_match(&arg, &superres_mode, argi)) {
      config->cfg.rc_superres_mode = arg_parse_uint(&arg);
    } else if (arg_match(&arg, &superres_denominator, argi)) {
      config->cfg.rc_superres_denominator = arg_parse_uint(&arg);
    } else if (arg_match(&arg, &superres_kf_denominator, argi)) {
      config->cfg.rc_superres_kf_denominator = arg_parse_uint(&arg);
    } else if (arg_match(&arg, &superres_qthresh, argi)) {
      config->cfg.rc_superres_qthresh = arg_parse_uint(&arg);
    } else if (arg_match(&arg, &superres_kf_qthresh, argi)) {
      config->cfg.rc_superres_kf_qthresh = arg_parse_uint(&arg);
    } else if (arg_match(&arg, &end_usage, argi)) {
      config->cfg.rc_end_usage = arg_parse_enum_or_int(&arg);
    } else if (arg_match(&arg, &target_bitrate, argi)) {
      config->cfg.rc_target_bitrate = arg_parse_uint(&arg);
    } else if (arg_match(&arg, &min_quantizer, argi)) {
      config->cfg.rc_min_quantizer = arg_parse_uint(&arg);
    } else if (arg_match(&arg, &max_quantizer, argi)) {
      config->cfg.rc_max_quantizer = arg_parse_uint(&arg);
    } else if (arg_match(&arg, &undershoot_pct, argi)) {
      config->cfg.rc_undershoot_pct = arg_parse_uint(&arg);
    } else if (arg_match(&arg, &overshoot_pct, argi)) {
      config->cfg.rc_overshoot_pct = arg_parse_uint(&arg);
    } else if (arg_match(&arg, &buf_sz, argi)) {
      config->cfg.rc_buf_sz = arg_parse_uint(&arg);
    } else if (arg_match(&arg, &buf_initial_sz, argi)) {
      config->cfg.rc_buf_initial_sz = arg_parse_uint(&arg);
    } else if (arg_match(&arg, &buf_optimal_sz, argi)) {
      config->cfg.rc_buf_optimal_sz = arg_parse_uint(&arg);
    } else if (arg_match(&arg, &bias_pct, argi)) {
      config->cfg.rc_2pass_vbr_bias_pct = arg_parse_uint(&arg);
      if (global->passes < 2)
        warn("option %s ignored in one-pass mode.\n", arg.name);
    } else if (arg_match(&arg, &minsection_pct, argi)) {
      config->cfg.rc_2pass_vbr_minsection_pct = arg_parse_uint(&arg);

      if (global->passes < 2)
        warn("option %s ignored in one-pass mode.\n", arg.name);
    } else if (arg_match(&arg, &maxsection_pct, argi)) {
      config->cfg.rc_2pass_vbr_maxsection_pct = arg_parse_uint(&arg);

      if (global->passes < 2)
        warn("option %s ignored in one-pass mode.\n", arg.name);
    } else if (arg_match(&arg, &fwd_kf_enabled, argi)) {
      config->cfg.fwd_kf_enabled = arg_parse_uint(&arg);
    } else if (arg_match(&arg, &kf_min_dist, argi)) {
      config->cfg.kf_min_dist = arg_parse_uint(&arg);
    } else if (arg_match(&arg, &kf_max_dist, argi)) {
      config->cfg.kf_max_dist = arg_parse_uint(&arg);
    } else if (arg_match(&arg, &kf_disabled, argi)) {
      config->cfg.kf_mode = AOM_KF_DISABLED;
    } else if (arg_match(&arg, &sframe_dist, argi)) {
      config->cfg.sframe_dist = arg_parse_uint(&arg);
    } else if (arg_match(&arg, &sframe_mode, argi)) {
      config->cfg.sframe_mode = arg_parse_uint(&arg);
    } else if (arg_match(&arg, &save_as_annexb, argi)) {
      config->cfg.save_as_annexb = arg_parse_uint(&arg);
    } else if (arg_match(&arg, &tile_width, argi)) {
      config->cfg.tile_width_count =
          arg_parse_list(&arg, config->cfg.tile_widths, MAX_TILE_WIDTHS);
    } else if (arg_match(&arg, &tile_height, argi)) {
      config->cfg.tile_height_count =
          arg_parse_list(&arg, config->cfg.tile_heights, MAX_TILE_HEIGHTS);
#if CONFIG_FILEOPTIONS
    } else if (arg_match(&arg, &ext_partition, argi)) {
      config->cfg.cfg.ext_partition = !!arg_parse_uint(&arg) > 0;
#endif
    } else {
      int i, match = 0;
      for (i = 0; ctrl_args[i]; i++) {
        if (arg_match(&arg, ctrl_args[i], argi)) {
          match = 1;
          if (ctrl_args_map) {
            set_config_arg_ctrls(config, ctrl_args_map[i], &arg);
          }
        }
      }
      if (!match) argj++;
    }
  }
  config->use_16bit_internal =
      config->cfg.g_bit_depth > AOM_BITS_8 || !CONFIG_LOWBITDEPTH;
  return eos_mark_found;
}

#define FOREACH_STREAM(iterator, list)                 \
  for (struct stream_state *iterator = list; iterator; \
       iterator = iterator->next)

static void validate_stream_config(const struct stream_state *stream,
                                   const struct AvxEncoderConfig *global) {
  const struct stream_state *streami;
  (void)global;

  if (!stream->config.cfg.g_w || !stream->config.cfg.g_h)
    fatal(
        "Stream %d: Specify stream dimensions with --width (-w) "
        " and --height (-h)",
        stream->index);

  // Check that the codec bit depth is greater than the input bit depth.
  if (stream->config.cfg.g_input_bit_depth >
      (unsigned int)stream->config.cfg.g_bit_depth) {
    fatal("Stream %d: codec bit depth (%d) less than input bit depth (%d)",
          stream->index, (int)stream->config.cfg.g_bit_depth,
          stream->config.cfg.g_input_bit_depth);
  }

  for (streami = stream; streami; streami = streami->next) {
    /* All streams require output files */
    if (!streami->config.out_fn)
      fatal("Stream %d: Output file is required (specify with -o)",
            streami->index);

    /* Check for two streams outputting to the same file */
    if (streami != stream) {
      const char *a = stream->config.out_fn;
      const char *b = streami->config.out_fn;
      if (!strcmp(a, b) && strcmp(a, "/dev/null") && strcmp(a, ":nul"))
        fatal("Stream %d: duplicate output file (from stream %d)",
              streami->index, stream->index);
    }

    /* Check for two streams sharing a stats file. */
    if (streami != stream) {
      const char *a = stream->config.stats_fn;
      const char *b = streami->config.stats_fn;
      if (a && b && !strcmp(a, b))
        fatal("Stream %d: duplicate stats file (from stream %d)",
              streami->index, stream->index);
    }

#if CONFIG_FP_MB_STATS
    /* Check for two streams sharing a mb stats file. */
    if (streami != stream) {
      const char *a = stream->config.fpmb_stats_fn;
      const char *b = streami->config.fpmb_stats_fn;
      if (a && b && !strcmp(a, b))
        fatal("Stream %d: duplicate mb stats file (from stream %d)",
              streami->index, stream->index);
    }
#endif
  }
}

static void set_stream_dimensions(struct stream_state *stream, unsigned int w,
                                  unsigned int h) {
  if (!stream->config.cfg.g_w) {
    if (!stream->config.cfg.g_h)
      stream->config.cfg.g_w = w;
    else
      stream->config.cfg.g_w = w * stream->config.cfg.g_h / h;
  }
  if (!stream->config.cfg.g_h) {
    stream->config.cfg.g_h = h * stream->config.cfg.g_w / w;
  }
}

static const char *file_type_to_string(enum VideoFileType t) {
  switch (t) {
    case FILE_TYPE_RAW: return "RAW";
    case FILE_TYPE_Y4M: return "Y4M";
    default: return "Other";
  }
}

static const char *image_format_to_string(aom_img_fmt_t f) {
  switch (f) {
    case AOM_IMG_FMT_I420: return "I420";
    case AOM_IMG_FMT_I422: return "I422";
    case AOM_IMG_FMT_I444: return "I444";
    case AOM_IMG_FMT_YV12: return "YV12";
    case AOM_IMG_FMT_I42016: return "I42016";
    case AOM_IMG_FMT_I42216: return "I42216";
    case AOM_IMG_FMT_I44416: return "I44416";
    default: return "Other";
  }
}

static void show_stream_config(struct stream_state *stream,
                               struct AvxEncoderConfig *global,
                               struct AvxInputContext *input) {
#define SHOW(field) \
  fprintf(stderr, "    %-28s = %d\n", #field, stream->config.cfg.field)

  if (stream->index == 0) {
    fprintf(stderr, "Codec: %s\n",
            aom_codec_iface_name(global->codec->codec_interface()));
    fprintf(stderr, "Source file: %s File Type: %s Format: %s\n",
            input->filename, file_type_to_string(input->file_type),
            image_format_to_string(input->fmt));
  }
  if (stream->next || stream->index)
    fprintf(stderr, "\nStream Index: %d\n", stream->index);
  fprintf(stderr, "Destination file: %s\n", stream->config.out_fn);
  fprintf(stderr, "Coding path: %s\n",
          stream->config.use_16bit_internal ? "HBD" : "LBD");
  fprintf(stderr, "Encoder parameters:\n");

  SHOW(g_usage);
  SHOW(g_threads);
  SHOW(g_profile);
  SHOW(g_w);
  SHOW(g_h);
  SHOW(g_bit_depth);
  SHOW(g_input_bit_depth);
  SHOW(g_timebase.num);
  SHOW(g_timebase.den);
  SHOW(g_error_resilient);
  SHOW(g_pass);
  SHOW(g_lag_in_frames);
  SHOW(large_scale_tile);
  SHOW(rc_dropframe_thresh);
  SHOW(rc_resize_mode);
  SHOW(rc_resize_denominator);
  SHOW(rc_resize_kf_denominator);
  SHOW(rc_superres_mode);
  SHOW(rc_superres_denominator);
  SHOW(rc_superres_kf_denominator);
  SHOW(rc_superres_qthresh);
  SHOW(rc_superres_kf_qthresh);
  SHOW(rc_end_usage);
  SHOW(rc_target_bitrate);
  SHOW(rc_min_quantizer);
  SHOW(rc_max_quantizer);
  SHOW(rc_undershoot_pct);
  SHOW(rc_overshoot_pct);
  SHOW(rc_buf_sz);
  SHOW(rc_buf_initial_sz);
  SHOW(rc_buf_optimal_sz);
  SHOW(rc_2pass_vbr_bias_pct);
  SHOW(rc_2pass_vbr_minsection_pct);
  SHOW(rc_2pass_vbr_maxsection_pct);
  SHOW(fwd_kf_enabled);
  SHOW(kf_mode);
  SHOW(kf_min_dist);
  SHOW(kf_max_dist);
}

static void open_output_file(struct stream_state *stream,
                             struct AvxEncoderConfig *global,
                             const struct AvxRational *pixel_aspect_ratio) {
  const char *fn = stream->config.out_fn;
  const struct aom_codec_enc_cfg *const cfg = &stream->config.cfg;

  if (cfg->g_pass == AOM_RC_FIRST_PASS) return;

  stream->file = strcmp(fn, "-") ? fopen(fn, "wb") : set_binary_mode(stdout);

  if (!stream->file) fatal("Failed to open output file");

  if (stream->config.write_webm && fseek(stream->file, 0, SEEK_CUR))
    fatal("WebM output to pipes not supported.");

#if CONFIG_WEBM_IO
  if (stream->config.write_webm) {
    stream->webm_ctx.stream = stream->file;
    write_webm_file_header(&stream->webm_ctx, cfg, stream->config.stereo_fmt,
                           global->codec->fourcc, pixel_aspect_ratio);
  }
#else
  (void)pixel_aspect_ratio;
#endif

  if (!stream->config.write_webm && stream->config.write_ivf) {
    ivf_write_file_header(stream->file, cfg, global->codec->fourcc, 0);
  }
}

static void close_output_file(struct stream_state *stream,
                              unsigned int fourcc) {
  const struct aom_codec_enc_cfg *const cfg = &stream->config.cfg;

  if (cfg->g_pass == AOM_RC_FIRST_PASS) return;

#if CONFIG_WEBM_IO
  if (stream->config.write_webm) {
    write_webm_file_footer(&stream->webm_ctx);
  }
#endif

  if (!stream->config.write_webm && stream->config.write_ivf) {
    if (!fseek(stream->file, 0, SEEK_SET))
      ivf_write_file_header(stream->file, &stream->config.cfg, fourcc,
                            stream->frames_out);
  }

  fclose(stream->file);
}

static void setup_pass(struct stream_state *stream,
                       struct AvxEncoderConfig *global, int pass) {
  if (stream->config.stats_fn) {
    if (!stats_open_file(&stream->stats, stream->config.stats_fn, pass))
      fatal("Failed to open statistics store");
  } else {
    if (!stats_open_mem(&stream->stats, pass))
      fatal("Failed to open statistics store");
  }

#if CONFIG_FP_MB_STATS
  if (stream->config.fpmb_stats_fn) {
    if (!stats_open_file(&stream->fpmb_stats, stream->config.fpmb_stats_fn,
                         pass))
      fatal("Failed to open mb statistics store");
  } else {
    if (!stats_open_mem(&stream->fpmb_stats, pass))
      fatal("Failed to open mb statistics store");
  }
#endif

  stream->config.cfg.g_pass = global->passes == 2
                                  ? pass ? AOM_RC_LAST_PASS : AOM_RC_FIRST_PASS
                                  : AOM_RC_ONE_PASS;
  if (pass) {
    stream->config.cfg.rc_twopass_stats_in = stats_get(&stream->stats);
#if CONFIG_FP_MB_STATS
    stream->config.cfg.rc_firstpass_mb_stats_in =
        stats_get(&stream->fpmb_stats);
#endif
  }

  stream->cx_time = 0;
  stream->nbytes = 0;
  stream->frames_out = 0;
}

static void initialize_encoder(struct stream_state *stream,
                               struct AvxEncoderConfig *global) {
  int i;
  int flags = 0;

  flags |= global->show_psnr ? AOM_CODEC_USE_PSNR : 0;
  flags |= global->out_part ? AOM_CODEC_USE_OUTPUT_PARTITION : 0;
  flags |= stream->config.use_16bit_internal ? AOM_CODEC_USE_HIGHBITDEPTH : 0;

  /* Construct Encoder Context */
  aom_codec_enc_init(&stream->encoder, global->codec->codec_interface(),
                     &stream->config.cfg, flags);
  ctx_exit_on_error(&stream->encoder, "Failed to initialize encoder");

  /* Note that we bypass the aom_codec_control wrapper macro because
   * we're being clever to store the control IDs in an array. Real
   * applications will want to make use of the enumerations directly
   */
  for (i = 0; i < stream->config.arg_ctrl_cnt; i++) {
    int ctrl = stream->config.arg_ctrls[i][0];
    int value = stream->config.arg_ctrls[i][1];
    if (aom_codec_control_(&stream->encoder, ctrl, value))
      fprintf(stderr, "Error: Tried to set control %d = %d\n", ctrl, value);

    ctx_exit_on_error(&stream->encoder, "Failed to control codec");
  }
  if (stream->config.film_grain_filename) {
    aom_codec_control_(&stream->encoder, AV1E_SET_FILM_GRAIN_TABLE,
                       stream->config.film_grain_filename);
  }

#if CONFIG_AV1_DECODER
  if (global->test_decode != TEST_DECODE_OFF) {
    const AvxInterface *decoder = get_aom_decoder_by_name(global->codec->name);
    aom_codec_dec_cfg_t cfg = { 0, 0, 0, CONFIG_LOWBITDEPTH, { 1 } };
    aom_codec_dec_init(&stream->decoder, decoder->codec_interface(), &cfg, 0);

    if (strcmp(global->codec->name, "av1") == 0) {
      aom_codec_control(&stream->decoder, AV1_SET_TILE_MODE,
                        stream->config.cfg.large_scale_tile);
      ctx_exit_on_error(&stream->decoder, "Failed to set decode_tile_mode");

      aom_codec_control(&stream->decoder, AV1D_SET_IS_ANNEXB,
                        stream->config.cfg.save_as_annexb);
      ctx_exit_on_error(&stream->decoder, "Failed to set is_annexb");

      aom_codec_control(&stream->decoder, AV1_SET_DECODE_TILE_ROW, -1);
      ctx_exit_on_error(&stream->decoder, "Failed to set decode_tile_row");

      aom_codec_control(&stream->decoder, AV1_SET_DECODE_TILE_COL, -1);
      ctx_exit_on_error(&stream->decoder, "Failed to set decode_tile_col");
    }
  }
#endif
}

static void encode_frame(struct stream_state *stream,
                         struct AvxEncoderConfig *global, struct aom_image *img,
                         unsigned int frames_in) {
  aom_codec_pts_t frame_start, next_frame_start;
  struct aom_codec_enc_cfg *cfg = &stream->config.cfg;
  struct aom_usec_timer timer;

  frame_start =
      (cfg->g_timebase.den * (int64_t)(frames_in - 1) * global->framerate.den) /
      cfg->g_timebase.num / global->framerate.num;
  next_frame_start =
      (cfg->g_timebase.den * (int64_t)(frames_in)*global->framerate.den) /
      cfg->g_timebase.num / global->framerate.num;

  /* Scale if necessary */
  if (img) {
    if ((img->fmt & AOM_IMG_FMT_HIGHBITDEPTH) &&
        (img->d_w != cfg->g_w || img->d_h != cfg->g_h)) {
      if (img->fmt != AOM_IMG_FMT_I42016) {
        fprintf(stderr, "%s can only scale 4:2:0 inputs\n", exec_name);
        exit(EXIT_FAILURE);
      }
#if CONFIG_LIBYUV
      if (!stream->img) {
        stream->img =
            aom_img_alloc(NULL, AOM_IMG_FMT_I42016, cfg->g_w, cfg->g_h, 16);
      }
      I420Scale_16(
          (uint16 *)img->planes[AOM_PLANE_Y], img->stride[AOM_PLANE_Y] / 2,
          (uint16 *)img->planes[AOM_PLANE_U], img->stride[AOM_PLANE_U] / 2,
          (uint16 *)img->planes[AOM_PLANE_V], img->stride[AOM_PLANE_V] / 2,
          img->d_w, img->d_h, (uint16 *)stream->img->planes[AOM_PLANE_Y],
          stream->img->stride[AOM_PLANE_Y] / 2,
          (uint16 *)stream->img->planes[AOM_PLANE_U],
          stream->img->stride[AOM_PLANE_U] / 2,
          (uint16 *)stream->img->planes[AOM_PLANE_V],
          stream->img->stride[AOM_PLANE_V] / 2, stream->img->d_w,
          stream->img->d_h, kFilterBox);
      img = stream->img;
#else
      stream->encoder.err = 1;
      ctx_exit_on_error(&stream->encoder,
                        "Stream %d: Failed to encode frame.\n"
                        "libyuv is required for scaling but is currently "
                        "disabled.\n"
                        "Be sure to specify -DCONFIG_LIBYUV=1 when running "
                        "cmake.\n",
                        stream->index);
#endif
    }
  }
  if (img && (img->d_w != cfg->g_w || img->d_h != cfg->g_h)) {
    if (img->fmt != AOM_IMG_FMT_I420 && img->fmt != AOM_IMG_FMT_YV12) {
      fprintf(stderr, "%s can only scale 4:2:0 8bpp inputs\n", exec_name);
      exit(EXIT_FAILURE);
    }
#if CONFIG_LIBYUV
    if (!stream->img)
      stream->img =
          aom_img_alloc(NULL, AOM_IMG_FMT_I420, cfg->g_w, cfg->g_h, 16);
    I420Scale(
        img->planes[AOM_PLANE_Y], img->stride[AOM_PLANE_Y],
        img->planes[AOM_PLANE_U], img->stride[AOM_PLANE_U],
        img->planes[AOM_PLANE_V], img->stride[AOM_PLANE_V], img->d_w, img->d_h,
        stream->img->planes[AOM_PLANE_Y], stream->img->stride[AOM_PLANE_Y],
        stream->img->planes[AOM_PLANE_U], stream->img->stride[AOM_PLANE_U],
        stream->img->planes[AOM_PLANE_V], stream->img->stride[AOM_PLANE_V],
        stream->img->d_w, stream->img->d_h, kFilterBox);
    img = stream->img;
#else
    stream->encoder.err = 1;
    ctx_exit_on_error(&stream->encoder,
                      "Stream %d: Failed to encode frame.\n"
                      "Scaling disabled in this configuration. \n"
                      "To enable, configure with --enable-libyuv\n",
                      stream->index);
#endif
  }

  aom_usec_timer_start(&timer);
  aom_codec_encode(&stream->encoder, img, frame_start,
                   (uint32_t)(next_frame_start - frame_start), 0);
  aom_usec_timer_mark(&timer);
  stream->cx_time += aom_usec_timer_elapsed(&timer);
  ctx_exit_on_error(&stream->encoder, "Stream %d: Failed to encode frame",
                    stream->index);
}

static void update_quantizer_histogram(struct stream_state *stream) {
  if (stream->config.cfg.g_pass != AOM_RC_FIRST_PASS) {
    int q;

    aom_codec_control(&stream->encoder, AOME_GET_LAST_QUANTIZER_64, &q);
    ctx_exit_on_error(&stream->encoder, "Failed to read quantizer");
    stream->counts[q]++;
  }
}

static void get_cx_data(struct stream_state *stream,
                        struct AvxEncoderConfig *global, int *got_data) {
  const aom_codec_cx_pkt_t *pkt;
  const struct aom_codec_enc_cfg *cfg = &stream->config.cfg;
  aom_codec_iter_t iter = NULL;

  *got_data = 0;
  while ((pkt = aom_codec_get_cx_data(&stream->encoder, &iter))) {
    static size_t fsize = 0;
    static FileOffset ivf_header_pos = 0;

    switch (pkt->kind) {
      case AOM_CODEC_CX_FRAME_PKT:
        if (!(pkt->data.frame.flags & AOM_FRAME_IS_FRAGMENT)) {
          stream->frames_out++;
        }
        if (!global->quiet)
          fprintf(stderr, " %6luF", (unsigned long)pkt->data.frame.sz);

        update_rate_histogram(stream->rate_hist, cfg, pkt);
#if CONFIG_WEBM_IO
        if (stream->config.write_webm) {
          write_webm_block(&stream->webm_ctx, cfg, pkt);
        }
#endif
        if (!stream->config.write_webm) {
          if (stream->config.write_ivf) {
            if (pkt->data.frame.partition_id <= 0) {
              ivf_header_pos = ftello(stream->file);
              fsize = pkt->data.frame.sz;

              ivf_write_frame_header(stream->file, pkt->data.frame.pts, fsize);
            } else {
              fsize += pkt->data.frame.sz;

              if (!(pkt->data.frame.flags & AOM_FRAME_IS_FRAGMENT)) {
                const FileOffset currpos = ftello(stream->file);
                fseeko(stream->file, ivf_header_pos, SEEK_SET);
                ivf_write_frame_size(stream->file, fsize);
                fseeko(stream->file, currpos, SEEK_SET);
              }
            }
          }

          (void)fwrite(pkt->data.frame.buf, 1, pkt->data.frame.sz,
                       stream->file);
        }
        stream->nbytes += pkt->data.raw.sz;

        *got_data = 1;
#if CONFIG_AV1_DECODER
        if (global->test_decode != TEST_DECODE_OFF && !stream->mismatch_seen) {
          aom_codec_decode(&stream->decoder, pkt->data.frame.buf,
                           pkt->data.frame.sz, NULL);
          if (stream->decoder.err) {
            warn_or_exit_on_error(&stream->decoder,
                                  global->test_decode == TEST_DECODE_FATAL,
                                  "Failed to decode frame %d in stream %d",
                                  stream->frames_out + 1, stream->index);
            stream->mismatch_seen = stream->frames_out + 1;
          }
        }
#endif
        break;
      case AOM_CODEC_STATS_PKT:
        stream->frames_out++;
        stats_write(&stream->stats, pkt->data.twopass_stats.buf,
                    pkt->data.twopass_stats.sz);
        stream->nbytes += pkt->data.raw.sz;
        break;
#if CONFIG_FP_MB_STATS
      case AOM_CODEC_FPMB_STATS_PKT:
        stats_write(&stream->fpmb_stats, pkt->data.firstpass_mb_stats.buf,
                    pkt->data.firstpass_mb_stats.sz);
        stream->nbytes += pkt->data.raw.sz;
        break;
#endif
      case AOM_CODEC_PSNR_PKT:

        if (global->show_psnr) {
          int i;

          stream->psnr_sse_total += pkt->data.psnr.sse[0];
          stream->psnr_samples_total += pkt->data.psnr.samples[0];
          for (i = 0; i < 4; i++) {
            if (!global->quiet)
              fprintf(stderr, "%.3f ", pkt->data.psnr.psnr[i]);
            stream->psnr_totals[i] += pkt->data.psnr.psnr[i];
          }
          stream->psnr_count++;
        }

        break;
      default: break;
    }
  }
}

static void show_psnr(struct stream_state *stream, double peak, int64_t bps) {
  int i;
  double ovpsnr;

  if (!stream->psnr_count) return;

  fprintf(stderr, "Stream %d PSNR (Overall/Avg/Y/U/V)", stream->index);
  ovpsnr = sse_to_psnr((double)stream->psnr_samples_total, peak,
                       (double)stream->psnr_sse_total);
  fprintf(stderr, " %.3f", ovpsnr);

  for (i = 0; i < 4; i++) {
    fprintf(stderr, " %.3f", stream->psnr_totals[i] / stream->psnr_count);
  }
  if (bps > 0) {
    fprintf(stderr, " %7" PRId64 " bps", bps);
  }
  fprintf(stderr, " %7" PRId64 " ms", stream->cx_time / 1000);
  fprintf(stderr, "\n");
}

static float usec_to_fps(uint64_t usec, unsigned int frames) {
  return (float)(usec > 0 ? frames * 1000000.0 / (float)usec : 0);
}

static void test_decode(struct stream_state *stream,
                        enum TestDecodeFatality fatal) {
  aom_image_t enc_img, dec_img;

  if (stream->mismatch_seen) return;

  /* Get the internal reference frame */
  aom_codec_control(&stream->encoder, AV1_GET_NEW_FRAME_IMAGE, &enc_img);
  aom_codec_control(&stream->decoder, AV1_GET_NEW_FRAME_IMAGE, &dec_img);

  if ((enc_img.fmt & AOM_IMG_FMT_HIGHBITDEPTH) !=
      (dec_img.fmt & AOM_IMG_FMT_HIGHBITDEPTH)) {
    if (enc_img.fmt & AOM_IMG_FMT_HIGHBITDEPTH) {
      aom_image_t enc_hbd_img;
      aom_img_alloc(&enc_hbd_img, enc_img.fmt - AOM_IMG_FMT_HIGHBITDEPTH,
                    enc_img.d_w, enc_img.d_h, 16);
      aom_img_truncate_16_to_8(&enc_hbd_img, &enc_img);
      enc_img = enc_hbd_img;
    }
    if (dec_img.fmt & AOM_IMG_FMT_HIGHBITDEPTH) {
      aom_image_t dec_hbd_img;
      aom_img_alloc(&dec_hbd_img, dec_img.fmt - AOM_IMG_FMT_HIGHBITDEPTH,
                    dec_img.d_w, dec_img.d_h, 16);
      aom_img_truncate_16_to_8(&dec_hbd_img, &dec_img);
      dec_img = dec_hbd_img;
    }
  }

  ctx_exit_on_error(&stream->encoder, "Failed to get encoder reference frame");
  ctx_exit_on_error(&stream->decoder, "Failed to get decoder reference frame");

  if (!aom_compare_img(&enc_img, &dec_img)) {
    int y[4], u[4], v[4];
    if (enc_img.fmt & AOM_IMG_FMT_HIGHBITDEPTH) {
      aom_find_mismatch_high(&enc_img, &dec_img, y, u, v);
    } else {
      aom_find_mismatch(&enc_img, &dec_img, y, u, v);
    }
    stream->decoder.err = 1;
    warn_or_exit_on_error(&stream->decoder, fatal == TEST_DECODE_FATAL,
                          "Stream %d: Encode/decode mismatch on frame %d at"
                          " Y[%d, %d] {%d/%d},"
                          " U[%d, %d] {%d/%d},"
                          " V[%d, %d] {%d/%d}",
                          stream->index, stream->frames_out, y[0], y[1], y[2],
                          y[3], u[0], u[1], u[2], u[3], v[0], v[1], v[2], v[3]);
    stream->mismatch_seen = stream->frames_out;
  }

  aom_img_free(&enc_img);
  aom_img_free(&dec_img);
}

static void print_time(const char *label, int64_t etl) {
  int64_t hours;
  int64_t mins;
  int64_t secs;

  if (etl >= 0) {
    hours = etl / 3600;
    etl -= hours * 3600;
    mins = etl / 60;
    etl -= mins * 60;
    secs = etl;

    fprintf(stderr, "[%3s %2" PRId64 ":%02" PRId64 ":%02" PRId64 "] ", label,
            hours, mins, secs);
  } else {
    fprintf(stderr, "[%3s  unknown] ", label);
  }
}

int main(int argc, const char **argv_) {
  int pass;
  aom_image_t raw;
  aom_image_t raw_shift;
  int allocated_raw_shift = 0;
  int use_16bit_internal = 0;
  int input_shift = 0;
  int frame_avail, got_data;

  struct AvxInputContext input;
  struct AvxEncoderConfig global;
  struct stream_state *streams = NULL;
  char **argv, **argi;
  uint64_t cx_time = 0;
  int stream_cnt = 0;
  int res = 0;
  int profile_updated = 0;

  memset(&input, 0, sizeof(input));
  exec_name = argv_[0];

  /* Setup default input stream settings */
  input.framerate.numerator = 30;
  input.framerate.denominator = 1;
  input.only_i420 = 1;
  input.bit_depth = 0;

  /* First parse the global configuration values, because we want to apply
   * other parameters on top of the default configuration provided by the
   * codec.
   */
  argv = argv_dup(argc - 1, argv_ + 1);
  parse_global_config(&global, &argc, &argv);

#if CONFIG_FILEOPTIONS
  if (argc < 2) usage_exit();
#else
  if (argc < 3) usage_exit();
#endif

  switch (global.color_type) {
    case I420: input.fmt = AOM_IMG_FMT_I420; break;
    case I422: input.fmt = AOM_IMG_FMT_I422; break;
    case I444: input.fmt = AOM_IMG_FMT_I444; break;
    case YV12: input.fmt = AOM_IMG_FMT_YV12; break;
  }

  {
    /* Now parse each stream's parameters. Using a local scope here
     * due to the use of 'stream' as loop variable in FOREACH_STREAM
     * loops
     */
    struct stream_state *stream = NULL;

    do {
      stream = new_stream(&global, stream);
      stream_cnt++;
      if (!streams) streams = stream;
    } while (parse_stream_params(&global, stream, argv));
  }

  /* Check for unrecognized options */
  for (argi = argv; *argi; argi++)
    if (argi[0][0] == '-' && argi[0][1])
      die("Error: Unrecognized option %s\n", *argi);

  FOREACH_STREAM(stream, streams) {
    check_encoder_config(global.disable_warning_prompt, &global,
                         &stream->config.cfg);
  }

  /* Handle non-option arguments */
  input.filename = argv[0];

  if (!input.filename) {
    fprintf(stderr, "No input file specified!\n");
    usage_exit();
  }

  /* Decide if other chroma subsamplings than 4:2:0 are supported */
  if (global.codec->fourcc == AV1_FOURCC) input.only_i420 = 0;

  for (pass = global.pass ? global.pass - 1 : 0; pass < global.passes; pass++) {
    int frames_in = 0, seen_frames = 0;
    int64_t estimated_time_left = -1;
    int64_t average_rate = -1;
    int64_t lagged_count = 0;

    open_input_file(&input);

    /* If the input file doesn't specify its w/h (raw files), try to get
     * the data from the first stream's configuration.
     */
    if (!input.width || !input.height) {
      FOREACH_STREAM(stream, streams) {
        if (stream->config.cfg.g_w && stream->config.cfg.g_h) {
          input.width = stream->config.cfg.g_w;
          input.height = stream->config.cfg.g_h;
          break;
        }
      };
    }

    /* Update stream configurations from the input file's parameters */
    if (!input.width || !input.height)
      fatal(
          "Specify stream dimensions with --width (-w) "
          " and --height (-h)");

    /* If input file does not specify bit-depth but input-bit-depth parameter
     * exists, assume that to be the input bit-depth. However, if the
     * input-bit-depth paramter does not exist, assume the input bit-depth
     * to be the same as the codec bit-depth.
     */
    if (!input.bit_depth) {
      FOREACH_STREAM(stream, streams) {
        if (stream->config.cfg.g_input_bit_depth)
          input.bit_depth = stream->config.cfg.g_input_bit_depth;
        else
          input.bit_depth = stream->config.cfg.g_input_bit_depth =
              (int)stream->config.cfg.g_bit_depth;
      }
      if (input.bit_depth > 8) input.fmt |= AOM_IMG_FMT_HIGHBITDEPTH;
    } else {
      FOREACH_STREAM(stream, streams) {
        stream->config.cfg.g_input_bit_depth = input.bit_depth;
      }
    }

    FOREACH_STREAM(stream, streams) {
      if (input.fmt != AOM_IMG_FMT_I420 && input.fmt != AOM_IMG_FMT_I42016) {
        /* Automatically upgrade if input is non-4:2:0 but a 4:2:0 profile
           was selected. */
        switch (stream->config.cfg.g_profile) {
          case 0:
            if (input.bit_depth < 12 && (input.fmt == AOM_IMG_FMT_I444 ||
                                         input.fmt == AOM_IMG_FMT_I44416)) {
              if (!stream->config.cfg.monochrome) {
                stream->config.cfg.g_profile = 1;
                profile_updated = 1;
              }
            } else if (input.bit_depth == 12 || input.fmt == AOM_IMG_FMT_I422 ||
                       input.fmt == AOM_IMG_FMT_I42216) {
              stream->config.cfg.g_profile = 2;
              profile_updated = 1;
            }
            break;
          case 1:
            if (input.bit_depth == 12 || input.fmt == AOM_IMG_FMT_I422 ||
                input.fmt == AOM_IMG_FMT_I42216) {
              stream->config.cfg.g_profile = 2;
              profile_updated = 1;
            } else if (input.bit_depth < 12 &&
                       (input.fmt == AOM_IMG_FMT_I420 ||
                        input.fmt == AOM_IMG_FMT_I42016)) {
              stream->config.cfg.g_profile = 0;
              profile_updated = 1;
            }
            break;
          case 2:
            if (input.bit_depth < 12 && (input.fmt == AOM_IMG_FMT_I444 ||
                                         input.fmt == AOM_IMG_FMT_I44416)) {
              stream->config.cfg.g_profile = 1;
              profile_updated = 1;
            } else if (input.bit_depth < 12 &&
                       (input.fmt == AOM_IMG_FMT_I420 ||
                        input.fmt == AOM_IMG_FMT_I42016)) {
              stream->config.cfg.g_profile = 0;
              profile_updated = 1;
            }
            break;
          default: break;
        }
      }
      /* Automatically set the codec bit depth to match the input bit depth.
       * Upgrade the profile if required. */
      if (stream->config.cfg.g_input_bit_depth >
          (unsigned int)stream->config.cfg.g_bit_depth) {
        stream->config.cfg.g_bit_depth = stream->config.cfg.g_input_bit_depth;
      }
      if (stream->config.cfg.g_bit_depth > 10) {
        switch (stream->config.cfg.g_profile) {
          case 0:
          case 1:
            stream->config.cfg.g_profile = 2;
            profile_updated = 1;
            break;
          default: break;
        }
      }
      if (stream->config.cfg.g_bit_depth > 8) {
        stream->config.use_16bit_internal = 1;
      }
      if (profile_updated && !global.quiet) {
        fprintf(stderr,
                "Warning: automatically updating to profile %d to "
                "match input format.\n",
                stream->config.cfg.g_profile);
      }
      /* Set limit */
      stream->config.cfg.g_limit = global.limit;
    }

    FOREACH_STREAM(stream, streams) {
      set_stream_dimensions(stream, input.width, input.height);
    }
    FOREACH_STREAM(stream, streams) { validate_stream_config(stream, &global); }

    /* Ensure that --passes and --pass are consistent. If --pass is set and
     * --passes=2, ensure --fpf was set.
     */
    if (global.pass && global.passes == 2) {
      FOREACH_STREAM(stream, streams) {
        if (!stream->config.stats_fn)
          die("Stream %d: Must specify --fpf when --pass=%d"
              " and --passes=2\n",
              stream->index, global.pass);
      }
    }

#if !CONFIG_WEBM_IO
    FOREACH_STREAM(stream, streams) {
      if (stream->config.write_webm) {
        stream->config.write_webm = 0;
        stream->config.write_ivf = 0;
        warn("aomenc compiled w/o WebM support. Writing OBU stream.");
      }
    }
#endif

    /* Use the frame rate from the file only if none was specified
     * on the command-line.
     */
    if (!global.have_framerate) {
      global.framerate.num = input.framerate.numerator;
      global.framerate.den = input.framerate.denominator;
    }
    FOREACH_STREAM(stream, streams) {
      stream->config.cfg.g_timebase.den = global.framerate.num;
      stream->config.cfg.g_timebase.num = global.framerate.den;
    }
    /* Show configuration */
    if (global.verbose && pass == 0) {
      FOREACH_STREAM(stream, streams) {
        show_stream_config(stream, &global, &input);
      }
    }

    if (pass == (global.pass ? global.pass - 1 : 0)) {
      if (input.file_type == FILE_TYPE_Y4M)
        /*The Y4M reader does its own allocation.
          Just initialize this here to avoid problems if we never read any
          frames.*/
        memset(&raw, 0, sizeof(raw));
      else
        aom_img_alloc(&raw, input.fmt, input.width, input.height, 32);

      FOREACH_STREAM(stream, streams) {
        stream->rate_hist =
            init_rate_histogram(&stream->config.cfg, &global.framerate);
      }
    }

    FOREACH_STREAM(stream, streams) { setup_pass(stream, &global, pass); }
    FOREACH_STREAM(stream, streams) {
      open_output_file(stream, &global, &input.pixel_aspect_ratio);
    }
    FOREACH_STREAM(stream, streams) { initialize_encoder(stream, &global); }
    if (strcmp(global.codec->name, "av1") == 0 ||
        strcmp(global.codec->name, "av1") == 0) {
      // Check to see if at least one stream uses 16 bit internal.
      // Currently assume that the bit_depths for all streams using
      // highbitdepth are the same.
      FOREACH_STREAM(stream, streams) {
        if (stream->config.use_16bit_internal) {
          use_16bit_internal = 1;
        }
        input_shift = (int)stream->config.cfg.g_bit_depth -
                      stream->config.cfg.g_input_bit_depth;
      };
    }

    frame_avail = 1;
    got_data = 0;

    while (frame_avail || got_data) {
      struct aom_usec_timer timer;

      if (!global.limit || frames_in < global.limit) {
        frame_avail = read_frame(&input, &raw);

        if (frame_avail) frames_in++;
        seen_frames =
            frames_in > global.skip_frames ? frames_in - global.skip_frames : 0;

        if (!global.quiet) {
          float fps = usec_to_fps(cx_time, seen_frames);
          fprintf(stderr, "\rPass %d/%d ", pass + 1, global.passes);

          if (stream_cnt == 1)
            fprintf(stderr, "frame %4d/%-4d %7" PRId64 "B ", frames_in,
                    streams->frames_out, (int64_t)streams->nbytes);
          else
            fprintf(stderr, "frame %4d ", frames_in);

          fprintf(stderr, "%7" PRId64 " %s %.2f %s ",
                  cx_time > 9999999 ? cx_time / 1000 : cx_time,
                  cx_time > 9999999 ? "ms" : "us", fps >= 1.0 ? fps : fps * 60,
                  fps >= 1.0 ? "fps" : "fpm");
          print_time("ETA", estimated_time_left);
        }

      } else {
        frame_avail = 0;
      }

      if (frames_in > global.skip_frames) {
        aom_image_t *frame_to_encode;
        if (input_shift || (use_16bit_internal && input.bit_depth == 8)) {
          assert(use_16bit_internal);
          // Input bit depth and stream bit depth do not match, so up
          // shift frame to stream bit depth
          if (!allocated_raw_shift) {
            aom_img_alloc(&raw_shift, raw.fmt | AOM_IMG_FMT_HIGHBITDEPTH,
                          input.width, input.height, 32);
            allocated_raw_shift = 1;
          }
          aom_img_upshift(&raw_shift, &raw, input_shift);
          frame_to_encode = &raw_shift;
        } else {
          frame_to_encode = &raw;
        }
        aom_usec_timer_start(&timer);
        if (use_16bit_internal) {
          assert(frame_to_encode->fmt & AOM_IMG_FMT_HIGHBITDEPTH);
          FOREACH_STREAM(stream, streams) {
            if (stream->config.use_16bit_internal)
              encode_frame(stream, &global,
                           frame_avail ? frame_to_encode : NULL, frames_in);
            else
              assert(0);
          };
        } else {
          assert((frame_to_encode->fmt & AOM_IMG_FMT_HIGHBITDEPTH) == 0);
          FOREACH_STREAM(stream, streams) {
            encode_frame(stream, &global, frame_avail ? frame_to_encode : NULL,
                         frames_in);
          }
        }
        aom_usec_timer_mark(&timer);
        cx_time += aom_usec_timer_elapsed(&timer);

        FOREACH_STREAM(stream, streams) { update_quantizer_histogram(stream); }

        got_data = 0;
        FOREACH_STREAM(stream, streams) {
          get_cx_data(stream, &global, &got_data);
        }

        if (!got_data && input.length && streams != NULL &&
            !streams->frames_out) {
          lagged_count = global.limit ? seen_frames : ftello(input.file);
        } else if (input.length) {
          int64_t remaining;
          int64_t rate;

          if (global.limit) {
            const int64_t frame_in_lagged = (seen_frames - lagged_count) * 1000;

            rate = cx_time ? frame_in_lagged * (int64_t)1000000 / cx_time : 0;
            remaining = 1000 * (global.limit - global.skip_frames -
                                seen_frames + lagged_count);
          } else {
            const int64_t input_pos = ftello(input.file);
            const int64_t input_pos_lagged = input_pos - lagged_count;
            const int64_t input_limit = input.length;

            rate = cx_time ? input_pos_lagged * (int64_t)1000000 / cx_time : 0;
            remaining = input_limit - input_pos + lagged_count;
          }

          average_rate =
              (average_rate <= 0) ? rate : (average_rate * 7 + rate) / 8;
          estimated_time_left = average_rate ? remaining / average_rate : -1;
        }

        if (got_data && global.test_decode != TEST_DECODE_OFF) {
          FOREACH_STREAM(stream, streams) {
            test_decode(stream, global.test_decode);
          }
        }
      }

      fflush(stdout);
      if (!global.quiet) fprintf(stderr, "\033[K");
    }

    if (stream_cnt > 1) fprintf(stderr, "\n");

    if (!global.quiet) {
      FOREACH_STREAM(stream, streams) {
        const int64_t bpf =
            seen_frames ? (int64_t)(stream->nbytes * 8 / seen_frames) : 0;
        const int64_t bps = bpf * global.framerate.num / global.framerate.den;
        fprintf(stderr,
                "\rPass %d/%d frame %4d/%-4d %7" PRId64 "B %7" PRId64
                "b/f %7" PRId64
                "b/s"
                " %7" PRId64 " %s (%.2f fps)\033[K\n",
                pass + 1, global.passes, frames_in, stream->frames_out,
                (int64_t)stream->nbytes, bpf, bps,
                stream->cx_time > 9999999 ? stream->cx_time / 1000
                                          : stream->cx_time,
                stream->cx_time > 9999999 ? "ms" : "us",
                usec_to_fps(stream->cx_time, seen_frames));
      }
    }

    if (global.show_psnr) {
      if (global.codec->fourcc == AV1_FOURCC) {
        FOREACH_STREAM(stream, streams) {
          int64_t bps = 0;
          if (stream->psnr_count && seen_frames && global.framerate.den) {
            bps = (int64_t)stream->nbytes * 8 * (int64_t)global.framerate.num /
                  global.framerate.den / seen_frames;
          }
          show_psnr(stream, (1 << stream->config.cfg.g_input_bit_depth) - 1,
                    bps);
        }
      } else {
        FOREACH_STREAM(stream, streams) { show_psnr(stream, 255.0, 0); }
      }
    }

    FOREACH_STREAM(stream, streams) { aom_codec_destroy(&stream->encoder); }

    if (global.test_decode != TEST_DECODE_OFF) {
      FOREACH_STREAM(stream, streams) { aom_codec_destroy(&stream->decoder); }
    }

    close_input_file(&input);

    if (global.test_decode == TEST_DECODE_FATAL) {
      FOREACH_STREAM(stream, streams) { res |= stream->mismatch_seen; }
    }
    FOREACH_STREAM(stream, streams) {
      close_output_file(stream, global.codec->fourcc);
    }

    FOREACH_STREAM(stream, streams) {
      stats_close(&stream->stats, global.passes - 1);
    }

#if CONFIG_FP_MB_STATS
    FOREACH_STREAM(stream, streams) {
      stats_close(&stream->fpmb_stats, global.passes - 1);
    }
#endif

    if (global.pass) break;
  }

  if (global.show_q_hist_buckets) {
    FOREACH_STREAM(stream, streams) {
      show_q_histogram(stream->counts, global.show_q_hist_buckets);
    }
  }

  if (global.show_rate_hist_buckets) {
    FOREACH_STREAM(stream, streams) {
      show_rate_histogram(stream->rate_hist, &stream->config.cfg,
                          global.show_rate_hist_buckets);
    }
  }
  FOREACH_STREAM(stream, streams) { destroy_rate_histogram(stream->rate_hist); }

#if CONFIG_INTERNAL_STATS
  /* TODO(jkoleszar): This doesn't belong in this executable. Do it for now,
   * to match some existing utilities.
   */
  if (!(global.pass == 1 && global.passes == 2)) {
    FOREACH_STREAM(stream, streams) {
      FILE *f = fopen("opsnr.stt", "a");
      if (stream->mismatch_seen) {
        fprintf(f, "First mismatch occurred in frame %d\n",
                stream->mismatch_seen);
      } else {
        fprintf(f, "No mismatch detected in recon buffers\n");
      }
      fclose(f);
    }
  }
#endif

  if (allocated_raw_shift) aom_img_free(&raw_shift);
  aom_img_free(&raw);
  free(argv);
  free(streams);
  return res ? EXIT_FAILURE : EXIT_SUCCESS;
}
