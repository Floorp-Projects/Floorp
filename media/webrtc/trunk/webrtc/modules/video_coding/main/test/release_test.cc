/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "ReleaseTest.h"
#include "ReceiverTests.h"
#include "TestMacros.h"
#include "MediaOptTest.h"
#include "CodecDataBaseTest.h"
#include "GenericCodecTest.h"




int ReleaseTest()
{
    printf("VCM RELEASE TESTS \n\n");
    
    // Automatic tests

    printf("Testing receive side timing...\n");
    TEST(ReceiverTimingTests() == 0);
    
    printf("Testing jitter buffer...\n");
    TEST(JitterBufferTest() == 0);
    
    printf("Testing Codec Data Base...\n");
    TEST(CodecDBTest() == 0);
    
    printf("Testing Media Optimization....\n");
    TEST(VCMMediaOptTest(1) == 0); 

    // Tests requiring verification
    
    printf("Testing Multi thread send-receive....\n");
    TEST(MTRxTxTest() == 0);
    printf("Verify by viewing output file MTRxTx_out.yuv \n");
    
    return 0;
}