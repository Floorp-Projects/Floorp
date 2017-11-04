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

// Lightfield Decoder
// ==================
//
// This is an example of a simple lightfield decoder. It builds upon the
// simple_decoder.c example.  It takes an input file containing the compressed
// data (in webm format), treating it as a lightfield instead of a video and
// will decode a single lightfield tile. The lf_width and lf_height arguments
// are the number of lightfield images in each dimension. The tile to decode
// is specified by the tile_u, tile_v, tile_s, tile_t arguments. The tile_u,
// tile_v specify the image and tile_s, tile_t specify the tile in the image.
// After running the lightfield encoder, run lightfield decoder to decode a
// single tile:
// examples/lightfield_decoder vase10x10.webm vase_tile.yuv 10 10 3 4 5 10 5

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "aom/aom_decoder.h"
#include "aom/aomdx.h"

#include "../tools_common.h"
#include "../video_reader.h"
#include "./aom_config.h"

static const char *exec_name;

void usage_exit(void) {
  fprintf(stderr,
          "Usage: %s <infile> <outfile> <lf_width> <lf_height> <tlie_u>"
          " <tile_v> <tile_s> <tile_t> <lf_blocksize>\n",
          exec_name);
  exit(EXIT_FAILURE);
}

aom_image_t *aom_img_copy(aom_image_t *src, aom_image_t *dst) {
  dst = aom_img_alloc(dst, src->fmt, src->d_w, src->d_h, 16);

  int plane;

  for (plane = 0; plane < 3; ++plane) {
    uint8_t *src_buf = src->planes[plane];
    const int src_stride = src->stride[plane];
    const int src_w = plane == 0 ? src->d_w : src->d_w >> 1;
    const int src_h = plane == 0 ? src->d_h : src->d_h >> 1;

    uint8_t *dst_buf = dst->planes[plane];
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

int main(int argc, char **argv) {
  int frame_cnt = 0;
  FILE *outfile = NULL;
  aom_codec_ctx_t codec;
  AvxVideoReader *reader = NULL;
  const AvxInterface *decoder = NULL;
  const AvxVideoInfo *info = NULL;
  const char *lf_width_arg;
  const char *lf_height_arg;
  const char *tile_u_arg;
  const char *tile_v_arg;
  const char *tile_s_arg;
  const char *tile_t_arg;
  const char *lf_blocksize_arg;
  int lf_width, lf_height;
  int tile_u, tile_v, tile_s, tile_t;
  int lf_blocksize;
  int u_blocks;
  int v_blocks;

  exec_name = argv[0];

  if (argc != 10) die("Invalid number of arguments.");

  reader = aom_video_reader_open(argv[1]);
  if (!reader) die("Failed to open %s for reading.", argv[1]);

  if (!(outfile = fopen(argv[2], "wb")))
    die("Failed to open %s for writing.", argv[2]);

  lf_width_arg = argv[3];
  lf_height_arg = argv[4];
  tile_u_arg = argv[5];
  tile_v_arg = argv[6];
  tile_s_arg = argv[7];
  tile_t_arg = argv[8];
  lf_blocksize_arg = argv[9];
  lf_width = (int)strtol(lf_width_arg, NULL, 0);
  lf_height = (int)strtol(lf_height_arg, NULL, 0);
  tile_u = (int)strtol(tile_u_arg, NULL, 0);
  tile_v = (int)strtol(tile_v_arg, NULL, 0);
  tile_s = (int)strtol(tile_s_arg, NULL, 0);
  tile_t = (int)strtol(tile_t_arg, NULL, 0);
  lf_blocksize = (int)strtol(lf_blocksize_arg, NULL, 0);

  info = aom_video_reader_get_info(reader);

  decoder = get_aom_decoder_by_fourcc(info->codec_fourcc);
  if (!decoder) die("Unknown input codec.");

  printf("Using %s\n", aom_codec_iface_name(decoder->codec_interface()));

  if (aom_codec_dec_init(&codec, decoder->codec_interface(), NULL, 0))
    die_codec(&codec, "Failed to initialize decoder.");

  // How many reference images we need to encode.
  u_blocks = (lf_width + lf_blocksize - 1) / lf_blocksize;
  v_blocks = (lf_height + lf_blocksize - 1) / lf_blocksize;
  aom_image_t *reference_images =
      (aom_image_t *)malloc(u_blocks * v_blocks * sizeof(aom_image_t));
  for (int bv = 0; bv < v_blocks; ++bv) {
    for (int bu = 0; bu < u_blocks; ++bu) {
      aom_video_reader_read_frame(reader);
      aom_codec_iter_t iter = NULL;
      aom_image_t *img = NULL;
      size_t frame_size = 0;
      const unsigned char *frame =
          aom_video_reader_get_frame(reader, &frame_size);
      if (aom_codec_decode(&codec, frame, (unsigned int)frame_size, NULL, 0))
        die_codec(&codec, "Failed to decode frame.");

      while ((img = aom_codec_get_frame(&codec, &iter)) != NULL) {
        aom_img_copy(img, &reference_images[bu + bv * u_blocks]);
        char name[1024];
        snprintf(name, sizeof(name), "ref_%d_%d.yuv", bu, bv);
        printf("writing ref image to %s, %d, %d\n", name, img->d_w, img->d_h);
        FILE *ref_file = fopen(name, "wb");
        aom_img_write(img, ref_file);
        fclose(ref_file);
        ++frame_cnt;
      }
    }
  }

  int decode_frame_index = tile_v * lf_width + tile_u;
  do {
    aom_video_reader_read_frame(reader);
  } while (frame_cnt++ != decode_frame_index);
  size_t frame_size = 0;
  const unsigned char *frame = aom_video_reader_get_frame(reader, &frame_size);

  int ref_bu = tile_u / lf_blocksize;
  int ref_bv = tile_v / lf_blocksize;
  int ref_bi = ref_bu + ref_bv * u_blocks;
  av1_ref_frame_t ref;
  ref.idx = 0;
  ref.img = reference_images[ref_bi];
  // This is too slow for real lightfield rendering.  This copies the
  // reference image bytes.  We need a way to just set a pointer
  // in order to make this fast enough.
  if (aom_codec_control(&codec, AV1_SET_REFERENCE, &ref)) {
    die_codec(&codec, "Failed to set reference image.");
  }
  aom_codec_control_(&codec, AV1_SET_DECODE_TILE_ROW, tile_t);
  aom_codec_control_(&codec, AV1_SET_DECODE_TILE_COL, tile_s);
  aom_codec_err_t aom_status =
      aom_codec_decode(&codec, frame, frame_size, NULL, 0);
  if (aom_status) die_codec(&codec, "Failed to decode tile.");
  aom_codec_iter_t iter = NULL;
  aom_image_t *img = aom_codec_get_frame(&codec, &iter);
  aom_img_write(img, outfile);

  if (aom_codec_destroy(&codec)) die_codec(&codec, "Failed to destroy codec");
  aom_video_reader_close(reader);
  fclose(outfile);

  return EXIT_SUCCESS;
}
