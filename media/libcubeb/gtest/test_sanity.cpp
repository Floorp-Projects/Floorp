/*
 * Copyright © 2011 Mozilla Foundation
 *
 * This program is made available under an ISC-style license.  See the
 * accompanying file LICENSE for details.
 */
#include "gtest/gtest.h"
#if !defined(_XOPEN_SOURCE)
#define _XOPEN_SOURCE 600
#endif
#include "cubeb/cubeb.h"
#include <atomic>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "common.h"

#define STREAM_RATE 44100
#define STREAM_LATENCY 100 * STREAM_RATE / 1000
#define STREAM_CHANNELS 1
#define STREAM_LAYOUT CUBEB_LAYOUT_MONO
#define STREAM_FORMAT CUBEB_SAMPLE_S16LE

int is_windows_7()
{
#ifdef __MINGW32__
  fprintf(stderr, "Warning: this test was built with MinGW.\n"
         "MinGW does not contain necessary version checking infrastructure. Claiming to be Windows 7, even if we're not.\n");
  return 1;
#endif
#if (defined(_WIN32) || defined(__WIN32__)) && ( !defined(__MINGW32__))
  OSVERSIONINFOEX osvi;
  DWORDLONG condition_mask = 0;

  ZeroMemory(&osvi, sizeof(OSVERSIONINFOEX));
  osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);

  // NT 6.1 is Windows 7
  osvi.dwMajorVersion = 6;
  osvi.dwMinorVersion = 1;

  VER_SET_CONDITION(condition_mask, VER_MAJORVERSION, VER_EQUAL);
  VER_SET_CONDITION(condition_mask, VER_MINORVERSION, VER_GREATER_EQUAL);

  return VerifyVersionInfo(&osvi, VER_MAJORVERSION | VER_MINORVERSION, condition_mask);
#else
  return 0;
#endif
}

static int dummy;
static std::atomic<uint64_t> total_frames_written;
static int delay_callback;

static long
test_data_callback(cubeb_stream * stm, void * user_ptr, const void * /*inputbuffer*/, void * outputbuffer, long nframes)
{
  EXPECT_TRUE(stm && user_ptr == &dummy && outputbuffer && nframes > 0);
  assert(outputbuffer);
  memset(outputbuffer, 0, nframes * sizeof(short));

  total_frames_written += nframes;
  if (delay_callback) {
    delay(10);
  }
  return nframes;
}

void
test_state_callback(cubeb_stream * /*stm*/, void * /*user_ptr*/, cubeb_state /*state*/)
{
}

TEST(cubeb, init_destroy_context)
{
  int r;
  cubeb * ctx;
  char const* backend_id;

  r = cubeb_init(&ctx, "test_sanity", NULL);
  ASSERT_EQ(r, CUBEB_OK);
  ASSERT_NE(ctx, nullptr);

  backend_id = cubeb_get_backend_id(ctx);
  ASSERT_TRUE(backend_id);

  fprintf(stderr, "Backend: %s\n", backend_id);

  cubeb_destroy(ctx);
}

TEST(cubeb, init_destroy_multiple_contexts)
{
  size_t i;
  int r;
  cubeb * ctx[4];
  int order[4] = {2, 0, 3, 1};
  ASSERT_EQ(ARRAY_LENGTH(ctx), ARRAY_LENGTH(order));

  for (i = 0; i < ARRAY_LENGTH(ctx); ++i) {
    r = cubeb_init(&ctx[i], NULL, NULL);
    ASSERT_EQ(r, CUBEB_OK);
    ASSERT_NE(ctx[i], nullptr);
  }

  /* destroy in a different order */
  for (i = 0; i < ARRAY_LENGTH(ctx); ++i) {
    cubeb_destroy(ctx[order[i]]);
  }
}

TEST(cubeb, context_variables)
{
  int r;
  cubeb * ctx;
  uint32_t value;
  cubeb_stream_params params;

  r = cubeb_init(&ctx, "test_context_variables", NULL);
  ASSERT_EQ(r, CUBEB_OK);
  ASSERT_NE(ctx, nullptr);

  params.channels = STREAM_CHANNELS;
  params.format = STREAM_FORMAT;
  params.rate = STREAM_RATE;
  params.layout = STREAM_LAYOUT;
#if defined(__ANDROID__)
  params.stream_type = CUBEB_STREAM_TYPE_MUSIC;
#endif
  r = cubeb_get_min_latency(ctx, params, &value);
  ASSERT_TRUE(r == CUBEB_OK || r == CUBEB_ERROR_NOT_SUPPORTED);
  if (r == CUBEB_OK) {
    ASSERT_TRUE(value > 0);
  }

  r = cubeb_get_preferred_sample_rate(ctx, &value);
  ASSERT_TRUE(r == CUBEB_OK || r == CUBEB_ERROR_NOT_SUPPORTED);
  if (r == CUBEB_OK) {
    ASSERT_TRUE(value > 0);
  }

  cubeb_destroy(ctx);
}

