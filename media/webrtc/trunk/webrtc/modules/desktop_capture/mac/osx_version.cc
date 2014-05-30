/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <sys/utsname.h>

#include "webrtc/system_wrappers/interface/logging.h"

namespace webrtc {

namespace {

int GetDarwinVersion() {
  struct utsname uname_info;
  if (uname(&uname_info) != 0) {
    LOG(LS_ERROR) << "uname failed";
    return 0;
  }

  if (strcmp(uname_info.sysname, "Darwin") != 0)
    return 0;

  char* dot;
  int result = strtol(uname_info.release, &dot, 10);
  if (*dot != '.') {
    LOG(LS_ERROR) << "Failed to parse version";
    return 0;
  }

  return result;
}

}  // namespace

bool IsOSLionOrLater() {
  static int darwin_version = GetDarwinVersion();

  // Verify that the version has been parsed correctly.
  if (darwin_version < 6) {
    LOG_F(LS_ERROR) << "Invalid Darwin version: " << darwin_version;
    abort();
  }

  // Darwin major version 11 corresponds to OSX 10.7.
  return darwin_version >= 11;
}

}  // namespace webrtc
