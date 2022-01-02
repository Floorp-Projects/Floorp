/*
 * Copyright © 2017 Mozilla Foundation
 *
 * This program is made available under an ISC-style license.  See the
 * accompanying file LICENSE for details.
 */

 /* libcubeb api/function test. Requests a loopback device and checks that
    output is being looped back to input. NOTE: Usage of output devices while
    performing this test will cause flakey results! */
#include "gtest/gtest.h"
#if !defined(_XOPEN_SOURCE)
#define _XOPEN_SOURCE 600
#endif
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <algorithm>
#include <memory>
#include <mutex>
#include <string>
#include "cubeb/cubeb.h"
//#define ENABLE_NORMAL_LOG
//#define ENABLE_VERBOSE_LOG
#include "common.h"
const uint32_t SAMPLE_FREQUENCY = 48000;
const uint32_t TONE_FREQUENCY = 440;
const double OUTPUT_AMPLITUDE = 0.25;
const int32_t NUM_FRAMES_TO_OUTPUT = SAMPLE_FREQUENCY / 20; /* play ~50ms of samples */

template<typename T> T ConvertSampleToOutput(double input);
template<> float ConvertSampleToOutput(double input) { return float(input); }
template<> short ConvertSampleToOutput(double input) { return short(input * 32767.0f); }

template<typename T> double ConvertSampleFromOutput(T sample);
template<> double ConvertSampleFromOutput(float sample) { return double(sample); }
template<> double ConvertSampleFromOutput(short sample) { return double(sample / 32767.0); }

/* Simple cross correlation to help find phase shift. Not a performant impl */
std::vector<double> cross_correlate(std::vector<double> & f,
                                    std::vector<double> & g,
                                    size_t signal_length)
{
  /* the length we sweep our window through to find the cross correlation */
  size_t sweep_length = f.size() - signal_length + 1;
  std::vector<double> correlation;
  correlation.reserve(sweep_length);
  for (size_t i = 0; i < sweep_length; i++) {
    double accumulator = 0.0;
    for (size_t j = 0; j < signal_length; j++) {
      accumulator += f.at(j) * g.at(i + j);
    }
    correlation.push_back(accumulator);
  }
  return correlation;
}

/* best effort discovery of phase shift between output and (looped) input*/
size_t find_phase(std::vector<double> & output_frames,
                  std::vector<double> & input_frames,
                  size_t signal_length)
{
  std::vector<double> correlation = cross_correlate(output_frames, input_frames, signal_length);
  size_t phase = 0;
  double max_correlation = correlation.at(0);
  for (size_t i = 1; i < correlation.size(); i++) {
    if (correlation.at(i) > max_correlation) {
      max_correlation = correlation.at(i);
      phase = i;
    }
  }
  return phase;
}

std::vector<double> normalize_frames(std::vector<double> & frames) {
  double max = abs(*std::max_element(frames.begin(), frames.end(),
                                     [](double a, double b) { return abs(a) < abs(b); }));
  std::vector<double> normalized_frames;
  normalized_frames.reserve(frames.size());
  for (const double frame : frames) {
    normalized_frames.push_back(frame / max);
  }
  return normalized_frames;
}

