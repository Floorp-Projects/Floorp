/*
 * Copyright © 2016 Mozilla Foundation
 *
 * This program is made available under an ISC-style license.  See the
 * accompanying file LICENSE for details.
 */
#ifndef NOMINMAX
#define NOMINMAX
#endif // NOMINMAX
#include "gtest/gtest.h"
#include "common.h"
#include "cubeb_resampler_internal.h"
#include <stdio.h>
#include <algorithm>
#include <iostream>

/* Windows cmath USE_MATH_DEFINE thing... */
const float PI = 3.14159265359f;

/* Testing all sample rates is very long, so if THOROUGH_TESTING is not defined,
 * only part of the test suite is ran. */
#ifdef THOROUGH_TESTING
/* Some standard sample rates we're testing with. */
const uint32_t sample_rates[] = {
    8000,
   16000,
   32000,
   44100,
   48000,
   88200,
   96000,
  192000
};
/* The maximum number of channels we're resampling. */
const uint32_t max_channels = 2;
/* The minimum an maximum number of milliseconds we're resampling for. This is
 * used to simulate the fact that the audio stream is resampled in chunks,
 * because audio is delivered using callbacks. */
const uint32_t min_chunks = 10; /* ms */
const uint32_t max_chunks = 30; /* ms */
const uint32_t chunk_increment = 1;

#else

const uint32_t sample_rates[] = {
    8000,
   44100,
   48000,
};
const uint32_t max_channels = 2;
const uint32_t min_chunks = 10; /* ms */
const uint32_t max_chunks = 30; /* ms */
const uint32_t chunk_increment = 10;
#endif

#define DUMP_ARRAYS
#ifdef DUMP_ARRAYS
/**
 * Files produced by dump(...) can be converted to .wave files using:
 *
 * sox -c <channel_count> -r <rate> -e float -b 32  file.raw file.wav
 *
 * for floating-point audio, or:
 *
 * sox -c <channel_count> -r <rate> -e unsigned -b 16  file.raw file.wav
 *
 * for 16bit integer audio.
 */

/* Use the correct implementation of fopen, depending on the platform. */
void fopen_portable(FILE ** f, const char * name, const char * mode)
{
#ifdef WIN32
  fopen_s(f, name, mode);
#else
  *f = fopen(name, mode);
#endif
}

template<typename T>
void dump(const char * name, T * frames, size_t count)
{
  FILE * file;
  fopen_portable(&file, name, "wb");

  if (!file) {
    fprintf(stderr, "error opening %s\n", name);
    return;
  }

  if (count != fwrite(frames, sizeof(T), count, file)) {
    fprintf(stderr, "error writing to %s\n", name);
  }
  fclose(file);
}
#else
template<typename T>
void dump(const char * name, T * frames, size_t count)
{ }
#endif

// The more the ratio is far from 1, the more we accept a big error.
float epsilon_tweak_ratio(float ratio)
{
  return ratio >= 1 ? ratio : 1 / ratio;
}

// Epsilon values for comparing resampled data to expected data.
// The bigger the resampling ratio is, the more lax we are about errors.
template<typename T>
T epsilon(float ratio);

template<>
float epsilon(float ratio) {
  return 0.08f * epsilon_tweak_ratio(ratio);
}

template<>
int16_t epsilon(float ratio) {
  return static_cast<int16_t>(10 * epsilon_tweak_ratio(ratio));
}

void test_delay_lines(uint32_t delay_frames, uint32_t channels, uint32_t chunk_ms)
{
  const size_t length_s = 2;
  const size_t rate = 44100;
  const size_t length_frames = rate * length_s;
  delay_line<float> delay(delay_frames, channels, rate);
  auto_array<float> input;
  auto_array<float> output;
  uint32_t chunk_length = channels * chunk_ms * rate / 1000;
  uint32_t output_offset = 0;
  uint32_t channel = 0;

  /** Generate diracs every 100 frames, and check they are delayed. */
  input.push_silence(length_frames * channels);
  for (uint32_t i = 0; i < input.length() - 1; i+=100) {
    input.data()[i + channel] = 0.5;
    channel = (channel + 1) % channels;
  }
  dump("input.raw", input.data(), input.length());
  while(input.length()) {
    uint32_t to_pop = std::min<uint32_t>(input.length(), chunk_length * channels);
    float * in = delay.input_buffer(to_pop / channels);
    input.pop(in, to_pop);
    delay.written(to_pop / channels);
    output.push_silence(to_pop);
    delay.output(output.data() + output_offset, to_pop / channels);
    output_offset += to_pop;
  }

  // Check the diracs have been shifted by `delay_frames` frames.
  for (uint32_t i = 0; i < output.length() - delay_frames * channels + 1; i+=100) {
    ASSERT_EQ(output.data()[i + channel + delay_frames * channels], 0.5);
    channel = (channel + 1) % channels;
  }

  dump("output.raw", output.data(), output.length());
}
/**
 * This takes sine waves with a certain `channels` count, `source_rate`, and
 * resample them, by chunk of `chunk_duration` milliseconds, to `target_rate`.
 * Then a sample-wise comparison is performed against a sine wave generated at
 * the correct rate.
 */
