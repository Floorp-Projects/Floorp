/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <windows.h>

#include <assert.h>
#include <tchar.h>

#include <iostream>

#include "webrtc/common_types.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_rtcp.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_utility.h"
#include "webrtc/modules/rtp_rtcp/source/tmmbr_help.h"

#define TEST_STR "Test TMMBR."
#define TEST_PASSED() std::cerr << TEST_STR << " : [OK]" << std::endl
#define PRINT_LINE std::cout << "------------------------------------------" << std::endl;


const int maxFileLen = 200;
uint8_t* dataFile[maxFileLen];


struct InputSet
{
    uint32_t TMMBR;
    uint32_t packetOH;
    uint32_t SSRC;
};

const InputSet set0   = {220,  80, 11111};  // bitRate, packetOH, ssrc
const InputSet set1   = {180,  90, 22222};
const InputSet set2   = {100, 210, 33333};
const InputSet set3   = { 35,  40, 44444};
const InputSet set4   = { 40,  60, 55555};
const InputSet set4_1 = {100,  60, 55555};
const InputSet set4_2 = { 10,  60, 55555};
const InputSet set5   = {200,  40, 66666};
const InputSet set00  = {  0,  40, 66666};

const int maxBitrate = 230;  // if this is lower than max in the list above test should fail

void Verify(TMMBRSet* boundingSet, int index, InputSet set)
{
    assert(boundingSet->ptrTmmbrSet[index]    == set.TMMBR);
    assert(boundingSet->ptrPacketOHSet[index] == set.packetOH);
    assert(boundingSet->ptrSsrcSet[index]     == set.SSRC);
};

int ParseRTCPPacket(const void *data, int len, TMMBRSet*& boundingSet)
{
    int numItems = -1;
    RTCPUtility::RTCPParserV2 rtcpParser((const uint8_t*)data, len, true);
    RTCPUtility::RTCPPacketTypes pktType = rtcpParser.Begin();
    while (pktType != RTCPUtility::kRtcpNotValidCode)
    {
        const RTCPUtility::RTCPPacket& rtcpPacket = rtcpParser.Packet();
        if (pktType == RTCPUtility::kRtcpRtpfbTmmbnCode)
        {
            assert(0 == rtcpPacket.TMMBN.SenderSSRC);
            assert(0 == rtcpPacket.TMMBN.MediaSSRC);
            numItems = 0;
        }
        if (pktType == RTCPUtility::kRtcpRtpfbTmmbnItemCode)
        {
            boundingSet->ptrTmmbrSet[numItems]    = rtcpPacket.TMMBNItem.MaxTotalMediaBitRate;
            boundingSet->ptrPacketOHSet[numItems] = rtcpPacket.TMMBNItem.MeasuredOverhead;
            boundingSet->ptrSsrcSet[numItems]     = rtcpPacket.TMMBNItem.SSRC;
            ++numItems;
        }
        pktType = rtcpParser.Iterate();
    }
    return numItems;
};

int32_t GetFile(char* fileName)
{
    if (!fileName[0])
    {
        return 0;
    }

    FILE* openFile = fopen(fileName, "rb");
    assert(openFile != NULL);
    fseek(openFile, 0, SEEK_END);
    int len = (int16_t)(ftell(openFile));
    rewind(openFile);
    assert(len > 0 && len < maxFileLen);
    fread(dataFile, 1, len, openFile);
    fclose(openFile);
    return len;
};


class LoopBackTransport2 : public webrtc::Transport, private TMMBRHelp
{
public:
    LoopBackTransport2(RtpRtcp* rtpRtcpModule)  :
      TMMBRHelp(false),
      _rtpRtcpModule(rtpRtcpModule),
      _cnt(0)
    {
    }
    virtual int SendPacket(int channel, const void *data, int len)
    {
        if( 0  == _rtpRtcpModule->IncomingPacket((const uint8_t*)data, len))
        {
            return len;
        }
        return -1;
    }
    virtual int SendRTCPPacket(int channel, const void *data, int len)
    {
        char fileName[256] = {0};
        TMMBRSet* boundingSet = BoundingSet();
        boundingSet->VerifyAndAllocateSet(3);

        if (_cnt == 0)
        {
            // TMMBN {}
            // TMMBN {}
            // TMMBN {}
            // TMMBN {2,4,0} -> {4,2}
            assert(2 == ParseRTCPPacket(data, len, boundingSet));
            Verify(boundingSet, 0, set4);
            Verify(boundingSet, 1, set2);

            strcpy(fileName, "RTCPPacketTMMBR3.bin");
        }

        ++_cnt;

        // Get stored rtcp packet w/ TMMBR
        len = GetFile(fileName);
        if (len == 0)
        {
            return 1;
        }

        // Send in bitrate request
        if(_rtpRtcpModule->IncomingPacket((const uint8_t*)dataFile, len) == 0)
        {
            return len;
        }
        return -1;
    }
    RtpRtcp* _rtpRtcpModule;
    uint32_t       _cnt;
};