/* heuristic comparison of aligned output and input signals, gets flaky if TONE_FREQUENCY is too high */
void compare_signals(std::vector<double> & output_frames,
                     std::vector<double> & input_frames)
{
  ASSERT_EQ(output_frames.size(), input_frames.size()) << "#Output frames != #input frames";
  size_t num_frames = output_frames.size();
  std::vector<double> normalized_output_frames = normalize_frames(output_frames);
  std::vector<double> normalized_input_frames = normalize_frames(input_frames);

  /* calculate mean absolute errors */
  /* mean absolute errors between output and input */
  double io_mas = 0.0;
  /* mean absolute errors between output and silence */
  double output_silence_mas = 0.0;
  /* mean absolute errors between input and silence */
  double input_silence_mas = 0.0;
  for (size_t i = 0; i < num_frames; i++) {
    io_mas += abs(normalized_output_frames.at(i) - normalized_input_frames.at(i));
    output_silence_mas += abs(normalized_output_frames.at(i));
    input_silence_mas += abs(normalized_input_frames.at(i));
  }
  io_mas /= num_frames;
  output_silence_mas /= num_frames;
  input_silence_mas /= num_frames;

  ASSERT_LT(io_mas, output_silence_mas)
    << "Error between output and input should be less than output and silence!";
  ASSERT_LT(io_mas, input_silence_mas)
    << "Error between output and input should be less than output and silence!";

  /* make sure extrema are in (roughly) correct location */
  /* number of maxima + minama expected in the frames*/
  const long NUM_EXTREMA = 2 * TONE_FREQUENCY * NUM_FRAMES_TO_OUTPUT / SAMPLE_FREQUENCY;
  /* expected index of first maxima */
  const long FIRST_MAXIMUM_INDEX = SAMPLE_FREQUENCY / TONE_FREQUENCY / 4;
  /* Threshold we expect all maxima and minima to be above or below. Ideally
     the extrema would be 1 or -1, but particularly at the start of loopback
     the values seen can be significantly lower. */
  const double THRESHOLD = 0.5;

  for (size_t i = 0; i < NUM_EXTREMA; i++) {
    bool is_maximum = i % 2 == 0;
    /* expected offset to current extreme: i * stide between extrema */
    size_t offset = i * SAMPLE_FREQUENCY / TONE_FREQUENCY / 2;
    if (is_maximum) {
      ASSERT_GT(normalized_output_frames.at(FIRST_MAXIMUM_INDEX + offset), THRESHOLD)
        << "Output frames have unexpected missing maximum!";
      ASSERT_GT(normalized_input_frames.at(FIRST_MAXIMUM_INDEX + offset), THRESHOLD)
        << "Input frames have unexpected missing maximum!";
    } else {
      ASSERT_LT(normalized_output_frames.at(FIRST_MAXIMUM_INDEX + offset), -THRESHOLD)
        << "Output frames have unexpected missing minimum!";
      ASSERT_LT(normalized_input_frames.at(FIRST_MAXIMUM_INDEX + offset), -THRESHOLD)
        << "Input frames have unexpected missing minimum!";
    }
  }
}

struct user_state_loopback {
  std::mutex user_state_mutex;
  long position = 0;
  /* track output */
  std::vector<double> output_frames;
  /* track input */
  std::vector<double> input_frames;
};

template<typename T>
long data_cb_loop_duplex(cubeb_stream * stream, void * user, const void * inputbuffer, void * outputbuffer, long nframes)
{
  struct user_state_loopback * u = (struct user_state_loopback *) user;
  T * ib = (T *) inputbuffer;
  T * ob = (T *) outputbuffer;

  if (stream == NULL || inputbuffer == NULL || outputbuffer == NULL) {
    return CUBEB_ERROR;
  }

  std::lock_guard<std::mutex> lock(u->user_state_mutex);
  /* generate our test tone on the fly */
  for (int i = 0; i < nframes; i++) {
    double tone = 0.0;
    if (u->position + i < NUM_FRAMES_TO_OUTPUT) {
      /* generate sine wave */
      tone = sin(2 * M_PI*(i + u->position) * TONE_FREQUENCY / SAMPLE_FREQUENCY);
      tone *= OUTPUT_AMPLITUDE;
    }
    ob[i] = ConvertSampleToOutput<T>(tone);
    u->output_frames.push_back(tone);
    /* store any looped back output, may be silence */
    u->input_frames.push_back(ConvertSampleFromOutput(ib[i]));
  }

  u->position += nframes;

  return nframes;
}

template<typename T>
long data_cb_loop_input_only(cubeb_stream * stream, void * user, const void * inputbuffer, void * outputbuffer, long nframes)
{
  struct user_state_loopback * u = (struct user_state_loopback *) user;
  T * ib = (T *) inputbuffer;

  if (outputbuffer != NULL) {
    // Can't assert as it needs to return, so expect to fail instead
    EXPECT_EQ(outputbuffer, (void *) NULL) << "outputbuffer should be null in input only callback";
    return CUBEB_ERROR;
  }

  if (stream == NULL || inputbuffer == NULL) {
    return CUBEB_ERROR;
  }

  std::lock_guard<std::mutex> lock(u->user_state_mutex);
  for (int i = 0; i < nframes; i++) {
    u->input_frames.push_back(ConvertSampleFromOutput(ib[i]));
  }

  return nframes;
}

