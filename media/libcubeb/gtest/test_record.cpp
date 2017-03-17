/*
 * Copyright © 2016 Mozilla Foundation
 *
 * This program is made available under an ISC-style license.  See the
 * accompanying file LICENSE for details.
 */

/* libcubeb api/function test. Record the mic and check there is sound. */
#include "gtest/gtest.h"
#if !defined(_XOPEN_SOURCE)
#define _XOPEN_SOURCE 600
#endif
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "cubeb/cubeb.h"
#include "common.h"

#define SAMPLE_FREQUENCY 48000
#define STREAM_FORMAT CUBEB_SAMPLE_FLOAT32LE

struct user_state_record
{
  bool seen_audio;
};

long data_cb_record(cubeb_stream * stream, void * user, const void * inputbuffer, void * outputbuffer, long nframes)
{
  user_state_record * u = reinterpret_cast<user_state_record*>(user);
  float *b = (float *)inputbuffer;

  if (stream == NULL  || inputbuffer == NULL || outputbuffer != NULL) {
    return CUBEB_ERROR;
  }

  bool seen_audio = true;
  for (long i = 0; i < nframes; i++) {
    if (b[i] <= -1.0 && b[i] >= 1.0) {
      seen_audio = false;
      break;
    }
  }

  u->seen_audio |= seen_audio;

  return nframes;
}

void state_cb_record(cubeb_stream * stream, void * /*user*/, cubeb_state state)
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

TEST(cubeb, record)
{
  if (cubeb_set_log_callback(CUBEB_LOG_DISABLED, nullptr /*print_log*/) != CUBEB_OK) {
    printf("Set log callback failed\n");
  }
  cubeb *ctx;
  cubeb_stream *stream;
  cubeb_stream_params params;
  int r;
  user_state_record stream_state = { false };

  r = cubeb_init(&ctx, "Cubeb record example", NULL);
  if (r != CUBEB_OK) {
    fprintf(stderr, "Error initializing cubeb library\n");
    ASSERT_EQ(r, CUBEB_OK);
  }

  /* This test needs an available input device, skip it if this host does not
   * have one. */
  if (!has_available_input_device(ctx)) {
    return;
  }

  params.format = STREAM_FORMAT;
  params.rate = SAMPLE_FREQUENCY;
  params.channels = 1;
  params.layout = CUBEB_LAYOUT_MONO;

  r = cubeb_stream_init(ctx, &stream, "Cubeb record (mono)", NULL, &params, NULL, nullptr,
                        4096, data_cb_record, state_cb_record, &stream_state);
  if (r != CUBEB_OK) {
    fprintf(stderr, "Error initializing cubeb stream\n");
    ASSERT_EQ(r, CUBEB_OK);
  }

  cubeb_stream_start(stream);
  delay(500);
  cubeb_stream_stop(stream);

  cubeb_stream_destroy(stream);
  cubeb_destroy(ctx);

#ifdef __linux__
  // user callback does not arrive in Linux, silence the error
  printf("Check is disabled in Linux\n");
#else
  ASSERT_TRUE(stream_state.seen_audio);
#endif
}
