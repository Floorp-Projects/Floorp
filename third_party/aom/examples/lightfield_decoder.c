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
// data (in ivf format), treating it as a lightfield instead of a video and
// will decode a single lightfield tile. The lf_width and lf_height arguments
// are the number of lightfield images in each dimension. The tile to decode
// is specified by the tile_u, tile_v, tile_s, tile_t arguments. The tile_u,
// tile_v specify the image and tile_s, tile_t specify the tile in the image.
// After running the lightfield encoder, run lightfield decoder to decode a
// single tile:
// examples/lightfield_decoder vase10x10.ivf vase_reference.yuv 10 10 5

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "aom/aom_decoder.h"
#include "aom/aomdx.h"
#include "common/tools_common.h"
#include "common/video_reader.h"

#define MAX_EXTERNAL_REFERENCES 128
#define AOM_BORDER_IN_PIXELS 288

static const char *exec_name;

void usage_exit(void) {
  fprintf(
      stderr,
      "Usage: %s <infile> <outfile> <lf_width> <lf_height> <lf_blocksize>\n",
      exec_name);
  exit(EXIT_FAILURE);
}

// Tile list entry provided by the application
typedef struct {
  int image_idx;
  int reference_idx;
  int tile_col;
  int tile_row;
} TILE_LIST_INFO;

// M references: 0 - M-1; N images(including references): 0 - N-1;
// Note: order the image index incrementally, so that we only go through the
// bitstream once to construct the tile list.
const int num_tile_lists = 2;
const uint16_t tile_count_minus_1 = 9 - 1;
const TILE_LIST_INFO tile_list[2][9] = {
  { { 16, 0, 4, 5 },
    { 83, 3, 13, 2 },
    { 57, 2, 2, 6 },
    { 31, 1, 11, 5 },
    { 2, 0, 7, 4 },
    { 77, 3, 9, 9 },
    { 49, 1, 0, 1 },
    { 6, 0, 3, 10 },
    { 63, 2, 5, 8 } },
  { { 65, 2, 11, 1 },
    { 42, 1, 3, 7 },
    { 88, 3, 8, 4 },
    { 76, 3, 1, 15 },
    { 1, 0, 2, 2 },
    { 19, 0, 5, 6 },
    { 60, 2, 4, 0 },
    { 25, 1, 11, 15 },
    { 50, 2, 5, 4 } },
};