template<typename T>
long data_cb_playback(cubeb_stream * stream, void * user, const void * inputbuffer, void * outputbuffer, long nframes)
{
  struct user_state_loopback * u = (struct user_state_loopback *) user;
  T * ob = (T *) outputbuffer;

  if (stream == NULL || outputbuffer == NULL) {
    return CUBEB_ERROR;
  }

  std::lock_guard<std::mutex> lock(u->user_state_mutex);
  /* generate our test tone on the fly */
  for (int i = 0; i < nframes; i++) {
    double tone = 0.0;
    if (u->position + i < NUM_FRAMES_TO_OUTPUT) {
      /* generate sine wave */
      tone = sin(2 * M_PI*(i + u->position) * TONE_FREQUENCY / SAMPLE_FREQUENCY);
      tone *= OUTPUT_AMPLITUDE;
    }
    ob[i] = ConvertSampleToOutput<T>(tone);
    u->output_frames.push_back(tone);
  }

  u->position += nframes;

  return nframes;
}

void state_cb_loop(cubeb_stream * stream, void * /*user*/, cubeb_state state)
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

void run_loopback_duplex_test(bool is_float)
{
  cubeb * ctx;
  cubeb_stream * stream;
  cubeb_stream_params input_params;
  cubeb_stream_params output_params;
  int r;
  uint32_t latency_frames = 0;

  r = common_init(&ctx, "Cubeb loopback example: duplex stream");
  ASSERT_EQ(r, CUBEB_OK) << "Error initializing cubeb library";

  std::unique_ptr<cubeb, decltype(&cubeb_destroy)>
    cleanup_cubeb_at_exit(ctx, cubeb_destroy);

  input_params.format = is_float ? CUBEB_SAMPLE_FLOAT32NE : CUBEB_SAMPLE_S16LE;
  input_params.rate = SAMPLE_FREQUENCY;
  input_params.channels = 1;
  input_params.layout = CUBEB_LAYOUT_MONO;
  input_params.prefs = CUBEB_STREAM_PREF_LOOPBACK;
  output_params.format = is_float ? CUBEB_SAMPLE_FLOAT32NE : CUBEB_SAMPLE_S16LE;
  output_params.rate = SAMPLE_FREQUENCY;
  output_params.channels = 1;
  output_params.layout = CUBEB_LAYOUT_MONO;
  output_params.prefs = CUBEB_STREAM_PREF_NONE;

  std::unique_ptr<user_state_loopback> user_data(new user_state_loopback());
  ASSERT_TRUE(!!user_data) << "Error allocating user data";

  r = cubeb_get_min_latency(ctx, &output_params, &latency_frames);
  ASSERT_EQ(r, CUBEB_OK) << "Could not get minimal latency";

  /* setup a duplex stream with loopback */
  r = cubeb_stream_init(ctx, &stream, "Cubeb loopback",
                        NULL, &input_params, NULL, &output_params, latency_frames,
                        is_float ? data_cb_loop_duplex<float> : data_cb_loop_duplex<short>,
                        state_cb_loop, user_data.get());
  ASSERT_EQ(r, CUBEB_OK) << "Error initializing cubeb stream";

  std::unique_ptr<cubeb_stream, decltype(&cubeb_stream_destroy)>
    cleanup_stream_at_exit(stream, cubeb_stream_destroy);

  cubeb_stream_start(stream);
  delay(300);
  cubeb_stream_stop(stream);

  /* access after stop should not happen, but lock just in case and to appease sanitization tools */
  std::lock_guard<std::mutex> lock(user_data->user_state_mutex);
  std::vector<double> & output_frames = user_data->output_frames;
  std::vector<double> & input_frames = user_data->input_frames;
  ASSERT_EQ(output_frames.size(), input_frames.size())
    << "#Output frames != #input frames";

  size_t phase = find_phase(user_data->output_frames, user_data->input_frames, NUM_FRAMES_TO_OUTPUT);

  /* extract vectors of just the relevant signal from output and input */
  auto output_frames_signal_start = output_frames.begin();
  auto output_frames_signal_end = output_frames.begin() + NUM_FRAMES_TO_OUTPUT;
  std::vector<double> trimmed_output_frames(output_frames_signal_start, output_frames_signal_end);
  auto input_frames_signal_start = input_frames.begin() + phase;
  auto input_frames_signal_end = input_frames.begin() + phase + NUM_FRAMES_TO_OUTPUT;
  std::vector<double> trimmed_input_frames(input_frames_signal_start, input_frames_signal_end);

  compare_signals(trimmed_output_frames, trimmed_input_frames);
}