TEST(cubeb, init_destroy_stream)
{
  int r;
  cubeb * ctx;
  cubeb_stream * stream;
  cubeb_stream_params params;

  r = cubeb_init(&ctx, "test_sanity", NULL);
  ASSERT_EQ(r, CUBEB_OK);
  ASSERT_NE(ctx, nullptr);

  params.format = STREAM_FORMAT;
  params.rate = STREAM_RATE;
  params.channels = STREAM_CHANNELS;
  params.layout = STREAM_LAYOUT;
#if defined(__ANDROID__)
  params.stream_type = CUBEB_STREAM_TYPE_MUSIC;
#endif

  r = cubeb_stream_init(ctx, &stream, "test", NULL, NULL, NULL, &params, STREAM_LATENCY,
                        test_data_callback, test_state_callback, &dummy);
  ASSERT_EQ(r, CUBEB_OK);
  ASSERT_NE(stream, nullptr);

  cubeb_stream_destroy(stream);
  cubeb_destroy(ctx);
}

TEST(cubeb, init_destroy_multiple_streams)
{
  size_t i;
  int r;
  cubeb * ctx;
  cubeb_stream * stream[8];
  cubeb_stream_params params;

  r = cubeb_init(&ctx, "test_sanity", NULL);
  ASSERT_EQ(r, CUBEB_OK);
  ASSERT_NE(ctx, nullptr);

  params.format = STREAM_FORMAT;
  params.rate = STREAM_RATE;
  params.channels = STREAM_CHANNELS;
  params.layout = STREAM_LAYOUT;
#if defined(__ANDROID__)
  params.stream_type = CUBEB_STREAM_TYPE_MUSIC;
#endif

  for (i = 0; i < ARRAY_LENGTH(stream); ++i) {
    r = cubeb_stream_init(ctx, &stream[i], "test", NULL, NULL, NULL, &params, STREAM_LATENCY,
                          test_data_callback, test_state_callback, &dummy);
    ASSERT_EQ(r, CUBEB_OK);
    ASSERT_NE(stream[i], nullptr);
  }

  for (i = 0; i < ARRAY_LENGTH(stream); ++i) {
    cubeb_stream_destroy(stream[i]);
  }

  cubeb_destroy(ctx);
}

TEST(cubeb, configure_stream)
{
  int r;
  cubeb * ctx;
  cubeb_stream * stream;
  cubeb_stream_params params;

  r = cubeb_init(&ctx, "test_sanity", NULL);
  ASSERT_EQ(r, CUBEB_OK);
  ASSERT_NE(ctx, nullptr);

  params.format = STREAM_FORMAT;
  params.rate = STREAM_RATE;
  params.channels = 2; // panning
  params.layout = CUBEB_LAYOUT_STEREO;
#if defined(__ANDROID__)
  params.stream_type = CUBEB_STREAM_TYPE_MUSIC;
#endif

  r = cubeb_stream_init(ctx, &stream, "test", NULL, NULL, NULL, &params, STREAM_LATENCY,
                        test_data_callback, test_state_callback, &dummy);
  ASSERT_EQ(r, CUBEB_OK);
  ASSERT_NE(stream, nullptr);

  r = cubeb_stream_set_volume(stream, 1.0f);
  ASSERT_TRUE(r == 0 || r == CUBEB_ERROR_NOT_SUPPORTED);

  r = cubeb_stream_set_panning(stream, 0.0f);
  ASSERT_TRUE(r == 0 || r == CUBEB_ERROR_NOT_SUPPORTED);

  cubeb_stream_destroy(stream);
  cubeb_destroy(ctx);
}