class LoopBackTransportVideo : public webrtc::Transport, private TMMBRHelp
{
public:
    LoopBackTransportVideo(RtpRtcp* rtpRtcpModule)  :
      TMMBRHelp(false),
      _rtpRtcpModule(rtpRtcpModule),
      _cnt(0)
    {
    }
    virtual int SendPacket(int channel, const void *data, int len)
    {
        if(_rtpRtcpModule->IncomingPacket((const uint8_t*)data, len)== 0)
        {
            return len;
        }
        return -1;
    }
    virtual int SendRTCPPacket(int channel, const void *data, int len)
    {
        char fileName[256] = {0};
        TMMBRSet* boundingSet = BoundingSet();
        boundingSet->VerifyAndAllocateSet(3);

        if (_cnt == 0)
        {
            strcpy(fileName, "RTCPPacketTMMBR0.bin");
        }
        else if (_cnt == 1)
        {
            // TMMBN {0} -> {0}
            assert(1 == ParseRTCPPacket(data, len, boundingSet));
            Verify(boundingSet, 0, set0);

            strcpy(fileName, "RTCPPacketTMMBR1.bin");
        }
        else if (_cnt == 2)
        {
            // TMMBN {0,1} -> {1}
            assert(1 == ParseRTCPPacket(data, len, boundingSet));
            Verify(boundingSet, 0, set1);

            strcpy(fileName, "RTCPPacketTMMBR2.bin");
        }
        else if (_cnt == 3)
        {
            // TMMBN {0,1,2} -> {2}
            assert(1 == ParseRTCPPacket(data, len, boundingSet));
            Verify(boundingSet, 0, set2);

            strcpy(fileName, "RTCPPacketTMMBR3.bin");
        }
        else if (_cnt == 4)
        {
            // TMMBN {0,1,2,3} -> {3,2}
            assert(2 == ParseRTCPPacket(data, len, boundingSet));
            Verify(boundingSet, 0, set3);
            Verify(boundingSet, 1, set2);

            strcpy(fileName, "RTCPPacketTMMBR4.bin");
        }
        else if (_cnt == 5)
        {
            // TMMBN {0,1,2,3,4} -> {3,4,2}
            assert(3 == ParseRTCPPacket(data, len, boundingSet));
            Verify(boundingSet, 0, set3);
            Verify(boundingSet, 1, set4);
            Verify(boundingSet, 2, set2);

            strcpy(fileName, "RTCPPacketTMMBR5.bin");
        }
        else if (_cnt == 6)
        {
            // TMMBN {0,1,2,3,4,5} -> {3,4,2}
            assert(3 == ParseRTCPPacket(data, len, boundingSet));
            Verify(boundingSet, 0, set3);
            Verify(boundingSet, 1, set4);
            Verify(boundingSet, 2, set2);

            strcpy(fileName, "RTCPPacketTMMBR4_2.bin");
        }
        else if (_cnt == 7)
        {
            // TMMBN {0,1,2,3,4_2,5} -> {4_2}
            assert(1 == ParseRTCPPacket(data, len, boundingSet));
            Verify(boundingSet, 0, set4_2);

            ++_cnt;
            ::Sleep(5*RTCP_INTERVAL_AUDIO_MS + 1000); // time out receiver
            _rtpRtcpModule->Process();             // SendRTCP() (_cnt == 8)
                                                   // a receiver has timed out -> UpdateTMMBR()
        }
        else if (_cnt == 8)
        {
            // No TMMBN in this packet
            assert(-1 == ParseRTCPPacket(data, len, boundingSet));
        }
        else if (_cnt == 10)
        {
            // TMMBN {} -> {}, empty set
            assert(0 == ParseRTCPPacket(data, len, boundingSet));

            strcpy(fileName, "RTCPPacketTMMBR2.bin");
        }
        else if (_cnt == 11)
        {
            // TMMBN {2} -> {2}
            assert(1 == ParseRTCPPacket(data, len, boundingSet));
            Verify(boundingSet, 0, set2);
        }
        else if (_cnt == 12) // ----- multi module -------------
        {
            // No TMMBN in this packet
            assert(-1 == ParseRTCPPacket(data, len, boundingSet));

            strcpy(fileName, "RTCPPacketTMMBR4.bin");
        }
        else if (_cnt == 13)
        {
            // TMMBN {}
            // TMMBN {}
            // TMMBN {}
            // TMMBN {2,4} -> {4,2}
            assert(2 == ParseRTCPPacket(data, len, boundingSet));
            Verify(boundingSet, 0, set4);
            Verify(boundingSet, 1, set2);

            strcpy(fileName, "RTCPPacketTMMBR0.bin");
        }
        else if (_cnt == 14)
        {
            // TMMBN {}
            // TMMBN {3}
            // TMMBN {}
            // TMMBN {2,4,0} -> {3,4,2}
            assert(3 == ParseRTCPPacket(data, len, boundingSet));
            Verify(boundingSet, 0, set3);
            Verify(boundingSet, 1, set4);
            Verify(boundingSet, 2, set2);

            strcpy(fileName, "RTCPPacketTMMBR1.bin");
        }
        //else if (_cnt == 15)
        //{
        //    // TMMBN {}
        //    // TMMBN {}
        //    // TMMBN {}
        //    // TMMBN {2,4,0,1} -> {4,2}
        //    //assert(2 == ParseRTCPPacket(data, len, boundingSet));
        //    //Verify(boundingSet, 0, set4);
        //    //Verify(boundingSet, 1, set2);
        //}
        //else if (_cnt == 15)
        //{
        //    // No TMMBN in this packet
        //    assert(-1 == ParseRTCPPacket(data, len, boundingSet));
        //}
        else if (_cnt == 15)
        {
            // TMMBN {}
            // TMMBN {}
            // TMMBN {}
            // TMMBN {} -> {}, empty set
            assert(0 == ParseRTCPPacket(data, len, boundingSet));
        }

        ++_cnt;

        // Get stored rtcp packet w/ TMMBR
        len = GetFile(fileName);
        if (len == 0)
        {
            return 1;
        }

        // Send in bitrate request
        if( 0 == _rtpRtcpModule->IncomingPacket((const uint8_t*)dataFile, len))
        {
            return len;
        }
        return -1;
    }

