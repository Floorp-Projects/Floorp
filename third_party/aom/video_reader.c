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

#include <stdlib.h>
#include <string.h>

#include "./ivfdec.h"
#include "./video_reader.h"

#include "aom_ports/mem_ops.h"

static const char *const kIVFSignature = "DKIF";

struct AvxVideoReaderStruct {
  AvxVideoInfo info;
  FILE *file;
  uint8_t *buffer;
  size_t buffer_size;
  size_t frame_size;
};

AvxVideoReader *aom_video_reader_open(const char *filename) {
  char header[32];
  AvxVideoReader *reader = NULL;
  FILE *const file = fopen(filename, "rb");
  if (!file) return NULL;  // Can't open file

  if (fread(header, 1, 32, file) != 32) return NULL;  // Can't read file header

  if (memcmp(kIVFSignature, header, 4) != 0)
    return NULL;  // Wrong IVF signature

  if (mem_get_le16(header + 4) != 0) return NULL;  // Wrong IVF version

  reader = (AvxVideoReader *)calloc(1, sizeof(*reader));
  if (!reader) return NULL;  // Can't allocate AvxVideoReader

  reader->file = file;
  reader->info.codec_fourcc = mem_get_le32(header + 8);
  reader->info.frame_width = mem_get_le16(header + 12);
  reader->info.frame_height = mem_get_le16(header + 14);
  reader->info.time_base.numerator = mem_get_le32(header + 16);
  reader->info.time_base.denominator = mem_get_le32(header + 20);

  return reader;
}

void aom_video_reader_close(AvxVideoReader *reader) {
  if (reader) {
    fclose(reader->file);
    free(reader->buffer);
    free(reader);
  }
}

int aom_video_reader_read_frame(AvxVideoReader *reader) {
  return !ivf_read_frame(reader->file, &reader->buffer, &reader->frame_size,
                         &reader->buffer_size);
}

const uint8_t *aom_video_reader_get_frame(AvxVideoReader *reader,
                                          size_t *size) {
  if (size) *size = reader->frame_size;

  return reader->buffer;
}

const AvxVideoInfo *aom_video_reader_get_info(AvxVideoReader *reader) {
  return &reader->info;
}