template<typename T>
void test_resampler_one_way(uint32_t channels, uint32_t source_rate, uint32_t target_rate, float chunk_duration)
{
  size_t chunk_duration_in_source_frames = static_cast<uint32_t>(ceil(chunk_duration * source_rate / 1000.));
  float resampling_ratio = static_cast<float>(source_rate) / target_rate;
  cubeb_resampler_speex_one_way<T> resampler(channels, source_rate, target_rate, 3);
  auto_array<T> source(channels * source_rate * 10);
  auto_array<T> destination(channels * target_rate * 10);
  auto_array<T> expected(channels * target_rate * 10);
  uint32_t phase_index = 0;
  uint32_t offset = 0;
  const uint32_t buf_len = 2; /* seconds */

  // generate a sine wave in each channel, at the source sample rate
  source.push_silence(channels * source_rate * buf_len);
  while(offset != source.length()) {
    float  p = phase_index++ / static_cast<float>(source_rate);
    for (uint32_t j = 0; j < channels; j++) {
      source.data()[offset++] = 0.5 * sin(440. * 2 * PI * p);
    }
  }

  dump("input.raw", source.data(), source.length());

  expected.push_silence(channels * target_rate * buf_len);
  // generate a sine wave in each channel, at the target sample rate.
  // Insert silent samples at the beginning to account for the resampler latency.
  offset = resampler.latency() * channels;
  for (uint32_t i = 0; i < offset; i++) {
    expected.data()[i] = 0.0f;
  }
  phase_index = 0;
  while (offset != expected.length()) {
    float  p = phase_index++ / static_cast<float>(target_rate);
    for (uint32_t j = 0; j < channels; j++) {
      expected.data()[offset++] = 0.5 * sin(440. * 2 * PI * p);
    }
  }

  dump("expected.raw", expected.data(), expected.length());

  // resample by chunk
  uint32_t write_offset = 0;
  destination.push_silence(channels * target_rate * buf_len);
  while (write_offset < destination.length())
  {
    size_t output_frames = static_cast<uint32_t>(floor(chunk_duration_in_source_frames / resampling_ratio));
    uint32_t input_frames = resampler.input_needed_for_output(output_frames);
    resampler.input(source.data(), input_frames);
    source.pop(nullptr, input_frames * channels);
    resampler.output(destination.data() + write_offset,
                     std::min(output_frames, (destination.length() - write_offset) / channels));
    write_offset += output_frames * channels;
  }

  dump("output.raw", destination.data(), expected.length());

  // compare, taking the latency into account
  bool fuzzy_equal = true;
  for (uint32_t i = resampler.latency() + 1; i < expected.length(); i++) {
    float diff = fabs(expected.data()[i] - destination.data()[i]);
    if (diff > epsilon<T>(resampling_ratio)) {
      fprintf(stderr, "divergence at %d: %f %f (delta %f)\n", i, expected.data()[i], destination.data()[i], diff);
      fuzzy_equal = false;
    }
  }
  ASSERT_TRUE(fuzzy_equal);
}

template<typename T>
cubeb_sample_format cubeb_format();

template<>
cubeb_sample_format cubeb_format<float>()
{
  return CUBEB_SAMPLE_FLOAT32NE;
}

template<>
cubeb_sample_format cubeb_format<short>()
{
  return CUBEB_SAMPLE_S16NE;
}

struct osc_state {
  osc_state()
    : input_phase_index(0)
    , output_phase_index(0)
    , output_offset(0)
    , input_channels(0)
    , output_channels(0)
  {}
  uint32_t input_phase_index;
  uint32_t max_output_phase_index;
  uint32_t output_phase_index;
  uint32_t output_offset;
  uint32_t input_channels;
  uint32_t output_channels;
  uint32_t output_rate;
  uint32_t target_rate;
  auto_array<float> input;
  auto_array<float> output;
};

uint32_t fill_with_sine(float * buf, uint32_t rate, uint32_t channels,
                        uint32_t frames, uint32_t initial_phase)
{
  uint32_t offset = 0;
  for (uint32_t i = 0; i < frames; i++) {
    float  p = initial_phase++ / static_cast<float>(rate);
    for (uint32_t j = 0; j < channels; j++) {
      buf[offset++] = 0.5 * sin(440. * 2 * PI * p);
    }
  }
  return initial_phase;
}

long data_cb_resampler(cubeb_stream * /*stm*/, void * user_ptr,
             const void * input_buffer, void * output_buffer, long frame_count)
{
  osc_state * state = reinterpret_cast<osc_state*>(user_ptr);
  const float * in = reinterpret_cast<const float*>(input_buffer);
  float * out = reinterpret_cast<float*>(output_buffer);

  state->input.push(in, frame_count * state->input_channels);

  /* Check how much output frames we need to write */
  uint32_t remaining = state->max_output_phase_index - state->output_phase_index;
  uint32_t to_write = std::min<uint32_t>(remaining, frame_count);
  state->output_phase_index = fill_with_sine(out,
                                             state->target_rate,
                                             state->output_channels,
                                             to_write,
                                             state->output_phase_index);

  return to_write;
}

