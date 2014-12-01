/*
 * Copyright © 2011 Mozilla Foundation
 *
 * This program is made available under an ISC-style license.  See the
 * accompanying file LICENSE for details.
 */
#ifdef NDEBUG
#undef NDEBUG
#endif
#define _XOPEN_SOURCE 500
#include "cubeb/cubeb.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "common.h"

#if (defined(_WIN32) || defined(__WIN32__))
#define __func__ __FUNCTION__
#endif

#define ARRAY_LENGTH(_x) (sizeof(_x) / sizeof(_x[0]))
#define BEGIN_TEST fprintf(stderr, "START %s\n", __func__);
#define END_TEST fprintf(stderr, "END %s\n", __func__);

#define STREAM_LATENCY 100
#define STREAM_RATE 44100
#define STREAM_CHANNELS 1
#define STREAM_FORMAT CUBEB_SAMPLE_S16LE

static int dummy;
static uint64_t total_frames_written;
static int delay_callback;

static long
test_data_callback(cubeb_stream * stm, void * user_ptr, void * p, long nframes)
{
  assert(stm && user_ptr == &dummy && p && nframes > 0);
  memset(p, 0, nframes * sizeof(short));
  total_frames_written += nframes;
  if (delay_callback) {
    delay(10);
  }
  return nframes;
}

void
test_state_callback(cubeb_stream * stm, void * user_ptr, cubeb_state state)
{
}

static void
test_init_destroy_context(void)
{
  int r;
  cubeb * ctx;
  char const* backend_id;

  BEGIN_TEST

    r = cubeb_init(&ctx, "test_sanity");
  assert(r == 0 && ctx);


  backend_id = cubeb_get_backend_id(ctx);
  assert(backend_id);

  fprintf(stderr, "Backend: %s\n", backend_id);

  cubeb_destroy(ctx);

  END_TEST
    }

static void
test_init_destroy_multiple_contexts(void)
{
  size_t i;
  int r;
  cubeb * ctx[4];
  int order[4] = {2, 0, 3, 1};
  assert(ARRAY_LENGTH(ctx) == ARRAY_LENGTH(order));

  BEGIN_TEST

    for (i = 0; i < ARRAY_LENGTH(ctx); ++i) {
      r = cubeb_init(&ctx[i], NULL);
      assert(r == 0 && ctx[i]);
    }

  /* destroy in a different order */
  for (i = 0; i < ARRAY_LENGTH(ctx); ++i) {
    cubeb_destroy(ctx[order[i]]);
  }

  END_TEST
    }

static void
test_context_variables(void)
{
  int r;
  cubeb * ctx;
  uint32_t value;
  cubeb_stream_params params;

  BEGIN_TEST

    r = cubeb_init(&ctx, "test_context_variables");
  assert(r == 0 && ctx);

  params.channels = 2;
  params.format = CUBEB_SAMPLE_S16LE;
  params.rate = 44100;
  r = cubeb_get_min_latency(ctx, params, &value);
  assert(r == CUBEB_OK || r == CUBEB_ERROR_NOT_SUPPORTED);
  if (r == CUBEB_OK) {
    assert(value > 0);
  }

  r = cubeb_get_preferred_sample_rate(ctx, &value);
  assert(r == CUBEB_OK || r == CUBEB_ERROR_NOT_SUPPORTED);
  if (r == CUBEB_OK) {
    assert(value > 0);
  }

  cubeb_destroy(ctx);

  END_TEST
    }

static void
test_init_destroy_stream(void)
{
  int r;
  cubeb * ctx;
  cubeb_stream * stream;
  cubeb_stream_params params;

  BEGIN_TEST

    r = cubeb_init(&ctx, "test_sanity");
  assert(r == 0 && ctx);

  params.format = STREAM_FORMAT;
  params.rate = STREAM_RATE;
  params.channels = STREAM_CHANNELS;

  r = cubeb_stream_init(ctx, &stream, "test", params, STREAM_LATENCY,
                        test_data_callback, test_state_callback, &dummy);
  assert(r == 0 && stream);

  cubeb_stream_destroy(stream);
  cubeb_destroy(ctx);

  END_TEST
    }

