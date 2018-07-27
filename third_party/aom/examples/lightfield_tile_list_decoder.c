/*
 * Copyright (c) 2018, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

// Lightfield Tile List Decoder
// ============================
//
// This is a lightfield tile list decoder example. It takes an input file that
// contains the anchor frames that are references of the coded tiles, the camera
// frame header, and tile list OBUs that include the tile information and the
// compressed tile data. This input file is reconstructed from the encoded
// lightfield ivf file, and is decodable by AV1 decoder. num_references is
// the number of anchor frames coded at the beginning of the light field file.
// num_tile_lists is the number of tile lists need to be decoded.
// Run lightfield tile list decoder to decode an AV1 tile list file:
// examples/lightfield_tile_list_decoder vase_tile_list.ivf vase_tile_list.yuv
// 4 2

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "aom/aom_decoder.h"
#include "aom/aomdx.h"
#include "common/tools_common.h"
#include "common/video_reader.h"

#define MAX_EXTERNAL_REFERENCES 128
#define AOM_BORDER_IN_PIXELS 288

static const char *exec_name;

void usage_exit(void) {
  fprintf(stderr,
          "Usage: %s <infile> <outfile> <num_references> <num_tile_lists>\n",
          exec_name);
  exit(EXIT_FAILURE);
}

int main(int argc, char **argv) {
  FILE *outfile = NULL;
  aom_codec_ctx_t codec;
  AvxVideoReader *reader = NULL;
  const AvxInterface *decoder = NULL;
  const AvxVideoInfo *info = NULL;
  int num_references;
  int num_tile_lists;
  aom_image_t reference_images[MAX_EXTERNAL_REFERENCES];
  size_t frame_size = 0;
  const unsigned char *frame = NULL;
  int i, j, n;

  exec_name = argv[0];

  if (argc != 5) die("Invalid number of arguments.");

  reader = aom_video_reader_open(argv[1]);
  if (!reader) die("Failed to open %s for reading.", argv[1]);

  if (!(outfile = fopen(argv[2], "wb")))
    die("Failed to open %s for writing.", argv[2]);

  num_references = (int)strtol(argv[3], NULL, 0);
  num_tile_lists = (int)strtol(argv[4], NULL, 0);

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

  // Decode the lightfield.
  aom_codec_control_(&codec, AV1_SET_TILE_MODE, 1);

  // Set external references.
  av1_ext_ref_frame_t set_ext_ref = { &reference_images[0], num_references };
  aom_codec_control_(&codec, AV1D_SET_EXT_REF_PTR, &set_ext_ref);
  // Must decode the camera frame header first.
  aom_video_reader_read_frame(reader);
  frame = aom_video_reader_get_frame(reader, &frame_size);
  if (aom_codec_decode(&codec, frame, frame_size, NULL))
    die_codec(&codec, "Failed to decode the frame.");
  // Decode tile lists one by one.
  for (n = 0; n < num_tile_lists; n++) {
    aom_video_reader_read_frame(reader);
    frame = aom_video_reader_get_frame(reader, &frame_size);

    if (aom_codec_decode(&codec, frame, frame_size, NULL))
      die_codec(&codec, "Failed to decode the tile list.");
    aom_codec_iter_t iter = NULL;
    aom_image_t *img;
    while ((img = aom_codec_get_frame(&codec, &iter)))
      fwrite(img->img_data, 1, img->sz, outfile);
  }

  for (i = 0; i < num_references; i++) aom_img_free(&reference_images[i]);
  if (aom_codec_destroy(&codec)) die_codec(&codec, "Failed to destroy codec");
  aom_video_reader_close(reader);
  fclose(outfile);

  return EXIT_SUCCESS;
}
