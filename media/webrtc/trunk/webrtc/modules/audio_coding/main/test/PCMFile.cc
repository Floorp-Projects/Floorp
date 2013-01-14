/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "PCMFile.h"

#include <cctype>
#include <stdio.h>
#include <string.h>

#include "gtest/gtest.h"
#include "module_common_types.h"

namespace webrtc {

#define MAX_FILE_NAME_LENGTH_BYTE 500

PCMFile::PCMFile()
    : pcm_file_(NULL),
      samples_10ms_(160),
      frequency_(16000),
      end_of_file_(false),
      auto_rewind_(false),
      rewinded_(false),
      read_stereo_(false),
      save_stereo_(false) {
  timestamp_ = (((WebRtc_UWord32)rand() & 0x0000FFFF) << 16) |
      ((WebRtc_UWord32)rand() & 0x0000FFFF);
}

PCMFile::PCMFile(WebRtc_UWord32 timestamp)
    : pcm_file_(NULL),
      samples_10ms_(160),
      frequency_(16000),
      end_of_file_(false),
      auto_rewind_(false),
      rewinded_(false),
      read_stereo_(false),
      save_stereo_(false) {
  timestamp_ = timestamp;
}

WebRtc_Word16 PCMFile::ChooseFile(std::string* file_name,
                                  WebRtc_Word16 max_len) {
  char tmp_name[MAX_FILE_NAME_LENGTH_BYTE];

  EXPECT_TRUE(fgets(tmp_name, MAX_FILE_NAME_LENGTH_BYTE, stdin) != NULL);
  tmp_name[MAX_FILE_NAME_LENGTH_BYTE - 1] = '\0';
  WebRtc_Word16 n = 0;

  // Removing leading spaces.
  while ((isspace(tmp_name[n]) || iscntrl(tmp_name[n])) && (tmp_name[n] != 0)
      && (n < MAX_FILE_NAME_LENGTH_BYTE)) {
    n++;
  }
  if (n > 0) {
    memmove(tmp_name, &tmp_name[n], MAX_FILE_NAME_LENGTH_BYTE - n);
  }

  // Removing trailing spaces.
  n = (WebRtc_Word16)(strlen(tmp_name) - 1);
  if (n >= 0) {
    while ((isspace(tmp_name[n]) || iscntrl(tmp_name[n])) && (n >= 0)) {
      n--;
    }
  }
  if (n >= 0) {
    tmp_name[n + 1] = '\0';
  }

  WebRtc_Word16 len = (WebRtc_Word16) strlen(tmp_name);
  if (len > max_len) {
    return -1;
  }
  if (len > 0) {
    std::string tmp_string(tmp_name, len + 1);
    *file_name = tmp_string;
  }
  return 0;
}

WebRtc_Word16 PCMFile::ChooseFile(std::string* file_name,
                                  WebRtc_Word16 max_len,
                                  WebRtc_UWord16* frequency_hz) {
  char tmp_name[MAX_FILE_NAME_LENGTH_BYTE];

  EXPECT_TRUE(fgets(tmp_name, MAX_FILE_NAME_LENGTH_BYTE, stdin) != NULL);
  tmp_name[MAX_FILE_NAME_LENGTH_BYTE - 1] = '\0';
  WebRtc_Word16 n = 0;

  // Removing trailing spaces.
  while ((isspace(tmp_name[n]) || iscntrl(tmp_name[n])) && (tmp_name[n] != 0)
      && (n < MAX_FILE_NAME_LENGTH_BYTE)) {
    n++;
  }
  if (n > 0) {
    memmove(tmp_name, &tmp_name[n], MAX_FILE_NAME_LENGTH_BYTE - n);
  }

  // Removing trailing spaces.
  n = (WebRtc_Word16)(strlen(tmp_name) - 1);
  if (n >= 0) {
    while ((isspace(tmp_name[n]) || iscntrl(tmp_name[n])) && (n >= 0)) {
      n--;
    }
  }
  if (n >= 0) {
    tmp_name[n + 1] = '\0';
  }

  WebRtc_Word16 len = (WebRtc_Word16) strlen(tmp_name);
  if (len > max_len) {
    return -1;
  }
  if (len > 0) {
    std::string tmp_string(tmp_name, len + 1);
    *file_name = tmp_string;
  }
  printf("Enter the sampling frequency (in Hz) of the above file [%u]: ",
         *frequency_hz);
  EXPECT_TRUE(fgets(tmp_name, 10, stdin) != NULL);
  WebRtc_UWord16 tmp_frequency = (WebRtc_UWord16) atoi(tmp_name);
  if (tmp_frequency > 0) {
    *frequency_hz = tmp_frequency;
  }
  return 0;
}

void PCMFile::Open(const std::string& file_name, WebRtc_UWord16 frequency,
                   const char* mode, bool auto_rewind) {
  if ((pcm_file_ = fopen(file_name.c_str(), mode)) == NULL) {
    printf("Cannot open file %s.\n", file_name.c_str());
    ADD_FAILURE() << "Unable to read file";
  }
  frequency_ = frequency;
  samples_10ms_ = (WebRtc_UWord16)(frequency_ / 100);
  auto_rewind_ = auto_rewind;
  end_of_file_ = false;
  rewinded_ = false;
}

WebRtc_Word32 PCMFile::SamplingFrequency() const {
  return frequency_;
}

WebRtc_UWord16 PCMFile::PayloadLength10Ms() const {
  return samples_10ms_;
}

WebRtc_Word32 PCMFile::Read10MsData(AudioFrame& audio_frame) {
  WebRtc_UWord16 channels = 1;
  if (read_stereo_) {
    channels = 2;
  }

  WebRtc_Word32 payload_size = (WebRtc_Word32) fread(audio_frame.data_,
                                                    sizeof(WebRtc_UWord16),
                                                    samples_10ms_ * channels,
                                                    pcm_file_);
  if (payload_size < samples_10ms_ * channels) {
    for (int k = payload_size; k < samples_10ms_ * channels; k++) {
      audio_frame.data_[k] = 0;
    }
    if (auto_rewind_) {
      rewind(pcm_file_);
      rewinded_ = true;
    } else {
      end_of_file_ = true;
    }
  }
  audio_frame.samples_per_channel_ = samples_10ms_;
  audio_frame.sample_rate_hz_ = frequency_;
  audio_frame.num_channels_ = channels;
  audio_frame.timestamp_ = timestamp_;
  timestamp_ += samples_10ms_;
  return samples_10ms_;
}

void PCMFile::Write10MsData(AudioFrame& audio_frame) {
  if (audio_frame.num_channels_ == 1) {
    if (!save_stereo_) {
      if (fwrite(audio_frame.data_, sizeof(WebRtc_UWord16),
                 audio_frame.samples_per_channel_, pcm_file_) !=
          static_cast<size_t>(audio_frame.samples_per_channel_)) {
        return;
      }
    } else {
      WebRtc_Word16* stereo_audio =
          new WebRtc_Word16[2 * audio_frame.samples_per_channel_];
      int k;
      for (k = 0; k < audio_frame.samples_per_channel_; k++) {
        stereo_audio[k << 1] = audio_frame.data_[k];
        stereo_audio[(k << 1) + 1] = audio_frame.data_[k];
      }
      if (fwrite(stereo_audio, sizeof(WebRtc_Word16),
                 2 * audio_frame.samples_per_channel_, pcm_file_) !=
          static_cast<size_t>(2 * audio_frame.samples_per_channel_)) {
        return;
      }
      delete[] stereo_audio;
    }
  } else {
    if (fwrite(audio_frame.data_, sizeof(WebRtc_Word16),
               audio_frame.num_channels_ * audio_frame.samples_per_channel_,
               pcm_file_) != static_cast<size_t>(
            audio_frame.num_channels_ * audio_frame.samples_per_channel_)) {
      return;
    }
  }
}

void PCMFile::Write10MsData(WebRtc_Word16* playout_buffer,
                            WebRtc_UWord16 length_smpls) {
  if (fwrite(playout_buffer, sizeof(WebRtc_UWord16),
             length_smpls, pcm_file_) != length_smpls) {
    return;
  }
}

void PCMFile::Close() {
  fclose(pcm_file_);
  pcm_file_ = NULL;
}

void PCMFile::Rewind() {
  rewind(pcm_file_);
  end_of_file_ = false;
}

bool PCMFile::Rewinded() {
  return rewinded_;
}

void PCMFile::SaveStereo(bool is_stereo) {
  save_stereo_ = is_stereo;
}

void PCMFile::ReadStereo(bool is_stereo) {
  read_stereo_ = is_stereo;
}

}  // namespace webrtc
