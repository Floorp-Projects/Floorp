/*
 * Copyright © 2016 Mozilla Foundation
 *
 * This program is made available under an ISC-style license.  See the
 * accompanying file LICENSE for details.
 */
#include "gtest/gtest.h"
#include "cubeb/cubeb.h"
#include "cubeb_mixer.h"
#include "common.h"
#include <memory>
#include <vector>

using std::vector;

#define STREAM_FREQUENCY 48000
#define STREAM_FORMAT CUBEB_SAMPLE_FLOAT32LE

float const M = 1.0f;     // Mono
float const L = 2.0f;     // Left
float const R = 3.0f;     // Right
float const C = 4.0f;     // Center
float const LS = 5.0f;    // Left Surround
float const RS = 6.0f;    // Right Surround
float const RLS = 7.0f;   // Rear Left Surround
float const RC = 8.0f;    // Rear Center
float const RRS = 9.0f;   // Rear Right Surround
float const LFE = 10.0f;  // Low Frequency Effects

float const INV_SQRT_2 = 0.707106f; // 1/sqrt(2)
static float const DOWNMIX_3F2_RESULTS[2][12][5] = {
  // 3F2
  {
    { INV_SQRT_2*(L+R) + C + 0.5f*(LS+RS) },                          // Mono
    { INV_SQRT_2*(L+R) + C + 0.5f*(LS+RS), 0 },                       // Mono-LFE
    { L + INV_SQRT_2*(C+LS), R + INV_SQRT_2*(C+RS) },                 // Stereo
    { L + INV_SQRT_2*(C+LS), R + INV_SQRT_2*(C+RS), 0 },              // Stereo-LFE
    { L + INV_SQRT_2*LS, R + INV_SQRT_2*RS, C },                      // 3F
    { L + INV_SQRT_2*LS, R + INV_SQRT_2*RS, C, 0 },                   // 3F-LFE
    { L + C*INV_SQRT_2, R + C*INV_SQRT_2, INV_SQRT_2*(LS+RS) },       // 2F1
    { L + C*INV_SQRT_2, R + C*INV_SQRT_2, 0, INV_SQRT_2*(LS+RS) },    // 2F1-LFE
    { L, R, C, INV_SQRT_2*(LS+RS) },                                  // 3F1
    { L, R, C, 0, INV_SQRT_2*(LS+RS) },                               // 3F1-LFE
    { L + INV_SQRT_2*C, R + INV_SQRT_2*C, LS, RS },                   // 2F2
    { L + INV_SQRT_2*C, R + INV_SQRT_2*C, 0, LS, RS }                 // 2F2-LFE
  },
  // 3F2-LFE
  {
    { INV_SQRT_2*(L+R) + C + 0.5f*(LS+RS) },                          // Mono
    { INV_SQRT_2*(L+R) + C + 0.5f*(LS+RS), LFE },                     // Mono-LFE
    { L + INV_SQRT_2*(C+LS), R + INV_SQRT_2*(C+RS) },                 // Stereo
    { L + INV_SQRT_2*(C+LS), R + INV_SQRT_2*(C+RS), LFE },            // Stereo-LFE
    { L + INV_SQRT_2*LS, R + INV_SQRT_2*RS, C },                      // 3F
    { L + INV_SQRT_2*LS, R + INV_SQRT_2*RS, C, LFE },                 // 3F-LFE
    { L + C*INV_SQRT_2, R + C*INV_SQRT_2, INV_SQRT_2*(LS+RS) },       // 2F1
    { L + C*INV_SQRT_2, R + C*INV_SQRT_2, LFE, INV_SQRT_2*(LS+RS) },  // 2F1-LFE
    { L, R, C, INV_SQRT_2*(LS+RS) },                                  // 3F1
    { L, R, C, LFE, INV_SQRT_2*(LS+RS) },                             // 3F1-LFE
    { L + INV_SQRT_2*C, R + INV_SQRT_2*C, LS, RS },                   // 2F2
    { L + INV_SQRT_2*C, R + INV_SQRT_2*C, LFE, LS, RS }               // 2F2-LFE
  }
};

