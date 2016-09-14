/*
 * Copyright © 2016 Mozilla Foundation
 *
 * This program is made available under an ISC-style license.  See the
 * accompanying file LICENSE for details.
 */

/* libcubeb api/function test. Record the mic and check there is sound. */
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
#else
#define STREAM_FORMAT CUBEB_SAMPLE_S16LE
#endif

struct user_state
{
  bool seen_noise;
};

long data_cb(cubeb_stream * stream, void * user, const void * inputbuffer, void * outputbuffer, long nframes)
{
  user_state * u = reinterpret_cast<user_state*>(user);
#if STREAM_FORMAT != CUBEB_SAMPLE_FLOAT32LE
  short *b = (short *)inputbuffer;
#else
  float *b = (float *)inputbuffer;
#endif

  if (stream == NULL  || inputbuffer == NULL || outputbuffer != NULL) {
    return CUBEB_ERROR;
  }

  bool seen_noise = false;
  for (long i = 0; i < nframes; i++) {
    if (b[i] != 0.0) {
      seen_noise = true;
    }
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
  ScopedXPCOM xpcom("test_record");
#endif

  cubeb *ctx;
  cubeb_stream *stream;
  cubeb_stream_params params;
  int r;
  user_state stream_state = { false };

  r = cubeb_init(&ctx, "Cubeb record example");
  if (r != CUBEB_OK) {
    fprintf(stderr, "Error initializing cubeb library\n");
    return r;
  }

  /* This test needs an available input device, skip it if this host does not
   * have one. */
  if (!has_available_input_device(ctx)) {
    return 0;
  }

  params.format = STREAM_FORMAT;
  params.rate = SAMPLE_FREQUENCY;
  params.channels = 1;

  r = cubeb_stream_init(ctx, &stream, "Cubeb record (mono)", NULL, &params, NULL, nullptr,
                        4096, data_cb, state_cb, &stream_state);
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
