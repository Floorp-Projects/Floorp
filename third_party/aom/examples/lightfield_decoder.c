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
// data (in ivf format), treating it as a lightfield instead of a video.
// After running the lightfield encoder, run lightfield decoder to decode a
// batch of tiles:
// examples/lightfield_decoder vase10x10.ivf vase_reference.yuv 4

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
  fprintf(stderr, "Usage: %s <infile> <outfile> <num_references>\n", exec_name);
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
  int num_references;
  aom_image_t reference_images[MAX_EXTERNAL_REFERENCES];
  size_t frame_size = 0;
  const unsigned char *frame = NULL;
  int n, i, j;
  exec_name = argv[0];

  if (argc != 4) die("Invalid number of arguments.");

  reader = aom_video_reader_open(argv[1]);
  if (!reader) die("Failed to open %s for reading.", argv[1]);

  if (!(outfile = fopen(argv[2], "wb")))
    die("Failed to open %s for writing.", argv[2]);

  num_references = (int)strtol(argv[3], NULL, 0);

  info = aom_video_reader_get_info(reader);

  decoder = get_aom_decoder_by_fourcc(info->codec_fourcc);
  if (!decoder) die("Unknown input codec.");
  printf("Using %s\n", aom_codec_iface_name(decoder->codec_interface()));

  if (aom_codec_dec_init(&codec, decoder->codec_interface(), NULL, 0))
    die_codec(&codec, "Failed to initialize decoder.");

  if (aom_codec_control(&codec, AV1D_SET_IS_ANNEXB, info->is_annexb)) {
    die("Failed to set annex b status");
  }

  // Decode anchor frames.
  aom_codec_control_(&codec, AV1_SET_TILE_MODE, 0);
  for (i = 0; i < num_references; ++i) {
    aom_video_reader_read_frame(reader);
    frame = aom_video_reader_get_frame(reader, &frame_size);
    if (aom_codec_decode(&codec, frame, frame_size, NULL))
      die_codec(&codec, "Failed to decode frame.");

    if (i == 0) {
      aom_img_fmt_t ref_fmt = 0;
      if (aom_codec_control(&codec, AV1D_GET_IMG_FORMAT, &ref_fmt))
        die_codec(&codec, "Failed to get the image format");

      int frame_res[2];
      if (aom_codec_control(&codec, AV1D_GET_FRAME_SIZE, frame_res))
        die_codec(&codec, "Failed to get the image frame size");

      // Allocate memory to store decoded references. Allocate memory with the
      // border so that it can be used as a reference.
      for (j = 0; j < num_references; j++) {
        unsigned int border = AOM_BORDER_IN_PIXELS;
        if (!aom_img_alloc_with_border(&reference_images[j], ref_fmt,
                                       frame_res[0], frame_res[1], 32, 8,
                                       border)) {
          die("Failed to allocate references.");
        }
      }
    }

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
