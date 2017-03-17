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
#include <limits.h>
#include "cubeb/cubeb.h"
#include "common.h"

#define SAMPLE_FREQUENCY 48000
#if (defined(_WIN32) || defined(__WIN32__))
#define STREAM_FORMAT CUBEB_SAMPLE_FLOAT32LE
#else
#define STREAM_FORMAT CUBEB_SAMPLE_S16LE
#endif

/* store the phase of the generated waveform */
struct cb_user_data {
  long position;
};

long data_cb_tone(cubeb_stream *stream, void *user, const void* /*inputbuffer*/, void *outputbuffer, long nframes)
{
  struct cb_user_data *u = (struct cb_user_data *)user;
#if (defined(_WIN32) || defined(__WIN32__))
  float *b = (float *)outputbuffer;
#else
  short *b = (short *)outputbuffer;
#endif
  float t1, t2;
  int i;

  if (stream == NULL || u == NULL)
    return CUBEB_ERROR;

  /* generate our test tone on the fly */
  for (i = 0; i < nframes; i++) {
    /* North American dial tone */
    t1 = sin(2*M_PI*(i + u->position)*350/SAMPLE_FREQUENCY);
    t2 = sin(2*M_PI*(i + u->position)*440/SAMPLE_FREQUENCY);
#if (defined(_WIN32) || defined(__WIN32__))
    b[i]  = 0.5 * t1;
    b[i] += 0.5 * t2;
#else
    b[i]  = (SHRT_MAX / 2) * t1;
    b[i] += (SHRT_MAX / 2) * t2;
#endif
    /* European dial tone */
    /*
    t1 = sin(2*M_PI*(i + u->position)*425/SAMPLE_FREQUENCY);
#if (defined(_WIN32) || defined(__WIN32__))
    b[i] = t1;
#else
    b[i]  = SHRT_MAX * t1;
#endif
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

TEST(cubeb, tone)
{
  cubeb *ctx;
  cubeb_stream *stream;
  cubeb_stream_params params;
  struct cb_user_data *user_data;
  int r;

  r = cubeb_init(&ctx, "Cubeb tone example", NULL);
  if (r != CUBEB_OK) {
    fprintf(stderr, "Error initializing cubeb library\n");
    ASSERT_EQ(r, CUBEB_OK);
  }

  params.format = STREAM_FORMAT;
  params.rate = SAMPLE_FREQUENCY;
  params.channels = 1;
  params.layout = CUBEB_LAYOUT_MONO;

  user_data = (struct cb_user_data *) malloc(sizeof(*user_data));
  if (user_data == NULL) {
    fprintf(stderr, "Error allocating user data\n");
    FAIL();
  }
  user_data->position = 0;

  r = cubeb_stream_init(ctx, &stream, "Cubeb tone (mono)", NULL, NULL, NULL, &params,
                        4096, data_cb_tone, state_cb_tone, user_data);
  if (r != CUBEB_OK) {
    fprintf(stderr, "Error initializing cubeb stream\n");
    ASSERT_EQ(r, CUBEB_OK);
  }

  cubeb_stream_start(stream);
  delay(500);
  cubeb_stream_stop(stream);

  cubeb_stream_destroy(stream);
  cubeb_destroy(ctx);

  ASSERT_TRUE(user_data->position);

  free(user_data);
}
