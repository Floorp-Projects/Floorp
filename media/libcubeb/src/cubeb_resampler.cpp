/*
 * Copyright © 2014 Mozilla Foundation
 *
 * This program is made available under an ISC-style license.  See the
 * accompanying file LICENSE for details.
 */
#include <cmath>
#include <cassert>
#include <cstring>
#include <cstddef>
#include <cstdio>
#include "cubeb_resampler.h"
#include "cubeb-speex-resampler.h"

namespace {

template<typename T>
class auto_array
{
public:
  auto_array(uint32_t size)
    : data(new T[size])
  {}

  ~auto_array()
  {
    delete [] data;
  }

  T * get() const
  {
    return data;
  }

private:
  T * data;
};

long
frame_count_at_rate(long frame_count, float rate)
{
  return static_cast<long>(ceilf(rate * frame_count) + 1);
}

size_t
frames_to_bytes(cubeb_stream_params params, size_t frames)
{
  assert(params.format == CUBEB_SAMPLE_S16NE || params.format == CUBEB_SAMPLE_FLOAT32NE);
  size_t sample_size = params.format == CUBEB_SAMPLE_S16NE ? sizeof(short) : sizeof(float);
  size_t frame_size = params.channels * sample_size;
  return frame_size * frames;
}

int
to_speex_quality(cubeb_resampler_quality q)
{
  switch(q) {
  case CUBEB_RESAMPLER_QUALITY_VOIP:
    return SPEEX_RESAMPLER_QUALITY_VOIP;
  case CUBEB_RESAMPLER_QUALITY_DEFAULT:
    return SPEEX_RESAMPLER_QUALITY_DEFAULT;
  case CUBEB_RESAMPLER_QUALITY_DESKTOP:
    return SPEEX_RESAMPLER_QUALITY_DESKTOP;
  default:
    assert(false);
    return 0XFFFFFFFF;
  }
}
} // end of anonymous namespace

struct cubeb_resampler {
  virtual long fill(void * buffer, long frames_needed) = 0;
  virtual ~cubeb_resampler() {}
};

class noop_resampler : public cubeb_resampler {
public:
  noop_resampler(cubeb_stream * s,
                 cubeb_data_callback cb,
                 void * ptr)
    : stream(s)
    , data_callback(cb)
    , user_ptr(ptr)
  {
  }

  virtual long fill(void * buffer, long frames_needed)
  {
    long got = data_callback(stream, user_ptr, buffer, frames_needed);
    assert(got <= frames_needed);
    return got;
  }

private:
  cubeb_stream * const stream;
  const cubeb_data_callback data_callback;
  void * const user_ptr;
};

class cubeb_resampler_speex : public cubeb_resampler {
public:
  cubeb_resampler_speex(SpeexResamplerState * r, cubeb_stream * s,
                        cubeb_stream_params params, uint32_t out_rate,
                        cubeb_data_callback cb, long max_count,
                        void * ptr);

  virtual ~cubeb_resampler_speex();

  virtual long fill(void * buffer, long frames_needed);

private:
  SpeexResamplerState * const speex_resampler;
  cubeb_stream * const stream;
  const cubeb_stream_params stream_params;
  const cubeb_data_callback data_callback;
  void * const user_ptr;

  // Maximum number of frames we can be requested in a callback.
  const long buffer_frame_count;
  // input rate / output rate
  const float resampling_ratio;
  // Maximum frames that can be stored in |leftover_frames_buffer|.
  const uint32_t leftover_frame_size;
  // Number of leftover frames stored in |leftover_frames_buffer|.
  uint32_t leftover_frame_count;

  // A little buffer to store the leftover frames,
  // that is, the samples not consumed by the resampler that we will end up
  // using next time fill() is called.
  auto_array<uint8_t> leftover_frames_buffer;
  // A buffer to store frames that will be consumed by the resampler.
  auto_array<uint8_t> resampling_src_buffer;
};

