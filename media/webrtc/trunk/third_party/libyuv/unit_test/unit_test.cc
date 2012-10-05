/*
 *  Copyright 2011 The LibYuv Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "unit_test/unit_test.h"

#include <cstring>

libyuvTest::libyuvTest() : rotate_max_w_(128), rotate_max_h_(128),
    benchmark_iterations_(1000), benchmark_width_(1280),
    benchmark_height_(720) {
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
