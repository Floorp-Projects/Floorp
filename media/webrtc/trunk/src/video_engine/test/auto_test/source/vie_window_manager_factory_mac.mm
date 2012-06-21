/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "vie_window_manager_factory.h"

#include "engine_configurations.h"
#if defined(COCOA_RENDERING)
#include "vie_autotest_mac_cocoa.h"
#elif defined(CARBON_RENDERING)
#include "vie_autotest_mac_carbon.h"
#endif

ViEAutoTestWindowManagerInterface*
ViEWindowManagerFactory::CreateWindowManagerForCurrentPlatform() {
  return new ViEAutoTestWindowManager();
}