cubeb_resampler_speex::cubeb_resampler_speex(SpeexResamplerState * r,
                                             cubeb_stream * s,
                                             cubeb_stream_params params,
                                             uint32_t out_rate,
                                             cubeb_data_callback cb,
                                             long max_count,
                                             void * ptr)
  : speex_resampler(r)
  , stream(s)
  , stream_params(params)
  , data_callback(cb)
  , user_ptr(ptr)
  , buffer_frame_count(max_count)
  , resampling_ratio(static_cast<float>(params.rate) / out_rate)
  , leftover_frame_size(static_cast<uint32_t>(ceilf(1 / resampling_ratio * 2) + 1))
  , leftover_frame_count(0)
  , leftover_frames_buffer(auto_array<uint8_t>(frames_to_bytes(params, leftover_frame_size)))
  , resampling_src_buffer(auto_array<uint8_t>(frames_to_bytes(params,
        frame_count_at_rate(buffer_frame_count, resampling_ratio))))
{
  assert(r);
}

cubeb_resampler_speex::~cubeb_resampler_speex()
{
  speex_resampler_destroy(speex_resampler);
}

long
cubeb_resampler_speex::fill(void * buffer, long frames_needed)
{
  // Use more input frames than strictly necessary, so in the worst case,
  // we have leftover unresampled frames at the end, that we can use
  // during the next iteration.
  assert(frames_needed <= buffer_frame_count);
  long before_resampling = frame_count_at_rate(frames_needed, resampling_ratio);
  long frames_requested = before_resampling - leftover_frame_count;

  // Copy the previous leftover frames to the front of the buffer.
  size_t leftover_bytes = frames_to_bytes(stream_params, leftover_frame_count);
  memcpy(resampling_src_buffer.get(), leftover_frames_buffer.get(), leftover_bytes);
  uint8_t * buffer_start = resampling_src_buffer.get() + leftover_bytes;

  long got = data_callback(stream, user_ptr, buffer_start, frames_requested);
  assert(got <= frames_requested);

  if (got < 0) {
    return CUBEB_ERROR;
  }

  uint32_t in_frames = leftover_frame_count + got;
  uint32_t out_frames = frames_needed;
  uint32_t old_in_frames = in_frames;

  if (stream_params.format == CUBEB_SAMPLE_FLOAT32NE) {
    float * in_buffer = reinterpret_cast<float *>(resampling_src_buffer.get());
    float * out_buffer = reinterpret_cast<float *>(buffer);
    speex_resampler_process_interleaved_float(speex_resampler, in_buffer, &in_frames,
                                              out_buffer, &out_frames);
  } else {
    short * in_buffer = reinterpret_cast<short *>(resampling_src_buffer.get());
    short * out_buffer = reinterpret_cast<short *>(buffer);
    speex_resampler_process_interleaved_int(speex_resampler, in_buffer, &in_frames,
                                            out_buffer, &out_frames);
  }

  // Copy the leftover frames to buffer for the next time.
  leftover_frame_count = old_in_frames - in_frames;
  assert(leftover_frame_count <= leftover_frame_size);

  size_t unresampled_bytes = frames_to_bytes(stream_params, leftover_frame_count);
  uint8_t * leftover_frames_start = resampling_src_buffer.get();
  leftover_frames_start += frames_to_bytes(stream_params, in_frames);
  memcpy(leftover_frames_buffer.get(), leftover_frames_start, unresampled_bytes);

  return out_frames;
}

cubeb_resampler *
cubeb_resampler_create(cubeb_stream * stream,
                       cubeb_stream_params params,
                       unsigned int out_rate,
                       cubeb_data_callback callback,
                       long buffer_frame_count,
                       void * user_ptr,
                       cubeb_resampler_quality quality)
{
  if (params.rate != out_rate) {
    SpeexResamplerState * resampler = NULL;
    resampler = speex_resampler_init(params.channels,
                                     params.rate,
                                     out_rate,
                                     to_speex_quality(quality),
                                     NULL);
    if (!resampler) {
      return NULL;
    }

    return new cubeb_resampler_speex(resampler, stream, params, out_rate,
                                     callback, buffer_frame_count, user_ptr);
  }

  return new noop_resampler(stream, callback, user_ptr);
}

long
cubeb_resampler_fill(cubeb_resampler * resampler,
                     void * buffer, long frames_needed)
{
  return resampler->fill(buffer, frames_needed);
}

void
cubeb_resampler_destroy(cubeb_resampler * resampler)
{
  delete resampler;
}
