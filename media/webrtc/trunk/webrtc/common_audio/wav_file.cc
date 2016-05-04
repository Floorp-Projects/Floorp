/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/common_audio/wav_file.h"

#include <algorithm>
#include <cstdio>
#include <limits>

#include "webrtc/base/checks.h"
#include "webrtc/base/safe_conversions.h"
#include "webrtc/common_audio/include/audio_util.h"
#include "webrtc/common_audio/wav_header.h"

namespace webrtc {

// We write 16-bit PCM WAV files.
static const WavFormat kWavFormat = kWavFormatPcm;
static const int kBytesPerSample = 2;

// Doesn't take ownership of the file handle and won't close it.
class ReadableWavFile : public ReadableWav {
 public:
  explicit ReadableWavFile(FILE* file) : file_(file) {}
  virtual size_t Read(void* buf, size_t num_bytes) {
    return fread(buf, 1, num_bytes, file_);
  }

 private:
  FILE* file_;
};

WavReader::WavReader(const std::string& filename)
    : file_handle_(fopen(filename.c_str(), "rb")) {
  CHECK(file_handle_ && "Could not open wav file for reading.");

  ReadableWavFile readable(file_handle_);
  WavFormat format;
  int bytes_per_sample;
  CHECK(ReadWavHeader(&readable, &num_channels_, &sample_rate_, &format,
                      &bytes_per_sample, &num_samples_));
  num_samples_remaining_ = num_samples_;
  CHECK_EQ(kWavFormat, format);
  CHECK_EQ(kBytesPerSample, bytes_per_sample);
}

WavReader::~WavReader() {
  Close();
}

size_t WavReader::ReadSamples(size_t num_samples, int16_t* samples) {
  // There could be metadata after the audio; ensure we don't read it.
  num_samples = std::min(rtc::checked_cast<uint32_t>(num_samples),
                         num_samples_remaining_);
  const size_t read =
      fread(samples, sizeof(*samples), num_samples, file_handle_);
  // If we didn't read what was requested, ensure we've reached the EOF.
  CHECK(read == num_samples || feof(file_handle_));
  CHECK_LE(read, num_samples_remaining_);
  num_samples_remaining_ -= rtc::checked_cast<uint32_t>(read);
#ifndef WEBRTC_ARCH_LITTLE_ENDIAN
  //convert to big-endian
  for(size_t idx = 0; idx < num_samples; idx++) {
    samples[idx] = (samples[idx]<<8) | (samples[idx]>>8);
  }
#endif
  return read;
}

size_t WavReader::ReadSamples(size_t num_samples, float* samples) {
  static const size_t kChunksize = 4096 / sizeof(uint16_t);
  size_t read = 0;
  for (size_t i = 0; i < num_samples; i += kChunksize) {
    int16_t isamples[kChunksize];
    size_t chunk = std::min(kChunksize, num_samples - i);
    chunk = ReadSamples(chunk, isamples);
    for (size_t j = 0; j < chunk; ++j)
      samples[i + j] = isamples[j];
    read += chunk;
  }
  return read;
}

void WavReader::Close() {
  CHECK_EQ(0, fclose(file_handle_));
  file_handle_ = NULL;
}

WavWriter::WavWriter(const std::string& filename, int sample_rate,
                     int num_channels)
    : sample_rate_(sample_rate),
      num_channels_(num_channels),
      num_samples_(0),
      file_handle_(fopen(filename.c_str(), "wb")) {
  if (file_handle_) {
    CHECK(file_handle_ && "Could not open wav file for writing.");
    CHECK(CheckWavParameters(num_channels_,
                             sample_rate_,
                             kWavFormat,
                             kBytesPerSample,
                             num_samples_));

    // Write a blank placeholder header, since we need to know the total number
    // of samples before we can fill in the real data.
    static const uint8_t blank_header[kWavHeaderSize] = {0};
    CHECK_EQ(1u, fwrite(blank_header, kWavHeaderSize, 1, file_handle_));
  }
}

WavWriter::~WavWriter() {
  Close();
}

void WavWriter::WriteSamples(const int16_t* samples, size_t num_samples) {
  if (!file_handle_) {
    return;
  }
#ifndef WEBRTC_ARCH_LITTLE_ENDIAN
  int16_t * le_samples = new int16_t[num_samples];
  for(size_t idx = 0; idx < num_samples; idx++) {
    le_samples[idx] = (samples[idx]<<8) | (samples[idx]>>8);
  }
  const size_t written =
      fwrite(le_samples, sizeof(*le_samples), num_samples, file_handle_);
  delete []le_samples;
#else
  const size_t written =
      fwrite(samples, sizeof(*samples), num_samples, file_handle_);
#endif
  CHECK_EQ(num_samples, written);
  num_samples_ += static_cast<uint32_t>(written);
  CHECK(written <= std::numeric_limits<uint32_t>::max() ||
        num_samples_ >= written);  // detect uint32_t overflow
  CHECK(CheckWavParameters(num_channels_,
                           sample_rate_,
                           kWavFormat,
                           kBytesPerSample,
                           num_samples_));
}

void WavWriter::WriteSamples(const float* samples, size_t num_samples) {
  static const size_t kChunksize = 4096 / sizeof(uint16_t);
  for (size_t i = 0; i < num_samples; i += kChunksize) {
    int16_t isamples[kChunksize];
    const size_t chunk = std::min(kChunksize, num_samples - i);
    FloatS16ToS16(samples + i, chunk, isamples);
    WriteSamples(isamples, chunk);
  }
}

void WavWriter::Close() {
  if (!file_handle_) {
    return;
  }
  CHECK_EQ(0, fseek(file_handle_, 0, SEEK_SET));
  uint8_t header[kWavHeaderSize];
  WriteWavHeader(header, num_channels_, sample_rate_, kWavFormat,
                 kBytesPerSample, num_samples_);
  CHECK_EQ(1u, fwrite(header, kWavHeaderSize, 1, file_handle_));
  CHECK_EQ(0, fclose(file_handle_));
  file_handle_ = NULL;
}

}  // namespace webrtc

rtc_WavWriter* rtc_WavOpen(const char* filename,
                           int sample_rate,
                           int num_channels) {
  return reinterpret_cast<rtc_WavWriter*>(
      new webrtc::WavWriter(filename, sample_rate, num_channels));
}

void rtc_WavClose(rtc_WavWriter* wf) {
  delete reinterpret_cast<webrtc::WavWriter*>(wf);
}

void rtc_WavWriteSamples(rtc_WavWriter* wf,
                         const float* samples,
                         size_t num_samples) {
  reinterpret_cast<webrtc::WavWriter*>(wf)->WriteSamples(samples, num_samples);
}

int rtc_WavSampleRate(const rtc_WavWriter* wf) {
  return reinterpret_cast<const webrtc::WavWriter*>(wf)->sample_rate();
}

int rtc_WavNumChannels(const rtc_WavWriter* wf) {
  return reinterpret_cast<const webrtc::WavWriter*>(wf)->num_channels();
}

uint32_t rtc_WavNumSamples(const rtc_WavWriter* wf) {
  return reinterpret_cast<const webrtc::WavWriter*>(wf)->num_samples();
}