static void
test_init_destroy_multiple_streams(void)
{
  size_t i;
  int r;
  cubeb * ctx;
  cubeb_stream * stream[8];
  cubeb_stream_params params;

  BEGIN_TEST

    r = cubeb_init(&ctx, "test_sanity");
  assert(r == 0 && ctx);

  params.format = STREAM_FORMAT;
  params.rate = STREAM_RATE;
  params.channels = STREAM_CHANNELS;

  for (i = 0; i < ARRAY_LENGTH(stream); ++i) {
    r = cubeb_stream_init(ctx, &stream[i], "test", params, STREAM_LATENCY,
                          test_data_callback, test_state_callback, &dummy);
    assert(r == 0);
    assert(stream[i]);
  }

  for (i = 0; i < ARRAY_LENGTH(stream); ++i) {
    cubeb_stream_destroy(stream[i]);
  }

  cubeb_destroy(ctx);

  END_TEST
    }

static void
test_configure_stream(void)
{
  int r;
  cubeb * ctx;
  cubeb_stream * stream;
  cubeb_stream_params params;

  BEGIN_TEST

    r = cubeb_init(&ctx, "test_sanity");
  assert(r == 0 && ctx);

  params.format = STREAM_FORMAT;
  params.rate = STREAM_RATE;
  params.channels = 2; // panning

  r = cubeb_stream_init(ctx, &stream, "test", params, STREAM_LATENCY,
                        test_data_callback, test_state_callback, &dummy);
  assert(r == 0 && stream);

  r = cubeb_stream_set_volume(stream, 1.0f);
  assert(r == 0 || r == CUBEB_ERROR_NOT_SUPPORTED);

  r = cubeb_stream_set_panning(stream, 0.0f);
  assert(r == 0 || r == CUBEB_ERROR_NOT_SUPPORTED);

  cubeb_stream_destroy(stream);
  cubeb_destroy(ctx);
  END_TEST
    }

static void
test_init_start_stop_destroy_multiple_streams(int early, int delay_ms)
{
  size_t i;
  int r;
  cubeb * ctx;
  cubeb_stream * stream[8];
  cubeb_stream_params params;

  BEGIN_TEST

    r = cubeb_init(&ctx, "test_sanity");
  assert(r == 0 && ctx);

  params.format = STREAM_FORMAT;
  params.rate = STREAM_RATE;
  params.channels = STREAM_CHANNELS;

  for (i = 0; i < ARRAY_LENGTH(stream); ++i) {
    r = cubeb_stream_init(ctx, &stream[i], "test", params, STREAM_LATENCY,
                          test_data_callback, test_state_callback, &dummy);
    assert(r == 0);
    assert(stream[i]);
    if (early) {
      r = cubeb_stream_start(stream[i]);
      assert(r == 0);
    }
  }


  if (!early) {
    for (i = 0; i < ARRAY_LENGTH(stream); ++i) {
      r = cubeb_stream_start(stream[i]);
      assert(r == 0);
    }
  }

  if (delay_ms) {
    delay(delay_ms);
  }

  if (!early) {
    for (i = 0; i < ARRAY_LENGTH(stream); ++i) {
      r = cubeb_stream_stop(stream[i]);
      assert(r == 0);
    }
  }

  for (i = 0; i < ARRAY_LENGTH(stream); ++i) {
    if (early) {
      r = cubeb_stream_stop(stream[i]);
      assert(r == 0);
    }
    cubeb_stream_destroy(stream[i]);
  }

  cubeb_destroy(ctx);

  END_TEST
    }

