/*
 * Copyright © 2016 Mozilla Foundation
 *
 * This program is made available under an ISC-style license.  See the
 * accompanying file LICENSE for details.
 */

/* libcubeb api/function test. Loops input back to output and check audio
 * is flowing. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#define _XOPEN_SOURCE 600
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>

#include "cubeb/cubeb.h"
#include "common.h"
#ifdef CUBEB_GECKO_BUILD
#include "TestHarness.h"
#endif

#define SAMPLE_FREQUENCY 48000
#if (defined(_WIN32) || defined(__WIN32__))
#define STREAM_FORMAT CUBEB_SAMPLE_FLOAT32LE
#define SILENT_SAMPLE 0.0f
#else
#define STREAM_FORMAT CUBEB_SAMPLE_S16LE
#define SILENT_SAMPLE 0
#endif

struct user_state
{
  bool seen_noise;
};



long data_cb(cubeb_stream * stream, void * user, const void * inputbuffer, void * outputbuffer, long nframes)
{
  user_state * u = reinterpret_cast<user_state*>(user);
#if (defined(_WIN32) || defined(__WIN32__))
  float *ib = (float *)inputbuffer;
  float *ob = (float *)outputbuffer;
#else
  short *ib = (short *)inputbuffer;
  short *ob = (short *)outputbuffer;
#endif
  bool seen_noise = false;

  if (stream == NULL || inputbuffer == NULL || outputbuffer == NULL) {
    return CUBEB_ERROR;
  }

  // Loop back: upmix the single input channel to the two output channels,
  // checking if there is noise in the process.
  long output_index = 0;
  for (long i = 0; i < nframes; i++) {
    if (ib[i] != SILENT_SAMPLE) {
      seen_noise = true;
    }
    ob[output_index] = ob[output_index + 1] = ib[i];
    output_index += 2;
  }

  u->seen_noise |= seen_noise;

  return nframes;
}

void state_cb(cubeb_stream * stream, void * /*user*/, cubeb_state state)
{
  if (stream == NULL)
    return;

  switch (state) {
  case CUBEB_STATE_STARTED:
    printf("stream started\n"); break;
  case CUBEB_STATE_STOPPED:
    printf("stream stopped\n"); break;
  case CUBEB_STATE_DRAINED:
    printf("stream drained\n"); break;
  default:
    printf("unknown stream state %d\n", state);
  }

  return;
}

int main(int /*argc*/, char * /*argv*/[])
{
#ifdef CUBEB_GECKO_BUILD
  ScopedXPCOM xpcom("test_duplex");
#endif

  cubeb *ctx;
  cubeb_stream *stream;
  cubeb_stream_params input_params;
  cubeb_stream_params output_params;
  int r;
  user_state stream_state = { false };
  uint32_t latency_frames = 0;

  r = cubeb_init(&ctx, "Cubeb duplex example");
  if (r != CUBEB_OK) {
    fprintf(stderr, "Error initializing cubeb library\n");
    return r;
  }

  /* This test needs an available input device, skip it if this host does not
   * have one. */
  if (!has_available_input_device(ctx)) {
    return 0;
  }

  /* typical user-case: mono input, stereo output, low latency. */
  input_params.format = STREAM_FORMAT;
  input_params.rate = 48000;
  input_params.channels = 1;
  output_params.format = STREAM_FORMAT;
  output_params.rate = 48000;
  output_params.channels = 2;

  r = cubeb_get_min_latency(ctx, output_params, &latency_frames);

  if (r != CUBEB_OK) {
    fprintf(stderr, "Could not get minimal latency\n");
    return r;
  }

  r = cubeb_stream_init(ctx, &stream, "Cubeb duplex",
                        NULL, &input_params, NULL, &output_params,
                        latency_frames, data_cb, state_cb, &stream_state);
  if (r != CUBEB_OK) {
    fprintf(stderr, "Error initializing cubeb stream\n");
    return r;
  }

  cubeb_stream_start(stream);
  delay(500);
  cubeb_stream_stop(stream);

  cubeb_stream_destroy(stream);
  cubeb_destroy(ctx);

  assert(stream_state.seen_noise);

  return CUBEB_OK;
}
