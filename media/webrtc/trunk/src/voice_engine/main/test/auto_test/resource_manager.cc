/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "resource_manager.h"

#include "testsupport/fileutils.h"

ResourceManager::ResourceManager() {
  std::string filename = "audio_long16.pcm";
#if defined(WEBRTC_ANDROID)
  long_audio_file_path_ = "/sdcard/" + filename;
#else
  std::string resource_path = webrtc::test::ProjectRootPath();
  if (resource_path == webrtc::test::kCannotFindProjectRootDir) {
    long_audio_file_path_ = "";
  } else {
    long_audio_file_path_ =
        resource_path + "test/data/voice_engine/" + filename;
  }
#endif
}

