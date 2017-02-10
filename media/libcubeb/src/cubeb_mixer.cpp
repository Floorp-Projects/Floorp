/*
 * Copyright © 2016 Mozilla Foundation
 *
 * This program is made available under an ISC-style license.  See the
 * accompanying file LICENSE for details.
 */

#include <cassert>
#include "cubeb-internal.h"
#include "cubeb_mixer.h"

// DUAL_MONO(_LFE) is same as STEREO(_LFE).
#define MASK_MONO         (1 << CHANNEL_MONO)
#define MASK_MONO_LFE     (MASK_MONO | (1 << CHANNEL_LFE))
#define MASK_STEREO       ((1 << CHANNEL_LEFT) | (1 << CHANNEL_RIGHT))
#define MASK_STEREO_LFE   (MASK_STEREO | (1 << CHANNEL_LFE))
#define MASK_3F           (MASK_STEREO | (1 << CHANNEL_CENTER))
#define MASK_3F_LFE       (MASK_3F | (1 << CHANNEL_LFE))
#define MASK_2F1          (MASK_STEREO | (1 << CHANNEL_RCENTER))
#define MASK_2F1_LFE      (MASK_2F1 | (1 << CHANNEL_LFE))
#define MASK_3F1          (MASK_3F | (1 << CHANNEL_RCENTER))
#define MASK_3F1_LFE      (MASK_3F1 | (1 << CHANNEL_LFE))
#define MASK_2F2          (MASK_STEREO | (1 << CHANNEL_LS) | (1 << CHANNEL_RS))
#define MASK_2F2_LFE      (MASK_2F2 | (1 << CHANNEL_LFE))
#define MASK_3F2          (MASK_2F2 | (1 << CHANNEL_CENTER))
#define MASK_3F2_LFE      (MASK_3F2 | (1 << CHANNEL_LFE))
#define MASK_3F3R_LFE     (MASK_3F2_LFE | (1 << CHANNEL_RCENTER))
#define MASK_3F4_LFE      (MASK_3F2_LFE | (1 << CHANNEL_RLS) | (1 << CHANNEL_RRS))

cubeb_channel_layout cubeb_channel_map_to_layout(cubeb_channel_map const * channel_map)
{
  uint32_t channel_mask = 0;
  for (uint8_t i = 0 ; i < channel_map->channels ; ++i) {
    if (channel_map->map[i] == CHANNEL_INVALID) {
      return CUBEB_LAYOUT_UNDEFINED;
    }
    channel_mask |= 1 << channel_map->map[i];
  }

  switch(channel_mask) {
    case MASK_MONO: return CUBEB_LAYOUT_MONO;
    case MASK_MONO_LFE: return CUBEB_LAYOUT_MONO_LFE;
    case MASK_STEREO: return CUBEB_LAYOUT_STEREO;
    case MASK_STEREO_LFE: return CUBEB_LAYOUT_STEREO_LFE;
    case MASK_3F: return CUBEB_LAYOUT_3F;
    case MASK_3F_LFE: return CUBEB_LAYOUT_3F_LFE;
    case MASK_2F1: return CUBEB_LAYOUT_2F1;
    case MASK_2F1_LFE: return CUBEB_LAYOUT_2F1_LFE;
    case MASK_3F1: return CUBEB_LAYOUT_3F1;
    case MASK_3F1_LFE: return CUBEB_LAYOUT_3F1_LFE;
    case MASK_2F2: return CUBEB_LAYOUT_2F2;
    case MASK_2F2_LFE: return CUBEB_LAYOUT_2F2_LFE;
    case MASK_3F2: return CUBEB_LAYOUT_3F2;
    case MASK_3F2_LFE: return CUBEB_LAYOUT_3F2_LFE;
    case MASK_3F3R_LFE: return CUBEB_LAYOUT_3F3R_LFE;
    case MASK_3F4_LFE: return CUBEB_LAYOUT_3F4_LFE;
    default: return CUBEB_LAYOUT_UNDEFINED;
  }
}