template<typename T>
bool array_fuzzy_equal(const auto_array<T>& lhs, const auto_array<T>& rhs, T epsi)
{
  uint32_t len = std::min(lhs.length(), rhs.length());

  for (uint32_t i = 0; i < len; i++) {
    if (fabs(lhs.at(i) - rhs.at(i)) > epsi) {
      std::cout << "not fuzzy equal at index: " << i
                << " lhs: " << lhs.at(i) <<  " rhs: " << rhs.at(i)
                << " delta: " << fabs(lhs.at(i) - rhs.at(i))
                << " epsilon: "<< epsi << std::endl;
      return false;
    }
  }
  return true;
}

template<typename T>
void test_resampler_duplex(uint32_t input_channels, uint32_t output_channels,
                           uint32_t input_rate, uint32_t output_rate,
                           uint32_t target_rate, float chunk_duration)
{
  cubeb_stream_params input_params;
  cubeb_stream_params output_params;
  osc_state state;

  input_params.format = output_params.format = cubeb_format<T>();
  state.input_channels = input_params.channels = input_channels;
  state.output_channels = output_params.channels = output_channels;
  input_params.rate = input_rate;
  state.output_rate = output_params.rate = output_rate;
  state.target_rate = target_rate;
  input_params.prefs = output_params.prefs = CUBEB_STREAM_PREF_NONE;
  long got;

  cubeb_resampler * resampler =
    cubeb_resampler_create((cubeb_stream*)nullptr, &input_params, &output_params, target_rate,
                           data_cb_resampler, (void*)&state, CUBEB_RESAMPLER_QUALITY_VOIP);

  long latency = cubeb_resampler_latency(resampler);

  const uint32_t duration_s = 2;
  int32_t duration_frames = duration_s * target_rate;
  uint32_t input_array_frame_count = ceil(chunk_duration * input_rate / 1000) + ceilf(static_cast<float>(input_rate) / target_rate) * 2;
  uint32_t output_array_frame_count = chunk_duration * output_rate / 1000;
  auto_array<float> input_buffer(input_channels * input_array_frame_count);
  auto_array<float> output_buffer(output_channels * output_array_frame_count);
  auto_array<float> expected_resampled_input(input_channels * duration_frames);
  auto_array<float> expected_resampled_output(output_channels * output_rate * duration_s);

  state.max_output_phase_index = duration_s * target_rate;

  expected_resampled_input.push_silence(input_channels * duration_frames);
  expected_resampled_output.push_silence(output_channels * output_rate * duration_s);

  /* expected output is a 440Hz sine wave at 16kHz */
  fill_with_sine(expected_resampled_input.data() + latency,
                 target_rate, input_channels, duration_frames - latency, 0);
  /* expected output is a 440Hz sine wave at 32kHz */
  fill_with_sine(expected_resampled_output.data() + latency,
                 output_rate, output_channels, output_rate * duration_s - latency, 0);

  while (state.output_phase_index != state.max_output_phase_index) {
    uint32_t leftover_samples = input_buffer.length() * input_channels;
    input_buffer.reserve(input_array_frame_count);
    state.input_phase_index = fill_with_sine(input_buffer.data() + leftover_samples,
                                             input_rate,
                                             input_channels,
                                             input_array_frame_count - leftover_samples,
                                             state.input_phase_index);
    long input_consumed = input_array_frame_count;
    input_buffer.set_length(input_array_frame_count);

    got = cubeb_resampler_fill(resampler,
                               input_buffer.data(), &input_consumed,
                               output_buffer.data(), output_array_frame_count);

    /* handle leftover input */
    if (input_array_frame_count != static_cast<uint32_t>(input_consumed)) {
      input_buffer.pop(nullptr, input_consumed * input_channels);
    } else {
      input_buffer.clear();
    }

    state.output.push(output_buffer.data(), got * state.output_channels);
  }

  dump("input_expected.raw", expected_resampled_input.data(), expected_resampled_input.length());
  dump("output_expected.raw", expected_resampled_output.data(), expected_resampled_output.length());
  dump("input.raw", state.input.data(), state.input.length());
  dump("output.raw", state.output.data(), state.output.length());

 // This is disabled because the latency estimation in the resampler code is
 // slightly off so we can generate expected vectors.
 // See https://github.com/kinetiknz/cubeb/issues/93
 // ASSERT_TRUE(array_fuzzy_equal(state.input, expected_resampled_input, epsilon<T>(input_rate/target_rate)));
 // ASSERT_TRUE(array_fuzzy_equal(state.output, expected_resampled_output, epsilon<T>(output_rate/target_rate)));

  cubeb_resampler_destroy(resampler);
}

#define array_size(x) (sizeof(x) / sizeof(x[0]))

TEST(cubeb, resampler_one_way)
{
  /* Test one way resamplers */
  for (uint32_t channels = 1; channels <= max_channels; channels++) {
    for (uint32_t source_rate = 0; source_rate < array_size(sample_rates); source_rate++) {
      for (uint32_t dest_rate = 0; dest_rate < array_size(sample_rates); dest_rate++) {
        for (uint32_t chunk_duration = min_chunks; chunk_duration < max_chunks; chunk_duration+=chunk_increment) {
          fprintf(stderr, "one_way: channels: %d, source_rate: %d, dest_rate: %d, chunk_duration: %d\n",
                  channels, sample_rates[source_rate], sample_rates[dest_rate], chunk_duration);
          test_resampler_one_way<float>(channels, sample_rates[source_rate],
                                        sample_rates[dest_rate], chunk_duration);
        }
      }
    }
  }
}

