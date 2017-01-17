// Copyright (c) 2016 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#include "hdr_util.h"

#include <cstddef>
#include <new>

#include "mkvparser/mkvparser.h"

namespace libwebm {
bool CopyPrimaryChromaticity(const mkvparser::PrimaryChromaticity& parser_pc,
                             PrimaryChromaticityPtr* muxer_pc) {
  muxer_pc->reset(new (std::nothrow)
                      mkvmuxer::PrimaryChromaticity(parser_pc.x, parser_pc.y));
  if (!muxer_pc->get())
    return false;
  return true;
}

bool MasteringMetadataValuePresent(double value) {
  return value != mkvparser::MasteringMetadata::kValueNotPresent;
}

bool CopyMasteringMetadata(const mkvparser::MasteringMetadata& parser_mm,
                           mkvmuxer::MasteringMetadata* muxer_mm) {
  if (MasteringMetadataValuePresent(parser_mm.luminance_max))
    muxer_mm->luminance_max = parser_mm.luminance_max;
  if (MasteringMetadataValuePresent(parser_mm.luminance_min))
    muxer_mm->luminance_min = parser_mm.luminance_min;

  PrimaryChromaticityPtr r_ptr(NULL);
  PrimaryChromaticityPtr g_ptr(NULL);
  PrimaryChromaticityPtr b_ptr(NULL);
  PrimaryChromaticityPtr wp_ptr(NULL);

  if (parser_mm.r) {
    if (!CopyPrimaryChromaticity(*parser_mm.r, &r_ptr))
      return false;
  }
  if (parser_mm.g) {
    if (!CopyPrimaryChromaticity(*parser_mm.g, &g_ptr))
      return false;
  }
  if (parser_mm.b) {
    if (!CopyPrimaryChromaticity(*parser_mm.b, &b_ptr))
      return false;
  }
  if (parser_mm.white_point) {
    if (!CopyPrimaryChromaticity(*parser_mm.white_point, &wp_ptr))
      return false;
  }

  if (!muxer_mm->SetChromaticity(r_ptr.get(), g_ptr.get(), b_ptr.get(),
                                 wp_ptr.get())) {
    return false;
  }

  return true;
}

bool ColourValuePresent(long long value) {
  return value != mkvparser::Colour::kValueNotPresent;
}

bool CopyColour(const mkvparser::Colour& parser_colour,
                mkvmuxer::Colour* muxer_colour) {
  if (!muxer_colour)
    return false;

  if (ColourValuePresent(parser_colour.matrix_coefficients))
    muxer_colour->matrix_coefficients = parser_colour.matrix_coefficients;
  if (ColourValuePresent(parser_colour.bits_per_channel))
    muxer_colour->bits_per_channel = parser_colour.bits_per_channel;
  if (ColourValuePresent(parser_colour.chroma_subsampling_horz))
    muxer_colour->chroma_subsampling_horz =
        parser_colour.chroma_subsampling_horz;
  if (ColourValuePresent(parser_colour.chroma_subsampling_vert))
    muxer_colour->chroma_subsampling_vert =
        parser_colour.chroma_subsampling_vert;
  if (ColourValuePresent(parser_colour.cb_subsampling_horz))
    muxer_colour->cb_subsampling_horz = parser_colour.cb_subsampling_horz;
  if (ColourValuePresent(parser_colour.cb_subsampling_vert))
    muxer_colour->cb_subsampling_vert = parser_colour.cb_subsampling_vert;
  if (ColourValuePresent(parser_colour.chroma_siting_horz))
    muxer_colour->chroma_siting_horz = parser_colour.chroma_siting_horz;
  if (ColourValuePresent(parser_colour.chroma_siting_vert))
    muxer_colour->chroma_siting_vert = parser_colour.chroma_siting_vert;
  if (ColourValuePresent(parser_colour.range))
    muxer_colour->range = parser_colour.range;
  if (ColourValuePresent(parser_colour.transfer_characteristics))
    muxer_colour->transfer_characteristics =
        parser_colour.transfer_characteristics;
  if (ColourValuePresent(parser_colour.primaries))
    muxer_colour->primaries = parser_colour.primaries;
  if (ColourValuePresent(parser_colour.max_cll))
    muxer_colour->max_cll = parser_colour.max_cll;
  if (ColourValuePresent(parser_colour.max_fall))
    muxer_colour->max_fall = parser_colour.max_fall;

  if (parser_colour.mastering_metadata) {
    mkvmuxer::MasteringMetadata muxer_mm;
    if (!CopyMasteringMetadata(*parser_colour.mastering_metadata, &muxer_mm))
      return false;
    if (!muxer_colour->SetMasteringMetadata(muxer_mm))
      return false;
  }
  return true;
}

// Format of VPx private data:
//
//   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |    ID Byte    |             Length            |               |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+               |
//  |                                                               |
//  :               Bytes 1..Length of Codec Feature                :
//  |                                                               |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//
// ID Byte Format
// ID byte is an unsigned byte.
//   0 1 2 3 4 5 6 7
//  +-+-+-+-+-+-+-+-+
//  |X|    ID       |
//  +-+-+-+-+-+-+-+-+
//
// The X bit is reserved.
//
// Currently only profile level is supported. ID byte must be set to 1, and
// length must be 1. Supported values are:
//
//   10: Level 1
//   11: Level 1.1
//   20: Level 2
//   21: Level 2.1
//   30: Level 3
//   31: Level 3.1
//   40: Level 4
//   41: Level 4.1
//   50: Level 5
//   51: Level 5.1
//   52: Level 5.2
//   60: Level 6
//   61: Level 6.1
//   62: Level 6.2
//
// See the following link for more information:
// http://www.webmproject.org/vp9/profiles/
int ParseVpxCodecPrivate(const uint8_t* private_data, int32_t length) {
  const int kVpxCodecPrivateLength = 3;
  if (!private_data || length != kVpxCodecPrivateLength)
    return 0;

  const uint8_t id_byte = *private_data;
  if (id_byte != 1)
    return 0;

  const int kVpxProfileLength = 1;
  const uint8_t length_byte = private_data[1];
  if (length_byte != kVpxProfileLength)
    return 0;

  const int level = static_cast<int>(private_data[2]);

  const int kNumLevels = 14;
  const int levels[kNumLevels] = {10, 11, 20, 21, 30, 31, 40,
                                  41, 50, 51, 52, 60, 61, 62};

  for (int i = 0; i < kNumLevels; ++i) {
    if (level == levels[i])
      return level;
  }

  return 0;
}
}  // namespace libwebm
