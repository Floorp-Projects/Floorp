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
#include <memory>
#include <atomic>
#include "cubeb/cubeb.h"
//#define ENABLE_NORMAL_LOG
//#define ENABLE_VERBOSE_LOG
#include "common.h"

#define SAMPLE_FREQUENCY 48000
#define STREAM_FORMAT CUBEB_SAMPLE_S16LE

std::atomic<bool> load_callback{ false };

long data_cb(cubeb_stream * stream, void * user, const void * inputbuffer, void * outputbuffer, long nframes)
{
  if (load_callback) {
    fprintf(stderr, "Sleeping...\n");
    delay(100000);
    fprintf(stderr, "Sleeping done\n");
  }
  return nframes;
}

void state_cb(cubeb_stream * stream, void * /*user*/, cubeb_state state)
{
  ASSERT_TRUE(!!stream);

  switch (state) {
  case CUBEB_STATE_STARTED:
    fprintf(stderr, "stream started\n"); break;
  case CUBEB_STATE_STOPPED:
    fprintf(stderr, "stream stopped\n"); break;
  case CUBEB_STATE_DRAINED:
    FAIL() << "this test is not supposed to drain"; break;
  case CUBEB_STATE_ERROR:
    fprintf(stderr, "stream error\n"); break;
  default:
    FAIL() << "this test is not supposed to have a weird state"; break;
  }
}

TEST(cubeb, overload_callback)
{
  cubeb * ctx;
  cubeb_stream * stream;
  cubeb_stream_params output_params;
  int r;
  uint32_t latency_frames = 0;

  r = common_init(&ctx, "Cubeb callback overload");
  ASSERT_EQ(r, CUBEB_OK);

  std::unique_ptr<cubeb, decltype(&cubeb_destroy)>
    cleanup_cubeb_at_exit(ctx, cubeb_destroy);

  output_params.format = STREAM_FORMAT;
  output_params.rate = 48000;
  output_params.channels = 2;
  output_params.layout = CUBEB_LAYOUT_STEREO;
  output_params.prefs = CUBEB_STREAM_PREF_NONE;

  r = cubeb_get_min_latency(ctx, &output_params, &latency_frames);
  ASSERT_EQ(r, CUBEB_OK);

  r = cubeb_stream_init(ctx, &stream, "Cubeb",
                        NULL, NULL, NULL, &output_params,
                        latency_frames, data_cb, state_cb, NULL);
  ASSERT_EQ(r, CUBEB_OK);

  std::unique_ptr<cubeb_stream, decltype(&cubeb_stream_destroy)>
    cleanup_stream_at_exit(stream, cubeb_stream_destroy);

  cubeb_stream_start(stream);
  delay(500);
  // This causes the callback to sleep for a large number of seconds.
  load_callback = true;
  delay(500);
  cubeb_stream_stop(stream);
}
