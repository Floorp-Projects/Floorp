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

  BEGIN_TEST

  r = cubeb_init(&ctx, "test_sanity");
  assert(r == 0 && ctx);

  cubeb_destroy(ctx);

  END_TEST
}

static void
test_init_destroy_multiple_contexts(void)
{
  int i;
  int r;
  cubeb * ctx[4];

  BEGIN_TEST

  for (i = 0; i < 4; ++i) {
    r = cubeb_init(&ctx[i], NULL);
    assert(r == 0 && ctx[i]);
  }

  /* destroy in a different order */
  cubeb_destroy(ctx[2]);
  cubeb_destroy(ctx[0]);
  cubeb_destroy(ctx[3]);
  cubeb_destroy(ctx[1]);

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
  int i;
  int r;
  cubeb * ctx;
  cubeb_stream * stream[16];
  cubeb_stream_params params;

  BEGIN_TEST

  r = cubeb_init(&ctx, "test_sanity");
  assert(r == 0 && ctx);

  params.format = STREAM_FORMAT;
  params.rate = STREAM_RATE;
  params.channels = STREAM_CHANNELS;

  for (i = 0; i < 16; ++i) {
    r = cubeb_stream_init(ctx, &stream[i], "test", params, STREAM_LATENCY,
                          test_data_callback, test_state_callback, &dummy);
    assert(r == 0);
    assert(stream[i]);
  }

  for (i = 0; i < 16; ++i) {
    cubeb_stream_destroy(stream[i]);
  }

  cubeb_destroy(ctx);

  END_TEST
}

static void
test_init_start_stop_destroy_multiple_streams(int early, int delay_ms)
{
  int i;
  int r;
  cubeb * ctx;
  cubeb_stream * stream[16];
  cubeb_stream_params params;

  BEGIN_TEST

  r = cubeb_init(&ctx, "test_sanity");
  assert(r == 0 && ctx);

  params.format = STREAM_FORMAT;
  params.rate = STREAM_RATE;
  params.channels = STREAM_CHANNELS;

  for (i = 0; i < 16; ++i) {
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
    for (i = 0; i < 16; ++i) {
      r = cubeb_stream_start(stream[i]);
      assert(r == 0);
    }
  }

  if (delay_ms) {
    delay(delay_ms);
  }

  if (!early) {
    for (i = 0; i < 16; ++i) {
      r = cubeb_stream_stop(stream[i]);
      assert(r == 0);
    }
  }

  for (i = 0; i < 16; ++i) {
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
  int i, j;
  int r;
  cubeb * ctx[4];
  cubeb_stream * stream[16];
  cubeb_stream_params params;

  BEGIN_TEST

  params.format = STREAM_FORMAT;
  params.rate = STREAM_RATE;
  params.channels = STREAM_CHANNELS;

  for (i = 0; i < 4; ++i) {
    r = cubeb_init(&ctx[i], "test_sanity");
    assert(r == 0 && ctx[i]);

    for (j = 0; j < 4; ++j) {
      r = cubeb_stream_init(ctx[i], &stream[i * 4 + j], "test", params, STREAM_LATENCY,
                            test_data_callback, test_state_callback, &dummy);
      assert(r == 0);
      assert(stream[i * 4 + j]);
    }
  }

  for (i = 0; i < 4; ++i) {
    for (j = 0; j < 4; ++j) {
      cubeb_stream_destroy(stream[i * 4 + j]);
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
  int i;
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
      uint32_t i, skip = 0;
      /* Latency passed to cubeb_stream_init is not really honored on OSX,
         win32/winmm and android, skip this test. */
      const char * backend_id = cubeb_get_backend_id(ctx);
      const char * latency_not_honored_bakends[] = {
        "audiounit",
        "winmm",
        "audiotrack",
        "opensl"
      };

      for (i = 0; i < ARRAY_LENGTH(latency_not_honored_bakends); i++) {
        if (!strcmp(backend_id, latency_not_honored_bakends[i])) {
          skip = 1;
        }
      }
      if (!skip) {
        /* Position should roughly be equal to the number of written frames. We
         * need to take the latency into account. */
        int latency = (STREAM_LATENCY * STREAM_RATE) / 1000;
        assert(position + latency <= total_frames_written);
      }
    }
    delay(500);
  }

  r = cubeb_stream_get_position(stream, &position);
  assert(r == 0);
  assert(got_drain);

  // Disabled due to failures in the ALSA backend.
  //assert(position == total_frames_written);

  cubeb_stream_destroy(stream);
  cubeb_destroy(ctx);

  END_TEST
}

int is_windows_7()
{
#if (defined(_WIN32) || defined(__WIN32__))
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
  test_init_destroy_stream();
  test_init_destroy_multiple_streams();
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