TEST(cubeb, DISABLED_resampler_duplex)
{
  for (uint32_t input_channels = 1; input_channels <= max_channels; input_channels++) {
    for (uint32_t output_channels = 1; output_channels <= max_channels; output_channels++) {
      for (uint32_t source_rate_input = 0; source_rate_input < array_size(sample_rates); source_rate_input++) {
        for (uint32_t source_rate_output = 0; source_rate_output < array_size(sample_rates); source_rate_output++) {
          for (uint32_t dest_rate = 0; dest_rate < array_size(sample_rates); dest_rate++) {
            for (uint32_t chunk_duration = min_chunks; chunk_duration < max_chunks; chunk_duration+=chunk_increment) {
              fprintf(stderr, "input channels:%d output_channels:%d input_rate:%d "
                     "output_rate:%d target_rate:%d chunk_ms:%d\n",
                     input_channels, output_channels,
                     sample_rates[source_rate_input],
                     sample_rates[source_rate_output],
                     sample_rates[dest_rate],
                     chunk_duration);
              test_resampler_duplex<float>(input_channels, output_channels,
                                           sample_rates[source_rate_input],
                                           sample_rates[source_rate_output],
                                           sample_rates[dest_rate],
                                           chunk_duration);
            }
          }
        }
      }
    }
  }
}

TEST(cubeb, resampler_delay_line)
{
  for (uint32_t channel = 1; channel <= 2; channel++) {
    for (uint32_t delay_frames = 4; delay_frames <= 40; delay_frames+=chunk_increment) {
      for (uint32_t chunk_size = 10; chunk_size <= 30; chunk_size++) {
       fprintf(stderr, "channel: %d, delay_frames: %d, chunk_size: %d\n",
              channel, delay_frames, chunk_size);
        test_delay_lines(delay_frames, channel, chunk_size);
      }
    }
  }
}

long test_output_only_noop_data_cb(cubeb_stream * /*stm*/, void * /*user_ptr*/,
                                   const void * input_buffer,
                                   void * output_buffer, long frame_count)
{
  EXPECT_TRUE(output_buffer);
  EXPECT_TRUE(!input_buffer);
  return frame_count;
}

TEST(cubeb, resampler_output_only_noop)
{
  cubeb_stream_params output_params;
  int target_rate;

  output_params.rate = 44100;
  output_params.channels = 1;
  output_params.format = CUBEB_SAMPLE_FLOAT32NE;
  target_rate = output_params.rate;

  cubeb_resampler * resampler =
    cubeb_resampler_create((cubeb_stream*)nullptr, nullptr, &output_params, target_rate,
                           test_output_only_noop_data_cb, nullptr,
                           CUBEB_RESAMPLER_QUALITY_VOIP);

  const long out_frames = 128;
  float out_buffer[out_frames];
  long got;

  got = cubeb_resampler_fill(resampler, nullptr, nullptr,
                             out_buffer, out_frames);

  ASSERT_EQ(got, out_frames);

  cubeb_resampler_destroy(resampler);
}

long test_drain_data_cb(cubeb_stream * /*stm*/, void * user_ptr,
                        const void * input_buffer,
                        void * output_buffer, long frame_count)
{
  EXPECT_TRUE(output_buffer);
  EXPECT_TRUE(!input_buffer);
  auto cb_count = static_cast<int *>(user_ptr);
  (*cb_count)++;
  return frame_count - 1;
}

TEST(cubeb, resampler_drain)
{
  cubeb_stream_params output_params;
  int target_rate;

  output_params.rate = 44100;
  output_params.channels = 1;
  output_params.format = CUBEB_SAMPLE_FLOAT32NE;
  target_rate = 48000;
  int cb_count = 0;

  cubeb_resampler * resampler =
    cubeb_resampler_create((cubeb_stream*)nullptr, nullptr, &output_params, target_rate,
                           test_drain_data_cb, &cb_count,
                           CUBEB_RESAMPLER_QUALITY_VOIP);

  const long out_frames = 128;
  float out_buffer[out_frames];
  long got;

  do {
    got = cubeb_resampler_fill(resampler, nullptr, nullptr,
                               out_buffer, out_frames);
  } while (got == out_frames);

  /* The callback should be called once but not again after returning <
   * frame_count. */
  ASSERT_EQ(cb_count, 1);

  cubeb_resampler_destroy(resampler);
}

// gtest does not support using ASSERT_EQ and friend in a function that returns
// a value.
void check_output(const void * input_buffer, void * output_buffer, long frame_count)
{
  ASSERT_EQ(input_buffer, nullptr);
  ASSERT_EQ(frame_count, 256);
  ASSERT_TRUE(!!output_buffer);
}

long cb_passthrough_resampler_output(cubeb_stream * /*stm*/, void * /*user_ptr*/,
                                     const void * input_buffer,
                                     void * output_buffer, long frame_count)
{
  check_output(input_buffer, output_buffer, frame_count);
  return frame_count;
}

