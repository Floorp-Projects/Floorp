/*
 * Copyright © 2017 Mozilla Foundation
 *
 * This program is made available under an ISC-style license.  See the
 * accompanying file LICENSE for details.
 */

#include "gtest/gtest.h"
#if !defined(_XOPEN_SOURCE)
#define _XOPEN_SOURCE 600
#endif
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <atomic>
#include "cubeb/cubeb.h"
#include "common.h"

#define SAMPLE_FREQUENCY 48000
#if (defined(_WIN32) || defined(__WIN32__))
#define STREAM_FORMAT CUBEB_SAMPLE_FLOAT32LE
#else
#define STREAM_FORMAT CUBEB_SAMPLE_S16LE
#endif

std::atomic<bool> load_callback{ false };

long data_cb(cubeb_stream * stream, void * user, const void * inputbuffer, void * outputbuffer, long nframes)
{
  if (load_callback) {
    printf("Sleeping...\n");
    delay(100000);
    printf("Sleeping done\n");
  }
  return nframes;
}

void state_cb(cubeb_stream * stream, void * /*user*/, cubeb_state state)
{
  assert(stream);

  switch (state) {
  case CUBEB_STATE_STARTED:
    printf("stream started\n"); break;
  case CUBEB_STATE_STOPPED:
    printf("stream stopped\n"); break;
  case CUBEB_STATE_DRAINED:
    assert(false && "this test is not supposed to drain"); break;
  case CUBEB_STATE_ERROR:
    printf("stream error\n"); break;
  default:
    assert(false && "this test is not supposed to have a weird state"); break;
  }
}

TEST(cubeb, overload_callback)
{
  cubeb * ctx;
  cubeb_stream * stream;
  cubeb_stream_params output_params;
  int r;
  uint32_t latency_frames = 0;

  r = cubeb_init(&ctx, "Cubeb callback overload", NULL);
  ASSERT_EQ(r, CUBEB_OK);

  output_params.format = STREAM_FORMAT;
  output_params.rate = 48000;
  output_params.channels = 2;
  output_params.layout = CUBEB_LAYOUT_STEREO;

  r = cubeb_get_min_latency(ctx, output_params, &latency_frames);
  ASSERT_EQ(r, CUBEB_OK);

  r = cubeb_stream_init(ctx, &stream, "Cubeb",
                        NULL, NULL, NULL, &output_params,
                        latency_frames, data_cb, state_cb, NULL);
  ASSERT_EQ(r, CUBEB_OK);

  cubeb_stream_start(stream);
  delay(500);
  // This causes the callback to sleep for a large number of seconds.
  load_callback = true;
  delay(500);
  cubeb_stream_stop(stream);

  cubeb_stream_destroy(stream);
  cubeb_destroy(ctx);
}