cubeb_layout_map const CUBEB_CHANNEL_LAYOUT_MAPS[CUBEB_LAYOUT_MAX] = {
  { "undefined",      0,  CUBEB_LAYOUT_UNDEFINED },
  { "dual mono",      2,  CUBEB_LAYOUT_DUAL_MONO },
  { "dual mono lfe",  3,  CUBEB_LAYOUT_DUAL_MONO_LFE },
  { "mono",           1,  CUBEB_LAYOUT_MONO },
  { "mono lfe",       2,  CUBEB_LAYOUT_MONO_LFE },
  { "stereo",         2,  CUBEB_LAYOUT_STEREO },
  { "stereo lfe",     3,  CUBEB_LAYOUT_STEREO_LFE },
  { "3f",             3,  CUBEB_LAYOUT_3F },
  { "3f lfe",         4,  CUBEB_LAYOUT_3F_LFE },
  { "2f1",            3,  CUBEB_LAYOUT_2F1 },
  { "2f1 lfe",        4,  CUBEB_LAYOUT_2F1_LFE },
  { "3f1",            4,  CUBEB_LAYOUT_3F1 },
  { "3f1 lfe",        5,  CUBEB_LAYOUT_3F1_LFE },
  { "2f2",            4,  CUBEB_LAYOUT_2F2 },
  { "2f2 lfe",        5,  CUBEB_LAYOUT_2F2_LFE },
  { "3f2",            5,  CUBEB_LAYOUT_3F2 },
  { "3f2 lfe",        6,  CUBEB_LAYOUT_3F2_LFE },
  { "3f3r lfe",       7,  CUBEB_LAYOUT_3F3R_LFE },
  { "3f4 lfe",        8,  CUBEB_LAYOUT_3F4_LFE }
};

static int const CHANNEL_ORDER_TO_INDEX[CUBEB_LAYOUT_MAX][CHANNEL_MAX] = {
 // M | L | R | C | LS | RS | RLS | RC | RRS | LFE
  { -1, -1, -1, -1,  -1,  -1,   -1,  -1,   -1,  -1 }, // UNDEFINED
  { -1,  0,  1, -1,  -1,  -1,   -1,  -1,   -1,  -1 }, // DUAL_MONO
  { -1,  0,  1, -1,  -1,  -1,   -1,  -1,   -1,   2 }, // DUAL_MONO_LFE
  {  0, -1, -1, -1,  -1,  -1,   -1,  -1,   -1,  -1 }, // MONO
  {  0, -1, -1, -1,  -1,  -1,   -1,  -1,   -1,   1 }, // MONO_LFE
  { -1,  0,  1, -1,  -1,  -1,   -1,  -1,   -1,  -1 }, // STEREO
  { -1,  0,  1, -1,  -1,  -1,   -1,  -1,   -1,   2 }, // STEREO_LFE
  { -1,  0,  1,  2,  -1,  -1,   -1,  -1,   -1,  -1 }, // 3F
  { -1,  0,  1,  2,  -1,  -1,   -1,  -1,   -1,   3 }, // 3F_LFE
  { -1,  0,  1, -1,  -1,  -1,   -1,   2,   -1,  -1 }, // 2F1
  { -1,  0,  1, -1,  -1,  -1,   -1,   3,   -1,   2 }, // 2F1_LFE
  { -1,  0,  1,  2,  -1,  -1,   -1,   3,   -1,  -1 }, // 3F1
  { -1,  0,  1,  2,  -1,  -1,   -1,   4,   -1,   3 }, // 3F1_LFE
  { -1,  0,  1, -1,   2,   3,   -1,  -1,   -1,  -1 }, // 2F2
  { -1,  0,  1, -1,   3,   4,   -1,  -1,   -1,   2 }, // 2F2_LFE
  { -1,  0,  1,  2,   3,   4,   -1,  -1,   -1,  -1 }, // 3F2
  { -1,  0,  1,  2,   4,   5,   -1,  -1,   -1,   3 }, // 3F2_LFE
  { -1,  0,  1,  2,   5,   6,   -1,   4,   -1,   3 }, // 3F3R_LFE
  { -1,  0,  1,  2,   6,   7,    4,  -1,    5,   3 }, // 3F4_LFE
};

// The downmix matrix from TABLE 2 in the ITU-R BS.775-3[1] defines a way to
// convert 3F2 input data to 1F, 2F, 3F, 2F1, 3F1, 2F2 output data. We extend it
// to convert 3F2-LFE input data to 1F, 2F, 3F, 2F1, 3F1, 2F2 and their LFEs
// output data.
// [1] https://www.itu.int/dms_pubrec/itu-r/rec/bs/R-REC-BS.775-3-201208-I!!PDF-E.pdf

