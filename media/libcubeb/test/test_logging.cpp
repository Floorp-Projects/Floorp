/*
 * Copyright © 2016 Mozilla Foundation
 *
 * This program is made available under an ISC-style license.  See the
 * accompanying file LICENSE for details.
 */

/* cubeb_logging test  */
#include "gtest/gtest.h"
#if !defined(_XOPEN_SOURCE)
#define _XOPEN_SOURCE 600
#endif
#include "cubeb/cubeb.h"
#include "cubeb_log.h"
#include <atomic>
#include <math.h>
#include <memory>
#include <stdio.h>
#include <stdlib.h>
#include <thread>

#include "common.h"

#define PRINT_LOGS_TO_STDERR 0

std::atomic<uint32_t> log_statements_received = {0};
std::atomic<uint32_t> data_callback_call_count = {0};

static void
test_logging_callback(char const * fmt, ...)
{
  log_statements_received++;
#if PRINT_LOGS_TO_STDERR == 1
  char buf[1024];
  va_list argslist;
  va_start(argslist, fmt);
  vsnprintf(buf, 1024, fmt, argslist);
  fprintf(stderr, "%s\n", buf);
  va_end(argslist);
#endif // PRINT_LOGS_TO_STDERR
}

static long
data_cb_load(cubeb_stream * stream, void * user, const void * inputbuffer,
             void * outputbuffer, long nframes)
{
  data_callback_call_count++;
  return nframes;
}

static void
state_cb(cubeb_stream * stream, void * /*user*/, cubeb_state state)
{
  if (stream == NULL)
    return;

  switch (state) {
  case CUBEB_STATE_STARTED:
    fprintf(stderr, "stream started\n");
    break;
  case CUBEB_STATE_STOPPED:
    fprintf(stderr, "stream stopped\n");
    break;
  case CUBEB_STATE_DRAINED:
    fprintf(stderr, "stream drained\n");
    break;
  default:
    fprintf(stderr, "unknown stream state %d\n", state);
  }

  return;
}

// Waits for at least one audio callback to have occured.
void
wait_for_audio_callback()
{
  uint32_t audio_callback_index =
      data_callback_call_count.load(std::memory_order_acquire);
  while (audio_callback_index ==
         data_callback_call_count.load(std::memory_order_acquire)) {
    delay(100);
  }
}

TEST(cubeb, logging)
{
  cubeb * ctx;
  cubeb_stream * stream;
  cubeb_stream_params output_params;
  int r;
  uint32_t latency_frames = 0;

  cubeb_set_log_callback(CUBEB_LOG_NORMAL, test_logging_callback);

  r = common_init(&ctx, "Cubeb logging test");
  ASSERT_EQ(r, CUBEB_OK) << "Error initializing cubeb library";

  std::unique_ptr<cubeb, decltype(&cubeb_destroy)> cleanup_cubeb_at_exit(
      ctx, cubeb_destroy);

  output_params.format = CUBEB_SAMPLE_FLOAT32LE;
  output_params.rate = 48000;
  output_params.channels = 2;
  output_params.layout = CUBEB_LAYOUT_STEREO;
  output_params.prefs = CUBEB_STREAM_PREF_NONE;

  r = cubeb_get_min_latency(ctx, &output_params, &latency_frames);
  if (r != CUBEB_OK) {
    // not fatal
    latency_frames = 1024;
  }

  r = cubeb_stream_init(ctx, &stream, "Cubeb logging", NULL, NULL, NULL,
                        &output_params, latency_frames, data_cb_load, state_cb,
                        NULL);
  ASSERT_EQ(r, CUBEB_OK) << "Error initializing cubeb stream";

  std::unique_ptr<cubeb_stream, decltype(&cubeb_stream_destroy)>
      cleanup_stream_at_exit(stream, cubeb_stream_destroy);

  ASSERT_NE(log_statements_received.load(std::memory_order_acquire), 0u);

  cubeb_set_log_callback(CUBEB_LOG_DISABLED, nullptr);
  log_statements_received.store(0, std::memory_order_release);

  // This is synchronous and we'll receive log messages on all backends that we
  // test
  cubeb_stream_start(stream);

  ASSERT_EQ(log_statements_received.load(std::memory_order_acquire), 0u);

  cubeb_set_log_callback(CUBEB_LOG_VERBOSE, test_logging_callback);

  wait_for_audio_callback();

  ASSERT_NE(log_statements_received.load(std::memory_order_acquire), 0u);

  bool log_callback_set = true;
  uint32_t iterations = 100;
  while (iterations--) {
    wait_for_audio_callback();

    if (!log_callback_set) {
      ASSERT_EQ(log_statements_received.load(std::memory_order_acquire), 0u);
      // Set a logging callback, start logging
      cubeb_set_log_callback(CUBEB_LOG_VERBOSE, test_logging_callback);
      log_callback_set = true;
    } else {
      // Disable the logging callback, stop logging.
      ASSERT_NE(log_statements_received.load(std::memory_order_acquire), 0u);
      cubeb_set_log_callback(CUBEB_LOG_DISABLED, nullptr);
      log_statements_received.store(0, std::memory_order_release);
      // Disabling logging should flush any log message -- wait a bit and check
      // that this is true.
      ASSERT_EQ(log_statements_received.load(std::memory_order_acquire), 0u);
      log_callback_set = false;
    }
  }

  cubeb_stream_stop(stream);
}

TEST(cubeb, logging_stress)
{
  cubeb_set_log_callback(CUBEB_LOG_NORMAL, test_logging_callback);

  std::atomic<bool> thread_done = {false};

  auto t = std::thread([&thread_done]() {
    uint32_t count = 0;
    do {
      while (rand() % 10) {
        ALOG("Log message #%d!", count++);
      }
    } while (count < 1e4);
    thread_done.store(true);
  });

  bool enabled = true;
  while (!thread_done.load()) {
    if (enabled) {
      cubeb_set_log_callback(CUBEB_LOG_DISABLED, nullptr);
      enabled = false;
    } else {
      cubeb_set_log_callback(CUBEB_LOG_NORMAL, test_logging_callback);
      enabled = true;
    }
  }

  cubeb_set_log_callback(CUBEB_LOG_DISABLED, nullptr);

  t.join();

  ASSERT_TRUE(true);
}