typedef struct {
  cubeb_channel_layout layout;
  float data[10];
} audio_input;

audio_input audio_inputs[CUBEB_LAYOUT_MAX] = {
  { CUBEB_LAYOUT_UNDEFINED,     { } },
  { CUBEB_LAYOUT_DUAL_MONO,     { L, R } },
  { CUBEB_LAYOUT_DUAL_MONO_LFE, { L, R, LFE } },
  { CUBEB_LAYOUT_MONO,          { M } },
  { CUBEB_LAYOUT_MONO_LFE,      { M, LFE } },
  { CUBEB_LAYOUT_STEREO,        { L, R } },
  { CUBEB_LAYOUT_STEREO_LFE,    { L, R, LFE } },
  { CUBEB_LAYOUT_3F,            { L, R, C } },
  { CUBEB_LAYOUT_3F_LFE,        { L, R, C, LFE } },
  { CUBEB_LAYOUT_2F1,           { L, R, RC } },
  { CUBEB_LAYOUT_2F1_LFE,       { L, R, LFE, RC } },
  { CUBEB_LAYOUT_3F1,           { L, R, C, RC } },
  { CUBEB_LAYOUT_3F1_LFE,       { L, R, C, LFE, RC } },
  { CUBEB_LAYOUT_2F2,           { L, R, LS, RS } },
  { CUBEB_LAYOUT_2F2_LFE,       { L, R, LFE, LS, RS } },
  { CUBEB_LAYOUT_3F2,           { L, R, C, LS, RS } },
  { CUBEB_LAYOUT_3F2_LFE,       { L, R, C, LFE, LS, RS } },
  { CUBEB_LAYOUT_3F3R_LFE,      { L, R, C, LFE, RC, LS, RS } },
  { CUBEB_LAYOUT_3F4_LFE,       { L, R, C, LFE, RLS, RRS, LS, RS } }
};

char const * channel_names[CHANNEL_UNMAPPED + 1] = {
  "mono",                   // CHANNEL_MONO
  "left",                   // CHANNEL_LEFT
  "right",                  // CHANNEL_RIGHT
  "center",                 // CHANNEL_CENTER
  "left surround",          // CHANNEL_LS
  "right surround",         // CHANNEL_RS
  "rear left surround",     // CHANNEL_RLS
  "rear center",            // CHANNEL_RCENTER
  "rear right surround",    // CHANNEL_RRS
  "low frequency effects",  // CHANNEL_LFE
  "unmapped"                // CHANNEL_UNMAPPED
};

