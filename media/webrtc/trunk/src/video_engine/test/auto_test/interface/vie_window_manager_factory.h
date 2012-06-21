/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SRC_VIDEO_ENGINE_MAIN_TEST_AUTOTEST_INTERFACE_VIE_WINDOW_MANAGER_FACTORY_H_
#define SRC_VIDEO_ENGINE_MAIN_TEST_AUTOTEST_INTERFACE_VIE_WINDOW_MANAGER_FACTORY_H_

class ViEAutoTestWindowManagerInterface;

class ViEWindowManagerFactory {
 public:
  // This method is implemented in different files depending on platform.
  // The caller is responsible for freeing the resulting object using
  // the delete operator.
  static ViEAutoTestWindowManagerInterface*
  CreateWindowManagerForCurrentPlatform();
};

#endif  // SRC_VIDEO_ENGINE_MAIN_TEST_AUTOTEST_INTERFACE_VIE_WINDOW_MANAGER_FACTORY_H_