// Number of converted layouts: 1F, 2F, 3F, 2F1, 3F1, 2F2 and their LFEs.
unsigned int const SUPPORTED_LAYOUT_NUM = 12;
// Number of input channel for downmix conversion.
unsigned int const INPUT_CHANNEL_NUM = 6; // 3F2-LFE
// Max number of possible output channels.
unsigned int const MAX_OUTPUT_CHANNEL_NUM = 5; // 2F2-LFE or 3F1-LFE
float const INV_SQRT_2 = 0.707106f; // 1/sqrt(2)
// Each array contains coefficients that will be multiplied with
// { L, R, C, LFE, LS, RS } channels respectively.
static float const DOWNMIX_MATRIX_3F2_LFE[SUPPORTED_LAYOUT_NUM][MAX_OUTPUT_CHANNEL_NUM][INPUT_CHANNEL_NUM] =
{
// 1F Mono
  {
    { INV_SQRT_2, INV_SQRT_2, 1, 0, 0.5, 0.5 }, // M
  },
// 1F Mono-LFE
  {
    { INV_SQRT_2, INV_SQRT_2, 1, 0, 0.5, 0.5 }, // M
    { 0, 0, 0, 1, 0, 0 }                        // LFE
  },
// 2F Stereo
  {
    { 1, 0, INV_SQRT_2, 0, INV_SQRT_2, 0 },     // L
    { 0, 1, INV_SQRT_2, 0, 0, INV_SQRT_2 }      // R
  },
// 2F Stereo-LFE
  {
    { 1, 0, INV_SQRT_2, 0, INV_SQRT_2, 0 },     // L
    { 0, 1, INV_SQRT_2, 0, 0, INV_SQRT_2 },     // R
    { 0, 0, 0, 1, 0, 0 }                        // LFE
  },
// 3F
  {
    { 1, 0, 0, 0, INV_SQRT_2, 0 },              // L
    { 0, 1, 0, 0, 0, INV_SQRT_2 },              // R
    { 0, 0, 1, 0, 0, 0 }                        // C
  },
// 3F-LFE
  {
    { 1, 0, 0, 0, INV_SQRT_2, 0 },              // L
    { 0, 1, 0, 0, 0, INV_SQRT_2 },              // R
    { 0, 0, 1, 0, 0, 0 },                       // C
    { 0, 0, 0, 1, 0, 0 }                        // LFE
  },
// 2F1
  {
    { 1, 0, INV_SQRT_2, 0, 0, 0 },              // L
    { 0, 1, INV_SQRT_2, 0, 0, 0 },              // R
    { 0, 0, 0, 0, INV_SQRT_2, INV_SQRT_2 }      // S
  },
// 2F1-LFE
  {
    { 1, 0, INV_SQRT_2, 0, 0, 0 },              // L
    { 0, 1, INV_SQRT_2, 0, 0, 0 },              // R
    { 0, 0, 0, 1, 0, 0 },                       // LFE
    { 0, 0, 0, 0, INV_SQRT_2, INV_SQRT_2 }      // S
  },
// 3F1
  {
    { 1, 0, 0, 0, 0, 0 },                       // L
    { 0, 1, 0, 0, 0, 0 },                       // R
    { 0, 0, 1, 0, 0, 0 },                       // C
    { 0, 0, 0, 0, INV_SQRT_2, INV_SQRT_2 }      // S
  },
// 3F1-LFE
  {
    { 1, 0, 0, 0, 0, 0 },                       // L
    { 0, 1, 0, 0, 0, 0 },                       // R
    { 0, 0, 1, 0, 0, 0 },                       // C
    { 0, 0, 0, 1, 0, 0 },                       // LFE
    { 0, 0, 0, 0, INV_SQRT_2, INV_SQRT_2 }      // S
  },
// 2F2
  {
    { 1, 0, INV_SQRT_2, 0, 0, 0 },              // L
    { 0, 1, INV_SQRT_2, 0, 0, 0 },              // R
    { 0, 0, 0, 0, 1, 0 },                       // LS
    { 0, 0, 0, 0, 0, 1 }                        // RS
  },
// 2F2-LFE
  {
    { 1, 0, INV_SQRT_2, 0, 0, 0 },              // L
    { 0, 1, INV_SQRT_2, 0, 0, 0 },              // R
    { 0, 0, 0, 1, 0, 0 },                       // LFE
    { 0, 0, 0, 0, 1, 0 },                       // LS
    { 0, 0, 0, 0, 0, 1 }                        // RS
  }
};

