/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <math.h>
#include <stdio.h>

#include "common_types.h"
#include "../source/event.h"
#include "frame_buffer.h"
#include "inter_frame_delay.h"
#include "jitter_buffer.h"
#include "jitter_estimate_test.h"
#include "jitter_estimator.h"
#include "media_opt_util.h"
#include "modules/video_coding/main/source/tick_time_base.h"
#include "packet.h"
#include "test_util.h"
#include "test_macros.h"

// TODO(holmer): Get rid of this to conform with style guide.
using namespace webrtc;

// TODO (Mikhal/Stefan): Update as gtest and separate to specific tests.

int CheckOutFrame(VCMEncodedFrame* frameOut, unsigned int size, bool startCode)
{
    if (frameOut == 0)
    {
        return -1;
    }

    const WebRtc_UWord8* outData = frameOut->Buffer();

    unsigned int i = 0;

    if(startCode)
    {
        if (outData[0] != 0 || outData[1] != 0 || outData[2] != 0 ||
            outData[3] != 1)
        {
            return -2;
        }
        i+= 4;
    }

    // check the frame data
    int count = 3;

    // check the frame length
    if (frameOut->Length() != size)
    {
        return -3;
    }

    for(; i < size; i++)
    {
        if (outData[i] == 0 && outData[i + 1] == 0 && outData[i + 2] == 0x80)
        {
            i += 2;
        }
        else if(startCode && outData[i] == 0 && outData[i + 1] == 0)
        {
            if (outData[i] != 0 || outData[i + 1] != 0 ||
                outData[i + 2] != 0 || outData[i + 3] != 1)
            {
                return -3;
            }
            i += 3;
        }
        else
        {
            if (outData[i] != count)
            {
                return -4;
            }
            count++;
            if(count == 10)
            {
                count = 3;
            }
        }
    }
    return 0;
}