static void
test_init_destroy_multiple_contexts_and_streams(void)
{
  size_t i, j;
  int r;
  cubeb * ctx[2];
  cubeb_stream * stream[8];
  cubeb_stream_params params;
  size_t streams_per_ctx = ARRAY_LENGTH(stream) / ARRAY_LENGTH(ctx);
  assert(ARRAY_LENGTH(ctx) * streams_per_ctx == ARRAY_LENGTH(stream));

  BEGIN_TEST

    params.format = STREAM_FORMAT;
  params.rate = STREAM_RATE;
  params.channels = STREAM_CHANNELS;

  for (i = 0; i < ARRAY_LENGTH(ctx); ++i) {
    r = cubeb_init(&ctx[i], "test_sanity");
    assert(r == 0 && ctx[i]);

    for (j = 0; j < streams_per_ctx; ++j) {
      r = cubeb_stream_init(ctx[i], &stream[i * streams_per_ctx + j], "test", params, STREAM_LATENCY,
                            test_data_callback, test_state_callback, &dummy);
      assert(r == 0);
      assert(stream[i * streams_per_ctx + j]);
    }
  }

  for (i = 0; i < ARRAY_LENGTH(ctx); ++i) {
    for (j = 0; j < streams_per_ctx; ++j) {
      cubeb_stream_destroy(stream[i * streams_per_ctx + j]);
    }
    cubeb_destroy(ctx[i]);
  }

  END_TEST
    }

static void
test_basic_stream_operations(void)
{
  int r;
  cubeb * ctx;
  cubeb_stream * stream;
  cubeb_stream_params params;
  uint64_t position;

  BEGIN_TEST

    r = cubeb_init(&ctx, "test_sanity");
  assert(r == 0 && ctx);

  params.format = STREAM_FORMAT;
  params.rate = STREAM_RATE;
  params.channels = STREAM_CHANNELS;

  r = cubeb_stream_init(ctx, &stream, "test", params, STREAM_LATENCY,
                        test_data_callback, test_state_callback, &dummy);
  assert(r == 0 && stream);

  /* position and volume before stream has started */
  r = cubeb_stream_get_position(stream, &position);
  assert(r == 0 && position == 0);

  r = cubeb_stream_start(stream);
  assert(r == 0);

  /* position and volume after while stream running */
  r = cubeb_stream_get_position(stream, &position);
  assert(r == 0);

  r = cubeb_stream_stop(stream);
  assert(r == 0);

  /* position and volume after stream has stopped */
  r = cubeb_stream_get_position(stream, &position);
  assert(r == 0);

  cubeb_stream_destroy(stream);
  cubeb_destroy(ctx);

  END_TEST
    }

static void
test_stream_position(void)
{
  size_t i;
  int r;
  cubeb * ctx;
  cubeb_stream * stream;
  cubeb_stream_params params;
  uint64_t position, last_position;

  BEGIN_TEST

    total_frames_written = 0;

  r = cubeb_init(&ctx, "test_sanity");
  assert(r == 0 && ctx);

  params.format = STREAM_FORMAT;
  params.rate = STREAM_RATE;
  params.channels = STREAM_CHANNELS;

  r = cubeb_stream_init(ctx, &stream, "test", params, STREAM_LATENCY,
                        test_data_callback, test_state_callback, &dummy);
  assert(r == 0 && stream);

  /* stream position should not advance before starting playback */
  r = cubeb_stream_get_position(stream, &position);
  assert(r == 0 && position == 0);

  delay(500);

  r = cubeb_stream_get_position(stream, &position);
  assert(r == 0 && position == 0);

  /* stream position should advance during playback */
  r = cubeb_stream_start(stream);
  assert(r == 0);

  /* XXX let start happen */
  delay(500);

  /* stream should have prefilled */
  assert(total_frames_written > 0);

  r = cubeb_stream_get_position(stream, &position);
  assert(r == 0);
  last_position = position;

  delay(500);

  r = cubeb_stream_get_position(stream, &position);
  assert(r == 0);
  assert(position >= last_position);
  last_position = position;

  /* stream position should not exceed total frames written */
  for (i = 0; i < 5; ++i) {
    r = cubeb_stream_get_position(stream, &position);
    assert(r == 0);
    assert(position >= last_position);
    assert(position <= total_frames_written);
    last_position = position;
    delay(500);
  }

  assert(last_position != 0);

  /* stream position should not advance after stopping playback */
  r = cubeb_stream_stop(stream);
  assert(r == 0);

  /* XXX allow stream to settle */
  delay(500);

  r = cubeb_stream_get_position(stream, &position);
  assert(r == 0);
  last_position = position;

  delay(500);

  r = cubeb_stream_get_position(stream, &position);
  assert(r == 0);
  assert(position == last_position);

  cubeb_stream_destroy(stream);
  cubeb_destroy(ctx);

  END_TEST
    }

