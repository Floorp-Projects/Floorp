/*
 * Copyright © 2023 Mozilla Foundation
 *
 * This program is made available under an ISC-style license.  See the
 * accompanying file LICENSE for details.
 */

#define NOMINMAX
#define _USE_MATH_DEFINES

#include "cubeb/cubeb.h"
#include <ratio>

#include "cubeb_audio_dump.h"
#include "gtest/gtest.h"
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <thread>

TEST(cubeb, audio_dump)
{
  cubeb_audio_dump_session_t session;
  int rv = cubeb_audio_dump_init(&session);
  ASSERT_EQ(rv, 0);

  cubeb_stream_params params;
  params.rate = 44100;
  params.channels = 2;
  params.format = CUBEB_SAMPLE_FLOAT32NE;

  cubeb_audio_dump_stream_t dump_stream;
  rv = cubeb_audio_dump_stream_init(session, &dump_stream, params, "test.wav");
  ASSERT_EQ(rv, 0);

  rv = cubeb_audio_dump_start(session);
  ASSERT_EQ(rv, 0);

  float phase = 0;
  const size_t buf_sz = 2 * 44100 / 50;
  float buf[buf_sz];
  for (uint32_t iteration = 0; iteration < 50; iteration++) {
    uint32_t write_idx = 0;
    for (uint32_t i = 0; i < buf_sz / params.channels; i++) {
      for (uint32_t j = 0; j < params.channels; j++) {
        buf[write_idx++] = sin(phase);
      }
      phase += 440 * M_PI * 2 / 44100;
      if (phase > 2 * M_PI) {
        phase -= 2 * M_PI;
      }
    }
    rv = cubeb_audio_dump_write(dump_stream, buf, 2 * 44100 / 50);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    ASSERT_EQ(rv, 0);
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  rv = cubeb_audio_dump_stop(session);
  ASSERT_EQ(rv, 0);

  rv = cubeb_audio_dump_stream_shutdown(session, dump_stream);
  ASSERT_EQ(rv, 0);

  rv = cubeb_audio_dump_shutdown(session);
  ASSERT_EQ(rv, 0);

  std::ifstream file("test.wav");
  ASSERT_TRUE(file.good());
}

#undef NOMINMAX
