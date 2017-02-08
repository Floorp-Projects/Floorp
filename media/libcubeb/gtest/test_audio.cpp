/*
 * Copyright © 2013 Sebastien Alaiwan <sebastien.alaiwan@gmail.com>
 *
 * This program is made available under an ISC-style license.  See the
 * accompanying file LICENSE for details.
 */

/* libcubeb api/function exhaustive test. Plays a series of tones in different
 * conditions. */
#include "gtest/gtest.h"
#if !defined(_XOPEN_SOURCE)
#define _XOPEN_SOURCE 600
#endif
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "cubeb/cubeb.h"
#include "common.h"
#include <string>

using namespace std;

#define MAX_NUM_CHANNELS 32

#if !defined(M_PI)
#define M_PI 3.14159265358979323846
#endif

#define VOLUME 0.2

float get_frequency(int channel_index)
{
  return 220.0f * (channel_index+1);
}

template<typename T> T ConvertSample(double input);
template<> float ConvertSample(double input) { return input; }
template<> short ConvertSample(double input) { return short(input * 32767.0f); }

/* store the phase of the generated waveform */
struct synth_state {
  synth_state(int num_channels_, float sample_rate_)
    : num_channels(num_channels_),
    sample_rate(sample_rate_)
  {
    for(int i=0;i < MAX_NUM_CHANNELS;++i)
      phase[i] = 0.0f;
  }

  template<typename T>
    void run(T* audiobuffer, long nframes)
    {
      for(int c=0;c < num_channels;++c) {
        float freq = get_frequency(c);
        float phase_inc = 2.0 * M_PI * freq / sample_rate;
        for(long n=0;n < nframes;++n) {
          audiobuffer[n*num_channels+c] = ConvertSample<T>(sin(phase[c]) * VOLUME);
          phase[c] += phase_inc;
        }
      }
    }

private:
  int num_channels;
  float phase[MAX_NUM_CHANNELS];
  float sample_rate;
};

template<typename T>
long data_cb(cubeb_stream * /*stream*/, void * user, const void * /*inputbuffer*/, void * outputbuffer, long nframes)
{
  synth_state *synth = (synth_state *)user;
  synth->run((T*)outputbuffer, nframes);
  return nframes;
}

struct CubebCleaner
{
  CubebCleaner(cubeb* ctx_) : ctx(ctx_) {}
  ~CubebCleaner() { cubeb_destroy(ctx); }
  cubeb* ctx;
};

struct CubebStreamCleaner
{
  CubebStreamCleaner(cubeb_stream* ctx_) : ctx(ctx_) {}
  ~CubebStreamCleaner() { cubeb_stream_destroy(ctx); }
  cubeb_stream* ctx;
};

void state_cb_audio(cubeb_stream * /*stream*/, void * /*user*/, cubeb_state /*state*/)
{
}

/* Our android backends don't support float, only int16. */
int supports_float32(string backend_id)
{
  return backend_id != "opensl"
    && backend_id != "audiotrack";
}

/* The WASAPI backend only supports float. */
int supports_int16(string backend_id)
{
  return backend_id != "wasapi";
}

/* Some backends don't have code to deal with more than mono or stereo. */
int supports_channel_count(string backend_id, int nchannels)
{
  return nchannels <= 2 ||
    (backend_id != "opensl" && backend_id != "audiotrack");
}

