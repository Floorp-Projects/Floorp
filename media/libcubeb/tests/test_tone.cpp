/*
 * Copyright © 2011 Mozilla Foundation
 *
 * This program is made available under an ISC-style license.  See the
 * accompanying file LICENSE for details.
 */

/* libcubeb api/function test. Plays a simple tone. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#define _XOPEN_SOURCE 500
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>

#include "cubeb/cubeb.h"
#include "common.h"

#define SAMPLE_FREQUENCY 48000

/* store the phase of the generated waveform */
struct cb_user_data {
  long position;
};

long data_cb(cubeb_stream *stream, void *user, void *buffer, long nframes)
{
  struct cb_user_data *u = (struct cb_user_data *)user;
  short *b = (short *)buffer;
  int i;

  if (stream == NULL || u == NULL)
    return CUBEB_ERROR;

  /* generate our test tone on the fly */
  for (i = 0; i < nframes; i++) {
    /* North American dial tone */
    b[i]  = 16000*sin(2*M_PI*(i + u->position)*350/SAMPLE_FREQUENCY);
    b[i] += 16000*sin(2*M_PI*(i + u->position)*440/SAMPLE_FREQUENCY);
    /* European dial tone */
    /*b[i]  = 30000*sin(2*M_PI*(i + u->position)*425/SAMPLE_FREQUENCY);*/
  }
  /* remember our phase to avoid clicking on buffer transitions */
  /* we'll still click if position overflows */
  u->position += nframes;

  return nframes;
}

void state_cb(cubeb_stream *stream, void *user, cubeb_state state)
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

int main(int argc, char *argv[])
{
  cubeb *ctx;
  cubeb_stream *stream;
  cubeb_stream_params params;
  struct cb_user_data *user_data;
  int r;

  r = cubeb_init(&ctx, "Cubeb tone example");
  if (r != CUBEB_OK) {
    fprintf(stderr, "Error initializing cubeb library\n");
    return r;
  }

  params.format = CUBEB_SAMPLE_S16NE;
  params.rate = SAMPLE_FREQUENCY;
  params.channels = 1;

  user_data = (struct cb_user_data *) malloc(sizeof(*user_data));
  if (user_data == NULL) {
    fprintf(stderr, "Error allocating user data\n");
    return CUBEB_ERROR;
  }
  user_data->position = 0;

  r = cubeb_stream_init(ctx, &stream, "Cubeb tone (mono)", params,
                        250, data_cb, state_cb, user_data);
  if (r != CUBEB_OK) {
    fprintf(stderr, "Error initializing cubeb stream\n");
    return r;
  }

  cubeb_stream_start(stream);
  delay(500);
  cubeb_stream_stop(stream);

  cubeb_stream_destroy(stream);
  cubeb_destroy(ctx);

  assert(user_data->position);

  free(user_data);

  return CUBEB_OK;
}
