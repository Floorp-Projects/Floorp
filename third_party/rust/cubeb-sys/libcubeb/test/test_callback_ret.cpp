/*
* Copyright � 2017 Mozilla Foundation
*
* This program is made available under an ISC-style license.  See the
* accompanying file LICENSE for details.
*/

/* libcubeb api/function test. Test that different return values from user
   specified callbacks are handled correctly. */
#include "gtest/gtest.h"
#if !defined(_XOPEN_SOURCE)
#define _XOPEN_SOURCE 600
#endif
#include <memory>
#include <atomic>
#include <string>
#include "cubeb/cubeb.h"

//#define ENABLE_NORMAL_LOG
//#define ENABLE_VERBOSE_LOG
#include "common.h"

const uint32_t SAMPLE_FREQUENCY = 48000;
const cubeb_sample_format SAMPLE_FORMAT = CUBEB_SAMPLE_S16NE;

enum test_direction {
  INPUT_ONLY,
  OUTPUT_ONLY,
  DUPLEX
};

// Structure which is used by data callbacks to track the total callbacks
// executed vs the number of callbacks expected.
struct user_state_callback_ret {
  std::atomic<int> cb_count{ 0 };
  std::atomic<int> expected_cb_count{ 0 };
};

// Data callback that always returns 0
long data_cb_ret_zero(cubeb_stream * stream, void * user, const void * inputbuffer, void * outputbuffer, long nframes)
{
  user_state_callback_ret * u = (user_state_callback_ret *) user;
  // If this is the first time the callback has been called set our expected
  // callback count
  if (u->cb_count == 0) {
    u->expected_cb_count = 1;
  }
  u->cb_count++;
  if (nframes < 1) {
    // This shouldn't happen
    EXPECT_TRUE(false) << "nframes should not be 0 in data callback!";
  }
  return 0;
}

// Data callback that always returns nframes - 1
long data_cb_ret_nframes_minus_one(cubeb_stream * stream, void * user, const void * inputbuffer, void * outputbuffer, long nframes)
{
  user_state_callback_ret * u = (user_state_callback_ret *)user;
  // If this is the first time the callback has been called set our expected
  // callback count
  if (u->cb_count == 0) {
    u->expected_cb_count = 1;
  }
  u->cb_count++;
  if (nframes < 1) {
    // This shouldn't happen
    EXPECT_TRUE(false) << "nframes should not be 0 in data callback!";
  }
  if (outputbuffer != NULL) {
    // If we have an output buffer insert silence
    short * ob = (short *) outputbuffer;
    for (long i = 0; i < nframes - 1; i++) {
      ob[i] = 0;
    }
  }
  return nframes - 1;
}

// Data callback that always returns nframes
long data_cb_ret_nframes(cubeb_stream * stream, void * user, const void * inputbuffer, void * outputbuffer, long nframes)
{
  user_state_callback_ret * u = (user_state_callback_ret *)user;
  u->cb_count++;
  // Every callback returns nframes, so every callback is expected
  u->expected_cb_count++;
  if (nframes < 1) {
    // This shouldn't happen
    EXPECT_TRUE(false) << "nframes should not be 0 in data callback!";
  }
  if (outputbuffer != NULL) {
    // If we have an output buffer insert silence
    short * ob = (short *) outputbuffer;
    for (long i = 0; i < nframes; i++) {
      ob[i] = 0;
    }
  }
  return nframes;
}

void state_cb_ret(cubeb_stream * stream, void * /*user*/, cubeb_state state)
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

void run_test_callback(test_direction direction,
                       cubeb_data_callback data_cb,
                       const std::string & test_desc) {
  cubeb * ctx;
  cubeb_stream * stream;
  cubeb_stream_params input_params;
  cubeb_stream_params output_params;
  int r;
  user_state_callback_ret user_state;
  uint32_t latency_frames = 0;

  r = common_init(&ctx, "Cubeb callback return value example");
  ASSERT_EQ(r, CUBEB_OK) << "Error initializing cubeb library";

  std::unique_ptr<cubeb, decltype(&cubeb_destroy)>
    cleanup_cubeb_at_exit(ctx, cubeb_destroy);

  if ((direction == INPUT_ONLY || direction == DUPLEX) &&
      !has_available_input_device(ctx)) {
    /* This test needs an available input device, skip it if this host does not
    * have one. */
    return;
  }

  // Setup all params, but only pass them later as required by direction
  input_params.format = SAMPLE_FORMAT;
  input_params.rate = SAMPLE_FREQUENCY;
  input_params.channels = 1;
  input_params.layout = CUBEB_LAYOUT_MONO;
  input_params.prefs = CUBEB_STREAM_PREF_NONE;
  output_params = input_params;

  r = cubeb_get_min_latency(ctx, &input_params, &latency_frames);
  ASSERT_EQ(r, CUBEB_OK) << "Could not get minimal latency";

  switch (direction)
  {
  case INPUT_ONLY:
    r = cubeb_stream_init(ctx, &stream, "Cubeb callback ret input",
                          NULL, &input_params, NULL, NULL,
                          latency_frames, data_cb, state_cb_ret, &user_state);
    break;
  case OUTPUT_ONLY:
    r = cubeb_stream_init(ctx, &stream, "Cubeb callback ret output",
                          NULL, NULL, NULL, &output_params,
                          latency_frames, data_cb, state_cb_ret, &user_state);
    break;
  case DUPLEX:
    r = cubeb_stream_init(ctx, &stream, "Cubeb callback ret duplex",
                          NULL, &input_params, NULL, &output_params,
                          latency_frames, data_cb, state_cb_ret, &user_state);
    break;
  default:
    ASSERT_TRUE(false) << "Unrecognized test direction!";
  }
  EXPECT_EQ(r, CUBEB_OK) << "Error initializing cubeb stream";

  std::unique_ptr<cubeb_stream, decltype(&cubeb_stream_destroy)>
    cleanup_stream_at_exit(stream, cubeb_stream_destroy);

  cubeb_stream_start(stream);
  delay(100);
  cubeb_stream_stop(stream);

  ASSERT_EQ(user_state.expected_cb_count, user_state.cb_count) <<
    "Callback called unexpected number of times for " << test_desc << "!";
}

TEST(cubeb, test_input_callback)
{
  run_test_callback(INPUT_ONLY, data_cb_ret_zero, "input only, return 0");
  run_test_callback(INPUT_ONLY, data_cb_ret_nframes_minus_one, "input only, return nframes - 1");
  run_test_callback(INPUT_ONLY, data_cb_ret_nframes, "input only, return nframes");
}

TEST(cubeb, test_output_callback)
{
  run_test_callback(OUTPUT_ONLY, data_cb_ret_zero, "output only, return 0");
  run_test_callback(OUTPUT_ONLY, data_cb_ret_nframes_minus_one, "output only, return nframes - 1");
  run_test_callback(OUTPUT_ONLY, data_cb_ret_nframes, "output only, return nframes");
}

TEST(cubeb, test_duplex_callback)
{
  run_test_callback(DUPLEX, data_cb_ret_zero, "duplex, return 0");
  run_test_callback(DUPLEX, data_cb_ret_nframes_minus_one, "duplex, return nframes - 1");
  run_test_callback(DUPLEX, data_cb_ret_nframes, "duplex, return nframes");
}
