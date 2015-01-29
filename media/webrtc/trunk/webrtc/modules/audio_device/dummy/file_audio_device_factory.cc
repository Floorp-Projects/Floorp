/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_device/dummy/file_audio_device_factory.h"

#include <cstring>

#include "webrtc/modules/audio_device/dummy/file_audio_device.h"

namespace webrtc {

char FileAudioDeviceFactory::_inputAudioFilename[MAX_FILENAME_LEN] = "";
char FileAudioDeviceFactory::_outputAudioFilename[MAX_FILENAME_LEN] = "";

FileAudioDevice* FileAudioDeviceFactory::CreateFileAudioDevice(
    const int32_t id) {
  // Bail out here if the files aren't set.
  if (strlen(_inputAudioFilename) == 0 || strlen(_outputAudioFilename) == 0) {
    printf("Was compiled with WEBRTC_DUMMY_AUDIO_PLAY_STATIC_FILE "
           "but did not set input/output files to use. Bailing out.\n");
    exit(1);
  }
  return new FileAudioDevice(id, _inputAudioFilename, _outputAudioFilename);
}

void FileAudioDeviceFactory::SetFilenamesToUse(
    const char* inputAudioFilename, const char* outputAudioFilename) {
#ifdef WEBRTC_DUMMY_FILE_DEVICES
  assert(strlen(inputAudioFilename) < MAX_FILENAME_LEN &&
         strlen(outputAudioFilename) < MAX_FILENAME_LEN);

  // Copy the strings since we don't know the lifetime of the input pointers.
  strncpy(_inputAudioFilename, inputAudioFilename, MAX_FILENAME_LEN);
  strncpy(_outputAudioFilename, outputAudioFilename, MAX_FILENAME_LEN);
#else
  // Sanity: must be compiled with the right define to run this.
  printf("Trying to use dummy file devices, but is not compiled "
         "with WEBRTC_DUMMY_FILE_DEVICES. Bailing out.\n");
  exit(1);
#endif
}

}  // namespace webrtc