TEST(cubeb, loopback_duplex)
{
  run_loopback_duplex_test(true);
  run_loopback_duplex_test(false);
}

void run_loopback_separate_streams_test(bool is_float)
{
  cubeb * ctx;
  cubeb_stream * input_stream;
  cubeb_stream * output_stream;
  cubeb_stream_params input_params;
  cubeb_stream_params output_params;
  int r;
  uint32_t latency_frames = 0;

  r = common_init(&ctx, "Cubeb loopback example: separate streams");
  ASSERT_EQ(r, CUBEB_OK) << "Error initializing cubeb library";

  std::unique_ptr<cubeb, decltype(&cubeb_destroy)>
    cleanup_cubeb_at_exit(ctx, cubeb_destroy);

  input_params.format = is_float ? CUBEB_SAMPLE_FLOAT32NE : CUBEB_SAMPLE_S16LE;
  input_params.rate = SAMPLE_FREQUENCY;
  input_params.channels = 1;
  input_params.layout = CUBEB_LAYOUT_MONO;
  input_params.prefs = CUBEB_STREAM_PREF_LOOPBACK;
  output_params.format = is_float ? CUBEB_SAMPLE_FLOAT32NE : CUBEB_SAMPLE_S16LE;
  output_params.rate = SAMPLE_FREQUENCY;
  output_params.channels = 1;
  output_params.layout = CUBEB_LAYOUT_MONO;
  output_params.prefs = CUBEB_STREAM_PREF_NONE;

  std::unique_ptr<user_state_loopback> user_data(new user_state_loopback());
  ASSERT_TRUE(!!user_data) << "Error allocating user data";

  r = cubeb_get_min_latency(ctx, &output_params, &latency_frames);
  ASSERT_EQ(r, CUBEB_OK) << "Could not get minimal latency";

  /* setup an input stream with loopback */
  r = cubeb_stream_init(ctx, &input_stream, "Cubeb loopback input only",
                        NULL, &input_params, NULL, NULL, latency_frames,
                        is_float ? data_cb_loop_input_only<float> : data_cb_loop_input_only<short>,
                        state_cb_loop, user_data.get());
  ASSERT_EQ(r, CUBEB_OK) << "Error initializing cubeb stream";

  std::unique_ptr<cubeb_stream, decltype(&cubeb_stream_destroy)>
    cleanup_input_stream_at_exit(input_stream, cubeb_stream_destroy);

  /* setup an output stream */
  r = cubeb_stream_init(ctx, &output_stream, "Cubeb loopback output only",
                        NULL, NULL, NULL, &output_params, latency_frames,
                        is_float ? data_cb_playback<float> : data_cb_playback<short>,
                        state_cb_loop, user_data.get());
  ASSERT_EQ(r, CUBEB_OK) << "Error initializing cubeb stream";

  std::unique_ptr<cubeb_stream, decltype(&cubeb_stream_destroy)>
    cleanup_output_stream_at_exit(output_stream, cubeb_stream_destroy);

  cubeb_stream_start(input_stream);
  cubeb_stream_start(output_stream);
  delay(300);
  cubeb_stream_stop(output_stream);
  cubeb_stream_stop(input_stream);

  /* access after stop should not happen, but lock just in case and to appease sanitization tools */
  std::lock_guard<std::mutex> lock(user_data->user_state_mutex);
  std::vector<double> & output_frames = user_data->output_frames;
  std::vector<double> & input_frames = user_data->input_frames;
  ASSERT_LE(output_frames.size(), input_frames.size())
    << "#Output frames should be less or equal to #input frames";

  size_t phase = find_phase(user_data->output_frames, user_data->input_frames, NUM_FRAMES_TO_OUTPUT);

  /* extract vectors of just the relevant signal from output and input */
  auto output_frames_signal_start = output_frames.begin();
  auto output_frames_signal_end = output_frames.begin() + NUM_FRAMES_TO_OUTPUT;
  std::vector<double> trimmed_output_frames(output_frames_signal_start, output_frames_signal_end);
  auto input_frames_signal_start = input_frames.begin() + phase;
  auto input_frames_signal_end = input_frames.begin() + phase + NUM_FRAMES_TO_OUTPUT;
  std::vector<double> trimmed_input_frames(input_frames_signal_start, input_frames_signal_end);

  compare_signals(trimmed_output_frames, trimmed_input_frames);
}

