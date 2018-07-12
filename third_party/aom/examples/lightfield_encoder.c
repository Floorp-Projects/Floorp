/*
 * Copyright (c) 2017, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

// Lightfield Encoder
// ==================
//
// This is an example of a simple lightfield encoder.  It builds upon the
// twopass_encoder.c example. It takes an input file in YV12 format,
// treating it as a planar lightfield instead of a video. The img_width
// and img_height arguments are the dimensions of the lightfield images,
// while the lf_width and lf_height arguments are the number of
// lightfield images in each dimension. The lf_blocksize determines the
// number of reference images used for MCP. For example, 5 means that there
// is a reference image for every 5x5 lightfield image block. All images
// within a block will use the center image in that block as the reference
// image for MCP.
// Run "make test" to download lightfield test data: vase10x10.yuv.
// Run lightfield encoder to encode whole lightfield:
// examples/lightfield_encoder 1024 1024 vase10x10.yuv vase10x10.webm 10 10 5

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "aom/aom_encoder.h"
#include "aom/aomcx.h"
#include "av1/common/enums.h"

#include "../tools_common.h"
#include "../video_writer.h"

static const char *exec_name;
static const unsigned int deadline = AOM_DL_GOOD_QUALITY;

void usage_exit(void) {
  fprintf(stderr,
          "Usage: %s <img_width> <img_height> <infile> <outfile> "
          "<lf_width> <lf_height> <lf_blocksize>\n",
          exec_name);
  exit(EXIT_FAILURE);
}

static aom_image_t *aom_img_copy(aom_image_t *src, aom_image_t *dst) {
  dst = aom_img_alloc(dst, src->fmt, src->d_w, src->d_h, 16);

  int plane;

  for (plane = 0; plane < 3; ++plane) {
    unsigned char *src_buf = src->planes[plane];
    const int src_stride = src->stride[plane];
    const int src_w = plane == 0 ? src->d_w : src->d_w >> 1;
    const int src_h = plane == 0 ? src->d_h : src->d_h >> 1;

    unsigned char *dst_buf = dst->planes[plane];
    const int dst_stride = dst->stride[plane];
    int y;

    for (y = 0; y < src_h; ++y) {
      memcpy(dst_buf, src_buf, src_w);
      src_buf += src_stride;
      dst_buf += dst_stride;
    }
  }
  return dst;
}

static int aom_img_size_bytes(aom_image_t *img) {
  int image_size_bytes = 0;
  int plane;
  for (plane = 0; plane < 3; ++plane) {
    const int stride = img->stride[plane];
    const int w = aom_img_plane_width(img, plane) *
                  ((img->fmt & AOM_IMG_FMT_HIGHBITDEPTH) ? 2 : 1);
    const int h = aom_img_plane_height(img, plane);
    image_size_bytes += (w + stride) * h;
  }
  return image_size_bytes;
}

static int get_frame_stats(aom_codec_ctx_t *ctx, const aom_image_t *img,
                           aom_codec_pts_t pts, unsigned int duration,
                           aom_enc_frame_flags_t flags, unsigned int dl,
                           aom_fixed_buf_t *stats) {
  int got_pkts = 0;
  aom_codec_iter_t iter = NULL;
  const aom_codec_cx_pkt_t *pkt = NULL;
  const aom_codec_err_t res =
      aom_codec_encode(ctx, img, pts, duration, flags, dl);
  if (res != AOM_CODEC_OK) die_codec(ctx, "Failed to get frame stats.");

  while ((pkt = aom_codec_get_cx_data(ctx, &iter)) != NULL) {
    got_pkts = 1;

    if (pkt->kind == AOM_CODEC_STATS_PKT) {
      const uint8_t *const pkt_buf = pkt->data.twopass_stats.buf;
      const size_t pkt_size = pkt->data.twopass_stats.sz;
      stats->buf = realloc(stats->buf, stats->sz + pkt_size);
      memcpy((uint8_t *)stats->buf + stats->sz, pkt_buf, pkt_size);
      stats->sz += pkt_size;
    }
  }

  return got_pkts;
}

static int encode_frame(aom_codec_ctx_t *ctx, const aom_image_t *img,
                        aom_codec_pts_t pts, unsigned int duration,
                        aom_enc_frame_flags_t flags, unsigned int dl,
                        AvxVideoWriter *writer) {
  int got_pkts = 0;
  aom_codec_iter_t iter = NULL;
  const aom_codec_cx_pkt_t *pkt = NULL;
  const aom_codec_err_t res =
      aom_codec_encode(ctx, img, pts, duration, flags, dl);
  if (res != AOM_CODEC_OK) die_codec(ctx, "Failed to encode frame.");

  while ((pkt = aom_codec_get_cx_data(ctx, &iter)) != NULL) {
    got_pkts = 1;
    if (pkt->kind == AOM_CODEC_CX_FRAME_PKT) {
      const int keyframe = (pkt->data.frame.flags & AOM_FRAME_IS_KEY) != 0;

      if (!aom_video_writer_write_frame(writer, pkt->data.frame.buf,
                                        pkt->data.frame.sz,
                                        pkt->data.frame.pts))
        die_codec(ctx, "Failed to write compressed frame.");
      printf(keyframe ? "K" : ".");
      fflush(stdout);
    }
  }

  return got_pkts;
}

static aom_fixed_buf_t pass0(aom_image_t *raw, FILE *infile,
                             const AvxInterface *encoder,
                             const aom_codec_enc_cfg_t *cfg, int lf_width,
                             int lf_height, int lf_blocksize) {
  aom_codec_ctx_t codec;
  int frame_count = 0;
  int image_size_bytes = 0;
  int u_blocks, v_blocks;
  int bu, bv;
  aom_fixed_buf_t stats = { NULL, 0 };

  if (aom_codec_enc_init(&codec, encoder->codec_interface(), cfg, 0))
    die_codec(&codec, "Failed to initialize encoder");
  if (aom_codec_control(&codec, AV1E_SET_FRAME_PARALLEL_DECODING, 1))
    die_codec(&codec, "Failed to set frame parallel decoding");
  if (aom_codec_control(&codec, AOME_SET_ENABLEAUTOALTREF, 0))
    die_codec(&codec, "Failed to turn off auto altref");
  if (aom_codec_control(&codec, AV1E_SET_SINGLE_TILE_DECODING, 1))
    die_codec(&codec, "Failed to turn on single tile decoding");

  image_size_bytes = aom_img_size_bytes(raw);

  // How many reference images we need to encode.
  u_blocks = (lf_width + lf_blocksize - 1) / lf_blocksize;
  v_blocks = (lf_height + lf_blocksize - 1) / lf_blocksize;
  aom_image_t *reference_images =
      (aom_image_t *)malloc(u_blocks * v_blocks * sizeof(aom_image_t));
  for (bv = 0; bv < v_blocks; ++bv) {
    for (bu = 0; bu < u_blocks; ++bu) {
      const int block_u_min = bu * lf_blocksize;
      const int block_v_min = bv * lf_blocksize;
      int block_u_end = (bu + 1) * lf_blocksize;
      int block_v_end = (bv + 1) * lf_blocksize;
      int u_block_size, v_block_size;
      int block_ref_u, block_ref_v;
      struct av1_ref_frame ref_frame;

      block_u_end = block_u_end < lf_width ? block_u_end : lf_width;
      block_v_end = block_v_end < lf_height ? block_v_end : lf_height;
      u_block_size = block_u_end - block_u_min;
      v_block_size = block_v_end - block_v_min;
      block_ref_u = block_u_min + u_block_size / 2;
      block_ref_v = block_v_min + v_block_size / 2;
      fseek(infile, (block_ref_u + block_ref_v * lf_width) * image_size_bytes,
            SEEK_SET);
      aom_img_read(raw, infile);
      if (aom_codec_control(&codec, AOME_USE_REFERENCE,
                            AOM_LAST_FLAG | AOM_GOLD_FLAG | AOM_ALT_FLAG))
        die_codec(&codec, "Failed to set reference flags");
      // Reference frames can be encoded encoded without tiles.
      ++frame_count;
      get_frame_stats(&codec, raw, frame_count, 1,
                      AOM_EFLAG_FORCE_GF | AOM_EFLAG_NO_UPD_ENTROPY, deadline,
                      &stats);
      ref_frame.idx = 0;
      aom_codec_control(&codec, AV1_GET_REFERENCE, &ref_frame);
      aom_img_copy(&ref_frame.img, &reference_images[frame_count - 1]);
    }
  }
  for (bv = 0; bv < v_blocks; ++bv) {
    for (bu = 0; bu < u_blocks; ++bu) {
      const int block_u_min = bu * lf_blocksize;
      const int block_v_min = bv * lf_blocksize;
      int block_u_end = (bu + 1) * lf_blocksize;
      int block_v_end = (bv + 1) * lf_blocksize;
      int u, v;
      block_u_end = block_u_end < lf_width ? block_u_end : lf_width;
      block_v_end = block_v_end < lf_height ? block_v_end : lf_height;
      for (v = block_v_min; v < block_v_end; ++v) {
        for (u = block_u_min; u < block_u_end; ++u) {
          // This was a work around for a bug in libvpx.  I'm not sure if this
          // same bug exists in current version of av1.  Need to call this,
          // otherwise the default is to not use any reference frames.  Then
          // if you don't have at least one AOM_EFLAG_NO_REF_* flag, all frames
          // will be intra encoded.  I'm not sure why the default is not to use
          // any reference frames.  It looks like there is something about the
          // way I encode the reference frames above that sets that as
          // default...
          if (aom_codec_control(&codec, AOME_USE_REFERENCE,
                                AOM_LAST_FLAG | AOM_GOLD_FLAG | AOM_ALT_FLAG))
            die_codec(&codec, "Failed to set reference flags");

          // Set tile size to 64 pixels. The tile_columns and
          // tile_rows in the tile coding are overloaded to represent
          // tile_width and tile_height, that range from 1 to 64, in the unit
          // of 64 pixels.
          if (aom_codec_control(&codec, AV1E_SET_TILE_COLUMNS, 1))
            die_codec(&codec, "Failed to set tile width");
          if (aom_codec_control(&codec, AV1E_SET_TILE_ROWS, 1))
            die_codec(&codec, "Failed to set tile height");

          av1_ref_frame_t ref;
          ref.idx = 0;
          ref.img = reference_images[bv * u_blocks + bu];
          if (aom_codec_control(&codec, AV1_SET_REFERENCE, &ref))
            die_codec(&codec, "Failed to set reference frame");

          fseek(infile, (u + v * lf_width) * image_size_bytes, SEEK_SET);
          aom_img_read(raw, infile);
          ++frame_count;
          get_frame_stats(&codec, raw, frame_count, 1,
                          AOM_EFLAG_NO_UPD_LAST | AOM_EFLAG_NO_UPD_GF |
                              AOM_EFLAG_NO_UPD_ARF | AOM_EFLAG_NO_UPD_ENTROPY |
                              AOM_EFLAG_NO_REF_GF | AOM_EFLAG_NO_REF_ARF,
                          deadline, &stats);
        }
      }
    }
  }
  // Flush encoder.
  while (get_frame_stats(&codec, NULL, frame_count, 1, 0, deadline, &stats)) {
  }

  printf("Pass 0 complete. Processed %d frames.\n", frame_count);
  if (aom_codec_destroy(&codec)) die_codec(&codec, "Failed to destroy codec.");

  return stats;
}

static void pass1(aom_image_t *raw, FILE *infile, const char *outfile_name,
                  const AvxInterface *encoder, const aom_codec_enc_cfg_t *cfg,
                  int lf_width, int lf_height, int lf_blocksize) {
  AvxVideoInfo info = { encoder->fourcc,
                        cfg->g_w,
                        cfg->g_h,
                        { cfg->g_timebase.num, cfg->g_timebase.den } };
  AvxVideoWriter *writer = NULL;
  aom_codec_ctx_t codec;
  int frame_count = 0;
  int image_size_bytes;
  int bu, bv;
  int u_blocks, v_blocks;

  writer = aom_video_writer_open(outfile_name, kContainerIVF, &info);
  if (!writer) die("Failed to open %s for writing", outfile_name);

  if (aom_codec_enc_init(&codec, encoder->codec_interface(), cfg, 0))
    die_codec(&codec, "Failed to initialize encoder");
  if (aom_codec_control(&codec, AV1E_SET_FRAME_PARALLEL_DECODING, 1))
    die_codec(&codec, "Failed to set frame parallel decoding");
  if (aom_codec_control(&codec, AOME_SET_ENABLEAUTOALTREF, 0))
    die_codec(&codec, "Failed to turn off auto altref");
  if (aom_codec_control(&codec, AV1E_SET_SINGLE_TILE_DECODING, 1))
    die_codec(&codec, "Failed to turn on single tile decoding");

  image_size_bytes = aom_img_size_bytes(raw);
  u_blocks = (lf_width + lf_blocksize - 1) / lf_blocksize;
  v_blocks = (lf_height + lf_blocksize - 1) / lf_blocksize;
  aom_image_t *reference_images =
      (aom_image_t *)malloc(u_blocks * v_blocks * sizeof(aom_image_t));
  // Encode reference images first.
  printf("Encoding Reference Images\n");
  for (bv = 0; bv < v_blocks; ++bv) {
    for (bu = 0; bu < u_blocks; ++bu) {
      const int block_u_min = bu * lf_blocksize;
      const int block_v_min = bv * lf_blocksize;
      int block_u_end = (bu + 1) * lf_blocksize;
      int block_v_end = (bv + 1) * lf_blocksize;
      int u_block_size, v_block_size;
      int block_ref_u, block_ref_v;
      struct av1_ref_frame ref_frame;

      block_u_end = block_u_end < lf_width ? block_u_end : lf_width;
      block_v_end = block_v_end < lf_height ? block_v_end : lf_height;
      u_block_size = block_u_end - block_u_min;
      v_block_size = block_v_end - block_v_min;
      block_ref_u = block_u_min + u_block_size / 2;
      block_ref_v = block_v_min + v_block_size / 2;
      fseek(infile, (block_ref_u + block_ref_v * lf_width) * image_size_bytes,
            SEEK_SET);
      aom_img_read(raw, infile);
      if (aom_codec_control(&codec, AOME_USE_REFERENCE,
                            AOM_LAST_FLAG | AOM_GOLD_FLAG | AOM_ALT_FLAG))
        die_codec(&codec, "Failed to set reference flags");
      // Reference frames may be encoded without tiles.
      ++frame_count;
      printf("Encoding reference image %d of %d\n", bv * u_blocks + bu,
             u_blocks * v_blocks);
      encode_frame(&codec, raw, frame_count, 1,
                   AOM_EFLAG_FORCE_GF | AOM_EFLAG_NO_UPD_ENTROPY, deadline,
                   writer);
      ref_frame.idx = 0;
      aom_codec_control(&codec, AV1_GET_REFERENCE, &ref_frame);
      aom_img_copy(&ref_frame.img, &reference_images[frame_count - 1]);
    }
  }

  for (bv = 0; bv < v_blocks; ++bv) {
    for (bu = 0; bu < u_blocks; ++bu) {
      const int block_u_min = bu * lf_blocksize;
      const int block_v_min = bv * lf_blocksize;
      int block_u_end = (bu + 1) * lf_blocksize;
      int block_v_end = (bv + 1) * lf_blocksize;
      int u, v;
      block_u_end = block_u_end < lf_width ? block_u_end : lf_width;
      block_v_end = block_v_end < lf_height ? block_v_end : lf_height;
      for (v = block_v_min; v < block_v_end; ++v) {
        for (u = block_u_min; u < block_u_end; ++u) {
          // This was a work around for a bug in libvpx.  I'm not sure if this
          // same bug exists in current version of av1.  Need to call this,
          // otherwise the default is to not use any reference frames.  Then
          // if you don't have at least one AOM_EFLAG_NO_REF_* flag, all frames
          // will be intra encoded.  I'm not sure why the default is not to use
          // any reference frames.  It looks like there is something about the
          // way I encode the reference frames above that sets that as
          // default...
          if (aom_codec_control(&codec, AOME_USE_REFERENCE,
                                AOM_LAST_FLAG | AOM_GOLD_FLAG | AOM_ALT_FLAG))
            die_codec(&codec, "Failed to set reference flags");

          // Set tile size to 64 pixels. The tile_columns and
          // tile_rows in the tile coding are overloaded to represent tile_width
          // and tile_height, that range from 1 to 64, in the unit of 64 pixels.
          if (aom_codec_control(&codec, AV1E_SET_TILE_COLUMNS, 1))
            die_codec(&codec, "Failed to set tile width");
          if (aom_codec_control(&codec, AV1E_SET_TILE_ROWS, 1))
            die_codec(&codec, "Failed to set tile height");

          av1_ref_frame_t ref;
          ref.idx = 0;
          ref.img = reference_images[bv * u_blocks + bu];
          if (aom_codec_control(&codec, AV1_SET_REFERENCE, &ref))
            die_codec(&codec, "Failed to set reference frame");
          fseek(infile, (u + v * lf_width) * image_size_bytes, SEEK_SET);
          aom_img_read(raw, infile);
          ++frame_count;

          printf("Encoding image %d of %d\n",
                 frame_count - (u_blocks * v_blocks), lf_width * lf_height);
          encode_frame(&codec, raw, frame_count, 1,
                       AOM_EFLAG_NO_UPD_LAST | AOM_EFLAG_NO_UPD_GF |
                           AOM_EFLAG_NO_UPD_ARF | AOM_EFLAG_NO_UPD_ENTROPY |
                           AOM_EFLAG_NO_REF_GF | AOM_EFLAG_NO_REF_ARF,
                       deadline, writer);
        }
      }
    }
  }

  // Flush encoder.
  while (encode_frame(&codec, NULL, -1, 1, 0, deadline, writer)) {
  }

  if (aom_codec_destroy(&codec)) die_codec(&codec, "Failed to destroy codec.");

  aom_video_writer_close(writer);

  printf("Pass 1 complete. Processed %d frames.\n", frame_count);
}

int main(int argc, char **argv) {
  FILE *infile = NULL;
  int w, h;
  // The number of lightfield images in the u and v dimensions.
  int lf_width, lf_height;
  // Defines how many images refer to the same reference image for MCP.
  // lf_blocksize X lf_blocksize images will all use the reference image
  // in the middle of the block of images.
  int lf_blocksize;
  aom_codec_ctx_t codec;
  aom_codec_enc_cfg_t cfg;
  aom_image_t raw;
  aom_codec_err_t res;
  aom_fixed_buf_t stats;

  const AvxInterface *encoder = NULL;
  const int fps = 30;
  const int bitrate = 200;  // kbit/s
  const char *const width_arg = argv[1];
  const char *const height_arg = argv[2];
  const char *const infile_arg = argv[3];
  const char *const outfile_arg = argv[4];
  const char *const lf_width_arg = argv[5];
  const char *const lf_height_arg = argv[6];
  const char *lf_blocksize_arg = argv[7];
  exec_name = argv[0];

  if (argc < 8) die("Invalid number of arguments");

  encoder = get_aom_encoder_by_name("av1");
  if (!encoder) die("Unsupported codec.");

  w = (int)strtol(width_arg, NULL, 0);
  h = (int)strtol(height_arg, NULL, 0);
  lf_width = (int)strtol(lf_width_arg, NULL, 0);
  lf_height = (int)strtol(lf_height_arg, NULL, 0);
  lf_blocksize = (int)strtol(lf_blocksize_arg, NULL, 0);
  lf_blocksize = lf_blocksize < lf_width ? lf_blocksize : lf_width;
  lf_blocksize = lf_blocksize < lf_height ? lf_blocksize : lf_height;

  if (w <= 0 || h <= 0 || (w % 2) != 0 || (h % 2) != 0)
    die("Invalid frame size: %dx%d", w, h);
  if (lf_width <= 0 || lf_height <= 0)
    die("Invalid lf_width and/or lf_height: %dx%d", lf_width, lf_height);
  if (lf_blocksize <= 0) die("Invalid lf_blocksize: %d", lf_blocksize);

  if (!aom_img_alloc(&raw, AOM_IMG_FMT_I420, w, h, 1))
    die("Failed to allocate image", w, h);

  printf("Using %s\n", aom_codec_iface_name(encoder->codec_interface()));

  // Configuration
  res = aom_codec_enc_config_default(encoder->codec_interface(), &cfg, 0);

  if (res) die_codec(&codec, "Failed to get default codec config.");

  cfg.g_w = w;
  cfg.g_h = h;
  cfg.g_timebase.num = 1;
  cfg.g_timebase.den = fps;
  cfg.rc_target_bitrate = bitrate;
  cfg.g_error_resilient = AOM_ERROR_RESILIENT_DEFAULT;
  // Need to set lag_in_frames to 1 or 0.  Otherwise the frame flags get
  // overridden after the first frame in encode_frame_to_data_rate() (see where
  // get_frame_flags() is called).
  cfg.g_lag_in_frames = 0;
  cfg.kf_mode = AOM_KF_DISABLED;
  cfg.large_scale_tile = 1;

  if (!(infile = fopen(infile_arg, "rb")))
    die("Failed to open %s for reading", infile_arg);

  // Pass 0
  cfg.g_pass = AOM_RC_FIRST_PASS;
  stats = pass0(&raw, infile, encoder, &cfg, lf_width, lf_height, lf_blocksize);

  // Pass 1
  rewind(infile);
  cfg.g_pass = AOM_RC_LAST_PASS;
  cfg.rc_twopass_stats_in = stats;
  pass1(&raw, infile, outfile_arg, encoder, &cfg, lf_width, lf_height,
        lf_blocksize);
  free(stats.buf);

  aom_img_free(&raw);
  fclose(infile);

  return EXIT_SUCCESS;
}