static void
test_init_start_stop_destroy_multiple_streams(int early, int delay_ms)
{
  size_t i;
  int r;
  cubeb * ctx;
  cubeb_stream * stream[8];
  cubeb_stream_params params;

  r = cubeb_init(&ctx, "test_sanity", NULL);
  ASSERT_EQ(r, CUBEB_OK);
  ASSERT_NE(ctx, nullptr);

  params.format = STREAM_FORMAT;
  params.rate = STREAM_RATE;
  params.channels = STREAM_CHANNELS;
  params.layout = STREAM_LAYOUT;
#if defined(__ANDROID__)
  params.stream_type = CUBEB_STREAM_TYPE_MUSIC;
#endif

  for (i = 0; i < ARRAY_LENGTH(stream); ++i) {
    r = cubeb_stream_init(ctx, &stream[i], "test", NULL, NULL, NULL, &params, STREAM_LATENCY,
                          test_data_callback, test_state_callback, &dummy);
    ASSERT_EQ(r, CUBEB_OK);
    ASSERT_NE(stream[i], nullptr);
    if (early) {
      r = cubeb_stream_start(stream[i]);
      ASSERT_EQ(r, CUBEB_OK);
    }
  }

  if (!early) {
    for (i = 0; i < ARRAY_LENGTH(stream); ++i) {
      r = cubeb_stream_start(stream[i]);
      ASSERT_EQ(r, CUBEB_OK);
    }
  }

  if (delay_ms) {
    delay(delay_ms);
  }

  if (!early) {
    for (i = 0; i < ARRAY_LENGTH(stream); ++i) {
      r = cubeb_stream_stop(stream[i]);
      ASSERT_EQ(r, CUBEB_OK);
    }
  }

  for (i = 0; i < ARRAY_LENGTH(stream); ++i) {
    if (early) {
      r = cubeb_stream_stop(stream[i]);
      ASSERT_EQ(r, CUBEB_OK);
    }
    cubeb_stream_destroy(stream[i]);
  }

  cubeb_destroy(ctx);
}

TEST(cubeb, init_start_stop_destroy_multiple_streams)
{
  /* Sometimes, when using WASAPI on windows 7 (vista and 8 are okay), and
   * calling Activate a lot on an AudioClient, 0x800700b7 is returned. This is
   * the HRESULT value for "Cannot create a file when that file already exists",
   * and is not documented as a possible return value for this call. Hence, we
   * try to limit the number of streams we create in this test. */
  if (!is_windows_7()) {
    delay_callback = 0;
    test_init_start_stop_destroy_multiple_streams(0, 0);
    test_init_start_stop_destroy_multiple_streams(1, 0);
    test_init_start_stop_destroy_multiple_streams(0, 150);
    test_init_start_stop_destroy_multiple_streams(1, 150);
    delay_callback = 1;
    test_init_start_stop_destroy_multiple_streams(0, 0);
    test_init_start_stop_destroy_multiple_streams(1, 0);
    test_init_start_stop_destroy_multiple_streams(0, 150);
    test_init_start_stop_destroy_multiple_streams(1, 150);
  }
}

TEST(cubeb, init_destroy_multiple_contexts_and_streams)
{
  size_t i, j;
  int r;
  cubeb * ctx[2];
  cubeb_stream * stream[8];
  cubeb_stream_params params;
  size_t streams_per_ctx = ARRAY_LENGTH(stream) / ARRAY_LENGTH(ctx);
  ASSERT_EQ(ARRAY_LENGTH(ctx) * streams_per_ctx, ARRAY_LENGTH(stream));

  /* Sometimes, when using WASAPI on windows 7 (vista and 8 are okay), and
   * calling Activate a lot on an AudioClient, 0x800700b7 is returned. This is
   * the HRESULT value for "Cannot create a file when that file already exists",
   * and is not documented as a possible return value for this call. Hence, we
   * try to limit the number of streams we create in this test. */
  if (is_windows_7())
    return;

  params.format = STREAM_FORMAT;
  params.rate = STREAM_RATE;
  params.channels = STREAM_CHANNELS;
  params.layout = STREAM_LAYOUT;
#if defined(__ANDROID__)
  params.stream_type = CUBEB_STREAM_TYPE_MUSIC;
#endif

  for (i = 0; i < ARRAY_LENGTH(ctx); ++i) {
    r = cubeb_init(&ctx[i], "test_sanity", NULL);
    ASSERT_EQ(r, CUBEB_OK);
    ASSERT_NE(ctx[i], nullptr);

    for (j = 0; j < streams_per_ctx; ++j) {
      r = cubeb_stream_init(ctx[i], &stream[i * streams_per_ctx + j], "test", NULL, NULL, NULL, &params, STREAM_LATENCY,
                            test_data_callback, test_state_callback, &dummy);
      ASSERT_EQ(r, CUBEB_OK);
      ASSERT_NE(stream[i * streams_per_ctx + j], nullptr);
    }
  }

  for (i = 0; i < ARRAY_LENGTH(ctx); ++i) {
    for (j = 0; j < streams_per_ctx; ++j) {
      cubeb_stream_destroy(stream[i * streams_per_ctx + j]);
    }
    cubeb_destroy(ctx[i]);
  }
}