TEST(cubeb, resampler_passthrough_output_only)
{
  // Test that the passthrough resampler works when there is only an output stream.
  cubeb_stream_params output_params;

  const size_t output_channels = 2;
  output_params.channels = output_channels;
  output_params.rate = 44100;
  output_params.format = CUBEB_SAMPLE_FLOAT32NE;
  int target_rate = output_params.rate;

  cubeb_resampler * resampler =
    cubeb_resampler_create((cubeb_stream*)nullptr, nullptr, &output_params,
                           target_rate, cb_passthrough_resampler_output, nullptr,
                           CUBEB_RESAMPLER_QUALITY_VOIP);

  float output_buffer[output_channels * 256];

  long got;
  for (uint32_t i = 0; i < 30; i++) {
    got = cubeb_resampler_fill(resampler, nullptr, nullptr, output_buffer, 256);
    ASSERT_EQ(got, 256);
  }

  cubeb_resampler_destroy(resampler);
}

// gtest does not support using ASSERT_EQ and friend in a function that returns
// a value.
void check_input(const void * input_buffer, void * output_buffer, long frame_count)
{
  ASSERT_EQ(output_buffer, nullptr);
  ASSERT_EQ(frame_count, 256);
  ASSERT_TRUE(!!input_buffer);
}

long cb_passthrough_resampler_input(cubeb_stream * /*stm*/, void * /*user_ptr*/,
                                    const void * input_buffer,
                                    void * output_buffer, long frame_count)
{
  check_input(input_buffer, output_buffer, frame_count);
  return frame_count;
}

TEST(cubeb, resampler_passthrough_input_only)
{
  // Test that the passthrough resampler works when there is only an output stream.
  cubeb_stream_params input_params;

  const size_t input_channels = 2;
  input_params.channels = input_channels;
  input_params.rate = 44100;
  input_params.format = CUBEB_SAMPLE_FLOAT32NE;
  int target_rate = input_params.rate;

  cubeb_resampler * resampler =
    cubeb_resampler_create((cubeb_stream*)nullptr, &input_params, nullptr,
                           target_rate, cb_passthrough_resampler_input, nullptr,
                           CUBEB_RESAMPLER_QUALITY_VOIP);

  float input_buffer[input_channels * 256];

  long got;
  for (uint32_t i = 0; i < 30; i++) {
    long int frames = 256;
    got = cubeb_resampler_fill(resampler, input_buffer, &frames, nullptr, 0);
    ASSERT_EQ(got, 256);
  }

  cubeb_resampler_destroy(resampler);
}

template<typename T>
long seq(T* array, int stride, long start, long count)
{
  uint32_t output_idx = 0;
  for(int i = 0; i < count; i++) {
    for (int j = 0; j < stride; j++) {
      array[output_idx + j] = static_cast<T>(start + i);
    }
    output_idx += stride;
  }
  return start + count;
}

template<typename T>
void is_seq(T * array, int stride, long count, long expected_start)
{
  uint32_t output_index = 0;
  for (long i = 0; i < count; i++) {
    for (int j = 0; j < stride; j++) {
      ASSERT_EQ(array[output_index + j], expected_start + i);
    }
    output_index += stride;
  }
}

template<typename T>
void is_not_seq(T * array, int stride, long count, long expected_start)
{
  uint32_t output_index = 0;
  for (long i = 0; i < count; i++) {
    for (int j = 0; j < stride; j++) {
      ASSERT_NE(array[output_index + j], expected_start + i);
    }
    output_index += stride;
  }
}

struct closure {
  int input_channel_count;
};

// gtest does not support using ASSERT_EQ and friend in a function that returns
// a value.
template<typename T>
void check_duplex(const T * input_buffer,
                  T * output_buffer, long frame_count,
                  int input_channel_count)
{
  ASSERT_EQ(frame_count, 256);
  // Silence scan-build warning.
  ASSERT_TRUE(!!output_buffer); assert(output_buffer);
  ASSERT_TRUE(!!input_buffer); assert(input_buffer);

  int output_index = 0;
  int input_index = 0;
  for (int i = 0; i < frame_count; i++) {
    // output is two channels, input one or two channels.
    if (input_channel_count == 1) {
      output_buffer[output_index] = output_buffer[output_index + 1] = input_buffer[i];
    } else if (input_channel_count == 2) {
      output_buffer[output_index] = input_buffer[input_index];
      output_buffer[output_index + 1] = input_buffer[input_index + 1];
    }
    output_index += 2;
    input_index += input_channel_count;
  }
}

long cb_passthrough_resampler_duplex(cubeb_stream * /*stm*/, void * user_ptr,
                                     const void * input_buffer,
                                     void * output_buffer, long frame_count)
{
  closure * c = reinterpret_cast<closure*>(user_ptr);
  check_duplex<float>(static_cast<const float*>(input_buffer),
                      static_cast<float*>(output_buffer),
                      frame_count, c->input_channel_count);
  return frame_count;
}