// The test cases must be aligned with cubeb_downmix.
void
downmix_test(float const * data, cubeb_channel_layout in_layout, cubeb_channel_layout out_layout)
{
  if (in_layout == CUBEB_LAYOUT_UNDEFINED) {
    return; // Only possible output layout would be UNDEFINED.
  }

  cubeb_stream_params in_params = {
    STREAM_FORMAT,
    STREAM_FREQUENCY,
    layout_infos[in_layout].channels,
    in_layout
#if defined(__ANDROID__)
    , CUBEB_STREAM_TYPE_MUSIC
#endif
  };

  cubeb_stream_params out_params = {
    STREAM_FORMAT,
    STREAM_FREQUENCY,
    // To downmix audio data with undefined layout, its channel number must be
    // smaller than or equal to the input channels.
    (out_layout == CUBEB_LAYOUT_UNDEFINED) ?
      layout_infos[in_layout].channels : layout_infos[out_layout].channels,
    out_layout
#if defined(__ANDROID__)
    , CUBEB_STREAM_TYPE_MUSIC
#endif
   };

  if (!cubeb_should_downmix(&in_params, &out_params)) {
    return;
  }

  fprintf(stderr, "Downmix from %s to %s\n", layout_infos[in_layout].name, layout_infos[out_layout].name);

  unsigned int const inframes = 10;
  vector<float> in(in_params.channels * inframes);
#if defined(__APPLE__)
  // The mixed buffer size doesn't be changed based on the channel layout set on OSX.
  // Please see the comment above downmix_3f2 in cubeb_mixer.cpp.
  vector<float> out(in_params.channels * inframes);
#else
  // In normal case, the mixed buffer size is based on the mixing channel layout.
  vector<float> out(out_params.channels * inframes);
#endif

  for (unsigned int offset = 0 ; offset < inframes * in_params.channels ; offset += in_params.channels) {
    for (unsigned int i = 0 ; i < in_params.channels ; ++i) {
      in[offset + i] = data[i];
    }
  }

  // Create a mixer for downmix only.
  std::unique_ptr<cubeb_mixer, decltype(&cubeb_mixer_destroy)>
    mixer(cubeb_mixer_create(in_params.format, CUBEB_MIXER_DIRECTION_DOWNMIX), cubeb_mixer_destroy);

  assert(!in.empty() && !out.empty() && out.size() <= in.size());
  cubeb_mixer_mix(mixer.get(), inframes, in.data(), in.size(), out.data(), out.size(), &in_params, &out_params);

  uint32_t in_layout_mask = 0;
  for (unsigned int i = 0 ; i < in_params.channels; ++i) {
    in_layout_mask |= 1 << CHANNEL_INDEX_TO_ORDER[in_layout][i];
  }

  uint32_t out_layout_mask = 0;
  for (unsigned int i = 0 ; out_layout != CUBEB_LAYOUT_UNDEFINED && i < out_params.channels; ++i) {
    out_layout_mask |= 1 << CHANNEL_INDEX_TO_ORDER[out_layout][i];
  }

  for (unsigned int i = 0 ; i < out.size() ; ++i) {
    assert(in_params.channels && out_params.channels); // to pass the scan-build warning: Division by zero.
#if defined(__APPLE__)
    // The size of audio mix buffer(vector out above) on OS X is same as input,
    // so we need to check whether the out[i] will be dropped or not.
    unsigned int index = i % in_params.channels;
    if (index >= out_params.channels) {
      // The out[i] will be dropped, so we don't care the data inside.
      fprintf(stderr, "\tOS X: %d will be dropped. Ignore it.\n", i);
      continue;
    }
#else
    unsigned int index = i % out_params.channels;
#endif

    // downmix_3f2
    if ((in_layout == CUBEB_LAYOUT_3F2 || in_layout == CUBEB_LAYOUT_3F2_LFE) &&
        out_layout >= CUBEB_LAYOUT_MONO && out_layout <= CUBEB_LAYOUT_2F2_LFE) {
      auto & downmix_results = DOWNMIX_3F2_RESULTS[in_layout - CUBEB_LAYOUT_3F2][out_layout - CUBEB_LAYOUT_MONO];
      fprintf(stderr, "\t[3f2] %d(%s) - Expect: %lf, Get: %lf\n", i, channel_names[ CHANNEL_INDEX_TO_ORDER[out_layout][index] ], downmix_results[index], out[i]);
      ASSERT_EQ(downmix_results[index], out[i]);
      continue;
    }

#if defined(__APPLE__)
    fprintf(stderr, "\tOS X: We only support downmix for audio 5.1 currently.\n");
    return;
#endif

    // mix_remap
    if (out_layout_mask & in_layout_mask) {
      uint32_t mask = 1 << CHANNEL_INDEX_TO_ORDER[out_layout][index];
      fprintf(stderr, "\t[remap] %d(%s) - Expect: %lf, Get: %lf\n", i, channel_names[ CHANNEL_INDEX_TO_ORDER[out_layout][index] ], (mask & in_layout_mask) ? audio_inputs[out_layout].data[index] : 0, out[i]);
      ASSERT_EQ((mask & in_layout_mask) ? audio_inputs[out_layout].data[index] : 0, out[i]);
      continue;
    }

    // downmix_fallback
    fprintf(stderr, "\t[fallback] %d - Expect: %lf, Get: %lf\n", i, audio_inputs[in_layout].data[index], out[i]);
    ASSERT_EQ(audio_inputs[in_layout].data[index], out[i]);
  }
}

TEST(cubeb, mixer)
{
  for (auto audio_input : audio_inputs) {
    for (auto audio_output : layout_infos) {
      downmix_test(audio_input.data, audio_input.layout, audio_output.layout);
    }
  }
}