int run_test(int num_channels, layout_info layout, int sampling_rate, int is_float)
{
  int r = CUBEB_OK;

  cubeb *ctx = NULL;

  r = cubeb_init(&ctx, "Cubeb audio test: channels");
  if (r != CUBEB_OK) {
    fprintf(stderr, "Error initializing cubeb library\n");
    return r;
  }
  CubebCleaner cleanup_cubeb_at_exit(ctx);

  const char * backend_id = cubeb_get_backend_id(ctx);

  if ((is_float && !supports_float32(backend_id)) ||
      (!is_float && !supports_int16(backend_id)) ||
      !supports_channel_count(backend_id, num_channels)) {
    /* don't treat this as a test failure. */
    return CUBEB_OK;
  }

  fprintf(stderr, "Testing %d channel(s), layout: %s, %d Hz, %s (%s)\n", num_channels, layout.name, sampling_rate, is_float ? "float" : "short", cubeb_get_backend_id(ctx));

  cubeb_stream_params params;
  params.format = is_float ? CUBEB_SAMPLE_FLOAT32NE : CUBEB_SAMPLE_S16NE;
  params.rate = sampling_rate;
  params.channels = num_channels;
  params.layout = layout.layout;

  synth_state synth(params.channels, params.rate);

  cubeb_stream *stream = NULL;
  r = cubeb_stream_init(ctx, &stream, "test tone", NULL, NULL, NULL, &params,
      4096, is_float ? &data_cb<float> : &data_cb<short>, state_cb_audio, &synth);
  if (r != CUBEB_OK) {
    fprintf(stderr, "Error initializing cubeb stream: %d\n", r);
    return r;
  }

  CubebStreamCleaner cleanup_stream_at_exit(stream);

  cubeb_stream_start(stream);
  delay(200);
  cubeb_stream_stop(stream);

  return r;
}

int run_panning_volume_test(int is_float)
{
  int r = CUBEB_OK;

  cubeb *ctx = NULL;

  r = cubeb_init(&ctx, "Cubeb audio test");
  if (r != CUBEB_OK) {
    fprintf(stderr, "Error initializing cubeb library\n");
    return r;
  }

  CubebCleaner cleanup_cubeb_at_exit(ctx);

  const char * backend_id = cubeb_get_backend_id(ctx);

  if ((is_float && !supports_float32(backend_id)) ||
      (!is_float && !supports_int16(backend_id))) {
    /* don't treat this as a test failure. */
    return CUBEB_OK;
  }

  cubeb_stream_params params;
  params.format = is_float ? CUBEB_SAMPLE_FLOAT32NE : CUBEB_SAMPLE_S16NE;
  params.rate = 44100;
  params.channels = 2;
  params.layout = CUBEB_LAYOUT_STEREO;

  synth_state synth(params.channels, params.rate);

  cubeb_stream *stream = NULL;
  r = cubeb_stream_init(ctx, &stream, "test tone", NULL, NULL, NULL, &params,
      4096, is_float ? &data_cb<float> : &data_cb<short>,
      state_cb_audio, &synth);
  if (r != CUBEB_OK) {
    fprintf(stderr, "Error initializing cubeb stream: %d\n", r);
    return r;
  }

  CubebStreamCleaner cleanup_stream_at_exit(stream);

  fprintf(stderr, "Testing: volume\n");
  for(int i=0;i <= 4; ++i)
  {
    fprintf(stderr, "Volume: %d%%\n", i*25);

    cubeb_stream_set_volume(stream, i/4.0f);
    cubeb_stream_start(stream);
    delay(400);
    cubeb_stream_stop(stream);
    delay(100);
  }

  fprintf(stderr, "Testing: panning\n");
  for(int i=-4;i <= 4; ++i)
  {
    fprintf(stderr, "Panning: %.2f\n", i/4.0f);

    cubeb_stream_set_panning(stream, i/4.0f);
    cubeb_stream_start(stream);
    delay(400);
    cubeb_stream_stop(stream);
    delay(100);
  }

  return r;
}

TEST(cubeb, run_panning_volume_test_short)
{
  ASSERT_EQ(run_panning_volume_test(0), CUBEB_OK);
}

TEST(cubeb, run_panning_volume_test_float)
{
  ASSERT_EQ(run_panning_volume_test(1), CUBEB_OK);
}

TEST(cubeb, run_channel_rate_test)
{
  unsigned int channel_values[] = {
    1,
    2,
    3,
    4,
    6,
  };

  int freq_values[] = {
    16000,
    24000,
    44100,
    48000,
  };

  for(auto channels : channel_values) {
    for(auto freq : freq_values) {
      ASSERT_TRUE(channels < MAX_NUM_CHANNELS);
      fprintf(stderr, "--------------------------\n");
      for (auto layout : layout_infos) {
        if (layout.channels == channels) {
          ASSERT_EQ(run_test(channels, layout, freq, 0), CUBEB_OK);
          ASSERT_EQ(run_test(channels, layout, freq, 1), CUBEB_OK);
        }
      }
    }
  }
}