TEST(cubeb, basic_stream_operations)
{
  int r;
  cubeb * ctx;
  cubeb_stream * stream;
  cubeb_stream_params params;
  uint64_t position;

  r = cubeb_init(&ctx, "test_sanity", NULL);
  ASSERT_EQ(r, CUBEB_OK);
  ASSERT_NE(ctx, nullptr);

  params.format = STREAM_FORMAT;
  params.rate = STREAM_RATE;
  params.channels = STREAM_CHANNELS;
  params.layout = STREAM_LAYOUT;
#if defined(__ANDROID__)
  params.stream_type = CUBEB_STREAM_TYPE_MUSIC;
#endif

  r = cubeb_stream_init(ctx, &stream, "test", NULL, NULL, NULL, &params, STREAM_LATENCY,
                        test_data_callback, test_state_callback, &dummy);
  ASSERT_EQ(r, CUBEB_OK);
  ASSERT_NE(stream, nullptr);

  /* position and volume before stream has started */
  r = cubeb_stream_get_position(stream, &position);
  ASSERT_EQ(r, CUBEB_OK);
  ASSERT_EQ(position, 0u);

  r = cubeb_stream_start(stream);
  ASSERT_EQ(r, CUBEB_OK);

  /* position and volume after while stream running */
  r = cubeb_stream_get_position(stream, &position);
  ASSERT_EQ(r, CUBEB_OK);

  r = cubeb_stream_stop(stream);
  ASSERT_EQ(r, CUBEB_OK);

  /* position and volume after stream has stopped */
  r = cubeb_stream_get_position(stream, &position);
  ASSERT_EQ(r, CUBEB_OK);

  cubeb_stream_destroy(stream);
  cubeb_destroy(ctx);
}

TEST(cubeb, stream_position)
{
  size_t i;
  int r;
  cubeb * ctx;
  cubeb_stream * stream;
  cubeb_stream_params params;
  uint64_t position, last_position;

  total_frames_written = 0;

  r = cubeb_init(&ctx, "test_sanity", NULL);
  ASSERT_EQ(r, CUBEB_OK);
  ASSERT_NE(ctx, nullptr);

  params.format = STREAM_FORMAT;
  params.rate = STREAM_RATE;
  params.channels = STREAM_CHANNELS;
  params.layout = STREAM_LAYOUT;
#if defined(__ANDROID__)
  params.stream_type = CUBEB_STREAM_TYPE_MUSIC;
#endif

  r = cubeb_stream_init(ctx, &stream, "test", NULL, NULL, NULL, &params, STREAM_LATENCY,
                        test_data_callback, test_state_callback, &dummy);
  ASSERT_EQ(r, CUBEB_OK);
  ASSERT_NE(stream, nullptr);

  /* stream position should not advance before starting playback */
  r = cubeb_stream_get_position(stream, &position);
  ASSERT_EQ(r, CUBEB_OK);
  ASSERT_EQ(position, 0u);

  delay(500);

  r = cubeb_stream_get_position(stream, &position);
  ASSERT_EQ(r, CUBEB_OK);
  ASSERT_EQ(position, 0u);

  /* stream position should advance during playback */
  r = cubeb_stream_start(stream);
  ASSERT_EQ(r, CUBEB_OK);

  /* XXX let start happen */
  delay(500);

  /* stream should have prefilled */
  ASSERT_TRUE(total_frames_written.load() > 0);

  r = cubeb_stream_get_position(stream, &position);
  ASSERT_EQ(r, CUBEB_OK);
  last_position = position;

  delay(500);

  r = cubeb_stream_get_position(stream, &position);
  ASSERT_EQ(r, CUBEB_OK);
  ASSERT_GE(position, last_position);
  last_position = position;

  /* stream position should not exceed total frames written */
  for (i = 0; i < 5; ++i) {
    r = cubeb_stream_get_position(stream, &position);
    ASSERT_EQ(r, CUBEB_OK);
    ASSERT_GE(position, last_position);
    ASSERT_LE(position, total_frames_written.load());
    last_position = position;
    delay(500);
  }

  /* test that the position is valid even when starting and
   * stopping the stream.  */
  for (i = 0; i < 5; ++i) {
    r = cubeb_stream_stop(stream);
    ASSERT_EQ(r, CUBEB_OK);
    r = cubeb_stream_get_position(stream, &position);
    ASSERT_EQ(r, CUBEB_OK);
    ASSERT_TRUE(last_position < position);
    last_position = position;
    delay(500);
    r = cubeb_stream_start(stream);
    ASSERT_EQ(r, CUBEB_OK);
    delay(500);
  }

  ASSERT_NE(last_position, 0u);

  /* stream position should not advance after stopping playback */
  r = cubeb_stream_stop(stream);
  ASSERT_EQ(r, CUBEB_OK);

  /* XXX allow stream to settle */
  delay(500);

  r = cubeb_stream_get_position(stream, &position);
  ASSERT_EQ(r, CUBEB_OK);
  last_position = position;

  delay(500);

  r = cubeb_stream_get_position(stream, &position);
  ASSERT_EQ(r, CUBEB_OK);
  ASSERT_EQ(position, last_position);

  cubeb_stream_destroy(stream);
  cubeb_destroy(ctx);
}

