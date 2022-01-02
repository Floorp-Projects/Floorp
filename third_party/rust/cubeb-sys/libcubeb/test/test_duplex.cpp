/*
 * Copyright © 2016 Mozilla Foundation
 *
 * This program is made available under an ISC-style license.  See the
 * accompanying file LICENSE for details.
 */

/* libcubeb api/function test. Loops input back to output and check audio
 * is flowing. */
#include "gtest/gtest.h"
#if !defined(_XOPEN_SOURCE)
#define _XOPEN_SOURCE 600
#endif
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <memory>
#include "cubeb/cubeb.h"
#include <atomic>

//#define ENABLE_NORMAL_LOG
//#define ENABLE_VERBOSE_LOG
#include "common.h"

#define SAMPLE_FREQUENCY 48000
#define STREAM_FORMAT CUBEB_SAMPLE_FLOAT32LE
#define INPUT_CHANNELS 1
#define INPUT_LAYOUT CUBEB_LAYOUT_MONO
#define OUTPUT_CHANNELS 2
#define OUTPUT_LAYOUT CUBEB_LAYOUT_STEREO

struct user_state_duplex
{
  std::atomic<int> invalid_audio_value{ 0 };
};

long data_cb_duplex(cubeb_stream * stream, void * user, const void * inputbuffer, void * outputbuffer, long nframes)
{
  user_state_duplex * u = reinterpret_cast<user_state_duplex*>(user);
  float *ib = (float *)inputbuffer;
  float *ob = (float *)outputbuffer;

  if (stream == NULL || inputbuffer == NULL || outputbuffer == NULL) {
    return CUBEB_ERROR;
  }

  // Loop back: upmix the single input channel to the two output channels,
  // checking if there is noise in the process.
  long output_index = 0;
  for (long i = 0; i < nframes; i++) {
    if (ib[i] <= -1.0 || ib[i] >= 1.0) {
      u->invalid_audio_value = 1;
      break;
    }
    ob[output_index] = ob[output_index + 1] = ib[i];
    output_index += 2;
  }

  return nframes;
}

void state_cb_duplex(cubeb_stream * stream, void * /*user*/, cubeb_state state)
{
  if (stream == NULL)
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

TEST(cubeb, duplex)
{
  cubeb *ctx;
  cubeb_stream *stream;
  cubeb_stream_params input_params;
  cubeb_stream_params output_params;
  int r;
  user_state_duplex stream_state;
  uint32_t latency_frames = 0;

  r = common_init(&ctx, "Cubeb duplex example");
  ASSERT_EQ(r, CUBEB_OK) << "Error initializing cubeb library";

  std::unique_ptr<cubeb, decltype(&cubeb_destroy)>
    cleanup_cubeb_at_exit(ctx, cubeb_destroy);

  /* This test needs an available input device, skip it if this host does not
   * have one. */
  if (!has_available_input_device(ctx)) {
    return;
  }

  /* typical user-case: mono input, stereo output, low latency. */
  input_params.format = STREAM_FORMAT;
  input_params.rate = SAMPLE_FREQUENCY;
  input_params.channels = INPUT_CHANNELS;
  input_params.layout = INPUT_LAYOUT;
  input_params.prefs = CUBEB_STREAM_PREF_NONE;
  output_params.format = STREAM_FORMAT;
  output_params.rate = SAMPLE_FREQUENCY;
  output_params.channels = OUTPUT_CHANNELS;
  output_params.layout = OUTPUT_LAYOUT;
  output_params.prefs = CUBEB_STREAM_PREF_NONE;

  r = cubeb_get_min_latency(ctx, &output_params, &latency_frames);
  ASSERT_EQ(r, CUBEB_OK) << "Could not get minimal latency";

  r = cubeb_stream_init(ctx, &stream, "Cubeb duplex",
                        NULL, &input_params, NULL, &output_params,
                        latency_frames, data_cb_duplex, state_cb_duplex, &stream_state);
  ASSERT_EQ(r, CUBEB_OK) << "Error initializing cubeb stream";

  std::unique_ptr<cubeb_stream, decltype(&cubeb_stream_destroy)>
    cleanup_stream_at_exit(stream, cubeb_stream_destroy);

  cubeb_stream_start(stream);
  delay(500);
  cubeb_stream_stop(stream);

  ASSERT_FALSE(stream_state.invalid_audio_value.load());
}

void device_collection_changed_callback(cubeb * context, void * user)
{
  fprintf(stderr, "collection changed callback\n");
  ASSERT_TRUE(false) << "Error: device collection changed callback"
                        " called when opening a stream";
}

TEST(cubeb, duplex_collection_change)
{
  cubeb *ctx;
  cubeb_stream *stream;
  cubeb_stream_params input_params;
  cubeb_stream_params output_params;
  int r;
  uint32_t latency_frames = 0;

  r = common_init(&ctx, "Cubeb duplex example with collection change");
  ASSERT_EQ(r, CUBEB_OK) << "Error initializing cubeb library";

  r = cubeb_register_device_collection_changed(ctx,
                                               static_cast<cubeb_device_type>(CUBEB_DEVICE_TYPE_INPUT),
                                               device_collection_changed_callback,
                                               nullptr);
  ASSERT_EQ(r, CUBEB_OK) << "Error initializing cubeb stream";

  std::unique_ptr<cubeb, decltype(&cubeb_destroy)>
    cleanup_cubeb_at_exit(ctx, cubeb_destroy);

  /* typical user-case: mono input, stereo output, low latency. */
  input_params.format = STREAM_FORMAT;
  input_params.rate = SAMPLE_FREQUENCY;
  input_params.channels = INPUT_CHANNELS;
  input_params.layout = INPUT_LAYOUT;
  input_params.prefs = CUBEB_STREAM_PREF_NONE;
  output_params.format = STREAM_FORMAT;
  output_params.rate = SAMPLE_FREQUENCY;
  output_params.channels = OUTPUT_CHANNELS;
  output_params.layout = OUTPUT_LAYOUT;
  output_params.prefs = CUBEB_STREAM_PREF_NONE;

  r = cubeb_get_min_latency(ctx, &output_params, &latency_frames);
  ASSERT_EQ(r, CUBEB_OK) << "Could not get minimal latency";

  r = cubeb_stream_init(ctx, &stream, "Cubeb duplex",
                        NULL, &input_params, NULL, &output_params,
                        latency_frames, data_cb_duplex, state_cb_duplex, nullptr);
  ASSERT_EQ(r, CUBEB_OK) << "Error initializing cubeb stream";
  cubeb_stream_destroy(stream);
}