int JitterBufferTest(CmdArgs& args)
{
    // Don't run these tests with debug event.
#if defined(EVENT_DEBUG)
    return -1;
#endif
    TickTimeBase clock;

    // Start test
    WebRtc_UWord16 seqNum = 1234;
    WebRtc_UWord32 timeStamp = 0;
    int size = 1400;
    WebRtc_UWord8 data[1500];
    VCMPacket packet(data, size, seqNum, timeStamp, true);

    VCMJitterBuffer jb(&clock);

    seqNum = 1234;
    timeStamp = 123*90;
    FrameType incomingFrameType(kVideoFrameKey);
    VCMEncodedFrame* frameOut=NULL;
    WebRtc_Word64 renderTimeMs = 0;
    packet.timestamp = timeStamp;
    packet.seqNum = seqNum;

    // build a data vector with 0, 0, 0x80, 3, 4, 5, 6, 7, 8, 9, 0, 0, 0x80, 3....
    data[0] = 0;
    data[1] = 0;
    data[2] = 0x80;
    int count = 3;
    for (unsigned int i = 3; i < sizeof(data) - 3; ++i)
    {
        data[i] = count;
        count++;
        if(count == 10)
        {
            data[i+1] = 0;
            data[i+2] = 0;
            data[i+3] = 0x80;
            count = 3;
            i += 3;
        }
    }

    // Test out of range inputs
    TEST(kSizeError == jb.InsertPacket(0, packet));
    jb.ReleaseFrame(0);

    // Not started
    TEST(0 == jb.GetFrame(packet));
    TEST(-1 == jb.GetNextTimeStamp(10, incomingFrameType, renderTimeMs));
    TEST(0 == jb.GetCompleteFrameForDecoding(10));
    TEST(0 == jb.GetFrameForDecoding());

    // Start
    jb.Start();

    // Get frame to use for this timestamp
    VCMEncodedFrame* frameIn = jb.GetFrame(packet);
    TEST(frameIn != 0);

    // No packets inserted
    TEST(0 == jb.GetCompleteFrameForDecoding(10));


    //
    // TEST single packet frame
    //
    //  --------
    // |  1234  |
    //  --------

    // packet.frameType;
    // packet.dataPtr;
    // packet.sizeBytes;
    // packet.timestamp;
    // packet.seqNum;
    // packet.isFirstPacket;
    // packet.markerBit;
    //
    packet.frameType = kVideoFrameDelta;
    packet.isFirstPacket = true;
    packet.markerBit = true;

    // Insert a packet into a frame
    TEST(kFirstPacket == jb.InsertPacket(frameIn, packet));

    // get packet notification
    TEST(timeStamp == jb.GetNextTimeStamp(10, incomingFrameType, renderTimeMs));

    // check incoming frame type
    TEST(incomingFrameType == kVideoFrameDelta);

    // get the frame
    frameOut = jb.GetCompleteFrameForDecoding(10);

    TEST(CheckOutFrame(frameOut, size, false) == 0);

    // check the frame type
    TEST(frameOut->FrameType() == kVideoFrameDelta);

    // Release frame (when done with decoding)
    jb.ReleaseFrame(frameOut);

    //printf("DONE delta frame 1 packet\n");

    //
    // TEST dual packet frame
    //
    //  -----------------
    // |  1235  |  1236  |
    //  -----------------
    //

    seqNum++;
    timeStamp += 33*90;
    packet.frameType = kVideoFrameDelta;
    packet.isFirstPacket = true;
    packet.markerBit = false;
    packet.seqNum = seqNum;
    packet.timestamp = timeStamp;

    frameIn = jb.GetFrame(packet);
    TEST(frameIn != 0);

    // Insert a packet into a frame
    TEST(kFirstPacket == jb.InsertPacket(frameIn, packet));

    // get packet notification
    TEST(timeStamp == jb.GetNextTimeStamp(10, incomingFrameType, renderTimeMs));

    // check incoming frame type
    TEST(incomingFrameType == kVideoFrameDelta);

    // get the frame
    frameOut = jb.GetCompleteFrameForDecoding(10);

    // it should not be complete
    TEST(frameOut == 0);

    seqNum++;
    packet.isFirstPacket = false;
    packet.markerBit = true;
    packet.seqNum = seqNum;

    frameIn = jb.GetFrame(packet);
    TEST(frameIn != 0);

    // Insert a packet into a frame
    TEST(kCompleteSession == jb.InsertPacket(frameIn, packet));

    // get the frame
    frameOut = jb.GetCompleteFrameForDecoding(10);

    TEST(CheckOutFrame(frameOut, size*2, false) == 0);

    // check the frame type
    TEST(frameOut->FrameType() == kVideoFrameDelta);

    // Release frame (when done with decoding)
    jb.ReleaseFrame(frameOut);

    //printf("DONE delta frame 2 packets\n");


    //
    // TEST 100 packets frame Key frame
    //
    //  ----------------------------------
    // |  1237  |  1238  |  .... |  1336  |
    //  ----------------------------------

    // insert first packet
    timeStamp += 33*90;
    seqNum++;
    packet.frameType = kVideoFrameKey;
    packet.isFirstPacket = true;
    packet.markerBit = false;
    packet.seqNum = seqNum;
    packet.timestamp = timeStamp;

    frameIn = jb.GetFrame(packet);
    TEST(frameIn != 0);

    // Insert a packet into a frame
    TEST(kFirstPacket == jb.InsertPacket(frameIn, packet));

    // get packet notification
    TEST(timeStamp == jb.GetNextTimeStamp(10, incomingFrameType, renderTimeMs));

    // check incoming frame type
    TEST(incomingFrameType == kVideoFrameKey);

    // get the frame
    frameOut = jb.GetCompleteFrameForDecoding(10);

    // it should not be complete
    TEST(frameOut == 0);

    // insert 98 frames
    int loop = 0;
    do
    {
        seqNum++;
        packet.isFirstPacket = false;
        packet.markerBit = false;
        packet.seqNum = seqNum;

        frameIn = jb.GetFrame(packet);
        TEST(frameIn != 0);

        // Insert a packet into a frame
        TEST(kIncomplete == jb.InsertPacket(frameIn, packet));
        loop++;
    } while (loop < 98);

    // insert last packet
    seqNum++;
    packet.isFirstPacket = false;
    packet.markerBit = true;
    packet.seqNum = seqNum;

    frameIn = jb.GetFrame(packet);
    TEST(frameIn != 0);

    // Insert a packet into a frame
    TEST(kCompleteSession == jb.InsertPacket(frameIn, packet));

    // get the frame
    frameOut = jb.GetCompleteFrameForDecoding(10);

    TEST(CheckOutFrame(frameOut, size*100, false) == 0);

    // check the frame type
    TEST(frameOut->FrameType() == kVideoFrameKey);

    // Release frame (when done with decoding)
    jb.ReleaseFrame(frameOut);

    //printf("DONE key frame 100 packets\n");

    //
    // TEST 100 packets frame Delta frame
    //
    //  ----------------------------------
    // |  1337  |  1238  |  .... |  1436  |
    //  ----------------------------------

    // insert first packet
    timeStamp += 33*90;
    seqNum++;
    packet.frameType = kVideoFrameDelta;
    packet.isFirstPacket = true;
    packet.markerBit = false;
    packet.seqNum = seqNum;
    packet.timestamp = timeStamp;

    frameIn = jb.GetFrame(packet);
    TEST(frameIn != 0);

    // Insert a packet into a frame
    TEST(kFirstPacket == jb.InsertPacket(frameIn, packet));

    // get packet notification
    TEST(timeStamp == jb.GetNextTimeStamp(10, incomingFrameType, renderTimeMs));

    // check incoming frame type
    TEST(incomingFrameType == kVideoFrameDelta);

    // get the frame
    frameOut = jb.GetCompleteFrameForDecoding(10);

    // it should not be complete
    TEST(frameOut == 0);

    // insert 98 frames
    loop = 0;
    do
    {
        seqNum++;
        packet.isFirstPacket = false;
        packet.markerBit = false;
        packet.seqNum = seqNum;

        frameIn = jb.GetFrame(packet);
        TEST(frameIn != 0);

        // Insert a packet into a frame
        TEST(kIncomplete == jb.InsertPacket(frameIn, packet));
        loop++;
    } while (loop < 98);

    // insert last packet
    seqNum++;
    packet.isFirstPacket = false;
    packet.markerBit = true;
    packet.seqNum = seqNum;

    frameIn = jb.GetFrame(packet);
    TEST(frameIn != 0);

    // Insert a packet into a frame
    TEST(kCompleteSession == jb.InsertPacket(frameIn, packet));

    // get the frame
    frameOut = jb.GetCompleteFrameForDecoding(10);

    TEST(CheckOutFrame(frameOut, size*100, false) == 0);

    // check the frame type
    TEST(frameOut->FrameType() == kVideoFrameDelta);

    // Release frame (when done with decoding)
    jb.ReleaseFrame(frameOut);

    //printf("DONE delta frame 100 packets\n");

    //
    // TEST packet re-ordering reverse order
    //
    //  ----------------------------------
    // |  1437  |  1238  |  .... |  1536  |
    //  ----------------------------------
    //            <----------

    // insert "first" packet last seqnum
    timeStamp += 33*90;
    seqNum += 100;
    packet.frameType = kVideoFrameDelta;
    packet.isFirstPacket = false;
    packet.markerBit = true;
    packet.seqNum = seqNum;
    packet.timestamp = timeStamp;

    frameIn = jb.GetFrame(packet);
    TEST(frameIn != 0);

    // Insert a packet into a frame
    TEST(kFirstPacket == jb.InsertPacket(frameIn, packet));

    // get packet notification
    TEST(timeStamp == jb.GetNextTimeStamp(10, incomingFrameType, renderTimeMs));

    // check incoming frame type
    TEST(incomingFrameType == kVideoFrameDelta);

    // get the frame
    frameOut = jb.GetCompleteFrameForDecoding(10);

    // it should not be complete
    TEST(frameOut == 0);

    // insert 98 packets
    loop = 0;
    do
    {
        seqNum--;
        packet.isFirstPacket = false;
        packet.markerBit = false;
        packet.seqNum = seqNum;

        frameIn = jb.GetFrame(packet);
        TEST(frameIn != 0);

        // Insert a packet into a frame
        TEST(kIncomplete == jb.InsertPacket(frameIn, packet));
        loop++;
    } while (loop < 98);

    // insert last packet
    seqNum--;
    packet.isFirstPacket = true;
    packet.markerBit = false;
    packet.seqNum = seqNum;

    frameIn = jb.GetFrame(packet);
    TEST(frameIn != 0);

    // Insert a packet into a frame
    TEST(kCompleteSession == jb.InsertPacket(frameIn, packet));

    // get the frame
    frameOut = jb.GetCompleteFrameForDecoding(10);

    TEST(CheckOutFrame(frameOut, size*100, false) == 0);

    // check the frame type
    TEST(frameOut->FrameType() == kVideoFrameDelta);

    // Release frame (when done with decoding)
    jb.ReleaseFrame(frameOut);

    //printf("DONE delta frame 100 packets reverse order\n");

    seqNum+= 100;

    //
    // TEST frame re-ordering 2 frames 2 packets each
    //
    //  -----------------     -----------------
    // |  1539  |  1540  |   |  1537  |  1538  |
    //  -----------------     -----------------

    seqNum += 2;
    timeStamp += 2* 33 * 90;
    packet.frameType = kVideoFrameDelta;
    packet.isFirstPacket = true;
    packet.markerBit = false;
    packet.seqNum = seqNum;
    packet.timestamp = timeStamp;

    frameIn = jb.GetFrame(packet);
    TEST(frameIn != 0);

    // Insert a packet into a frame
    TEST(kFirstPacket == jb.InsertPacket(frameIn, packet));

    // get packet notification
    TEST(timeStamp == jb.GetNextTimeStamp(10, incomingFrameType, renderTimeMs));

    // check incoming frame type
    TEST(incomingFrameType == kVideoFrameDelta);

    // get the frame
    frameOut = jb.GetCompleteFrameForDecoding(10);

    // it should not be complete
    TEST(frameOut == 0);

    seqNum++;
    packet.isFirstPacket = false;
    packet.markerBit = true;
    packet.seqNum = seqNum;

    frameIn = jb.GetFrame(packet);
    TEST(frameIn != 0);

    // Insert a packet into a frame
    TEST(kCompleteSession == jb.InsertPacket(frameIn, packet));

    // check that we fail to get frame since seqnum is not continuous
    frameOut = jb.GetCompleteFrameForDecoding(10);
    TEST(frameOut == 0);

    seqNum -= 3;
    timeStamp -= 33*90;
    packet.frameType = kVideoFrameDelta;
    packet.isFirstPacket = true;
    packet.markerBit = false;
    packet.seqNum = seqNum;
    packet.timestamp = timeStamp;

    frameIn = jb.GetFrame(packet);
    TEST(frameIn != 0);

    // Insert a packet into a frame
    TEST(kFirstPacket == jb.InsertPacket(frameIn, packet));

    // get packet notification
    TEST(timeStamp == jb.GetNextTimeStamp(10, incomingFrameType, renderTimeMs));

    // check incoming frame type
    TEST(incomingFrameType == kVideoFrameDelta);

    // get the frame
    frameOut = jb.GetCompleteFrameForDecoding(10);

    // it should not be complete
    TEST(frameOut == 0);

    seqNum++;
    packet.isFirstPacket = false;
    packet.markerBit = true;
    packet.seqNum = seqNum;

    frameIn = jb.GetFrame(packet);
    TEST(frameIn != 0);

    // Insert a packet into a frame
    TEST(kCompleteSession == jb.InsertPacket(frameIn, packet));

    // get the frame
    frameOut = jb.GetCompleteFrameForDecoding(10);

    TEST(CheckOutFrame(frameOut, size*2, false) == 0);

    // check the frame type
    TEST(frameOut->FrameType() == kVideoFrameDelta);

    // Release frame (when done with decoding)
    jb.ReleaseFrame(frameOut);

    // get the frame
    frameOut = jb.GetCompleteFrameForDecoding(10);

    TEST(CheckOutFrame(frameOut, size*2, false) == 0);

    // check the frame type
    TEST(frameOut->FrameType() == kVideoFrameDelta);

    // Release frame (when done with decoding)
    jb.ReleaseFrame(frameOut);

    seqNum += 2;
    //printf("DONE frame re-ordering 2 frames 2 packets\n");

    // restore
    packet.dataPtr = data;
    packet.codec = kVideoCodecUnknown;

    //
    // TEST duplicate packets
    //
    //  -----------------
    // |  1543  |  1543  |
    //  -----------------
    //

   seqNum++;
    timeStamp += 2*33*90;
    packet.frameType = kVideoFrameDelta;
    packet.isFirstPacket = true;
    packet.markerBit = false;
    packet.seqNum = seqNum;
    packet.timestamp = timeStamp;

    frameIn = jb.GetFrame(packet);
    TEST(frameIn != 0);

    // Insert a packet into a frame
    TEST(kFirstPacket == jb.InsertPacket(frameIn, packet));

    // get packet notification
    TEST(timeStamp == jb.GetNextTimeStamp(10, incomingFrameType, renderTimeMs));

    // check incoming frame type
    TEST(incomingFrameType == kVideoFrameDelta);

    // get the frame
    frameOut = jb.GetCompleteFrameForDecoding(10);

    // it should not be complete
    TEST(frameOut == 0);

    packet.isFirstPacket = false;
    packet.markerBit = true;

    frameIn = jb.GetFrame(packet);
    TEST(frameIn != 0);

    // Insert a packet into a frame
    TEST(kDuplicatePacket == jb.InsertPacket(frameIn, packet));

    seqNum++;
    packet.seqNum = seqNum;

    TEST(kCompleteSession == jb.InsertPacket(frameIn, packet));

    // get the frame
    frameOut = jb.GetCompleteFrameForDecoding(10);

    TEST(CheckOutFrame(frameOut, size*2, false) == 0);

    // check the frame type
    TEST(frameOut->FrameType() == kVideoFrameDelta);

    // Release frame (when done with decoding)
    jb.ReleaseFrame(frameOut);

    //printf("DONE test duplicate packets\n");

    //
    // TEST H.264 insert start code
    //
    //  -----------------
    // |  1544  |  1545  |
    //  -----------------
    // insert start code, both packets

    seqNum++;
    timeStamp += 33 * 90;
    packet.frameType = kVideoFrameDelta;
    packet.isFirstPacket = true;
    packet.markerBit = false;
    packet.seqNum = seqNum;
    packet.timestamp = timeStamp;
    packet.insertStartCode = true;

    frameIn = jb.GetFrame(packet);
    TEST(frameIn != 0);

    // Insert a packet into a frame
    TEST(kFirstPacket == jb.InsertPacket(frameIn, packet));

    // get packet notification
    TEST(timeStamp == jb.GetNextTimeStamp(10, incomingFrameType, renderTimeMs));

    // check incoming frame type
    TEST(incomingFrameType == kVideoFrameDelta);

    // get the frame
    frameOut = jb.GetCompleteFrameForDecoding(10);

    // it should not be complete
    TEST(frameOut == 0);

    seqNum++;
    packet.isFirstPacket = false;
    packet.markerBit = true;
    packet.seqNum = seqNum;

    frameIn = jb.GetFrame(packet);
    TEST(frameIn != 0);

    // Insert a packet into a frame
    TEST(kCompleteSession == jb.InsertPacket(frameIn, packet));

    // get the frame
    frameOut = jb.GetCompleteFrameForDecoding(10);

    TEST(CheckOutFrame(frameOut, size * 2 + 4 * 2, true) == 0);

    // check the frame type
    TEST(frameOut->FrameType() == kVideoFrameDelta);

    // Release frame (when done with decoding)
    jb.ReleaseFrame(frameOut);

    // reset
    packet.insertStartCode = false;
    //printf("DONE H.264 insert start code test 2 packets\n");

    //
    // TEST statistics
    //
    WebRtc_UWord32 numDeltaFrames = 0;
    WebRtc_UWord32 numKeyFrames = 0;
    TEST(jb.GetFrameStatistics(numDeltaFrames, numKeyFrames) == 0);

    TEST(numDeltaFrames == 8);
    TEST(numKeyFrames == 1);

    WebRtc_UWord32 frameRate;
    WebRtc_UWord32 bitRate;
    TEST(jb.GetUpdate(frameRate, bitRate) == 0);

    // these depend on CPU speed works on a T61
    TEST(frameRate > 30);
    TEST(bitRate > 10000000);


    jb.Flush();

    //
    // TEST packet loss. Verify missing packets statistics and not decodable
    // packets statistics.
    // Insert 10 frames consisting of 4 packets and remove one from all of them.
    // The last packet is an empty (non-media) packet
    //

    // Select a start seqNum which triggers a difficult wrap situation
    // The JB will only output (incomplete)frames if the next one has started
    // to arrive. Start by inserting one frame (key).
    seqNum = 0xffff - 4;
    seqNum++;
    timeStamp += 33*90;
    packet.frameType = kVideoFrameKey;
    packet.isFirstPacket = true;
    packet.markerBit = false;
    packet.seqNum = seqNum;
    packet.timestamp = timeStamp;
    packet.completeNALU = kNaluStart;
    frameIn = jb.GetFrame(packet);
    TEST(frameIn != 0);
    // Insert a packet into a frame
    TEST(kFirstPacket == jb.InsertPacket(frameIn, packet));

    for (int i = 0; i < 11; ++i) {
      webrtc::FrameType frametype = kVideoFrameDelta;
      seqNum++;
      timeStamp += 33*90;
      packet.frameType = frametype;
      packet.isFirstPacket = true;
      packet.markerBit = false;
      packet.seqNum = seqNum;
      packet.timestamp = timeStamp;
      packet.completeNALU = kNaluStart;

      frameIn = jb.GetFrame(packet);
      TEST(frameIn != 0);

      // Insert a packet into a frame
      TEST(kFirstPacket == jb.InsertPacket(frameIn, packet));

      // Get packet notification
      TEST(timeStamp - 33 * 90 == jb.GetNextTimeStamp(10, incomingFrameType,
                                                      renderTimeMs));

      // Check incoming frame type
      if (i == 0)
      {
          TEST(incomingFrameType == kVideoFrameKey);
      }
      else
      {
          TEST(incomingFrameType == frametype);
      }

      // Get the frame
      frameOut = jb.GetCompleteFrameForDecoding(10);

      // Should not be complete
      TEST(frameOut == 0);

      seqNum += 2;
      packet.isFirstPacket = false;
      packet.markerBit = true;
      packet.seqNum = seqNum;
      packet.completeNALU = kNaluEnd;

      frameIn = jb.GetFrame(packet);
      TEST(frameIn != 0);

      // Insert a packet into a frame
      TEST(kIncomplete == jb.InsertPacket(frameIn, packet));


      // Insert an empty (non-media) packet
      seqNum++;
      packet.isFirstPacket = false;
      packet.markerBit = false;
      packet.seqNum = seqNum;
      packet.completeNALU = kNaluEnd;
      packet.frameType = kFrameEmpty;

      frameIn = jb.GetFrame(packet);
      TEST(frameIn != 0);

      // Insert a packet into a frame
      TEST(kIncomplete == jb.InsertPacket(frameIn, packet));

      // Get the frame
      frameOut = jb.GetFrameForDecoding();

      // One of the packets has been discarded by the jitter buffer.
      // Last frame can't be extracted yet.
      if (i < 10)
      {
          TEST(CheckOutFrame(frameOut, size, false) == 0);

          // check the frame type
          if (i == 0)
          {
              TEST(frameOut->FrameType() == kVideoFrameKey);
          }
         else
         {
             TEST(frameOut->FrameType() == frametype);
         }
          TEST(frameOut->Complete() == false);
          TEST(frameOut->MissingFrame() == false);
      }

      // Release frame (when done with decoding)
      jb.ReleaseFrame(frameOut);
    }

    TEST(jb.NumNotDecodablePackets() == 10);

    // Insert 3 old packets and verify that we have 3 discarded packets
    // Match value to actual latest timestamp decoded
    timeStamp -= 33 * 90;
    packet.timestamp = timeStamp - 1000;
    frameIn = jb.GetFrame(packet);
    TEST(frameIn == NULL);

    packet.timestamp = timeStamp - 500;
    frameIn = jb.GetFrame(packet);
    TEST(frameIn == NULL);

    packet.timestamp = timeStamp - 100;
    frameIn = jb.GetFrame(packet);
    TEST(frameIn == NULL);

    TEST(jb.DiscardedPackets() == 3);

    jb.Flush();

    // This statistic shouldn't be reset by a flush.
    TEST(jb.DiscardedPackets() == 3);

    //printf("DONE Statistics\n");


    // Temporarily do this to make the rest of the test work:
    timeStamp += 33*90;
    seqNum += 4;


    //
    // TEST delta frame 100 packets with seqNum wrap
    //
    //  ---------------------------------------
    // |  65520  |  65521  | ... |  82  |  83  |
    //  ---------------------------------------
    //

    jb.Flush();

    // insert first packet
    timeStamp += 33*90;
    seqNum = 0xfff0;
    packet.frameType = kVideoFrameDelta;
    packet.isFirstPacket = true;
    packet.markerBit = false;
    packet.seqNum = seqNum;
    packet.timestamp = timeStamp;

    frameIn = jb.GetFrame(packet);
    TEST(frameIn != 0);

    // Insert a packet into a frame
    TEST(kFirstPacket == jb.InsertPacket(frameIn, packet));

    // get packet notification
    TEST(timeStamp == jb.GetNextTimeStamp(10, incomingFrameType, renderTimeMs));

    // check incoming frame type
    TEST(incomingFrameType == kVideoFrameDelta);

    // get the frame
    frameOut = jb.GetCompleteFrameForDecoding(10);

    // it should not be complete
    TEST(frameOut == 0);

    // insert 98 packets
    loop = 0;
    do
    {
        seqNum++;
        packet.isFirstPacket = false;
        packet.markerBit = false;
        packet.seqNum = seqNum;

        frameIn = jb.GetFrame(packet);
        TEST(frameIn != 0);

        // Insert a packet into a frame
        TEST(kIncomplete == jb.InsertPacket(frameIn, packet));

        // get packet notification
        TEST(timeStamp == jb.GetNextTimeStamp(2, incomingFrameType, renderTimeMs));

        // check incoming frame type
        TEST(incomingFrameType == kVideoFrameDelta);

        // get the frame
        frameOut = jb.GetCompleteFrameForDecoding(2);

        // it should not be complete
        TEST(frameOut == 0);

        loop++;
    } while (loop < 98);

    // insert last packet
    seqNum++;
    packet.isFirstPacket = false;
    packet.markerBit = true;
    packet.seqNum = seqNum;

    frameIn = jb.GetFrame(packet);
    TEST(frameIn != 0);

    // Insert a packet into a frame
    TEST(kCompleteSession == jb.InsertPacket(frameIn, packet));

    // get the frame
    frameOut = jb.GetCompleteFrameForDecoding(10);

    TEST(CheckOutFrame(frameOut, size*100, false) == 0);

    // check the frame type
    TEST(frameOut->FrameType() == kVideoFrameDelta);

    // Release frame (when done with decoding)
    jb.ReleaseFrame(frameOut);

    //printf("DONE delta frame 100 packets with wrap in seqNum\n");

    //
    // TEST packet re-ordering reverse order with neg seqNum warp
    //
    //  ----------------------------------------
    // |  65447  |  65448  | ... |   9   |  10  |
    //  ----------------------------------------
    //              <-------------

    // test flush
    jb.Flush();

    // insert "first" packet last seqnum
    timeStamp += 33*90;
    seqNum = 10;
    packet.frameType = kVideoFrameDelta;
    packet.isFirstPacket = false;
    packet.markerBit = true;
    packet.seqNum = seqNum;
    packet.timestamp = timeStamp;

    frameIn = jb.GetFrame(packet);
    TEST(frameIn != 0);

    // Insert a packet into a frame
    TEST(kFirstPacket == jb.InsertPacket(frameIn, packet));

    // get packet notification
    TEST(timeStamp == jb.GetNextTimeStamp(10, incomingFrameType, renderTimeMs));

    // check incoming frame type
    TEST(incomingFrameType == kVideoFrameDelta);

    // get the frame
    frameOut = jb.GetCompleteFrameForDecoding(10);

    // it should not be complete
    TEST(frameOut == 0);

    // insert 98 frames
    loop = 0;
    do
    {
        seqNum--;
        packet.isFirstPacket = false;
        packet.markerBit = false;
        packet.seqNum = seqNum;

        frameIn = jb.GetFrame(packet);
        TEST(frameIn != 0);

        // Insert a packet into a frame
        TEST(kIncomplete == jb.InsertPacket(frameIn, packet));

        // get packet notification
        TEST(timeStamp == jb.GetNextTimeStamp(2, incomingFrameType, renderTimeMs));

        // check incoming frame type
        TEST(incomingFrameType == kVideoFrameDelta);

        // get the frame
        frameOut = jb.GetCompleteFrameForDecoding(2);

        // it should not be complete
        TEST(frameOut == 0);

        loop++;
    } while (loop < 98);

    // insert last packet
    seqNum--;
    packet.isFirstPacket = true;
    packet.markerBit = false;
    packet.seqNum = seqNum;

    frameIn = jb.GetFrame(packet);
    TEST(frameIn != 0);

    // Insert a packet into a frame
    TEST(kCompleteSession == jb.InsertPacket(frameIn, packet));

    // get the frame
    frameOut = jb.GetCompleteFrameForDecoding(10);

    TEST(CheckOutFrame(frameOut, size*100, false) == 0);

    // check the frame type
    TEST(frameOut->FrameType() == kVideoFrameDelta);

    // Release frame (when done with decoding)
    jb.ReleaseFrame(frameOut);

    //printf("DONE delta frame 100 packets reverse order with wrap in seqNum \n");

    // test flush
    jb.Flush();

    //
    // TEST packet re-ordering with seqNum wrap
    //
    //  -----------------------
    // |   1   | 65535 |   0   |
    //  -----------------------

    // insert "first" packet last seqnum
    timeStamp += 33*90;
    seqNum = 1;
    packet.frameType = kVideoFrameDelta;
    packet.isFirstPacket = false;
    packet.markerBit = true;
    packet.seqNum = seqNum;
    packet.timestamp = timeStamp;

    frameIn = jb.GetFrame(packet);
    TEST(frameIn != 0);

    // Insert a packet into a frame
    TEST(kFirstPacket == jb.InsertPacket(frameIn, packet));

    // get packet notification
    TEST(timeStamp == jb.GetNextTimeStamp(10, incomingFrameType, renderTimeMs));

    // check incoming frame type
    TEST(incomingFrameType == kVideoFrameDelta);

    // get the frame
    frameOut = jb.GetCompleteFrameForDecoding(10);

    // it should not be complete
    TEST(frameOut == 0);

    // insert last packet
    seqNum -= 2;
    packet.isFirstPacket = true;
    packet.markerBit = false;
    packet.seqNum = seqNum;

    frameIn = jb.GetFrame(packet);
    TEST(frameIn != 0);

    // Insert a packet into a frame
    TEST(kIncomplete == jb.InsertPacket(frameIn, packet));

    // get packet notification
    TEST(timeStamp == jb.GetNextTimeStamp(10, incomingFrameType, renderTimeMs));

    // check incoming frame type
    TEST(incomingFrameType == kVideoFrameDelta);

    // get the frame
    frameOut = jb.GetCompleteFrameForDecoding(10);

    // it should not be complete
    TEST(frameOut == 0);

    seqNum++;
    packet.isFirstPacket = false;
    packet.markerBit = false;
    packet.seqNum = seqNum;

    frameIn = jb.GetFrame(packet);
    TEST(frameIn != 0);

    // Insert a packet into a frame
    TEST(kCompleteSession == jb.InsertPacket(frameIn, packet));

    // get the frame
    frameOut = jb.GetCompleteFrameForDecoding(10);

    TEST(CheckOutFrame(frameOut, size*3, false) == 0);

    // check the frame type
    TEST(frameOut->FrameType() == kVideoFrameDelta);

    // Release frame (when done with decoding)
    jb.ReleaseFrame(frameOut);

    //printf("DONE delta frame 3 packets re-ordering with wrap in seqNum \n");

    // test flush
    jb.Flush();

    //
    // TEST insert old frame
    //
    //   -------      -------
    //  |   2   |    |   1   |
    //   -------      -------
    //  t = 3000     t = 2000

    seqNum = 2;
    timeStamp = 3000;
    packet.frameType = kVideoFrameDelta;
    packet.isFirstPacket = true;
    packet.markerBit = true;
    packet.seqNum = seqNum;
    packet.timestamp = timeStamp;

    frameIn = jb.GetFrame(packet);
    TEST(frameIn != 0);

    // Insert a packet into a frame
    TEST(kFirstPacket == jb.InsertPacket(frameIn, packet));

    // get packet notification
    TEST(3000 == jb.GetNextTimeStamp(10, incomingFrameType, renderTimeMs));
    TEST(kVideoFrameDelta == incomingFrameType);

    // Get the frame
    frameOut = jb.GetCompleteFrameForDecoding(10);
    TEST(3000 == frameOut->TimeStamp());

    TEST(CheckOutFrame(frameOut, size, false) == 0);

    // check the frame type
    TEST(frameOut->FrameType() == kVideoFrameDelta);

    jb.ReleaseFrame(frameOut);

    seqNum--;
    timeStamp = 2000;
    packet.frameType = kVideoFrameDelta;
    packet.isFirstPacket = true;
    packet.markerBit = true;
    packet.seqNum = seqNum;
    packet.timestamp = timeStamp;

    frameIn = jb.GetFrame(packet);
    // Changed behavior, never insert packets into frames older than the
    // last decoded frame.
    TEST(frameIn == 0);

    //printf("DONE insert old frame\n");

    jb.Flush();

   //
    // TEST insert old frame with wrap in timestamp
    //
    //   -------      -------
    //  |   2   |    |   1   |
    //   -------      -------
    //  t = 3000     t = 0xffffff00

    seqNum = 2;
    timeStamp = 3000;
    packet.frameType = kVideoFrameDelta;
    packet.isFirstPacket = true;
    packet.markerBit = true;
    packet.seqNum = seqNum;
    packet.timestamp = timeStamp;

    frameIn = jb.GetFrame(packet);
    TEST(frameIn != 0);

    // Insert a packet into a frame
    TEST(kFirstPacket == jb.InsertPacket(frameIn, packet));

    // get packet notification
    TEST(timeStamp == jb.GetNextTimeStamp(10, incomingFrameType, renderTimeMs));
    TEST(kVideoFrameDelta == incomingFrameType);

    // Get the frame
    frameOut = jb.GetCompleteFrameForDecoding(10);
    TEST(timeStamp == frameOut->TimeStamp());

    TEST(CheckOutFrame(frameOut, size, false) == 0);

    // check the frame type
    TEST(frameOut->FrameType() == kVideoFrameDelta);

    jb.ReleaseFrame(frameOut);

    seqNum--;
    timeStamp = 0xffffff00;
    packet.frameType = kVideoFrameDelta;
    packet.isFirstPacket = true;
    packet.markerBit = true;
    packet.seqNum = seqNum;
    packet.timestamp = timeStamp;

    frameIn = jb.GetFrame(packet);
    // This timestamp is old
    TEST(frameIn == 0);

    jb.Flush();

    //
    // TEST wrap in timeStamp
    //
    //  ---------------     ---------------
    // |   1   |   2   |   |   3   |   4   |
    //  ---------------     ---------------
    //  t = 0xffffff00        t = 33*90

    seqNum = 1;
    timeStamp = 0xffffff00;
    packet.frameType = kVideoFrameDelta;
    packet.isFirstPacket = true;
    packet.markerBit = false;
    packet.seqNum = seqNum;
    packet.timestamp = timeStamp;

    frameIn = jb.GetFrame(packet);
    TEST(frameIn != 0);

    // Insert a packet into a frame
    TEST(kFirstPacket == jb.InsertPacket(frameIn, packet));

    // get packet notification
    TEST(timeStamp == jb.GetNextTimeStamp(10, incomingFrameType, renderTimeMs));

    // check incoming frame type
    TEST(incomingFrameType == kVideoFrameDelta);

    // get the frame
    frameOut = jb.GetCompleteFrameForDecoding(10);

    // it should not be complete
    TEST(frameOut == 0);

    seqNum++;
    packet.isFirstPacket = false;
    packet.markerBit = true;
    packet.seqNum = seqNum;

    frameIn = jb.GetFrame(packet);
    TEST(frameIn != 0);

    // Insert a packet into a frame
    TEST(kCompleteSession == jb.InsertPacket(frameIn, packet));

    frameOut = jb.GetCompleteFrameForDecoding(10);

    TEST(CheckOutFrame(frameOut, size*2, false) == 0);

    jb.ReleaseFrame(frameOut);

    seqNum++;
    timeStamp += 33*90;
    packet.frameType = kVideoFrameDelta;
    packet.isFirstPacket = true;
    packet.markerBit = false;
    packet.seqNum = seqNum;
    packet.timestamp = timeStamp;

    frameIn = jb.GetFrame(packet);
    TEST(frameIn != 0);

    // Insert a packet into a frame
    TEST(kFirstPacket == jb.InsertPacket(frameIn, packet));

    // get packet notification
    TEST(timeStamp == jb.GetNextTimeStamp(10, incomingFrameType, renderTimeMs));

    // check incoming frame type
    TEST(incomingFrameType == kVideoFrameDelta);

    // get the frame
    frameOut = jb.GetCompleteFrameForDecoding(10);

    // it should not be complete
    TEST(frameOut == 0);

    seqNum++;
    packet.isFirstPacket = false;
    packet.markerBit = true;
    packet.seqNum = seqNum;

    frameIn = jb.GetFrame(packet);
    TEST(frameIn != 0);

    // Insert a packet into a frame
    TEST(kCompleteSession == jb.InsertPacket(frameIn, packet));

    // get the frame
    frameOut = jb.GetCompleteFrameForDecoding(10);

    TEST(CheckOutFrame(frameOut, size*2, false) == 0);

    // check the frame type
    TEST(frameOut->FrameType() == kVideoFrameDelta);

    // Release frame (when done with decoding)
    jb.ReleaseFrame(frameOut);

    //printf("DONE time stamp wrap 2 frames 2 packets\n");

    jb.Flush();

    //
    // TEST insert 2 frames with wrap in timeStamp
    //
    //   -------          -------
    //  |   1   |        |   2   |
    //   -------          -------
    // t = 0xffffff00    t = 2700

    seqNum = 1;
    timeStamp = 0xffffff00;
    packet.frameType = kVideoFrameDelta;
    packet.isFirstPacket = true;
    packet.markerBit = true;
    packet.seqNum = seqNum;
    packet.timestamp = timeStamp;

    frameIn = jb.GetFrame(packet);
    TEST(frameIn != 0);

    // Insert first frame
    TEST(kFirstPacket == jb.InsertPacket(frameIn, packet));

    // Get packet notification
    TEST(0xffffff00 == jb.GetNextTimeStamp(10, incomingFrameType, renderTimeMs));
    TEST(kVideoFrameDelta == incomingFrameType);

    // Insert next frame
    seqNum++;
    timeStamp = 2700;
    packet.frameType = kVideoFrameDelta;
    packet.isFirstPacket = true;
    packet.markerBit = true;
    packet.seqNum = seqNum;
    packet.timestamp = timeStamp;

    frameIn = jb.GetFrame(packet);
    TEST(frameIn != 0);

    // Insert a packet into a frame
    TEST(kFirstPacket == jb.InsertPacket(frameIn, packet));

    // Get packet notification
    TEST(0xffffff00 == jb.GetNextTimeStamp(10, incomingFrameType, renderTimeMs));
    TEST(kVideoFrameDelta == incomingFrameType);

    // Get frame
    frameOut = jb.GetCompleteFrameForDecoding(10);
    TEST(0xffffff00 == frameOut->TimeStamp());

    TEST(CheckOutFrame(frameOut, size, false) == 0);

    // check the frame type
    TEST(frameOut->FrameType() == kVideoFrameDelta);

    // Get packet notification
    TEST(2700 == jb.GetNextTimeStamp(0, incomingFrameType, renderTimeMs));
    TEST(kVideoFrameDelta == incomingFrameType);

    // Get frame
    VCMEncodedFrame* frameOut2 = jb.GetCompleteFrameForDecoding(10);
    TEST(2700 == frameOut2->TimeStamp());

    TEST(CheckOutFrame(frameOut2, size, false) == 0);

    // check the frame type
    TEST(frameOut2->FrameType() == kVideoFrameDelta);

    // Release frame (when done with decoding)
    jb.ReleaseFrame(frameOut);
    jb.ReleaseFrame(frameOut2);

    //printf("DONE insert 2 frames (1 packet) with wrap in timestamp\n");

    jb.Flush();

    //
    // TEST insert 2 frames re-ordered with wrap in timeStamp
    //
    //   -------          -------
    //  |   2   |        |   1   |
    //   -------          -------
    //  t = 2700        t = 0xffffff00

    seqNum = 2;
    timeStamp = 2700;
    packet.frameType = kVideoFrameDelta;
    packet.isFirstPacket = true;
    packet.markerBit = true;
    packet.seqNum = seqNum;
    packet.timestamp = timeStamp;

    frameIn = jb.GetFrame(packet);
    TEST(frameIn != 0);

    // Insert first frame
    TEST(kFirstPacket == jb.InsertPacket(frameIn, packet));

    // Get packet notification
    TEST(2700 == jb.GetNextTimeStamp(10, incomingFrameType, renderTimeMs));
    TEST(kVideoFrameDelta == incomingFrameType);

    // Insert second frame
    seqNum--;
    timeStamp = 0xffffff00;
    packet.frameType = kVideoFrameDelta;
    packet.isFirstPacket = true;
    packet.markerBit = true;
    packet.seqNum = seqNum;
    packet.timestamp = timeStamp;

    frameIn = jb.GetFrame(packet);
    TEST(frameIn != 0);

    // Insert a packet into a frame
    TEST(kFirstPacket == jb.InsertPacket(frameIn, packet));

    // Get packet notification
    TEST(0xffffff00 == jb.GetNextTimeStamp(10, incomingFrameType, renderTimeMs));
    TEST(kVideoFrameDelta == incomingFrameType);

    // Get frame
    frameOut = jb.GetCompleteFrameForDecoding(10);
    TEST(0xffffff00 == frameOut->TimeStamp());

    TEST(CheckOutFrame(frameOut, size, false) == 0);

    // check the frame type
    TEST(frameOut->FrameType() == kVideoFrameDelta);

    // get packet notification
    TEST(2700 == jb.GetNextTimeStamp(0, incomingFrameType, renderTimeMs));
    TEST(kVideoFrameDelta == incomingFrameType);

    // Get frame
    frameOut2 = jb.GetCompleteFrameForDecoding(10);
    TEST(2700 == frameOut2->TimeStamp());

    TEST(CheckOutFrame(frameOut2, size, false) == 0);

    // check the frame type
    TEST(frameOut2->FrameType() == kVideoFrameDelta);

    // Release frame (when done with decoding)
    jb.ReleaseFrame(frameOut);
    jb.ReleaseFrame(frameOut2);

    //printf("DONE insert 2 frames (1 packet) re-ordered with wrap in timestamp\n");

    //
    // TEST delta frame with more than max number of packets
    //

    jb.Start();

    loop = 0;
    packet.timestamp += 33*90;
    bool firstPacket = true;
    // insert kMaxPacketsInJitterBuffer into frame
    do
    {
        seqNum++;
        packet.isFirstPacket = false;
        packet.markerBit = false;
        packet.seqNum = seqNum;

        frameIn = jb.GetFrame(packet);
        TEST(frameIn != 0);

        // Insert frame
        if (firstPacket)
        {
            TEST(kFirstPacket == jb.InsertPacket(frameIn, packet));
            firstPacket = false;
        }
        else
        {
            TEST(kIncomplete == jb.InsertPacket(frameIn, packet));
        }

        // get packet notification
        TEST(packet.timestamp == jb.GetNextTimeStamp(10, incomingFrameType, renderTimeMs));

        // check incoming frame type
        TEST(incomingFrameType == kVideoFrameDelta);

        loop++;
    } while (loop < kMaxPacketsInSession);

    // Max number of packets inserted

    // Insert one more packet
    seqNum++;
    packet.isFirstPacket = false;
    packet.markerBit = true;
    packet.seqNum = seqNum;

    frameIn = jb.GetFrame(packet);
    TEST(frameIn != 0);

    // Insert the packet -> frame recycled
    TEST(kSizeError == jb.InsertPacket(frameIn, packet));

    TEST(0 == jb.GetCompleteFrameForDecoding(10));

    //printf("DONE fill frame - packets > max number of packets\n");

    //
    // TEST fill JB with more than max number of frame (50 delta frames +
    // 51 key frames) with wrap in seqNum
    //
    //  --------------------------------------------------------------
    // | 65485 | 65486 | 65487 | .... | 65535 | 0 | 1 | 2 | .....| 50 |
    //  --------------------------------------------------------------
    // |<-----------delta frames------------->|<------key frames----->|

    jb.Flush();

    loop = 0;
    seqNum = 65485;
    WebRtc_UWord32 timeStampStart = timeStamp +  33*90;
    WebRtc_UWord32 timeStampFirstKey = 0;
    VCMEncodedFrame* ptrLastDeltaFrame = NULL;
    VCMEncodedFrame* ptrFirstKeyFrame = NULL;
    // insert MAX_NUMBER_OF_FRAMES frames
    do
    {
        timeStamp += 33*90;
        seqNum++;
        packet.isFirstPacket = true;
        packet.markerBit = true;
        packet.seqNum = seqNum;
        packet.timestamp = timeStamp;

        frameIn = jb.GetFrame(packet);
        TEST(frameIn != 0);

        if (loop == 49)  // last delta
        {
            ptrLastDeltaFrame = frameIn;
        }
        if (loop == 50)  // first key
        {
            ptrFirstKeyFrame = frameIn;
            packet.frameType = kVideoFrameKey;
            timeStampFirstKey = packet.timestamp;
        }

        // Insert frame
        TEST(kFirstPacket == jb.InsertPacket(frameIn, packet));

        // Get packet notification, should be first inserted frame
        TEST(timeStampStart == jb.GetNextTimeStamp(10, incomingFrameType,
                                                   renderTimeMs));

        // check incoming frame type
        TEST(incomingFrameType == kVideoFrameDelta);

        loop++;
    } while (loop < kMaxNumberOfFrames);

    // Max number of frames inserted

    // Insert one more frame
    timeStamp += 33*90;
    seqNum++;
    packet.isFirstPacket = true;
    packet.markerBit = true;
    packet.seqNum = seqNum;
    packet.timestamp = timeStamp;

    // Now, no free frame - frames will be recycled until first key frame
    frameIn = jb.GetFrame(packet);
    // ptr to last inserted delta frame should be returned
    TEST(frameIn != 0 && frameIn && ptrLastDeltaFrame);

    // Insert frame
    TEST(kFirstPacket == jb.InsertPacket(frameIn, packet));

    // First inserted key frame should be oldest in buffer
    TEST(timeStampFirstKey == jb.GetNextTimeStamp(10, incomingFrameType,
                                                  renderTimeMs));

    // check incoming frame type
    TEST(incomingFrameType == kVideoFrameKey);

    // get the first key frame
    frameOut = jb.GetCompleteFrameForDecoding(10);
    TEST(ptrFirstKeyFrame == frameOut);

    TEST(CheckOutFrame(frameOut, size, false) == 0);

    // check the frame type
    TEST(frameOut->FrameType() == kVideoFrameKey);

    // Release frame (when done with decoding)
    jb.ReleaseFrame(frameOut);

    jb.Flush();

    // printf("DONE fill JB - nr of delta + key frames (w/ wrap in seqNum) >
    // max nr of frames\n");

    // Testing that 1 empty packet inserted last will not be set for decoding
    seqNum = 3;
    // Insert one empty packet per frame, should never return the last timestamp
    // inserted. Only return empty frames in the presence of subsequent frames.
    int maxSize = 1000;
    for (int i = 0; i < maxSize + 10; i++)
    {
        timeStamp += 33 * 90;
        seqNum++;
        packet.isFirstPacket = false;
        packet.markerBit = false;
        packet.seqNum = seqNum;
        packet.timestamp = timeStamp;
        packet.frameType = kFrameEmpty;
        VCMEncodedFrame* testFrame = jb.GetFrameForDecoding();
        // timestamp should bever be the last TS inserted
        if (testFrame != NULL)
        {
            TEST(testFrame->TimeStamp() < timeStamp);
            printf("Not null TS = %d\n",testFrame->TimeStamp());
        }
    }

    jb.Flush();


    // printf(DONE testing inserting empty packets to the JB)


    // H.264 tests
    // Test incomplete NALU frames

    jb.Flush();
    jb.SetNackMode(kNoNack, -1, -1);
    seqNum ++;
    timeStamp += 33 * 90;
    int insertedLength = 0;
    packet.seqNum = seqNum;
    packet.timestamp = timeStamp;
    packet.frameType = kVideoFrameKey;
    packet.isFirstPacket = true;
    packet.completeNALU = kNaluStart;
    packet.markerBit = false;

    frameIn = jb.GetFrame(packet);

     // Insert a packet into a frame
    TEST(kFirstPacket == jb.InsertPacket(frameIn, packet));

    seqNum += 2; // Skip one packet
    packet.seqNum = seqNum;
    packet.frameType = kVideoFrameKey;
    packet.isFirstPacket = false;
    packet.completeNALU = kNaluIncomplete;
    packet.markerBit = false;

     // Insert a packet into a frame
    TEST(kIncomplete == jb.InsertPacket(frameIn, packet));

    seqNum++;
    packet.seqNum = seqNum;
    packet.frameType = kVideoFrameKey;
    packet.isFirstPacket = false;
    packet.completeNALU = kNaluEnd;
    packet.markerBit = false;

    // Insert a packet into a frame
    TEST(kIncomplete == jb.InsertPacket(frameIn, packet));

    seqNum++;
    packet.seqNum = seqNum;
    packet.completeNALU = kNaluComplete;
    packet.markerBit = true; // Last packet
    TEST(kIncomplete == jb.InsertPacket(frameIn, packet));


    // The JB will only output (incomplete) frames if a packet belonging to a
    // subsequent frame was already inserted. Insert one packet of a subsequent
    // frame. place high timestamp so the JB would always have a next frame
    // (otherwise, for every inserted frame we need to take care of the next
    // frame as well).
    packet.seqNum = 1;
    packet.timestamp = timeStamp + 33 * 90 * 10;
    packet.frameType = kVideoFrameDelta;
    packet.isFirstPacket = false;
    packet.completeNALU = kNaluStart;
    packet.markerBit = false;
    frameIn = jb.GetFrame(packet);
    TEST(kFirstPacket == jb.InsertPacket(frameIn, packet));

    // Get packet notification
    TEST(timeStamp == jb.GetNextTimeStamp(10, incomingFrameType, renderTimeMs));
    frameOut = jb.GetFrameForDecoding();

    // We can decode everything from a NALU until a packet has been lost.
    // Thus we can decode the first packet of the first NALU and the second NALU
    // which consists of one packet.
    TEST(CheckOutFrame(frameOut, packet.sizeBytes * 2, false) == 0);
    jb.ReleaseFrame(frameOut);

    // Test reordered start frame + 1 lost
    seqNum += 2; // Reoreder 1 frame
    timeStamp += 33*90;
    insertedLength = 0;

    packet.seqNum = seqNum;
    packet.timestamp = timeStamp;
    packet.frameType = kVideoFrameKey;
    packet.isFirstPacket = false;
    packet.completeNALU = kNaluEnd;
    packet.markerBit = false;

    TEST(frameIn = jb.GetFrame(packet));
    TEST(kFirstPacket == jb.InsertPacket(frameIn, packet));
    insertedLength += packet.sizeBytes; // This packet should be decoded

    seqNum--;
    packet.seqNum = seqNum;
    packet.timestamp = timeStamp;
    packet.frameType = kVideoFrameKey;
    packet.isFirstPacket = true;
    packet.completeNALU = kNaluStart;
    packet.markerBit = false;
    TEST(kIncomplete == jb.InsertPacket(frameIn, packet));
    insertedLength += packet.sizeBytes; // This packet should be decoded

    seqNum += 3; // One packet drop
    packet.seqNum = seqNum;
    packet.timestamp = timeStamp;
    packet.frameType = kVideoFrameKey;
    packet.isFirstPacket = false;
    packet.completeNALU = kNaluComplete;
    packet.markerBit = false;
    TEST(kIncomplete == jb.InsertPacket(frameIn, packet));
    insertedLength += packet.sizeBytes; // This packet should be decoded

    seqNum += 1;
    packet.seqNum = seqNum;
    packet.timestamp = timeStamp;
    packet.frameType = kVideoFrameKey;
    packet.isFirstPacket = false;
    packet.completeNALU = kNaluStart;
    packet.markerBit = false;
    TEST(kIncomplete == jb.InsertPacket(frameIn, packet));
    // This packet should be decoded since it's the beginning of a NAL
    insertedLength += packet.sizeBytes;

    seqNum += 2;
    packet.seqNum = seqNum;
    packet.timestamp = timeStamp;
    packet.frameType = kVideoFrameKey;
    packet.isFirstPacket = false;
    packet.completeNALU = kNaluEnd;
    packet.markerBit = true;
    TEST(kIncomplete == jb.InsertPacket(frameIn, packet));
    // This packet should not be decoded because it is an incomplete NAL if it
    // is the last

    frameOut = jb.GetFrameForDecoding();
    // Only last NALU is complete
    TEST(CheckOutFrame(frameOut, insertedLength, false) == 0);
    jb.ReleaseFrame(frameOut);


    // Test to insert empty packet
    seqNum += 1;
    timeStamp += 33 * 90;
    VCMPacket emptypacket(data, 0, seqNum, timeStamp, true);
    emptypacket.seqNum = seqNum;
    emptypacket.timestamp = timeStamp;
    emptypacket.frameType = kVideoFrameKey;
    emptypacket.isFirstPacket = true;
    emptypacket.completeNALU = kNaluComplete;
    emptypacket.markerBit = true;
    TEST(frameIn = jb.GetFrame(emptypacket));
    TEST(kFirstPacket == jb.InsertPacket(frameIn, emptypacket));
    // This packet should not be decoded because it is an incomplete NAL if it
    // is the last
    insertedLength += 0;

    // Will be sent to the decoder, as a packet belonging to a subsequent frame
    // has arrived.
    frameOut = jb.GetFrameForDecoding();


    // Test that a frame can include an empty packet.
    seqNum += 1;
    timeStamp += 33 * 90;

    packet.seqNum = seqNum;
    packet.timestamp = timeStamp;
    packet.frameType = kVideoFrameKey;
    packet.isFirstPacket = true;
    packet.completeNALU = kNaluComplete;
    packet.markerBit = false;
    TEST(frameIn = jb.GetFrame(packet));
    TEST(kFirstPacket == jb.InsertPacket(frameIn, packet));

    seqNum += 1;
    emptypacket.seqNum = seqNum;
    emptypacket.timestamp = timeStamp;
    emptypacket.frameType = kVideoFrameKey;
    emptypacket.isFirstPacket = true;
    emptypacket.completeNALU = kNaluComplete;
    emptypacket.markerBit = true;
    TEST(kCompleteSession == jb.InsertPacket(frameIn, emptypacket));

    // get the frame
    frameOut = jb.GetCompleteFrameForDecoding(10);
    // Only last NALU is complete
    TEST(CheckOutFrame(frameOut, packet.sizeBytes, false) == 0);

    jb.ReleaseFrame(frameOut);

    jb.Flush();

    // Test that a we cannot get incomplete frames from the JB if we haven't
    // received the marker bit, unless we have received a packet from a later
    // timestamp.

    packet.seqNum += 2;
    packet.frameType = kVideoFrameDelta;
    packet.isFirstPacket = false;
    packet.markerBit = false;

    TEST(frameIn = jb.GetFrame(packet));
    TEST(kFirstPacket == jb.InsertPacket(frameIn, packet));

    frameOut = jb.GetFrameForDecoding();
    TEST(frameOut == NULL);

    packet.seqNum += 2;
    packet.timestamp += 33 * 90;

    TEST(frameIn = jb.GetFrame(packet));
    TEST(kFirstPacket == jb.InsertPacket(frameIn, packet));

    frameOut = jb.GetFrameForDecoding();

    TEST(frameOut != NULL);
    TEST(CheckOutFrame(frameOut, packet.sizeBytes, false) == 0);
    jb.ReleaseFrame(frameOut);

    jb.Stop();

    printf("DONE !!!\n");

    printf("\nVCM Jitter Buffer Test: \n\n%i tests completed\n",
           vcmMacrosTests);
    if (vcmMacrosErrors > 0)
    {
        printf("%i FAILED\n\n", vcmMacrosErrors);
    }
    else
    {
        printf("ALL PASSED\n\n");
    }

    return 0;

}