static std::atomic<int> do_drain;
static std::atomic<int> got_drain;

static long
test_drain_data_callback(cubeb_stream * stm, void * user_ptr, const void * /*inputbuffer*/, void * outputbuffer, long nframes)
{
  EXPECT_TRUE(stm && user_ptr == &dummy && outputbuffer && nframes > 0);
  assert(outputbuffer);
  if (do_drain == 1) {
    do_drain = 2;
    return 0;
  }
  /* once drain has started, callback must never be called again */
  EXPECT_TRUE(do_drain != 2);
  memset(outputbuffer, 0, nframes * sizeof(short));
  total_frames_written += nframes;
  return nframes;
}

void
test_drain_state_callback(cubeb_stream * /*stm*/, void * /*user_ptr*/, cubeb_state state)
{
  if (state == CUBEB_STATE_DRAINED) {
    ASSERT_TRUE(!got_drain);
    got_drain = 1;
  }
}

TEST(cubeb, drain)
{
  int r;
  cubeb * ctx;
  cubeb_stream * stream;
  cubeb_stream_params params;
  uint64_t position;

  delay_callback = 0;
  total_frames_written = 0;

  r = cubeb_init(&ctx, "test_sanity", NULL);
  ASSERT_EQ(r, CUBEB_OK);
  ASSERT_NE(ctx, nullptr);

  params.format = STREAM_FORMAT;
  params.rate = STREAM_RATE;
  params.channels = STREAM_CHANNELS;
  params.layout = STREAM_LAYOUT;
#if defined(__ANDROID__)
  params.stream_type = CUBEB_STREAM_TYPE_MUSIC;
#endif

  r = cubeb_stream_init(ctx, &stream, "test", NULL, NULL, NULL, &params, STREAM_LATENCY,
                        test_drain_data_callback, test_drain_state_callback, &dummy);
  ASSERT_EQ(r, CUBEB_OK);
  ASSERT_NE(stream, nullptr);

  r = cubeb_stream_start(stream);
  ASSERT_EQ(r, CUBEB_OK);

  delay(500);

  do_drain = 1;

  for (;;) {
    r = cubeb_stream_get_position(stream, &position);
    ASSERT_EQ(r, CUBEB_OK);
    if (got_drain) {
      break;
    } else {
      ASSERT_LE(position, total_frames_written.load());
    }
    delay(500);
  }

  r = cubeb_stream_get_position(stream, &position);
  ASSERT_EQ(r, CUBEB_OK);
  ASSERT_TRUE(got_drain);

  // Really, we should be able to rely on position reaching our final written frame, but
  // for now let's make sure it doesn't continue beyond that point.
  //ASSERT_LE(position, total_frames_written.load());

  cubeb_stream_destroy(stream);
  cubeb_destroy(ctx);

  got_drain = 0;
  do_drain = 0;
}

TEST(cubeb, DISABLED_eos_during_prefill)
{
  // This test needs to be implemented.
}

TEST(cubeb, DISABLED_stream_destroy_pending_drain)
{
  // This test needs to be implemented.
}