int main(int argc, char **argv) {
  FILE *outfile = NULL;
  aom_codec_ctx_t codec;
  AvxVideoReader *reader = NULL;
  const AvxInterface *decoder = NULL;
  const AvxVideoInfo *info = NULL;
  const char *lf_width_arg;
  const char *lf_height_arg;
  const char *lf_blocksize_arg;
  int width, height;
  int lf_width, lf_height;
  int lf_blocksize;
  int u_blocks;
  int v_blocks;
  aom_image_t reference_images[MAX_EXTERNAL_REFERENCES];
  size_t frame_size = 0;
  const unsigned char *frame = NULL;
  int n, i;

  exec_name = argv[0];

  if (argc != 6) die("Invalid number of arguments.");

  reader = aom_video_reader_open(argv[1]);
  if (!reader) die("Failed to open %s for reading.", argv[1]);

  if (!(outfile = fopen(argv[2], "wb")))
    die("Failed to open %s for writing.", argv[2]);

  lf_width_arg = argv[3];
  lf_height_arg = argv[4];
  lf_blocksize_arg = argv[5];
  lf_width = (int)strtol(lf_width_arg, NULL, 0);
  lf_height = (int)strtol(lf_height_arg, NULL, 0);
  lf_blocksize = (int)strtol(lf_blocksize_arg, NULL, 0);

  info = aom_video_reader_get_info(reader);
  width = info->frame_width;
  height = info->frame_height;

  decoder = get_aom_decoder_by_fourcc(info->codec_fourcc);
  if (!decoder) die("Unknown input codec.");
  printf("Using %s\n", aom_codec_iface_name(decoder->codec_interface()));

  if (aom_codec_dec_init(&codec, decoder->codec_interface(), NULL, 0))
    die_codec(&codec, "Failed to initialize decoder.");

  // How many anchor frames we have.
  u_blocks = (lf_width + lf_blocksize - 1) / lf_blocksize;
  v_blocks = (lf_height + lf_blocksize - 1) / lf_blocksize;

  int num_references = v_blocks * u_blocks;

  // Allocate memory to store decoded references.
  aom_img_fmt_t ref_fmt = AOM_IMG_FMT_I420;
  if (!CONFIG_LOWBITDEPTH) ref_fmt |= AOM_IMG_FMT_HIGHBITDEPTH;
  // Allocate memory with the border so that it can be used as a reference.
  for (i = 0; i < num_references; i++) {
    unsigned int border = AOM_BORDER_IN_PIXELS;
    if (!aom_img_alloc_with_border(&reference_images[i], ref_fmt, width, height,
                                   32, 8, border)) {
      die("Failed to allocate references.");
    }
  }

  // Decode anchor frames.
  aom_codec_control_(&codec, AV1_SET_TILE_MODE, 0);

  for (i = 0; i < num_references; ++i) {
    aom_video_reader_read_frame(reader);
    frame = aom_video_reader_get_frame(reader, &frame_size);
    if (aom_codec_decode(&codec, frame, frame_size, NULL))
      die_codec(&codec, "Failed to decode frame.");

    if (aom_codec_control(&codec, AV1_COPY_NEW_FRAME_IMAGE,
                          &reference_images[i]))
      die_codec(&codec, "Failed to copy decoded reference frame");

    aom_codec_iter_t iter = NULL;
    aom_image_t *img = NULL;
    while ((img = aom_codec_get_frame(&codec, &iter)) != NULL) {
      char name[1024];
      snprintf(name, sizeof(name), "ref_%d.yuv", i);
      printf("writing ref image to %s, %d, %d\n", name, img->d_w, img->d_h);
      FILE *ref_file = fopen(name, "wb");
      aom_img_write(img, ref_file);
      fclose(ref_file);
    }
  }

  FILE *infile = aom_video_reader_get_file(reader);
  // Record the offset of the first camera image.
  const FileOffset camera_frame_pos = ftello(infile);

  // Process 1 tile.
  for (n = 0; n < num_tile_lists; n++) {
    for (i = 0; i <= tile_count_minus_1; i++) {
      int image_idx = tile_list[n][i].image_idx;
      int ref_idx = tile_list[n][i].reference_idx;
      int tc = tile_list[n][i].tile_col;
      int tr = tile_list[n][i].tile_row;
      int frame_cnt = -1;

      // Seek to the first camera image.
      fseeko(infile, camera_frame_pos, SEEK_SET);

      // Read out the camera image
      while (frame_cnt != image_idx) {
        aom_video_reader_read_frame(reader);
        frame_cnt++;
      }

      frame = aom_video_reader_get_frame(reader, &frame_size);

      aom_codec_control_(&codec, AV1_SET_TILE_MODE, 1);
      aom_codec_control_(&codec, AV1D_EXT_TILE_DEBUG, 1);
      aom_codec_control_(&codec, AV1_SET_DECODE_TILE_ROW, tr);
      aom_codec_control_(&codec, AV1_SET_DECODE_TILE_COL, tc);

      av1_ref_frame_t ref;
      ref.idx = 0;
      ref.use_external_ref = 1;
      ref.img = reference_images[ref_idx];
      if (aom_codec_control(&codec, AV1_SET_REFERENCE, &ref)) {
        die_codec(&codec, "Failed to set reference frame.");
      }

      aom_codec_err_t aom_status =
          aom_codec_decode(&codec, frame, frame_size, NULL);
      if (aom_status) die_codec(&codec, "Failed to decode tile.");

      aom_codec_iter_t iter = NULL;
      aom_image_t *img = aom_codec_get_frame(&codec, &iter);
      aom_img_write(img, outfile);
    }
  }

  for (i = 0; i < num_references; i++) aom_img_free(&reference_images[i]);
  if (aom_codec_destroy(&codec)) die_codec(&codec, "Failed to destroy codec");
  aom_video_reader_close(reader);
  fclose(outfile);

  return EXIT_SUCCESS;
}
