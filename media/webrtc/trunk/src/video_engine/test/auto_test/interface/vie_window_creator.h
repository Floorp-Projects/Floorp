/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SRC_VIDEO_ENGINE_MAIN_TEST_AUTOTEST_HELPERS_VIE_WINDOW_CREATOR_H_
#define SRC_VIDEO_ENGINE_MAIN_TEST_AUTOTEST_HELPERS_VIE_WINDOW_CREATOR_H_

class ViEAutoTestWindowManagerInterface;

class ViEWindowCreator {
 public:
  ViEWindowCreator();
  virtual ~ViEWindowCreator();

  // The pointer returned here will still be owned by this object.
  // Only use it to retrieve the created windows.
  ViEAutoTestWindowManagerInterface* CreateTwoWindows();

  // Terminates windows opened by CreateTwoWindows, which must
  // have been called before this method.
  void TerminateWindows();
 private:
  ViEAutoTestWindowManagerInterface* window_manager_;
};

#endif  // SRC_VIDEO_ENGINE_MAIN_TEST_AUTOTEST_HELPERS_VIE_WINDOW_CREATOR_H_