/* Convert audio data from 3F2(-LFE) to 1F, 2F, 3F, 2F1, 3F1, 2F2 and their LFEs. */
template<typename T>
bool
downmix_3f2(T const * const in, unsigned long inframes, T * out, cubeb_channel_layout in_layout, cubeb_channel_layout out_layout)
{
  if ((in_layout != CUBEB_LAYOUT_3F2 && in_layout != CUBEB_LAYOUT_3F2_LFE) ||
      out_layout < CUBEB_LAYOUT_MONO || out_layout > CUBEB_LAYOUT_2F2_LFE) {
    return false;
  }

  unsigned int in_channels = CUBEB_CHANNEL_LAYOUT_MAPS[in_layout].channels;
  unsigned int out_channels = CUBEB_CHANNEL_LAYOUT_MAPS[out_layout].channels;

  // Conversion from 3F2 to 2F2-LFE or 3F1-LFE is allowed, so we use '<=' instead of '<'.
  assert(out_channels <= in_channels);

  long out_index = 0;
  auto & downmix_matrix = DOWNMIX_MATRIX_3F2_LFE[out_layout - CUBEB_LAYOUT_MONO]; // The matrix is started from mono.
  for (unsigned long i = 0; i < inframes * in_channels; i += in_channels) {
    for (unsigned int j = 0; j < out_channels; ++j) {
      out[out_index + j] = 0; // Clear its value.
      for (unsigned int k = 0 ; k < INPUT_CHANNEL_NUM ; ++k) {
        // 3F2-LFE has 6 channels: L, R, C, LFE, LS, RS, while 3F2 has only 5
        // channels: L, R, C, LS, RS. Thus, we need to append 0 to LFE(index 3)
        // to simulate a 3F2-LFE data when input layout is 3F2.
        T data = (in_layout == CUBEB_LAYOUT_3F2_LFE) ? in[i + k] : (k == 3) ? 0 : in[i + ((k < 3) ? k : k - 1)];
        out[out_index + j] += downmix_matrix[j][k] * data;
      }
    }
    out_index += out_channels;
  }

  return true;
}

/* Map the audio data by channel name. */
template<class T>
bool
mix_remap(T const * const in, unsigned long inframes, T * out, cubeb_channel_layout in_layout, cubeb_channel_layout out_layout) {
  assert(in_layout != out_layout);
  unsigned int in_channels = CUBEB_CHANNEL_LAYOUT_MAPS[in_layout].channels;
  unsigned int out_channels = CUBEB_CHANNEL_LAYOUT_MAPS[out_layout].channels;

  uint32_t in_layout_mask = 0;
  for (unsigned int i = 0 ; i < in_channels ; ++i) {
    in_layout_mask |= 1 << CHANNEL_INDEX_TO_ORDER[in_layout][i];
  }

  uint32_t out_layout_mask = 0;
  for (unsigned int i = 0 ; i < out_channels ; ++i) {
    out_layout_mask |= 1 << CHANNEL_INDEX_TO_ORDER[out_layout][i];
  }

  // If there is no matched channel, then do nothing.
  if (!(out_layout_mask & in_layout_mask)) {
    return false;
  }

  long out_index = 0;
  for (unsigned long i = 0; i < inframes * in_channels; i += in_channels) {
    for (unsigned int j = 0; j < out_channels; ++j) {
      cubeb_channel channel = CHANNEL_INDEX_TO_ORDER[out_layout][j];
      uint32_t channel_mask = 1 << channel;
      int channel_index = CHANNEL_ORDER_TO_INDEX[in_layout][channel];
      if (in_layout_mask & channel_mask) {
        assert(channel_index != -1);
        out[out_index + j] = in[i + channel_index];
      } else {
        assert(channel_index == -1);
        out[out_index + j] = 0;
      }
    }
    out_index += out_channels;
  }

  return true;
}