TEST(cubeb, resampler_passthrough_duplex_callback_reordering)
{
  // Test that when pre-buffering on resampler creation, we can survive an input
  // callback being delayed.

  cubeb_stream_params input_params;
  cubeb_stream_params output_params;

  const int input_channels = 1;
  const int output_channels = 2;

  input_params.channels = input_channels;
  input_params.rate = 44100;
  input_params.format = CUBEB_SAMPLE_FLOAT32NE;

  output_params.channels = output_channels;
  output_params.rate = input_params.rate;
  output_params.format = CUBEB_SAMPLE_FLOAT32NE;

  int target_rate = input_params.rate;

  closure c;
  c.input_channel_count = input_channels;

  cubeb_resampler * resampler =
    cubeb_resampler_create((cubeb_stream*)nullptr, &input_params, &output_params,
                           target_rate, cb_passthrough_resampler_duplex, &c,
                           CUBEB_RESAMPLER_QUALITY_VOIP);

  const long BUF_BASE_SIZE = 256;
  float input_buffer_prebuffer[input_channels * BUF_BASE_SIZE * 2];
  float input_buffer_glitch[input_channels * BUF_BASE_SIZE * 2];
  float input_buffer_normal[input_channels * BUF_BASE_SIZE];
  float output_buffer[output_channels * BUF_BASE_SIZE];

  long seq_idx = 0;
  long output_seq_idx = 0;

  long prebuffer_frames = ARRAY_LENGTH(input_buffer_prebuffer) / input_params.channels;
  seq_idx = seq(input_buffer_prebuffer, input_channels, seq_idx,
                prebuffer_frames);

  long got = cubeb_resampler_fill(resampler, input_buffer_prebuffer, &prebuffer_frames,
                                  output_buffer, BUF_BASE_SIZE);

  output_seq_idx += BUF_BASE_SIZE;

  // prebuffer_frames will hold the frames used by the resampler.
  ASSERT_EQ(prebuffer_frames, BUF_BASE_SIZE);
  ASSERT_EQ(got, BUF_BASE_SIZE);

  for (uint32_t i = 0; i < 300; i++) {
    long int frames = BUF_BASE_SIZE;
    // Simulate that sometimes, we don't have the input callback on time
    if (i != 0 && (i % 100) == 0) {
      long zero = 0;
      got = cubeb_resampler_fill(resampler, input_buffer_normal /* unused here */,
                                 &zero, output_buffer, BUF_BASE_SIZE);
      is_seq(output_buffer, 2, BUF_BASE_SIZE, output_seq_idx);
      output_seq_idx += BUF_BASE_SIZE;
    } else if (i != 0 && (i % 100) == 1) {
      // if this is the case, the on the next iteration, we'll have twice the
      // amount of input frames
      seq_idx = seq(input_buffer_glitch, input_channels, seq_idx, BUF_BASE_SIZE * 2);
      frames = 2 * BUF_BASE_SIZE;
      got = cubeb_resampler_fill(resampler, input_buffer_glitch, &frames, output_buffer, BUF_BASE_SIZE);
      is_seq(output_buffer, 2, BUF_BASE_SIZE, output_seq_idx);
      output_seq_idx += BUF_BASE_SIZE;
    } else {
       // normal case
      seq_idx = seq(input_buffer_normal, input_channels, seq_idx, BUF_BASE_SIZE);
      long normal_input_frame_count = 256;
      got = cubeb_resampler_fill(resampler, input_buffer_normal, &normal_input_frame_count, output_buffer, BUF_BASE_SIZE);
      is_seq(output_buffer, 2, BUF_BASE_SIZE, output_seq_idx);
      output_seq_idx += BUF_BASE_SIZE;
    }
    ASSERT_EQ(got, BUF_BASE_SIZE);
  }

  cubeb_resampler_destroy(resampler);
}

