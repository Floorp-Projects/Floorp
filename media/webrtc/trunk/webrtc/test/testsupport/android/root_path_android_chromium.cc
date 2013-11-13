/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "base/android/path_utils.h"
#include "base/files/file_path.h"

namespace webrtc {
namespace test {

std::string OutputPathImpl();

// This file is only compiled when running WebRTC tests in a Chromium workspace.
// The Android testing framework will push files relative to the root path of
// the Chromium workspace. The root path for webrtc is one directory up from
// trunk/webrtc (in standalone) or src/third_party/webrtc (in Chromium).
std::string ProjectRootPathAndroid() {
  base::FilePath root_path;
  base::android::GetExternalStorageDirectory(&root_path);
  return root_path.value() + "/";
}

std::string OutputPathAndroid() {
  return OutputPathImpl();
}

}  // namespace test
}  // namespace webrtc
