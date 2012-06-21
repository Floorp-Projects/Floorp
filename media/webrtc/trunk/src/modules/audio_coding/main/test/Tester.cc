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

#include "audio_coding_module.h"
#include "trace.h"

#include "APITest.h"
#include "EncodeDecodeTest.h"
#include "gtest/gtest.h"
#include "iSACTest.h"
#include "SpatialAudio.h"
#include "TestAllCodecs.h"
#include "TestFEC.h"
#include "TestStereo.h"
#include "TestVADDTX.h"
#include "TwoWayCommunication.h"
#include "testsupport/fileutils.h"

using webrtc::AudioCodingModule;
using webrtc::Trace;

// Be sure to create the following directories before running the tests:
// ./modules/audio_coding/main/test/res_tests
// ./modules/audio_coding/main/test/res_autotests

// Choose what tests to run by defining one or more of the following:
#define ACM_AUTO_TEST            // Most common codecs and settings will be tested
//#define ACM_TEST_ENC_DEC        // You decide what to test in run time.
                                  // Used for debugging and for testing while implementing.
//#define ACM_TEST_TWO_WAY        // Debugging
//#define ACM_TEST_ALL_ENC_DEC    // Loop through all defined codecs and settings
//#define ACM_TEST_STEREO         // Run stereo and spatial audio tests
//#define ACM_TEST_VAD_DTX        // Run all VAD/DTX tests
//#define ACM_TEST_FEC            // Test FEC (also called RED)
//#define ACM_TEST_CODEC_SPEC_API // Only iSAC has codec specfic APIs in this version
//#define ACM_TEST_FULL_API       // Test all APIs with threads (long test)


void PopulateTests(std::vector<ACMTest*>* tests)
{

     Trace::CreateTrace();
     std::string trace_file = webrtc::test::OutputPath() + "acm_trace.txt";
     Trace::SetTraceFile(trace_file.c_str());

     printf("The following tests will be executed:\n");
#ifdef ACM_AUTO_TEST
    printf("  ACM auto test\n");
    tests->push_back(new webrtc::EncodeDecodeTest(0));
    tests->push_back(new webrtc::TwoWayCommunication(0));
    tests->push_back(new webrtc::TestAllCodecs(0));
    tests->push_back(new webrtc::TestStereo(0));
//    tests->push_back(new webrtc::SpatialAudio(0));
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
#ifdef ACM_TEST_ALL_ENC_DEC
    printf("  ACM all codecs test\n");
    tests->push_back(new webrtc::TestAllCodecs(1));
#endif
#ifdef ACM_TEST_STEREO
    printf("  ACM stereo test\n");
    tests->push_back(new webrtc::TestStereo(1));
    //tests->push_back(new webrtc::SpatialAudio(2));
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
TEST(AudioCodingModuleTest, RunAllTests)
{
    std::vector<ACMTest*> tests;
    PopulateTests(&tests);
    std::vector<ACMTest*>::iterator it;
    for (it=tests.begin() ; it < tests.end(); it++)
    {
        (*it)->Perform();
        delete (*it);
    }

    Trace::ReturnTrace();
    printf("ACM test completed\n");
}
