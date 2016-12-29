/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_PROCESSING_AEC_AEC_LOGGING_
#define WEBRTC_MODULES_AUDIO_PROCESSING_AEC_AEC_LOGGING_

#include <stdio.h>

#include "webrtc/modules/audio_processing/logging/aec_logging_file_handling.h"

// To enable AEC logging, invoke GYP with -Daec_debug_dump=1.
#ifdef WEBRTC_AEC_DEBUG_DUMP
// Dumps a wav data to file.
#define RTC_AEC_DEBUG_WAV_WRITE(file, data, num_samples) \
  do {                                                   \
    rtc_WavWriteSamples(file, data, num_samples);        \
  } while (0)

// (Re)opens a wav file for writing using the specified sample rate.
#define RTC_AEC_DEBUG_WAV_REOPEN(name, instance_index, count,     \
                                 sample_rate, wav_file)                  \
  do {                                                                   \
    WebRtcAec_ReopenWav(name, instance_index, process_rate, sample_rate, \
                        wav_file);                                       \
  } while (0)

// Closes a wav file.
#define RTC_AEC_DEBUG_WAV_CLOSE(wav_file) \
  do {                                    \
    rtc_WavClose(wav_file);               \
  } while (0)

// Dumps a raw data to file.
#define RTC_AEC_DEBUG_RAW_WRITE(file, data, data_size) \
  do {                                                 \
    (void) fwrite(data, data_size, 1, file);           \
  } while (0)

// Dumps a raw scalar int32 to file.
#define RTC_AEC_DEBUG_RAW_WRITE_SCALAR_INT32(file, data)             \
  do {                                                               \
    int32_t value_to_store = data;                                   \
    (void) fwrite(&value_to_store, sizeof(value_to_store), 1, file); \
  } while (0)

// Dumps a raw scalar double to file.
#define RTC_AEC_DEBUG_RAW_WRITE_SCALAR_DOUBLE(file, data)            \
  do {                                                               \
    double value_to_store = data;                                    \
    (void) fwrite(&value_to_store, sizeof(value_to_store), 1, file); \
  } while (0)

// Opens a raw data file for writing using the specified sample rate.
#define RTC_AEC_DEBUG_RAW_OPEN(name, instance_index, counter, file)     \
  do {                                                       \
    WebRtcAec_RawFileOpen(name, instance_index, counter, file); \
  } while (0)

// Closes a raw data file.
#define RTC_AEC_DEBUG_RAW_CLOSE(file) \
  do {                                \
    if (file) {                       \
      fclose(file);                   \
    }                                 \
  } while (0)

#else  // RTC_AEC_DEBUG_DUMP
#define RTC_AEC_DEBUG_WAV_WRITE(file, data, num_samples) \
  do {                                                   \
  } while (0)

#define RTC_AEC_DEBUG_WAV_REOPEN(wav_file, name, instance_index, process_rate, \
                                 sample_rate)                                  \
  do {                                                                         \
  } while (0)

#define RTC_AEC_DEBUG_WAV_CLOSE(wav_file) \
  do {                                    \
  } while (0)

#define RTC_AEC_DEBUG_RAW_WRITE(file, data, data_size) \
  do {                                                 \
  } while (0)

#define RTC_AEC_DEBUG_RAW_WRITE_SCALAR_INT32(file, data) \
  do {                                                   \
  } while (0)

#define RTC_AEC_DEBUG_RAW_WRITE_SCALAR_DOUBLE(file, data) \
  do {                                                    \
  } while (0)

#define RTC_AEC_DEBUG_RAW_OPEN(file, name, instance_counter) \
  do {                                                       \
  } while (0)

#define RTC_AEC_DEBUG_RAW_CLOSE(file) \
  do {                                \
  } while (0)

#endif  // WEBRTC_AEC_DEBUG_DUMP

#endif  // WEBRTC_MODULES_AUDIO_PROCESSING_AEC_AEC_LOGGING_