TEST(cubeb, loopback_separate_streams)
{
  run_loopback_separate_streams_test(true);
  run_loopback_separate_streams_test(false);
}

void run_loopback_silence_test(bool is_float)
{
  cubeb * ctx;
  cubeb_stream * input_stream;
  cubeb_stream_params input_params;
  int r;
  uint32_t latency_frames = 0;

  r = common_init(&ctx, "Cubeb loopback example: record silence");
  ASSERT_EQ(r, CUBEB_OK) << "Error initializing cubeb library";

  std::unique_ptr<cubeb, decltype(&cubeb_destroy)>
    cleanup_cubeb_at_exit(ctx, cubeb_destroy);

  input_params.format = is_float ? CUBEB_SAMPLE_FLOAT32NE : CUBEB_SAMPLE_S16LE;
  input_params.rate = SAMPLE_FREQUENCY;
  input_params.channels = 1;
  input_params.layout = CUBEB_LAYOUT_MONO;
  input_params.prefs = CUBEB_STREAM_PREF_LOOPBACK;

  std::unique_ptr<user_state_loopback> user_data(new user_state_loopback());
  ASSERT_TRUE(!!user_data) << "Error allocating user data";

  r = cubeb_get_min_latency(ctx, &input_params, &latency_frames);
  ASSERT_EQ(r, CUBEB_OK) << "Could not get minimal latency";

  /* setup an input stream with loopback */
  r = cubeb_stream_init(ctx, &input_stream, "Cubeb loopback input only",
                        NULL, &input_params, NULL, NULL, latency_frames,
                        is_float ? data_cb_loop_input_only<float> : data_cb_loop_input_only<short>,
                        state_cb_loop, user_data.get());
  ASSERT_EQ(r, CUBEB_OK) << "Error initializing cubeb stream";

  std::unique_ptr<cubeb_stream, decltype(&cubeb_stream_destroy)>
    cleanup_input_stream_at_exit(input_stream, cubeb_stream_destroy);

  cubeb_stream_start(input_stream);
  delay(300);
  cubeb_stream_stop(input_stream);

  /* access after stop should not happen, but lock just in case and to appease sanitization tools */
  std::lock_guard<std::mutex> lock(user_data->user_state_mutex);
  std::vector<double> & input_frames = user_data->input_frames;

  /* expect to have at least ~50ms of frames */
  ASSERT_GE(input_frames.size(), SAMPLE_FREQUENCY / 20);
  double EPISILON = 0.0001;
  /* frames should be 0.0, but use epsilon to avoid possible issues with impls
  that may use ~0.0 silence values. */
  for (double frame : input_frames) {
    ASSERT_LT(abs(frame), EPISILON);
  }
}

TEST(cubeb, loopback_silence)
{
  run_loopback_silence_test(true);
  run_loopback_silence_test(false);
}

