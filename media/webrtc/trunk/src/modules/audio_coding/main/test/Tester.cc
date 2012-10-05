/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stdio.h>
#include <string>
#include <vector>

#include "gtest/gtest.h"

#include "APITest.h"
#include "audio_coding_module.h"
#include "EncodeDecodeTest.h"
#include "iSACTest.h"
#include "TestAllCodecs.h"
#include "TestFEC.h"
#include "TestStereo.h"
#include "testsupport/fileutils.h"
#include "TestVADDTX.h"
#include "trace.h"
#include "TwoWayCommunication.h"

using webrtc::AudioCodingModule;
using webrtc::Trace;

// This parameter is used to describe how to run the tests. It is normally
// set to 1, but in auto test all printing is turned off, and the parameter is
// set to 0.
#define ACM_TEST_MODE 1

// TODO(tlegrand): Add all tests as individual gtests, like already done for
// TestAllCodecs (ACM_TEST_ALL_ENC_DEC).

// Choose what tests to run by defining one or more of the following:
//
// ACM_AUTO_TEST - Most common codecs and settings will be tested. All the
//                 other tests will be activated.
// ACM_TEST_ENC_DEC - You decide what to test in run time. Used for debugging
//                    and for testing while implementing.
// ACM_TEST_TWO_WAY - Mainly for debugging.
// ACM_TEST_ALL_CODECS - Loop through all defined codecs and settings.
// ACM_TEST_STEREO - Run stereo and spatial audio tests.
// ACM_TEST_VAD_DTX - Run all VAD/DTX tests.
// ACM_TEST_FEC - Test FEC (also called RED).
// ACM_TEST_CODEC_SPEC_API - Test the iSAC has codec specfic APIs.
// ACM_TEST_FULL_API - Test all APIs with threads (long test).

#define ACM_AUTO_TEST
//#define ACM_TEST_ENC_DEC
//#define ACM_TEST_TWO_WAY
//#define ACM_TEST_ALL_CODECS
//#define ACM_TEST_STEREO
//#define ACM_TEST_VAD_DTX
//#define ACM_TEST_FEC
//#define ACM_TEST_CODEC_SPEC_API
//#define ACM_TEST_FULL_API

// If Auto test is active, we activate all tests.
#ifdef ACM_AUTO_TEST
#undef ACM_TEST_MODE
#define ACM_TEST_MODE 0
#ifndef ACM_TEST_ALL_CODECS
#define ACM_TEST_ALL_CODECS
#endif
#endif

void PopulateTests(std::vector<ACMTest*>* tests) {
  Trace::CreateTrace();
  Trace::SetTraceFile((webrtc::test::OutputPath() + "acm_trace.txt").c_str());

  printf("The following tests will be executed:\n");
#ifdef ACM_AUTO_TEST
  printf("  ACM auto test\n");
  tests->push_back(new webrtc::EncodeDecodeTest(0));
  tests->push_back(new webrtc::TwoWayCommunication(0));
  tests->push_back(new webrtc::TestStereo(0));
  tests->push_back(new webrtc::TestVADDTX(0));
  tests->push_back(new webrtc::TestFEC(0));
  tests->push_back(new webrtc::ISACTest(0));
#endif
#ifdef ACM_TEST_ENC_DEC
  printf("  ACM encode-decode test\n");
  tests->push_back(new webrtc::EncodeDecodeTest(2));
#endif
#ifdef ACM_TEST_TWO_WAY
  printf("  ACM two-way communication test\n");
  tests->push_back(new webrtc::TwoWayCommunication(1));
#endif
#ifdef ACM_TEST_STEREO
  printf("  ACM stereo test\n");
  tests->push_back(new webrtc::TestStereo(1));
#endif
#ifdef ACM_TEST_VAD_DTX
  printf("  ACM VAD-DTX test\n");
  tests->push_back(new webrtc::TestVADDTX(1));
#endif
#ifdef ACM_TEST_FEC
  printf("  ACM FEC test\n");
  tests->push_back(new webrtc::TestFEC(1));
#endif
#ifdef ACM_TEST_CODEC_SPEC_API
  printf("  ACM codec API test\n");
  tests->push_back(new webrtc::ISACTest(1));
#endif
#ifdef ACM_TEST_FULL_API
  printf("  ACM full API test\n");
  tests->push_back(new webrtc::APITest());
#endif
  printf("\n");
}

// TODO(kjellander): Make this a proper gtest instead of using this single test
// to run all the tests.

#ifdef ACM_TEST_ALL_CODECS
TEST(AudioCodingModuleTest, TestAllCodecs) {
  Trace::CreateTrace();
  Trace::SetTraceFile((webrtc::test::OutputPath() +
      "acm_allcodecs_trace.txt").c_str());
  webrtc::TestAllCodecs(ACM_TEST_MODE).Perform();
  Trace::ReturnTrace();
}
#endif

TEST(AudioCodingModuleTest, RunAllTests) {
  std::vector<ACMTest*> tests;
  PopulateTests(&tests);
  std::vector<ACMTest*>::iterator it;
  for (it = tests.begin(); it < tests.end(); it++) {
    (*it)->Perform();
    delete (*it);
  }

  Trace::ReturnTrace();
  printf("ACM test completed\n");
}