// Artificially simulate output thread underruns,
// by building up artificial delay in the input.
// Check that the frame drop logic kicks in.
TEST(cubeb, resampler_drift_drop_data)
{
  for (uint32_t input_channels = 1; input_channels < 3; input_channels++) {
    cubeb_stream_params input_params;
    cubeb_stream_params output_params;

    const int output_channels = 2;
    const int sample_rate = 44100;

    input_params.channels = input_channels;
    input_params.rate = sample_rate;
    input_params.format = CUBEB_SAMPLE_FLOAT32NE;

    output_params.channels = output_channels;
    output_params.rate = sample_rate;
    output_params.format = CUBEB_SAMPLE_FLOAT32NE;

    int target_rate = input_params.rate;

    closure c;
    c.input_channel_count = input_channels;

    cubeb_resampler * resampler =
      cubeb_resampler_create((cubeb_stream*)nullptr, &input_params, &output_params,
        target_rate, cb_passthrough_resampler_duplex, &c,
        CUBEB_RESAMPLER_QUALITY_VOIP);

    const long BUF_BASE_SIZE = 256;

    // The factor by which the deadline is missed. This is intentionally
    // kind of large to trigger the frame drop quickly. In real life, multiple
    // smaller under-runs would accumulate.
    const long UNDERRUN_FACTOR = 10;
    // Number buffer used for pre-buffering, that some backends do.
    const long PREBUFFER_FACTOR = 2;

    std::vector<float> input_buffer_prebuffer(input_channels * BUF_BASE_SIZE * PREBUFFER_FACTOR);
    std::vector<float> input_buffer_glitch(input_channels * BUF_BASE_SIZE * UNDERRUN_FACTOR);
    std::vector<float> input_buffer_normal(input_channels * BUF_BASE_SIZE);
    std::vector<float> output_buffer(output_channels * BUF_BASE_SIZE);

    long seq_idx = 0;
    long output_seq_idx = 0;

    long prebuffer_frames = input_buffer_prebuffer.size() / input_params.channels;
    seq_idx = seq(input_buffer_prebuffer.data(), input_channels, seq_idx,
      prebuffer_frames);

    long got = cubeb_resampler_fill(resampler, input_buffer_prebuffer.data(), &prebuffer_frames,
      output_buffer.data(), BUF_BASE_SIZE);

    output_seq_idx += BUF_BASE_SIZE;

    // prebuffer_frames will hold the frames used by the resampler.
    ASSERT_EQ(prebuffer_frames, BUF_BASE_SIZE);
    ASSERT_EQ(got, BUF_BASE_SIZE);

    for (uint32_t i = 0; i < 300; i++) {
      long int frames = BUF_BASE_SIZE;
      if (i != 0 && (i % 100) == 1) {
        // Once in a while, the output thread misses its deadline.
        // The input thread still produces data, so it ends up accumulating. Simulate this by providing a
        // much bigger input buffer. Check that the sequence is now unaligned, meaning we've dropped data
        // to keep everything in sync.
        seq_idx = seq(input_buffer_glitch.data(), input_channels, seq_idx, BUF_BASE_SIZE * UNDERRUN_FACTOR);
        frames = BUF_BASE_SIZE * UNDERRUN_FACTOR;
        got = cubeb_resampler_fill(resampler, input_buffer_glitch.data(), &frames, output_buffer.data(), BUF_BASE_SIZE);
        is_seq(output_buffer.data(), 2, BUF_BASE_SIZE, output_seq_idx);
        output_seq_idx += BUF_BASE_SIZE;
      }
      else if (i != 0 && (i % 100) == 2) {
        // On the next iteration, the sequence should be broken
        seq_idx = seq(input_buffer_normal.data(), input_channels, seq_idx, BUF_BASE_SIZE);
        long normal_input_frame_count = 256;
        got = cubeb_resampler_fill(resampler, input_buffer_normal.data(), &normal_input_frame_count, output_buffer.data(), BUF_BASE_SIZE);
        is_not_seq(output_buffer.data(), output_channels, BUF_BASE_SIZE, output_seq_idx);
        // Reclock so that we can use is_seq again.
        output_seq_idx = output_buffer[BUF_BASE_SIZE * output_channels - 1] + 1;
      }
      else {
        // normal case
        seq_idx = seq(input_buffer_normal.data(), input_channels, seq_idx, BUF_BASE_SIZE);
        long normal_input_frame_count = 256;
        got = cubeb_resampler_fill(resampler, input_buffer_normal.data(), &normal_input_frame_count, output_buffer.data(), BUF_BASE_SIZE);
        is_seq(output_buffer.data(), output_channels, BUF_BASE_SIZE, output_seq_idx);
        output_seq_idx += BUF_BASE_SIZE;
      }
      ASSERT_EQ(got, BUF_BASE_SIZE);
    }

    cubeb_resampler_destroy(resampler);
  }
}

static long
passthrough_resampler_fill_eq_input(cubeb_stream * stream,
                                    void * user_ptr,
                                    void const * input_buffer,
                                    void * output_buffer,
                                    long nframes) {
  // gtest does not support using ASSERT_EQ and friends in a
  // function that returns a value.
  [nframes, input_buffer]() {
    ASSERT_EQ(nframes, 32);
    const float* input = static_cast<const float*>(input_buffer);
    for (int i = 0; i < 64; ++i) {
      ASSERT_FLOAT_EQ(input[i], 0.01 * i);
    }
  }();
  return nframes;
}

TEST(cubeb, passthrough_resampler_fill_eq_input) {
  uint32_t channels = 2;
  uint32_t sample_rate = 44100;
  passthrough_resampler<float> resampler =
    passthrough_resampler<float>(nullptr, passthrough_resampler_fill_eq_input,
                                 nullptr, channels, sample_rate);

  long input_frame_count = 32;
  long output_frame_count = 32;
  float input[64] = {};
  float output[64] = {};
  for (uint32_t i = 0; i < input_frame_count * channels; ++i) {
    input[i] = 0.01 * i;
  }
  long got = resampler.fill(input, &input_frame_count, output, output_frame_count);
  ASSERT_EQ(got, output_frame_count);
  // Input frames used must be equal to output frames.
  ASSERT_EQ(input_frame_count, output_frame_count);
}

static long
passthrough_resampler_fill_short_input(cubeb_stream * stream,
                                       void * user_ptr,
                                       void const * input_buffer,
                                       void * output_buffer,
                                       long nframes) {
  // gtest does not support using ASSERT_EQ and friends in a
  // function that returns a value.
  [nframes, input_buffer]() {
    ASSERT_EQ(nframes, 32);
    const float* input = static_cast<const float*>(input_buffer);
    // First part contains the input
    for (int i = 0; i < 32; ++i) {
      ASSERT_FLOAT_EQ(input[i], 0.01 * i);
    }
    // missing part contains silence
    for (int i = 32; i < 64; ++i) {
      ASSERT_FLOAT_EQ(input[i], 0.0);
    }
  }();
  return nframes;
}

