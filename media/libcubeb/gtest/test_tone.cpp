/*
 * Copyright © 2011 Mozilla Foundation
 *
 * This program is made available under an ISC-style license.  See the
 * accompanying file LICENSE for details.
 */

/* libcubeb api/function test. Plays a simple tone. */
#include "gtest/gtest.h"
#if !defined(_XOPEN_SOURCE)
#define _XOPEN_SOURCE 600
#endif
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <memory>
#include <limits.h>
#include "cubeb/cubeb.h"
#include "common.h"
#include <atomic>

#define SAMPLE_FREQUENCY 48000
#define STREAM_FORMAT CUBEB_SAMPLE_S16LE

/* store the phase of the generated waveform */
struct cb_user_data {
  std::atomic<long> position;
};

long data_cb_tone(cubeb_stream *stream, void *user, const void* /*inputbuffer*/, void *outputbuffer, long nframes)
{
  struct cb_user_data *u = (struct cb_user_data *)user;
  short *b = (short *)outputbuffer;
  float t1, t2;
  int i;

  if (stream == NULL || u == NULL)
    return CUBEB_ERROR;

  /* generate our test tone on the fly */
  for (i = 0; i < nframes; i++) {
    /* North American dial tone */
    t1 = sin(2*M_PI*(i + u->position)*350/SAMPLE_FREQUENCY);
    t2 = sin(2*M_PI*(i + u->position)*440/SAMPLE_FREQUENCY);
    b[i]  = (SHRT_MAX / 2) * t1;
    b[i] += (SHRT_MAX / 2) * t2;
    /* European dial tone */
    /*
    t1 = sin(2*M_PI*(i + u->position)*425/SAMPLE_FREQUENCY);
    b[i]  = SHRT_MAX * t1;
    */
  }
  /* remember our phase to avoid clicking on buffer transitions */
  /* we'll still click if position overflows */
  u->position += nframes;

  return nframes;
}

void state_cb_tone(cubeb_stream *stream, void *user, cubeb_state state)
{
  struct cb_user_data *u = (struct cb_user_data *)user;

  if (stream == NULL || u == NULL)
    return;

  switch (state) {
  case CUBEB_STATE_STARTED:
    fprintf(stderr, "stream started\n"); break;
  case CUBEB_STATE_STOPPED:
    fprintf(stderr, "stream stopped\n"); break;
  case CUBEB_STATE_DRAINED:
    fprintf(stderr, "stream drained\n"); break;
  default:
    fprintf(stderr, "unknown stream state %d\n", state);
  }

  return;
}

TEST(cubeb, tone)
{
  cubeb *ctx;
  cubeb_stream *stream;
  cubeb_stream_params params;
  int r;

  r = common_init(&ctx, "Cubeb tone example");
  ASSERT_EQ(r, CUBEB_OK) << "Error initializing cubeb library";

  std::unique_ptr<cubeb, decltype(&cubeb_destroy)>
    cleanup_cubeb_at_exit(ctx, cubeb_destroy);

  params.format = STREAM_FORMAT;
  params.rate = SAMPLE_FREQUENCY;
  params.channels = 1;
  params.layout = CUBEB_LAYOUT_MONO;

  std::unique_ptr<cb_user_data> user_data(new cb_user_data());
  ASSERT_TRUE(!!user_data) << "Error allocating user data";

  user_data->position = 0;

  r = cubeb_stream_init(ctx, &stream, "Cubeb tone (mono)", NULL, NULL, NULL, &params,
                        4096, data_cb_tone, state_cb_tone, user_data.get());
  ASSERT_EQ(r, CUBEB_OK) << "Error initializing cubeb stream";

  std::unique_ptr<cubeb_stream, decltype(&cubeb_stream_destroy)>
    cleanup_stream_at_exit(stream, cubeb_stream_destroy);

  cubeb_stream_start(stream);
  delay(500);
  cubeb_stream_stop(stream);

  ASSERT_TRUE(user_data->position.load());
}
