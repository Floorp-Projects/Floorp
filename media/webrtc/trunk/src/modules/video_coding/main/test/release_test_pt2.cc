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




int ReleaseTestPart2()
{
    printf("Verify that TICK_TIME_DEBUG and EVENT_DEBUG are uncommented");
    // Tests requiring verification

    printf("Testing Generic Codecs...\n");
    TEST(VCMGenericCodecTest() == 0);
    printf("Verify by viewing output file GCTest_out.yuv \n");
    
    return 0;
}