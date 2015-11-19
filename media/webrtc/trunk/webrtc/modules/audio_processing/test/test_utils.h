/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <math.h>
#include <limits>

#include "webrtc/audio_processing/debug.pb.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/common_audio/channel_buffer.h"
#include "webrtc/common_audio/include/audio_util.h"
#include "webrtc/common_audio/wav_file.h"
#include "webrtc/modules/audio_processing/include/audio_processing.h"
#include "webrtc/modules/interface/module_common_types.h"

namespace webrtc {

static const AudioProcessing::Error kNoErr = AudioProcessing::kNoError;
#define EXPECT_NOERR(expr) EXPECT_EQ(kNoErr, (expr))

class RawFile {
 public:
  RawFile(const std::string& filename)
      : file_handle_(fopen(filename.c_str(), "wb")) {}

  ~RawFile() {
    fclose(file_handle_);
  }

  void WriteSamples(const int16_t* samples, size_t num_samples) {
#ifndef WEBRTC_ARCH_LITTLE_ENDIAN
#error "Need to convert samples to little-endian when writing to PCM file"
#endif
    fwrite(samples, sizeof(*samples), num_samples, file_handle_);
  }

  void WriteSamples(const float* samples, size_t num_samples) {
    fwrite(samples, sizeof(*samples), num_samples, file_handle_);
  }

 private:
  FILE* file_handle_;
};

static inline void WriteIntData(const int16_t* data,
                                size_t length,
                                WavWriter* wav_file,
                                RawFile* raw_file) {
  if (wav_file) {
    wav_file->WriteSamples(data, length);
  }
  if (raw_file) {
    raw_file->WriteSamples(data, length);
  }
}

static inline void WriteFloatData(const float* const* data,
                                  size_t samples_per_channel,
                                  int num_channels,
                                  WavWriter* wav_file,
                                  RawFile* raw_file) {
  size_t length = num_channels * samples_per_channel;
  rtc::scoped_ptr<float[]> buffer(new float[length]);
  Interleave(data, samples_per_channel, num_channels, buffer.get());
  if (raw_file) {
    raw_file->WriteSamples(buffer.get(), length);
  }
  // TODO(aluebs): Use ScaleToInt16Range() from audio_util
  for (size_t i = 0; i < length; ++i) {
    buffer[i] = buffer[i] > 0 ?
                buffer[i] * std::numeric_limits<int16_t>::max() :
                -buffer[i] * std::numeric_limits<int16_t>::min();
  }
  if (wav_file) {
    wav_file->WriteSamples(buffer.get(), length);
  }
}

// Exits on failure; do not use in unit tests.
static inline FILE* OpenFile(const std::string& filename, const char* mode) {
  FILE* file = fopen(filename.c_str(), mode);
  if (!file) {
    printf("Unable to open file %s\n", filename.c_str());
    exit(1);
  }
  return file;
}

static inline int SamplesFromRate(int rate) {
  return AudioProcessing::kChunkSizeMs * rate / 1000;
}

static inline void SetFrameSampleRate(AudioFrame* frame,
                                      int sample_rate_hz) {
  frame->sample_rate_hz_ = sample_rate_hz;
  frame->samples_per_channel_ = AudioProcessing::kChunkSizeMs *
      sample_rate_hz / 1000;
}

template <typename T>
void SetContainerFormat(int sample_rate_hz,
                        int num_channels,
                        AudioFrame* frame,
                        rtc::scoped_ptr<ChannelBuffer<T> >* cb) {
  SetFrameSampleRate(frame, sample_rate_hz);
  frame->num_channels_ = num_channels;
  cb->reset(new ChannelBuffer<T>(frame->samples_per_channel_, num_channels));
}

static inline AudioProcessing::ChannelLayout LayoutFromChannels(
    int num_channels) {
  switch (num_channels) {
    case 1:
      return AudioProcessing::kMono;
    case 2:
      return AudioProcessing::kStereo;
    default:
      assert(false);
      return AudioProcessing::kMono;
  }
}

// Allocates new memory in the scoped_ptr to fit the raw message and returns the
// number of bytes read.
static inline size_t ReadMessageBytesFromFile(
    FILE* file,
    rtc::scoped_ptr<uint8_t[]>* bytes) {
  // The "wire format" for the size is little-endian. Assume we're running on
  // a little-endian machine.
  int32_t size = 0;
  if (fread(&size, sizeof(size), 1, file) != 1)
    return 0;
  if (size <= 0)
    return 0;

  bytes->reset(new uint8_t[size]);
  return fread(bytes->get(), sizeof((*bytes)[0]), size, file);
}

// Returns true on success, false on error or end-of-file.
static inline bool ReadMessageFromFile(FILE* file,
                                       ::google::protobuf::MessageLite* msg) {
  rtc::scoped_ptr<uint8_t[]> bytes;
  size_t size = ReadMessageBytesFromFile(file, &bytes);
  if (!size)
    return false;

  msg->Clear();
  return msg->ParseFromArray(bytes.get(), size);
}

template <typename T>
float ComputeSNR(const T* ref, const T* test, int length, float* variance) {
  float mse = 0;
  float mean = 0;
  *variance = 0;
  for (int i = 0; i < length; ++i) {
    T error = ref[i] - test[i];
    mse += error * error;
    *variance += ref[i] * ref[i];
    mean += ref[i];
  }
  mse /= length;
  *variance /= length;
  mean /= length;
  *variance -= mean * mean;

  float snr = 100;  // We assign 100 dB to the zero-error case.
  if (mse > 0)
    snr = 10 * log10(*variance / mse);
  return snr;
}

}  // namespace webrtc
