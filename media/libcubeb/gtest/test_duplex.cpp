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

long data_cb_input(cubeb_stream * stream, void * user, const void * inputbuffer, void * outputbuffer, long nframes)
{
  if (stream == NULL || inputbuffer == NULL || outputbuffer != NULL) {
    return CUBEB_ERROR;
  }

  return nframes;
}

void state_cb_input(cubeb_stream * stream, void * /*user*/, cubeb_state state)
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
  case CUBEB_STATE_ERROR:
    fprintf(stderr, "stream runs into error state\n"); break;
  default:
    fprintf(stderr, "unknown stream state %d\n", state);
  }

  return;
}

std::vector<cubeb_devid> get_devices(cubeb * ctx, cubeb_device_type type) {
  std::vector<cubeb_devid> devices;

  cubeb_device_collection collection;
  int r = cubeb_enumerate_devices(ctx, type, &collection);

  if (r != CUBEB_OK) {
    fprintf(stderr, "Failed to enumerate devices\n");
    return devices;
  }

  for (uint32_t i = 0; i < collection.count; i++) {
    if (collection.device[i].state == CUBEB_DEVICE_STATE_ENABLED) {
      devices.emplace_back(collection.device[i].devid);
    }
  }

  cubeb_device_collection_destroy(ctx, &collection);

  return devices;
}

TEST(cubeb, one_duplex_one_input)
{
  cubeb *ctx;
  cubeb_stream *duplex_stream;
  cubeb_stream_params input_params;
  cubeb_stream_params output_params;
  int r;
  user_state_duplex duplex_stream_state;
  uint32_t latency_frames = 0;

  r = common_init(&ctx, "Cubeb duplex example");
  ASSERT_EQ(r, CUBEB_OK) << "Error initializing cubeb library";

  std::unique_ptr<cubeb, decltype(&cubeb_destroy)>
    cleanup_cubeb_at_exit(ctx, cubeb_destroy);

  /* This test needs at least two available input devices. */
  std::vector<cubeb_devid> input_devices = get_devices(ctx, CUBEB_DEVICE_TYPE_INPUT);
  if (input_devices.size() < 2) {
    return;
  }

  /* This test needs at least one available output device. */
  std::vector<cubeb_devid> output_devices = get_devices(ctx, CUBEB_DEVICE_TYPE_OUTPUT);
  if (output_devices.size() < 1) {
    return;
  }

  cubeb_devid duplex_input = input_devices.front();
  cubeb_devid duplex_output = nullptr; // default device
  cubeb_devid input_only = input_devices.back();

  /* typical use-case: mono voice input, stereo output, low latency. */
  input_params.format = STREAM_FORMAT;
  input_params.rate = SAMPLE_FREQUENCY;
  input_params.channels = INPUT_CHANNELS;
  input_params.layout = CUBEB_LAYOUT_UNDEFINED;
  input_params.prefs = CUBEB_STREAM_PREF_VOICE;

  output_params.format = STREAM_FORMAT;
  output_params.rate = SAMPLE_FREQUENCY;
  output_params.channels = OUTPUT_CHANNELS;
  output_params.layout = OUTPUT_LAYOUT;
  output_params.prefs = CUBEB_STREAM_PREF_NONE;

  r = cubeb_get_min_latency(ctx, &output_params, &latency_frames);
  ASSERT_EQ(r, CUBEB_OK) << "Could not get minimal latency";

  r = cubeb_stream_init(ctx, &duplex_stream, "Cubeb duplex",
                        duplex_input, &input_params, duplex_output, &output_params,
                        latency_frames, data_cb_duplex, state_cb_duplex, &duplex_stream_state);
  ASSERT_EQ(r, CUBEB_OK) << "Error initializing duplex cubeb stream";

  std::unique_ptr<cubeb_stream, decltype(&cubeb_stream_destroy)>
    cleanup_stream_at_exit(duplex_stream, cubeb_stream_destroy);

  r = cubeb_stream_start(duplex_stream);
  ASSERT_EQ(r, CUBEB_OK) << "Could not start duplex stream";
  delay(500);

  cubeb_stream *input_stream;
  r = cubeb_stream_init(ctx, &input_stream, "Cubeb input",
                        input_only, &input_params, NULL, NULL,
                        latency_frames, data_cb_input, state_cb_input, nullptr);
  ASSERT_EQ(r, CUBEB_OK) << "Error initializing input-only cubeb stream";

  std::unique_ptr<cubeb_stream, decltype(&cubeb_stream_destroy)>
    cleanup_input_stream_at_exit(input_stream, cubeb_stream_destroy);
  
  r = cubeb_stream_start(input_stream);
  ASSERT_EQ(r, CUBEB_OK) << "Could not start input stream";
  delay(500);

  r = cubeb_stream_stop(duplex_stream);
  ASSERT_EQ(r, CUBEB_OK) << "Could not stop duplex stream";

  r = cubeb_stream_stop(input_stream);
  ASSERT_EQ(r, CUBEB_OK) << "Could not stop input stream";

  ASSERT_FALSE(duplex_stream_state.invalid_audio_value.load());
}