    RtpRtcp* _rtpRtcpModule;
    uint32_t       _cnt;
};

class TestTMMBR : private TMMBRHelp
{
public:
    TestTMMBR() : TMMBRHelp(false) {};

    void Add(TMMBRSet* candidateSet, int index, InputSet set)
    {
        candidateSet->ptrTmmbrSet[index]    = set.TMMBR;
        candidateSet->ptrPacketOHSet[index] = set.packetOH;
        candidateSet->ptrSsrcSet[index]     = set.SSRC;
    };

    void Start()
    {
        // Get sets
        TMMBRSet* candidateSet = CandidateSet();
        assert(0 == candidateSet->sizeOfSet);
        TMMBRSet* boundingSet = BoundingSet();
        assert(0 == boundingSet->sizeOfSet);
        TMMBRSet* boundingSetToSend = BoundingSetToSend();
        assert(0 == boundingSetToSend->sizeOfSet);

        int32_t numBoundingSet = FindTMMBRBoundingSet(boundingSet);
        assert(0 == numBoundingSet); // should be empty

        assert( 0 == SetTMMBRBoundingSetToSend(NULL,0));        // ok to send empty set
        assert( 0 == SetTMMBRBoundingSetToSend(boundingSet,0)); // ok to send empty set

        uint32_t minBitrateKbit = 0;
        uint32_t maxBitrateKbit = 0;
        assert(-1 == CalcMinMaxBitRate(0, 0, 1, false, minBitrateKbit, maxBitrateKbit)); // no bounding set

        // ---------------------------------
        // Test candidate set {0} -> {0}
        // ---------------------------------
        candidateSet = VerifyAndAllocateCandidateSet(1);
        assert(1 == candidateSet->sizeOfSet);
        Add(candidateSet, 0, set0);

        // Find bounding set
        numBoundingSet = FindTMMBRBoundingSet(boundingSet);
        assert(1 == numBoundingSet);
        Verify(boundingSet, 0, set0);

        // Is owner of set
        assert(!IsOwner(set0.SSRC, 0));   // incorrect length
        assert(!IsOwner(set1.SSRC, 100)); // incorrect length

        assert( IsOwner(set0.SSRC, numBoundingSet));
        assert(!IsOwner(set1.SSRC, numBoundingSet));
        assert(!IsOwner(set2.SSRC, numBoundingSet));

        // Set boundingSet to send
        assert(0 == SetTMMBRBoundingSetToSend(boundingSet, maxBitrate));

        // Get boundingSet to send
        boundingSetToSend = BoundingSetToSend();
        assert(boundingSetToSend->sizeOfSet == numBoundingSet);
        Verify(boundingSetToSend, 0, set0);

        // Get net bitrate depending on packet rate
        assert( 0 == CalcMinMaxBitRate(0, numBoundingSet, false,0, minBitrateKbit, maxBitrateKbit));
        assert(set0.TMMBR == minBitrateKbit);
        assert(set0.TMMBR == maxBitrateKbit);
        assert(0 == CalcMinMaxBitRate(0, 100, false,0, minBitrateKbit, maxBitrateKbit));  // incorrect length
        assert(set0.TMMBR == minBitrateKbit);
        assert(set0.TMMBR == maxBitrateKbit);

        // ---------------------------------
        // Test candidate set {0,1} -> {1}
        // ---------------------------------
        candidateSet = VerifyAndAllocateCandidateSet(2);
        assert(2 == candidateSet->sizeOfSet);
        Add(candidateSet, 0, set0);
        Add(candidateSet, 1, set1);

        // Find bounding set
        numBoundingSet = FindTMMBRBoundingSet(boundingSet);
        assert(1 == numBoundingSet);
        Verify(boundingSet, 0, set1);

        // Is owner of set
        assert(!IsOwner(set0.SSRC, numBoundingSet));
        assert( IsOwner(set1.SSRC, numBoundingSet));
        assert(!IsOwner(set2.SSRC, numBoundingSet));

        // Set boundingSet to send
        assert(0 == SetTMMBRBoundingSetToSend(boundingSet, maxBitrate));

        // Get boundingSet to send
        boundingSetToSend = BoundingSetToSend();
        assert(boundingSetToSend->sizeOfSet == numBoundingSet);
        Verify(boundingSetToSend, 0, set1);

        // Get net bitrate depending on packet rate
        assert(0 == CalcMinMaxBitRate(0, numBoundingSet, true,0, minBitrateKbit, maxBitrateKbit));
        assert(set1.TMMBR == minBitrateKbit);
        assert(set0.TMMBR == maxBitrateKbit);

        // ---------------------------------
        // Test candidate set {0,1,2} -> {2}
        // ---------------------------------
        candidateSet = VerifyAndAllocateCandidateSet(3);
        assert(3 == candidateSet->sizeOfSet);
        Add(candidateSet, 0, set0);
        Add(candidateSet, 1, set1);
        Add(candidateSet, 2, set2);

        // Find bounding set
        numBoundingSet = FindTMMBRBoundingSet(boundingSet);
        assert(1 == numBoundingSet);
        Verify(boundingSet, 0, set2);

        // Is owner of set
        assert(!IsOwner(set0.SSRC, numBoundingSet));
        assert(!IsOwner(set1.SSRC, numBoundingSet));
        assert( IsOwner(set2.SSRC, numBoundingSet));

        // Set boundingSet to send
        assert(0 == SetTMMBRBoundingSetToSend(boundingSet, maxBitrate));

        // Get boundingSet to send
        boundingSetToSend = BoundingSetToSend();
        assert(boundingSetToSend->sizeOfSet == numBoundingSet);
        Verify(boundingSetToSend, 0, set2);

        // Get net bitrate depending on packet rate
        assert(0 == CalcMinMaxBitRate(0, numBoundingSet, true,0, minBitrateKbit, maxBitrateKbit));
        assert(set2.TMMBR == minBitrateKbit);
        assert(set0.TMMBR == maxBitrateKbit);

        // ---------------------------------
        // Test candidate set {0,1,2,3} -> {3,2}
        // ---------------------------------
        candidateSet = VerifyAndAllocateCandidateSet(4);
        assert(4 == candidateSet->sizeOfSet);
        Add(candidateSet, 0, set0);
        Add(candidateSet, 1, set1);
        Add(candidateSet, 2, set2);
        Add(candidateSet, 3, set3);

        // Find bounding set
        numBoundingSet = FindTMMBRBoundingSet(boundingSet);
        assert(2 == numBoundingSet);
        Verify(boundingSet, 0, set3);
        Verify(boundingSet, 1, set2);

        // Is owner of set
        assert(!IsOwner(set0.SSRC, numBoundingSet));
        assert(!IsOwner(set1.SSRC, numBoundingSet));
        assert( IsOwner(set2.SSRC, numBoundingSet));
        assert( IsOwner(set3.SSRC, numBoundingSet));

        // Set boundingSet to send
        assert(0 == SetTMMBRBoundingSetToSend(boundingSet, maxBitrate));

        // Get boundingSet to send
        boundingSetToSend = BoundingSetToSend();
        assert(boundingSetToSend->sizeOfSet == numBoundingSet);
        Verify(boundingSetToSend, 0, set3);
        Verify(boundingSetToSend, 1, set2);

        // Get net bitrate depending on packet rate
        assert(0 == CalcMinMaxBitRate(0, numBoundingSet, true,0, minBitrateKbit, maxBitrateKbit));
        assert(set3.TMMBR == minBitrateKbit);
        assert(set0.TMMBR == maxBitrateKbit);

        // ---------------------------------
        // Test candidate set {0,1,2,3,4} -> {3,4,2}
        // ---------------------------------
        candidateSet = VerifyAndAllocateCandidateSet(5);
        assert(5 == candidateSet->sizeOfSet);
        Add(candidateSet, 0, set0);
        Add(candidateSet, 1, set1);
        Add(candidateSet, 2, set2);
        Add(candidateSet, 3, set3);
        Add(candidateSet, 4, set4);

        // Find bounding set
        numBoundingSet = FindTMMBRBoundingSet(boundingSet);
        assert(3 == numBoundingSet);
        Verify(boundingSet, 0, set3);
        Verify(boundingSet, 1, set4);
        Verify(boundingSet, 2, set2);

        // Is owner of set
        assert(!IsOwner(set0.SSRC, numBoundingSet));
        assert(!IsOwner(set1.SSRC, numBoundingSet));
        assert( IsOwner(set2.SSRC, numBoundingSet));
        assert( IsOwner(set3.SSRC, numBoundingSet));
        assert( IsOwner(set4.SSRC, numBoundingSet));

        // Set boundingSet to send
        assert(0 == SetTMMBRBoundingSetToSend(boundingSet, maxBitrate));

        // Get boundingSet to send
        boundingSetToSend = BoundingSetToSend();
        assert(boundingSetToSend->sizeOfSet == numBoundingSet);
        Verify(boundingSetToSend, 0, set3);
        Verify(boundingSetToSend, 1, set4);
        Verify(boundingSetToSend, 2, set2);

        // Get net bitrate depending on packet rate
        assert(0 == CalcMinMaxBitRate(0,numBoundingSet, true,0, minBitrateKbit, maxBitrateKbit));
        assert(set3.TMMBR == minBitrateKbit);
        assert(set0.TMMBR == maxBitrateKbit);

        // ---------------------------------
        // Test candidate set {0,1,2,3,4,5} -> {3,4,2}
        // ---------------------------------
        candidateSet = VerifyAndAllocateCandidateSet(6);
        assert(6 == candidateSet->sizeOfSet);
        Add(candidateSet, 0, set0);
        Add(candidateSet, 1, set1);
        Add(candidateSet, 2, set2);
        Add(candidateSet, 3, set3);
        Add(candidateSet, 4, set4);
        Add(candidateSet, 5, set5);

        // Find bounding set
        numBoundingSet = FindTMMBRBoundingSet(boundingSet);
        assert(3 == numBoundingSet);
        Verify(boundingSet, 0, set3);
        Verify(boundingSet, 1, set4);
        Verify(boundingSet, 2, set2);

        // Is owner of set
        assert(!IsOwner(set0.SSRC, numBoundingSet));
        assert(!IsOwner(set1.SSRC, numBoundingSet));
        assert( IsOwner(set2.SSRC, numBoundingSet));
        assert( IsOwner(set3.SSRC, numBoundingSet));
        assert( IsOwner(set4.SSRC, numBoundingSet));
        assert(!IsOwner(set5.SSRC, numBoundingSet));

        // Set boundingSet to send
        assert(0 == SetTMMBRBoundingSetToSend(boundingSet, maxBitrate));

        // Get boundingSet to send
        boundingSetToSend = BoundingSetToSend();
        assert(boundingSetToSend->sizeOfSet == numBoundingSet);
        Verify(boundingSetToSend, 0, set3);
        Verify(boundingSetToSend, 1, set4);
        Verify(boundingSetToSend, 2, set2);

        // Get net bitrate depending on packet rate
        assert(0 == CalcMinMaxBitRate(0,numBoundingSet, true,0, minBitrateKbit, maxBitrateKbit));
        assert(set3.TMMBR == minBitrateKbit);
        assert(set0.TMMBR == maxBitrateKbit);


        // ---------------------------------
        // Test candidate set {1,2,3,4,5} -> {3,4,2}
        // ---------------------------------
        candidateSet = VerifyAndAllocateCandidateSet(5);
        assert(6 == candidateSet->sizeOfSet);
        Add(candidateSet, 0, set1);
        Add(candidateSet, 1, set2);
        Add(candidateSet, 2, set3);
        Add(candidateSet, 3, set4);
        Add(candidateSet, 4, set5);

        // Find bounding set
        numBoundingSet = FindTMMBRBoundingSet(boundingSet);
        assert(3 == numBoundingSet);
        Verify(boundingSet, 0, set3);
        Verify(boundingSet, 1, set4);
        Verify(boundingSet, 2, set2);

        // Is owner of set
        assert(!IsOwner(set0.SSRC, numBoundingSet));
        assert(!IsOwner(set1.SSRC, numBoundingSet));
        assert( IsOwner(set2.SSRC, numBoundingSet));
        assert( IsOwner(set3.SSRC, numBoundingSet));
        assert( IsOwner(set4.SSRC, numBoundingSet));
        assert(!IsOwner(set5.SSRC, numBoundingSet));

        // Set boundingSet to send
        assert(0 == SetTMMBRBoundingSetToSend(boundingSet, maxBitrate));

        // Get boundingSet to send
        boundingSetToSend = BoundingSetToSend();
        assert(boundingSetToSend->sizeOfSet == numBoundingSet);
        Verify(boundingSetToSend, 0, set3);
        Verify(boundingSetToSend, 1, set4);
        Verify(boundingSetToSend, 2, set2);

        // Get net bitrate depending on packet rate
        assert(0 == CalcMinMaxBitRate(0,numBoundingSet, true,0, minBitrateKbit, maxBitrateKbit));
        assert(set3.TMMBR == minBitrateKbit);
        assert(set5.TMMBR == maxBitrateKbit);


        // ---------------------------------
        // Test candidate set {1,3,4,5} -> {3,4}
        // ---------------------------------
        candidateSet = VerifyAndAllocateCandidateSet(4);
        assert(6 == candidateSet->sizeOfSet);
        Add(candidateSet, 0, set1);
        Add(candidateSet, 1, set3);
        Add(candidateSet, 2, set4);
        Add(candidateSet, 3, set5);

        // Find bounding set
        numBoundingSet = FindTMMBRBoundingSet(boundingSet);
        assert(2 == numBoundingSet);
        Verify(boundingSet, 0, set3);
        Verify(boundingSet, 1, set4);

        // Is owner of set
        assert(!IsOwner(set0.SSRC, numBoundingSet));
        assert(!IsOwner(set1.SSRC, numBoundingSet));
        assert(!IsOwner(set2.SSRC, numBoundingSet));
        assert( IsOwner(set3.SSRC, numBoundingSet));
        assert( IsOwner(set4.SSRC, numBoundingSet));
        assert(!IsOwner(set5.SSRC, numBoundingSet));

        // Set boundingSet to send
        assert(0 == SetTMMBRBoundingSetToSend(boundingSet, maxBitrate));

        // Get boundingSet to send
        boundingSetToSend = BoundingSetToSend();
        Verify(boundingSetToSend, 0, set3);
        Verify(boundingSetToSend, 1, set4);

        // Get net bitrate depending on packet rate
        assert(0 == CalcMinMaxBitRate(0, numBoundingSet,true,0,  minBitrateKbit, maxBitrateKbit));
        assert(set3.TMMBR == minBitrateKbit);
        assert(set5.TMMBR == maxBitrateKbit);

        // ---------------------------------
        // Test candidate set {1,2,4,5} -> {4,2}
        // ---------------------------------
        candidateSet = VerifyAndAllocateCandidateSet(4);
        assert(6 == candidateSet->sizeOfSet);
        Add(candidateSet, 0, set1);
        Add(candidateSet, 1, set2);
        Add(candidateSet, 2, set4);
        Add(candidateSet, 3, set5);

        // Find bounding set
        numBoundingSet = FindTMMBRBoundingSet(boundingSet);
        assert(2 == numBoundingSet);
        Verify(boundingSet, 0, set4);
        Verify(boundingSet, 1, set2);

        // Is owner of set
        assert(!IsOwner(set0.SSRC, numBoundingSet));
        assert(!IsOwner(set1.SSRC, numBoundingSet));
        assert( IsOwner(set2.SSRC, numBoundingSet));
        assert(!IsOwner(set3.SSRC, numBoundingSet));
        assert( IsOwner(set4.SSRC, numBoundingSet));
        assert(!IsOwner(set5.SSRC, numBoundingSet));

        // Set boundingSet to send
        assert(0 == SetTMMBRBoundingSetToSend(boundingSet, maxBitrate));

        // Get boundingSet to send
        boundingSetToSend = BoundingSetToSend();
        Verify(boundingSetToSend, 0, set4);
        Verify(boundingSetToSend, 1, set2);

        // Get net bitrate depending on packet rate
        assert(0 == CalcMinMaxBitRate(0, numBoundingSet, true,0, minBitrateKbit, maxBitrateKbit));
        assert(set4.TMMBR == minBitrateKbit);
        assert(set5.TMMBR == maxBitrateKbit);

        // ---------------------------------
        // Test candidate set {1,2,3,5} -> {3,2}
        // ---------------------------------
        candidateSet = VerifyAndAllocateCandidateSet(4);
        assert(6 == candidateSet->sizeOfSet);
        Add(candidateSet, 0, set1);
        Add(candidateSet, 1, set2);
        Add(candidateSet, 2, set3);
        Add(candidateSet, 3, set5);

        // Find bounding set
        numBoundingSet = FindTMMBRBoundingSet(boundingSet);
        assert(2 == numBoundingSet);
        Verify(boundingSet, 0, set3);
        Verify(boundingSet, 1, set2);

        // Is owner of set
        assert(!IsOwner(set0.SSRC, numBoundingSet));
        assert(!IsOwner(set1.SSRC, numBoundingSet));
        assert( IsOwner(set2.SSRC, numBoundingSet));
        assert( IsOwner(set3.SSRC, numBoundingSet));
        assert(!IsOwner(set4.SSRC, numBoundingSet));
        assert(!IsOwner(set5.SSRC, numBoundingSet));

        // Set boundingSet to send
        assert(0 == SetTMMBRBoundingSetToSend(boundingSet, maxBitrate));

        // Get boundingSet to send
        boundingSetToSend = BoundingSetToSend();
        Verify(boundingSetToSend, 0, set3);
        Verify(boundingSetToSend, 1, set2);

        // Get net bitrate depending on packet rate
        assert(0 == CalcMinMaxBitRate(0, numBoundingSet, true,0, minBitrateKbit, maxBitrateKbit));
        assert(set3.TMMBR == minBitrateKbit);
        assert(set5.TMMBR == maxBitrateKbit);

        // ---------------------------------
        // Test candidate set {1,2,3,4_1,5} -> {3,2}
        // ---------------------------------
        candidateSet = VerifyAndAllocateCandidateSet(5);
        assert(6 == candidateSet->sizeOfSet);
        Add(candidateSet, 0, set1);
        Add(candidateSet, 1, set2);
        Add(candidateSet, 2, set3);
        Add(candidateSet, 3, set4_1);
        Add(candidateSet, 4, set5);

        // Find bounding set
        numBoundingSet = FindTMMBRBoundingSet(boundingSet);
        assert(2 == numBoundingSet);
        Verify(boundingSet, 0, set3);
        Verify(boundingSet, 1, set2);

        // Is owner of set
        assert(!IsOwner(set0.SSRC, numBoundingSet));
        assert(!IsOwner(set1.SSRC, numBoundingSet));
        assert( IsOwner(set2.SSRC, numBoundingSet));
        assert( IsOwner(set3.SSRC, numBoundingSet));
        assert(!IsOwner(set4.SSRC, numBoundingSet));
        assert(!IsOwner(set5.SSRC, numBoundingSet));

        // Set boundingSet to send
        assert(0 == SetTMMBRBoundingSetToSend(boundingSet, maxBitrate));

        // Get boundingSet to send
        boundingSetToSend = BoundingSetToSend();
        Verify(boundingSetToSend, 0, set3);
        Verify(boundingSetToSend, 1, set2);

        // Get net bitrate depending on packet rate
        assert(0 == CalcMinMaxBitRate(0, numBoundingSet, true,0, minBitrateKbit, maxBitrateKbit));
        assert(set3.TMMBR == minBitrateKbit);
        assert(set5.TMMBR == maxBitrateKbit);

        // ---------------------------------
        // Test candidate set {1,2,3,4_2,5} -> {4_2}
        // ---------------------------------
        candidateSet = VerifyAndAllocateCandidateSet(5);
        assert(6 == candidateSet->sizeOfSet);
        Add(candidateSet, 0, set1);
        Add(candidateSet, 1, set2);
        Add(candidateSet, 2, set3);
        Add(candidateSet, 3, set4_2);
        Add(candidateSet, 4, set5);

        // Find bounding set
        numBoundingSet = FindTMMBRBoundingSet(boundingSet);
        assert(1 == numBoundingSet);
        Verify(boundingSet, 0, set4_2);

        // Is owner of set
        assert(!IsOwner(set0.SSRC, numBoundingSet));
        assert(!IsOwner(set1.SSRC, numBoundingSet));
        assert(!IsOwner(set2.SSRC, numBoundingSet));
        assert(!IsOwner(set3.SSRC, numBoundingSet));
        assert( IsOwner(set4.SSRC, numBoundingSet));
        assert(!IsOwner(set5.SSRC, numBoundingSet));

        // Set boundingSet to send
        assert(0 == SetTMMBRBoundingSetToSend(boundingSet, maxBitrate));

        // Get boundingSet to send
        boundingSetToSend = BoundingSetToSend();
        Verify(boundingSetToSend, 0, set4_2);

        // Get net bitrate depending on packet rate
        assert(0 == CalcMinMaxBitRate(0, numBoundingSet, true,0, minBitrateKbit, maxBitrateKbit));
        assert(MIN_VIDEO_BW_MANAGEMENT_BITRATE == minBitrateKbit);
        assert(set5.TMMBR == maxBitrateKbit);

        // ---------------------------------
        // Test candidate set {} -> {}
        // ---------------------------------
        candidateSet = VerifyAndAllocateCandidateSet(0);
        assert(6 == candidateSet->sizeOfSet);

        // Find bounding set
        numBoundingSet = FindTMMBRBoundingSet(boundingSet);
        assert(0 == numBoundingSet);

        // Is owner of set
        assert(!IsOwner(set0.SSRC, numBoundingSet));
        assert(!IsOwner(set1.SSRC, numBoundingSet));
        assert(!IsOwner(set2.SSRC, numBoundingSet));
        assert(!IsOwner(set3.SSRC, numBoundingSet));
        assert(!IsOwner(set4.SSRC, numBoundingSet));
        assert(!IsOwner(set5.SSRC, numBoundingSet));

        // Set boundingSet to send
        assert(0 == SetTMMBRBoundingSetToSend(boundingSet, maxBitrate));

        // Get boundingSet to send
        boundingSetToSend = BoundingSetToSend();

        // Get net bitrate depending on packet rate
        assert(-1 == CalcMinMaxBitRate(0,numBoundingSet, true,0, minBitrateKbit, maxBitrateKbit));

        // ---------------------------------
        // Test candidate set {x0,5} -> {5}
        // ---------------------------------
        candidateSet = VerifyAndAllocateCandidateSet(2);
        assert(6 == candidateSet->sizeOfSet);
        Add(candidateSet, 0, set00);
        Add(candidateSet, 1, set5);

        // Find bounding set
        numBoundingSet = FindTMMBRBoundingSet(boundingSet);
        assert(1 == numBoundingSet);
        Verify(boundingSet, 0, set5);

        // Is owner of set
        assert(!IsOwner(set0.SSRC, numBoundingSet));
        assert(!IsOwner(set1.SSRC, numBoundingSet));
        assert(!IsOwner(set2.SSRC, numBoundingSet));
        assert(!IsOwner(set3.SSRC, numBoundingSet));
        assert(!IsOwner(set4.SSRC, numBoundingSet));
        assert( IsOwner(set5.SSRC, numBoundingSet));

        // Set boundingSet to send
        assert(0 == SetTMMBRBoundingSetToSend(boundingSet, maxBitrate));

        // Get boundingSet to send
        boundingSetToSend = BoundingSetToSend();
        Verify(boundingSetToSend, 0, set5);

        // Get net bitrate depending on packet rate
        assert(0 == CalcMinMaxBitRate(0,numBoundingSet, true,0, minBitrateKbit, maxBitrateKbit));
        assert(set5.TMMBR == minBitrateKbit);
        assert(set5.TMMBR == maxBitrateKbit);

        // ---------------------------------
        // Test candidate set {x0,4,2} -> {4,2}
        // ---------------------------------
        candidateSet = VerifyAndAllocateCandidateSet(3);
        assert(6 == candidateSet->sizeOfSet);
        Add(candidateSet, 0, set00);
        Add(candidateSet, 1, set4);
        Add(candidateSet, 2, set2);

        // Find bounding set
        numBoundingSet = FindTMMBRBoundingSet(boundingSet);
        assert(2 == numBoundingSet);
        Verify(boundingSet, 0, set4);
        Verify(boundingSet, 1, set2);

        // Is owner of set
        assert(!IsOwner(set0.SSRC, numBoundingSet));
        assert(!IsOwner(set1.SSRC, numBoundingSet));
        assert( IsOwner(set2.SSRC, numBoundingSet));
        assert(!IsOwner(set3.SSRC, numBoundingSet));
        assert( IsOwner(set4.SSRC, numBoundingSet));
        assert(!IsOwner(set5.SSRC, numBoundingSet));

        // Set boundingSet to send
        assert(0 == SetTMMBRBoundingSetToSend(boundingSet, maxBitrate));

        // Get boundingSet to send
        boundingSetToSend = BoundingSetToSend();
        Verify(boundingSetToSend, 0, set4);
        Verify(boundingSetToSend, 1, set2);

        // Get net bitrate depending on packet rate
        assert(0 == CalcMinMaxBitRate(0,numBoundingSet, true,0, minBitrateKbit, maxBitrateKbit));
        assert(set4.TMMBR == minBitrateKbit);
        assert(set2.TMMBR == maxBitrateKbit);
    };
};

