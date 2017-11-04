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

// AV1 Set Reference Frame
// ============================
//
// This is an example demonstrating how to overwrite the AV1 encoder's
// internal reference frame. In the sample we set the last frame to the
// current frame. This technique could be used to bounce between two cameras.
//
// The decoder would also have to set the reference frame to the same value
// on the same frame, or the video will become corrupt. The 'test_decode'
// variable is set to 1 in this example that tests if the encoder and decoder
// results are matching.
//
// Usage
// -----
// This example encodes a raw video. And the last argument passed in specifies
// the frame number to update the reference frame on. For example, run
// examples/aom_cx_set_ref av1 352 288 in.yuv out.ivf 4 30
// The parameter is parsed as follows:
//
//
// Extra Variables
// ---------------
// This example maintains the frame number passed on the command line
// in the `update_frame_num` variable.
//
//
// Configuration
// -------------
//
// The reference frame is updated on the frame specified on the command
// line.
//
// Observing The Effects
// ---------------------
// The encoder and decoder results should be matching when the same reference
// frame setting operation is done in both encoder and decoder. Otherwise,
// the encoder/decoder mismatch would be seen.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "aom/aomcx.h"
#include "aom/aom_decoder.h"
#include "aom/aom_encoder.h"
#include "examples/encoder_util.h"
#include "./tools_common.h"
#include "./video_writer.h"

static const char *exec_name;

void usage_exit() {
  fprintf(stderr,
          "Usage: %s <codec> <width> <height> <infile> <outfile> "
          "<frame> <limit(optional)>\n",
          exec_name);
  exit(EXIT_FAILURE);
}

static void testing_decode(aom_codec_ctx_t *encoder, aom_codec_ctx_t *decoder,
                           unsigned int frame_out, int *mismatch_seen) {
  aom_image_t enc_img, dec_img;
  struct av1_ref_frame ref_enc, ref_dec;

  if (*mismatch_seen) return;

  ref_enc.idx = 0;
  ref_dec.idx = 0;
  if (aom_codec_control(encoder, AV1_GET_REFERENCE, &ref_enc))
    die_codec(encoder, "Failed to get encoder reference frame");
  enc_img = ref_enc.img;
  if (aom_codec_control(decoder, AV1_GET_REFERENCE, &ref_dec))
    die_codec(decoder, "Failed to get decoder reference frame");
  dec_img = ref_dec.img;

  if (!aom_compare_img(&enc_img, &dec_img)) {
    int y[4], u[4], v[4];

    *mismatch_seen = 1;

    aom_find_mismatch(&enc_img, &dec_img, y, u, v);
    printf(
        "Encode/decode mismatch on frame %d at"
        " Y[%d, %d] {%d/%d},"
        " U[%d, %d] {%d/%d},"
        " V[%d, %d] {%d/%d}",
        frame_out, y[0], y[1], y[2], y[3], u[0], u[1], u[2], u[3], v[0], v[1],
        v[2], v[3]);
  }

  aom_img_free(&enc_img);
  aom_img_free(&dec_img);
}

static int encode_frame(aom_codec_ctx_t *ecodec, aom_image_t *img,
                        unsigned int frame_in, AvxVideoWriter *writer,
                        int test_decode, aom_codec_ctx_t *dcodec,
                        unsigned int *frame_out, int *mismatch_seen) {
  int got_pkts = 0;
  aom_codec_iter_t iter = NULL;
  const aom_codec_cx_pkt_t *pkt = NULL;
  int got_data;
  const aom_codec_err_t res =
      aom_codec_encode(ecodec, img, frame_in, 1, 0, AOM_DL_GOOD_QUALITY);
  if (res != AOM_CODEC_OK) die_codec(ecodec, "Failed to encode frame");

  got_data = 0;

  while ((pkt = aom_codec_get_cx_data(ecodec, &iter)) != NULL) {
    got_pkts = 1;

    if (pkt->kind == AOM_CODEC_CX_FRAME_PKT) {
      const int keyframe = (pkt->data.frame.flags & AOM_FRAME_IS_KEY) != 0;

      if (!(pkt->data.frame.flags & AOM_FRAME_IS_FRAGMENT)) {
        *frame_out += 1;
      }

      if (!aom_video_writer_write_frame(writer, pkt->data.frame.buf,
                                        pkt->data.frame.sz,
                                        pkt->data.frame.pts)) {
        die_codec(ecodec, "Failed to write compressed frame");
      }
      printf(keyframe ? "K" : ".");
      fflush(stdout);
      got_data = 1;

      // Decode 1 frame.
      if (test_decode) {
        if (aom_codec_decode(dcodec, pkt->data.frame.buf,
                             (unsigned int)pkt->data.frame.sz, NULL, 0))
          die_codec(dcodec, "Failed to decode frame.");
      }
    }
  }

  // Mismatch checking
  if (got_data && test_decode) {
    testing_decode(ecodec, dcodec, *frame_out, mismatch_seen);
  }

  return got_pkts;
}

