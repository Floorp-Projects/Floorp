/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/pc/e2e/metric_metadata_keys.h"

#include <string>

#include "test/gtest.h"

namespace webrtc {
namespace webrtc_pc_e2e {

std::string GetCurrentTestName() {
  const testing::TestInfo* test_info =
      testing::UnitTest::GetInstance()->current_test_info();
  return test_info->name();
}

}  // namespace webrtc_pc_e2e
}  // namespace webrtc