class NULLDataZink: public RtpData
{
    virtual int32_t OnReceivedPayloadData(const uint8_t* payloadData,
        const uint16_t payloadSize,
        const webrtc::WebRtcRTPHeader* rtpHeader,
        const uint8_t* incomingRtpPacket,
        const uint16_t incomingRtpPacketLengt)
    {
        return 0;
    };
};


int _tmain(int argc, _TCHAR* argv[])
{

    std::string str;
    std::cout << "------------------------" << std::endl;
    std::cout << "------ Test TMMBR ------" << std::endl;
    std::cout << "------------------------" << std::endl;
    std::cout << "  "  << std::endl;

    // --------------------
    // Test TMMBRHelp class
    // --------------------
    TestTMMBR test;
    test.Start();

    printf("TMMBRHelp-class test done.\n");

    // ------------------------
    // Test TMMBR single module
    // ------------------------
    RtpRtcp* rtpRtcpModuleVideo = RtpRtcp::CreateRtpRtcp(0, false);

    LoopBackTransportVideo* myLoopBackTransportVideo = new LoopBackTransportVideo(rtpRtcpModuleVideo);
    assert(0 == rtpRtcpModuleVideo->RegisterSendTransport(myLoopBackTransportVideo));

    assert(false == rtpRtcpModuleVideo->TMMBR());
    rtpRtcpModuleVideo->SetTMMBRStatus(true);
    assert(true == rtpRtcpModuleVideo->TMMBR());

    assert(0 == rtpRtcpModuleVideo->RegisterSendPayload( "I420", 96));
    assert(0 == rtpRtcpModuleVideo->RegisterReceivePayload( "I420", 96));

    // send a RTP packet with SSRC 11111 to get 11111 as the received SSRC
    assert(0 == rtpRtcpModuleVideo->SetSSRC(11111));
    const uint8_t testStream[9] = "testtest";
    assert(0 == rtpRtcpModuleVideo->RegisterIncomingDataCallback(new NULLDataZink())); // needed to avoid error from parsing the incoming stream
    assert(0 == rtpRtcpModuleVideo->SendOutgoingData(webrtc::kVideoFrameKey,96, 0, testStream, 8));

    // set the SSRC to 0
    assert(0 == rtpRtcpModuleVideo->SetSSRC(0));

    //
    assert(0 == rtpRtcpModuleVideo->SetRTCPStatus(kRtcpCompound));

    assert(0 == rtpRtcpModuleVideo->SendRTCP());  // -> incoming TMMBR {0}                     // should this make us remember a TMMBR?
    assert(0 == rtpRtcpModuleVideo->SendRTCP());  // -> incoming TMMBR {1},   verify TMMBN {0}
    assert(0 == rtpRtcpModuleVideo->SendRTCP());  // -> incoming TMMBR {2},   verify TMMBN {1}
    assert(0 == rtpRtcpModuleVideo->SendRTCP());  // -> incoming TMMBR {3},   verify TMMBN {2}
    assert(0 == rtpRtcpModuleVideo->SendRTCP());  // -> incoming TMMBR {4},   verify TMMBN {3,2}
    assert(0 == rtpRtcpModuleVideo->SendRTCP());  // -> incoming TMMBR {5},   verify TMMBN {3,4,2}
    assert(0 == rtpRtcpModuleVideo->SendRTCP());  // -> incoming TMMBR {4_2}, verify TMMBN {3,4,2}
    assert(0 == rtpRtcpModuleVideo->SendRTCP()); // -> time out receivers,   verify TMMBN {4_2}
    assert(0 == rtpRtcpModuleVideo->SendRTCP());  // -> incoming TMMBR {2}
    assert(0 == rtpRtcpModuleVideo->SendRTCP());  // ->                       verify TMMBN {2}

    printf("Single module test done.\n");

    // ------------------------
    // Test TMMBR multi module
    // ------------------------
    RtpRtcp* rtpRtcpModuleVideoDef = RtpRtcp::CreateRtpRtcp(10, false);
    assert(0 == rtpRtcpModuleVideo->RegisterDefaultModule(rtpRtcpModuleVideoDef));

    RtpRtcp* rtpRtcpModuleVideo1 = RtpRtcp::CreateRtpRtcp(1, false);
    assert(0 == rtpRtcpModuleVideo1->RegisterDefaultModule(rtpRtcpModuleVideoDef));

    RtpRtcp* rtpRtcpModuleVideo2 = RtpRtcp::CreateRtpRtcp(2, false);
    assert(0 == rtpRtcpModuleVideo2->RegisterDefaultModule(rtpRtcpModuleVideoDef));

    RtpRtcp* rtpRtcpModuleVideo3 = RtpRtcp::CreateRtpRtcp(3, false);
    assert(0 == rtpRtcpModuleVideo3->RegisterDefaultModule(rtpRtcpModuleVideoDef));

    LoopBackTransport2* myLoopBackTransport2 = new LoopBackTransport2(rtpRtcpModuleVideo2);
    assert(0 == rtpRtcpModuleVideo2->RegisterSendTransport(myLoopBackTransport2));

    assert(0 == rtpRtcpModuleVideo2->SetRTCPStatus(kRtcpCompound));

    // set the SSRC to 0
    assert(0 == rtpRtcpModuleVideo2->SetSSRC(0));

    assert(0 == rtpRtcpModuleVideo->SendRTCP());   // -> incoming TMMBR {4}, verify no TMMBN in this packet
    assert(0 == rtpRtcpModuleVideo->SendRTCP());   // -> incoming TMMBR {0}, verify TMMBN {4,2}
    assert(0 == rtpRtcpModuleVideo2->SendRTCP());  // -> incoming TMMBR {3}, verify TMMBN {4,2}
    assert(0 == rtpRtcpModuleVideo->SendRTCP());   // -> incoming TMMBR {1}, verify TMMBN {3,4,2}
    ::Sleep(5*RTCP_INTERVAL_AUDIO_MS + 1000);
    rtpRtcpModuleVideo2->Process();                // time out receiver2 -> UpdateTMMBR()
    assert(0 == rtpRtcpModuleVideo->SendRTCP());   // verify TMMBN {}

    printf("Multi module test done.\n");


    RtpRtcp::DestroyRtpRtcp(rtpRtcpModuleVideo);
    RtpRtcp::DestroyRtpRtcp(rtpRtcpModuleVideo1);
    RtpRtcp::DestroyRtpRtcp(rtpRtcpModuleVideo2);
    RtpRtcp::DestroyRtpRtcp(rtpRtcpModuleVideo3);
    RtpRtcp::DestroyRtpRtcp(rtpRtcpModuleVideoDef);

    TEST_PASSED();
    ::Sleep(5000);

    return 0;
}