int main(int argc, char **argv) {
  FILE *infile = NULL;
  // Encoder
  aom_codec_ctx_t ecodec;
  aom_codec_enc_cfg_t cfg;
  unsigned int frame_in = 0;
  aom_image_t raw;
  aom_codec_err_t res;
  AvxVideoInfo info;
  AvxVideoWriter *writer = NULL;
  const AvxInterface *encoder = NULL;

  // Test encoder/decoder mismatch.
  int test_decode = 1;
  // Decoder
  aom_codec_ctx_t dcodec;
  unsigned int frame_out = 0;

  // The frame number to set reference frame on
  unsigned int update_frame_num = 0;
  int mismatch_seen = 0;

  const int fps = 30;
  const int bitrate = 500;

  const char *codec_arg = NULL;
  const char *width_arg = NULL;
  const char *height_arg = NULL;
  const char *infile_arg = NULL;
  const char *outfile_arg = NULL;
  const char *update_frame_num_arg = NULL;
  unsigned int limit = 0;
  exec_name = argv[0];

  // Clear explicitly, as simply assigning "{ 0 }" generates
  // "missing-field-initializers" warning in some compilers.
  memset(&ecodec, 0, sizeof(ecodec));
  memset(&cfg, 0, sizeof(cfg));
  memset(&info, 0, sizeof(info));

  if (argc < 7) die("Invalid number of arguments");

  codec_arg = argv[1];
  width_arg = argv[2];
  height_arg = argv[3];
  infile_arg = argv[4];
  outfile_arg = argv[5];
  update_frame_num_arg = argv[6];

  encoder = get_aom_encoder_by_name(codec_arg);
  if (!encoder) die("Unsupported codec.");

  update_frame_num = (unsigned int)strtoul(update_frame_num_arg, NULL, 0);
  // In AV1, the reference buffers (cm->buffer_pool->frame_bufs[i].buf) are
  // allocated while calling aom_codec_encode(), thus, setting reference for
  // 1st frame isn't supported.
  if (update_frame_num <= 1) {
    die("Couldn't parse frame number '%s'\n", update_frame_num_arg);
  }

  if (argc > 7) {
    limit = (unsigned int)strtoul(argv[7], NULL, 0);
    if (update_frame_num > limit)
      die("Update frame number couldn't larger than limit\n");
  }

  info.codec_fourcc = encoder->fourcc;
  info.frame_width = (int)strtol(width_arg, NULL, 0);
  info.frame_height = (int)strtol(height_arg, NULL, 0);
  info.time_base.numerator = 1;
  info.time_base.denominator = fps;

  if (info.frame_width <= 0 || info.frame_height <= 0 ||
      (info.frame_width % 2) != 0 || (info.frame_height % 2) != 0) {
    die("Invalid frame size: %dx%d", info.frame_width, info.frame_height);
  }

  if (!aom_img_alloc(&raw, AOM_IMG_FMT_I420, info.frame_width,
                     info.frame_height, 1)) {
    die("Failed to allocate image.");
  }

  printf("Using %s\n", aom_codec_iface_name(encoder->codec_interface()));

  res = aom_codec_enc_config_default(encoder->codec_interface(), &cfg, 0);
  if (res) die_codec(&ecodec, "Failed to get default codec config.");

  cfg.g_w = info.frame_width;
  cfg.g_h = info.frame_height;
  cfg.g_timebase.num = info.time_base.numerator;
  cfg.g_timebase.den = info.time_base.denominator;
  cfg.rc_target_bitrate = bitrate;
  cfg.g_lag_in_frames = 3;

  writer = aom_video_writer_open(outfile_arg, kContainerIVF, &info);
  if (!writer) die("Failed to open %s for writing.", outfile_arg);

  if (!(infile = fopen(infile_arg, "rb")))
    die("Failed to open %s for reading.", infile_arg);

  if (aom_codec_enc_init(&ecodec, encoder->codec_interface(), &cfg, 0))
    die_codec(&ecodec, "Failed to initialize encoder");

  // Disable alt_ref.
  if (aom_codec_control(&ecodec, AOME_SET_ENABLEAUTOALTREF, 0))
    die_codec(&ecodec, "Failed to set enable auto alt ref");

  if (test_decode) {
    const AvxInterface *decoder = get_aom_decoder_by_name(codec_arg);
    if (aom_codec_dec_init(&dcodec, decoder->codec_interface(), NULL, 0))
      die_codec(&dcodec, "Failed to initialize decoder.");
  }

  // Encode frames.
  while (aom_img_read(&raw, infile)) {
    if (limit && frame_in >= limit) break;
    if (update_frame_num > 1 && frame_out + 1 == update_frame_num) {
      av1_ref_frame_t ref;
      ref.idx = 0;
      ref.img = raw;
      // Set reference frame in encoder.
      if (aom_codec_control(&ecodec, AV1_SET_REFERENCE, &ref))
        die_codec(&ecodec, "Failed to set reference frame");
      printf(" <SET_REF>");

      // If set_reference in decoder is commented out, the enc/dec mismatch
      // would be seen.
      if (test_decode) {
        if (aom_codec_control(&dcodec, AV1_SET_REFERENCE, &ref))
          die_codec(&dcodec, "Failed to set reference frame");
      }
    }

    encode_frame(&ecodec, &raw, frame_in, writer, test_decode, &dcodec,
                 &frame_out, &mismatch_seen);
    frame_in++;
    if (mismatch_seen) break;
  }

  // Flush encoder.
  if (!mismatch_seen)
    while (encode_frame(&ecodec, NULL, frame_in, writer, test_decode, &dcodec,
                        &frame_out, &mismatch_seen)) {
    }

  printf("\n");
  fclose(infile);
  printf("Processed %d frames.\n", frame_out);

  if (test_decode) {
    if (!mismatch_seen)
      printf("Encoder/decoder results are matching.\n");
    else
      printf("Encoder/decoder results are NOT matching.\n");
  }

  if (test_decode)
    if (aom_codec_destroy(&dcodec))
      die_codec(&dcodec, "Failed to destroy decoder");

  aom_img_free(&raw);
  if (aom_codec_destroy(&ecodec))
    die_codec(&ecodec, "Failed to destroy encoder.");

  aom_video_writer_close(writer);

  return EXIT_SUCCESS;
}
