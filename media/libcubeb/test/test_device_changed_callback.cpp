/*
 * Copyright © 2018 Mozilla Foundation
 *
 * This program is made available under an ISC-style license.  See the
 * accompanying file LICENSE for details.
 */

/* libcubeb api/function test. Check behaviors of registering device changed
 * callbacks for the streams. */
#include "gtest/gtest.h"
#if !defined(_XOPEN_SOURCE)
#define _XOPEN_SOURCE 600
#endif
#include <stdio.h>
#include <memory>
#include "cubeb/cubeb.h"

//#define ENABLE_NORMAL_LOG
//#define ENABLE_VERBOSE_LOG
#include "common.h"

#define SAMPLE_FREQUENCY 48000
#define STREAM_FORMAT CUBEB_SAMPLE_FLOAT32LE
#define INPUT_CHANNELS 1
#define INPUT_LAYOUT CUBEB_LAYOUT_MONO
#define OUTPUT_CHANNELS 2
#define OUTPUT_LAYOUT CUBEB_LAYOUT_STEREO

long data_callback(cubeb_stream * stream, void * user, const void * inputbuffer, void * outputbuffer, long nframes)
{
  return 0;
}

void state_callback(cubeb_stream * stream, void * user, cubeb_state state)
{
}

void device_changed_callback(void * user)
{
  fprintf(stderr, "device changed callback\n");
  ASSERT_TRUE(false) << "Error: device changed callback"
                        " called without changing devices";
}

void test_registering_null_callback_twice(cubeb_stream * stream)
{
  int r = cubeb_stream_register_device_changed_callback(stream, nullptr);
  if (r == CUBEB_ERROR_NOT_SUPPORTED) {
    return;
  }
  ASSERT_EQ(r, CUBEB_OK) << "Error registering null device changed callback";

  r = cubeb_stream_register_device_changed_callback(stream, nullptr);
  ASSERT_EQ(r, CUBEB_OK) << "Error registering null device changed callback again";
}

void test_registering_and_unregistering_callback(cubeb_stream * stream)
{
  int r = cubeb_stream_register_device_changed_callback(stream, device_changed_callback);
  if (r == CUBEB_ERROR_NOT_SUPPORTED) {
    return;
  }
  ASSERT_EQ(r, CUBEB_OK) << "Error registering device changed callback";

  r = cubeb_stream_register_device_changed_callback(stream, nullptr);
  ASSERT_EQ(r, CUBEB_OK) << "Error unregistering device changed callback";
}

TEST(cubeb, device_changed_callbacks)
{
  cubeb * ctx;
  cubeb_stream * stream;
  cubeb_stream_params input_params;
  cubeb_stream_params output_params;
  int r = CUBEB_OK;
  uint32_t latency_frames = 0;

  r = common_init(&ctx, "Cubeb duplex example with device change");
  ASSERT_EQ(r, CUBEB_OK) << "Error initializing cubeb library";

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
                        latency_frames, data_callback, state_callback, nullptr);
  ASSERT_EQ(r, CUBEB_OK) << "Error initializing cubeb stream";

  test_registering_null_callback_twice(stream);

  test_registering_and_unregistering_callback(stream);

  cubeb_stream_destroy(stream);
}