static int do_drain;
static int got_drain;

static long
test_drain_data_callback(cubeb_stream * stm, void * user_ptr, void * p, long nframes)
{
  assert(stm && user_ptr == &dummy && p && nframes > 0);
  if (do_drain == 1) {
    do_drain = 2;
    return 0;
  }
  /* once drain has started, callback must never be called again */
  assert(do_drain != 2);
  memset(p, 0, nframes * sizeof(short));
  total_frames_written += nframes;
  return nframes;
}

void
test_drain_state_callback(cubeb_stream * stm, void * user_ptr, cubeb_state state)
{
  if (state == CUBEB_STATE_DRAINED) {
    assert(!got_drain);
    got_drain = 1;
  }
}

static void
test_drain(void)
{
  int r;
  cubeb * ctx;
  cubeb_stream * stream;
  cubeb_stream_params params;
  uint64_t position;

  BEGIN_TEST

    total_frames_written = 0;

  r = cubeb_init(&ctx, "test_sanity");
  assert(r == 0 && ctx);

  params.format = STREAM_FORMAT;
  params.rate = STREAM_RATE;
  params.channels = STREAM_CHANNELS;

  r = cubeb_stream_init(ctx, &stream, "test", params, STREAM_LATENCY,
                        test_drain_data_callback, test_drain_state_callback, &dummy);
  assert(r == 0 && stream);

  r = cubeb_stream_start(stream);
  assert(r == 0);

  delay(500);

  do_drain = 1;

  for (;;) {
    r = cubeb_stream_get_position(stream, &position);
    assert(r == 0);
    if (got_drain) {
      break;
    } else {
      assert(position <= total_frames_written);
    }
    delay(500);
  }

  r = cubeb_stream_get_position(stream, &position);
  assert(r == 0);
  assert(got_drain);

  // Really, we should be able to rely on position reaching our final written frame, but
  // for now let's make sure it doesn't continue beyond that point.
  //assert(position <= total_frames_written);

  cubeb_stream_destroy(stream);
  cubeb_destroy(ctx);

  END_TEST
    }

int is_windows_7()
{
#ifdef __MINGW32__
  printf("Warning: this test was built with MinGW.\n"
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

int
main(int argc, char * argv[])
{
  test_init_destroy_context();
  test_init_destroy_multiple_contexts();
  test_context_variables();
  test_init_destroy_stream();
  test_init_destroy_multiple_streams();
  test_configure_stream();
  test_basic_stream_operations();
  test_stream_position();

  /* Sometimes, when using WASAPI on windows 7 (vista and 8 are okay), and
   * calling Activate a lot on an AudioClient, 0x800700b7 is returned. This is
   * the HRESULT value for "Cannot create a file when that file already exists",
   * and is not documented as a possible return value for this call. Hence, we
   * try to limit the number of streams we create in this test. */
  if (!is_windows_7()) {
    test_init_destroy_multiple_contexts_and_streams();

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
  delay_callback = 0;
  test_drain();
/*
  to implement:
  test_eos_during_prefill();
  test_stream_destroy_pending_drain();
*/
  printf("\n");
  return 0;
}
