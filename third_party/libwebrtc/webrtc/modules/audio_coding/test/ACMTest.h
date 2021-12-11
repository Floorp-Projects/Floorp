/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_CODING_TEST_ACMTEST_H_
#define MODULES_AUDIO_CODING_TEST_ACMTEST_H_

class ACMTest {
 public:
  ACMTest() {}
  virtual ~ACMTest() {}
  virtual void Perform() = 0;
};

#endif  // MODULES_AUDIO_CODING_TEST_ACMTEST_H_