void run_loopback_device_selection_test(bool is_float)
{
  cubeb * ctx;
  cubeb_device_collection collection;
  cubeb_stream * input_stream;
  cubeb_stream * output_stream;
  cubeb_stream_params input_params;
  cubeb_stream_params output_params;
  int r;
  uint32_t latency_frames = 0;

  r = common_init(&ctx, "Cubeb loopback example: device selection, separate streams");
  ASSERT_EQ(r, CUBEB_OK) << "Error initializing cubeb library";

  std::unique_ptr<cubeb, decltype(&cubeb_destroy)>
    cleanup_cubeb_at_exit(ctx, cubeb_destroy);

  r = cubeb_enumerate_devices(ctx, CUBEB_DEVICE_TYPE_OUTPUT, &collection);
  if (r == CUBEB_ERROR_NOT_SUPPORTED) {
    fprintf(stderr, "Device enumeration not supported"
            " for this backend, skipping this test.\n");
    return;
  }

  ASSERT_EQ(r, CUBEB_OK) << "Error enumerating devices " << r;
  /* get first preferred output device id */
  std::string device_id;
  for (size_t i = 0; i < collection.count; i++) {
    if (collection.device[i].preferred) {
      device_id = collection.device[i].device_id;
      break;
    }
  }
  cubeb_device_collection_destroy(ctx, &collection);
  if (device_id.empty()) {
    fprintf(stderr, "Could not find preferred device, aborting test.\n");
    return;
  }

  input_params.format = is_float ? CUBEB_SAMPLE_FLOAT32NE : CUBEB_SAMPLE_S16LE;
  input_params.rate = SAMPLE_FREQUENCY;
  input_params.channels = 1;
  input_params.layout = CUBEB_LAYOUT_MONO;
  input_params.prefs = CUBEB_STREAM_PREF_LOOPBACK;
  output_params.format = is_float ? CUBEB_SAMPLE_FLOAT32NE : CUBEB_SAMPLE_S16LE;
  output_params.rate = SAMPLE_FREQUENCY;
  output_params.channels = 1;
  output_params.layout = CUBEB_LAYOUT_MONO;
  output_params.prefs = CUBEB_STREAM_PREF_NONE;

  std::unique_ptr<user_state_loopback> user_data(new user_state_loopback());
  ASSERT_TRUE(!!user_data) << "Error allocating user data";

  r = cubeb_get_min_latency(ctx, &output_params, &latency_frames);
  ASSERT_EQ(r, CUBEB_OK) << "Could not get minimal latency";

  /* setup an input stream with loopback */
  r = cubeb_stream_init(ctx, &input_stream, "Cubeb loopback input only",
                        device_id.c_str(), &input_params, NULL, NULL, latency_frames,
                        is_float ? data_cb_loop_input_only<float> : data_cb_loop_input_only<short>,
                        state_cb_loop, user_data.get());
  ASSERT_EQ(r, CUBEB_OK) << "Error initializing cubeb stream";

  std::unique_ptr<cubeb_stream, decltype(&cubeb_stream_destroy)>
    cleanup_input_stream_at_exit(input_stream, cubeb_stream_destroy);

  /* setup an output stream */
  r = cubeb_stream_init(ctx, &output_stream, "Cubeb loopback output only",
                        NULL, NULL, device_id.c_str(), &output_params, latency_frames,
                        is_float ? data_cb_playback<float> : data_cb_playback<short>,
                        state_cb_loop, user_data.get());
  ASSERT_EQ(r, CUBEB_OK) << "Error initializing cubeb stream";

  std::unique_ptr<cubeb_stream, decltype(&cubeb_stream_destroy)>
    cleanup_output_stream_at_exit(output_stream, cubeb_stream_destroy);

  cubeb_stream_start(input_stream);
  cubeb_stream_start(output_stream);
  delay(300);
  cubeb_stream_stop(output_stream);
  cubeb_stream_stop(input_stream);

  /* access after stop should not happen, but lock just in case and to appease sanitization tools */
  std::lock_guard<std::mutex> lock(user_data->user_state_mutex);
  std::vector<double> & output_frames = user_data->output_frames;
  std::vector<double> & input_frames = user_data->input_frames;
  ASSERT_LE(output_frames.size(), input_frames.size())
    << "#Output frames should be less or equal to #input frames";

  size_t phase = find_phase(user_data->output_frames, user_data->input_frames, NUM_FRAMES_TO_OUTPUT);

  /* extract vectors of just the relevant signal from output and input */
  auto output_frames_signal_start = output_frames.begin();
  auto output_frames_signal_end = output_frames.begin() + NUM_FRAMES_TO_OUTPUT;
  std::vector<double> trimmed_output_frames(output_frames_signal_start, output_frames_signal_end);
  auto input_frames_signal_start = input_frames.begin() + phase;
  auto input_frames_signal_end = input_frames.begin() + phase + NUM_FRAMES_TO_OUTPUT;
  std::vector<double> trimmed_input_frames(input_frames_signal_start, input_frames_signal_end);

  compare_signals(trimmed_output_frames, trimmed_input_frames);
}

TEST(cubeb, loopback_device_selection)
{
  run_loopback_device_selection_test(true);
  run_loopback_device_selection_test(false);
}