TEST(cubeb, passthrough_resampler_fill_short_input) {
  uint32_t channels = 2;
  uint32_t sample_rate = 44100;
  passthrough_resampler<float> resampler =
    passthrough_resampler<float>(nullptr, passthrough_resampler_fill_short_input,
                                 nullptr, channels, sample_rate);

  long input_frame_count = 16;
  long output_frame_count = 32;
  float input[64] = {};
  float output[64] = {};
  for (uint32_t i = 0; i < input_frame_count * channels; ++i) {
    input[i] = 0.01 * i;
  }
  long got = resampler.fill(input, &input_frame_count, output, output_frame_count);
  ASSERT_EQ(got, output_frame_count);
  // Input frames used are less than the output frames due to glitch.
  ASSERT_EQ(input_frame_count, output_frame_count - 16);
}

static long
passthrough_resampler_fill_input_left(cubeb_stream * stream,
                                     void * user_ptr,
                                     void const * input_buffer,
                                     void * output_buffer,
                                     long nframes) {
  // gtest does not support using ASSERT_EQ and friends in a
  // function that returns a value.
  int iteration = *static_cast<int*>(user_ptr);
  if (iteration == 1) {
    [nframes, input_buffer]() {
      ASSERT_EQ(nframes, 32);
      const float* input = static_cast<const float*>(input_buffer);
      for (int i = 0; i < 64; ++i) {
        ASSERT_FLOAT_EQ(input[i], 0.01 * i);
      }
    }();
  } else if (iteration == 2) {
    [nframes, input_buffer]() {
      ASSERT_EQ(nframes, 32);
      const float* input = static_cast<const float*>(input_buffer);
      for (int i = 0; i < 32; ++i) {
        // First part contains the reamaining input samples from previous
        // iteration (since they were more).
        ASSERT_FLOAT_EQ(input[i], 0.01 * (i + 64));
        // next part contains the new buffer
        ASSERT_FLOAT_EQ(input[i + 32], 0.01 * i);
      }
    }();
  } else if (iteration == 3) {
    [nframes, input_buffer]() {
      ASSERT_EQ(nframes, 32);
      const float* input = static_cast<const float*>(input_buffer);
      for (int i = 0; i < 32; ++i) {
        // First part (16 frames) contains the reamaining input samples
        // from previous iteration (since they were more).
        ASSERT_FLOAT_EQ(input[i], 0.01 * (i + 32));
      }
      for (int i = 0; i < 16; ++i) {
        // next part (8 frames) contains the new input buffer.
        ASSERT_FLOAT_EQ(input[i + 32], 0.01 * i);
        // last part (8 frames) contains silence.
        ASSERT_FLOAT_EQ(input[i + 32 + 16], 0.0);
      }
    }();
  }
  return nframes;
}

TEST(cubeb, passthrough_resampler_fill_input_left) {
  const uint32_t channels = 2;
  const uint32_t sample_rate = 44100;
  int iteration = 0;
  passthrough_resampler<float> resampler =
    passthrough_resampler<float>(nullptr, passthrough_resampler_fill_input_left,
                                 &iteration, channels, sample_rate);

  long input_frame_count = 48; // 32 + 16
  const long output_frame_count = 32;
  float input[96] = {};
  float output[64] = {};
  for (uint32_t i = 0; i < input_frame_count * channels; ++i) {
    input[i] = 0.01 * i;
  }

  // 1st iteration, add the extra input.
  iteration = 1;
  long got = resampler.fill(input, &input_frame_count, output, output_frame_count);
  ASSERT_EQ(got, output_frame_count);
  // Input frames used must be equal to output frames.
  ASSERT_EQ(input_frame_count, output_frame_count);

  // 2st iteration, use the extra input from previous iteration,
  // 16 frames are remaining in the input buffer.
  input_frame_count = 32; // we need 16 input frames but we get more;
  iteration = 2;
  got = resampler.fill(input, &input_frame_count, output, output_frame_count);
  ASSERT_EQ(got, output_frame_count);
  // Input frames used must be equal to output frames.
  ASSERT_EQ(input_frame_count, output_frame_count);

  // 3rd iteration, use the extra input from previous iteration.
  // 16 frames are remaining in the input buffer.
  input_frame_count = 16 - 8; // We need 16 more input frames but we only get 8.
  iteration = 3;
  got = resampler.fill(input, &input_frame_count, output, output_frame_count);
  ASSERT_EQ(got, output_frame_count);
  // Input frames used are less than the output frames due to glitch.
  ASSERT_EQ(input_frame_count, output_frame_count - 8);
}

TEST(cubeb, individual_methods) {
  const uint32_t channels = 2;
  const uint32_t sample_rate = 44100;
  const uint32_t frames = 256;

  delay_line<float> dl(10, channels, sample_rate);
  uint32_t frames_needed1 = dl.input_needed_for_output(0);
  ASSERT_EQ(frames_needed1, 0u);

  cubeb_resampler_speex_one_way<float> one_way(channels, sample_rate, sample_rate, CUBEB_RESAMPLER_QUALITY_DEFAULT);
  float buffer[channels * frames] = {0.0};
  // Add all frames in the resampler's internal buffer.
  one_way.input(buffer, frames);
  // Ask for less than the existing frames, this would create a uint overlflow without the fix.
  uint32_t frames_needed2 = one_way.input_needed_for_output(0);
  ASSERT_EQ(frames_needed2, 0u);
}