/* Drop the extra channels beyond the provided output channels. */
template<typename T>
void
downmix_fallback(T const * const in, unsigned long inframes, T * out, unsigned int in_channels, unsigned int out_channels)
{
  assert(in_channels >= out_channels);
  long out_index = 0;
  for (unsigned long i = 0; i < inframes * in_channels; i += in_channels) {
    for (unsigned int j = 0; j < out_channels; ++j) {
      out[out_index + j] = in[i + j];
    }
    out_index += out_channels;
  }
}


template<typename T>
void
cubeb_downmix(T const * const in, long inframes, T * out,
              unsigned int in_channels, unsigned int out_channels,
              cubeb_channel_layout in_layout, cubeb_channel_layout out_layout)
{
  assert(in_channels >= out_channels && in_layout != CUBEB_LAYOUT_UNDEFINED);

  // If the channel number is different from the layout's setting or it's not a
  // valid audio 5.1 downmix, then we use fallback downmix mechanism.
  if (out_channels == CUBEB_CHANNEL_LAYOUT_MAPS[out_layout].channels &&
      in_channels == CUBEB_CHANNEL_LAYOUT_MAPS[in_layout].channels) {
    if (downmix_3f2(in, inframes, out, in_layout, out_layout)) {
      return;
    }

    if (mix_remap(in, inframes, out, in_layout, out_layout)) {
      return;
    }
  }

  downmix_fallback(in, inframes, out, in_channels, out_channels);
}

/* Upmix function, copies a mono channel into L and R. */
template<typename T>
void
mono_to_stereo(T const * in, long insamples, T * out, unsigned int out_channels)
{
  for (long i = 0, j = 0; i < insamples; ++i, j += out_channels) {
    out[j] = out[j + 1] = in[i];
  }
}

template<typename T>
void
cubeb_upmix(T const * in, long inframes, T * out,
            unsigned int in_channels, unsigned int out_channels)
{
  assert(out_channels >= in_channels && in_channels > 0);

  /* Either way, if we have 2 or more channels, the first two are L and R. */
  /* If we are playing a mono stream over stereo speakers, copy the data over. */
  if (in_channels == 1 && out_channels >= 2) {
    mono_to_stereo(in, inframes, out, out_channels);
  } else {
    /* Copy through. */
    for (unsigned int i = 0, o = 0; i < inframes * in_channels;
        i += in_channels, o += out_channels) {
      for (unsigned int j = 0; j < in_channels; ++j) {
        out[o + j] = in[i + j];
      }
    }
  }

  /* Check if more channels. */
  if (out_channels <= 2) {
    return;
  }

  /* Put silence in remaining channels. */
  for (long i = 0, o = 0; i < inframes; ++i, o += out_channels) {
    for (unsigned int j = 2; j < out_channels; ++j) {
      out[o + j] = 0.0;
    }
  }
}

bool
cubeb_should_upmix(cubeb_stream_params const * stream, cubeb_stream_params const * mixer)
{
  return mixer->channels > stream->channels;
}

bool
cubeb_should_downmix(cubeb_stream_params const * stream, cubeb_stream_params const * mixer)
{
  if (mixer->channels > stream->channels || mixer->layout == stream->layout) {
    return false;
  }

  return mixer->channels < stream->channels ||
         // When mixer.channels == stream.channels
         mixer->layout == CUBEB_LAYOUT_UNDEFINED ||  // fallback downmix
         (stream->layout == CUBEB_LAYOUT_3F2 &&        // 3f2 downmix
          (mixer->layout == CUBEB_LAYOUT_2F2_LFE ||
           mixer->layout == CUBEB_LAYOUT_3F1_LFE));
}

void
cubeb_downmix_float(float * const in, long inframes, float * out,
                    unsigned int in_channels, unsigned int out_channels,
                    cubeb_channel_layout in_layout, cubeb_channel_layout out_layout)
{
  cubeb_downmix(in, inframes, out, in_channels, out_channels, in_layout, out_layout);
}

void
cubeb_upmix_float(float * const in, long inframes, float * out,
                  unsigned int in_channels, unsigned int out_channels)
{
  cubeb_upmix(in, inframes, out, in_channels, out_channels);
}
