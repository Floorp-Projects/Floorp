/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <iostream>

#include "engine_configurations.h"
#include "video_engine/test/libvietest/include/tb_capture_device.h"
#include "video_engine/test/libvietest/include/tb_external_transport.h"
#include "video_engine/test/libvietest/include/tb_interfaces.h"
#include "video_engine/test/libvietest/include/tb_video_channel.h"
#include "test/testsupport/fileutils.h"
#include "video_engine/test/auto_test/interface/vie_autotest.h"
#include "video_engine/test/auto_test/interface/vie_autotest_defines.h"

class ViERtpObserver: public webrtc::ViERTPObserver
{
public:
    ViERtpObserver()
    {
    }
    virtual ~ViERtpObserver()
    {
    }

    virtual void IncomingSSRCChanged(const int videoChannel,
                                     const unsigned int SSRC)
    {
    }
    virtual void IncomingCSRCChanged(const int videoChannel,
                                     const unsigned int CSRC, const bool added)
    {
    }
};

class ViERtcpObserver: public webrtc::ViERTCPObserver
{
public:
    int _channel;
    unsigned char _subType;
    unsigned int _name;
    char* _data;
    unsigned short _dataLength;

    ViERtcpObserver() :
        _channel(-1),
        _subType(0),
        _name(-1),
        _data(NULL),
        _dataLength(0)
    {
    }
    ~ViERtcpObserver()
    {
        if (_data)
        {
            delete[] _data;
        }
    }
    virtual void OnApplicationDataReceived(
        const int videoChannel, const unsigned char subType,
        const unsigned int name, const char* data,
        const unsigned short dataLengthInBytes)
    {
        _channel = videoChannel;
        _subType = subType;
        _name = name;
        if (dataLengthInBytes > _dataLength)
        {
            delete[] _data;
            _data = NULL;
        }
        if (_data == NULL)
        {
            _data = new char[dataLengthInBytes];
        }
        memcpy(_data, data, dataLengthInBytes);
        _dataLength = dataLengthInBytes;
    }
};

void ViEAutoTest::ViERtpRtcpStandardTest()
{
    // ***************************************************************
    // Begin create/initialize WebRTC Video Engine for testing
    // ***************************************************************

    // Create VIE
    TbInterfaces ViE("ViERtpRtcpStandardTest");
    // Create a video channel
    TbVideoChannel tbChannel(ViE, webrtc::kVideoCodecVP8);

    // Create a capture device
    TbCaptureDevice tbCapture(ViE);
    tbCapture.ConnectTo(tbChannel.videoChannel);

    ViETest::Log("\n");
    TbExternalTransport myTransport(*(ViE.network), tbChannel.videoChannel,
                                    NULL);

    EXPECT_EQ(0, ViE.network->RegisterSendTransport(
        tbChannel.videoChannel, myTransport));

    // ***************************************************************
    // Engine ready. Begin testing class
    // ***************************************************************
    unsigned short startSequenceNumber = 12345;
    ViETest::Log("Set start sequence number: %u", startSequenceNumber);
    EXPECT_EQ(0, ViE.rtp_rtcp->SetStartSequenceNumber(
        tbChannel.videoChannel, startSequenceNumber));

    myTransport.EnableSequenceNumberCheck();

    EXPECT_EQ(0, ViE.base->StartReceive(tbChannel.videoChannel));
    EXPECT_EQ(0, ViE.base->StartSend(tbChannel.videoChannel));

    AutoTestSleep(2000);

    unsigned short receivedSequenceNumber =
        myTransport.GetFirstSequenceNumber();
    ViETest::Log("First received sequence number: %u\n",
                 receivedSequenceNumber);
    EXPECT_EQ(startSequenceNumber, receivedSequenceNumber);

    EXPECT_EQ(0, ViE.base->StopSend(tbChannel.videoChannel));

    //
    // RTCP CName
    //
    ViETest::Log("Testing CName\n");
    const char* sendCName = "ViEAutoTestCName\0";
    EXPECT_EQ(0, ViE.rtp_rtcp->SetRTCPCName(tbChannel.videoChannel, sendCName));

    char returnCName[webrtc::ViERTP_RTCP::KMaxRTCPCNameLength];
    memset(returnCName, 0, webrtc::ViERTP_RTCP::KMaxRTCPCNameLength);
    EXPECT_EQ(0, ViE.rtp_rtcp->GetRTCPCName(
        tbChannel.videoChannel, returnCName));
    EXPECT_STRCASEEQ(sendCName, returnCName);

    EXPECT_EQ(0, ViE.base->StartSend(tbChannel.videoChannel));

    AutoTestSleep(1000);

    if (FLAGS_include_timing_dependent_tests) {
      char remoteCName[webrtc::ViERTP_RTCP::KMaxRTCPCNameLength];
      memset(remoteCName, 0, webrtc::ViERTP_RTCP::KMaxRTCPCNameLength);
      EXPECT_EQ(0, ViE.rtp_rtcp->GetRemoteRTCPCName(
          tbChannel.videoChannel, remoteCName));
      EXPECT_STRCASEEQ(sendCName, remoteCName);
    }

    //
    //  Statistics
    //
    // Stop and restart to clear stats
    ViETest::Log("Testing statistics\n");
    EXPECT_EQ(0, ViE.base->StopReceive(tbChannel.videoChannel));
    EXPECT_EQ(0, ViE.base->StopSend(tbChannel.videoChannel));

    myTransport.ClearStats();
    const int kPacketLossRate = 20;
    NetworkParameters network;
    network.packet_loss_rate = kPacketLossRate;
    network.loss_model = kUniformLoss;
    myTransport.SetNetworkParameters(network);

    // Start send to verify sending stats

    EXPECT_EQ(0, ViE.rtp_rtcp->SetStartSequenceNumber(
        tbChannel.videoChannel, startSequenceNumber));
    EXPECT_EQ(0, ViE.base->StartSend(tbChannel.videoChannel));
    EXPECT_EQ(0, ViE.base->StartReceive(tbChannel.videoChannel));

    AutoTestSleep(kAutoTestSleepTimeMs);

    unsigned short sentFractionsLost = 0;
    unsigned int sentCumulativeLost = 0;
    unsigned int sentExtendedMax = 0;
    unsigned int sentJitter = 0;
    int sentRttMs = 0;
    unsigned short recFractionsLost = 0;
    unsigned int recCumulativeLost = 0;
    unsigned int recExtendedMax = 0;
    unsigned int recJitter = 0;
    int recRttMs = 0;

    unsigned int sentTotalBitrate = 0;
    unsigned int sentVideoBitrate = 0;
    unsigned int sentFecBitrate = 0;
    unsigned int sentNackBitrate = 0;

    EXPECT_EQ(0, ViE.rtp_rtcp->GetBandwidthUsage(
        tbChannel.videoChannel, sentTotalBitrate, sentVideoBitrate,
        sentFecBitrate, sentNackBitrate));

    EXPECT_GT(sentTotalBitrate, 0u);
    EXPECT_EQ(sentFecBitrate, 0u);
    EXPECT_EQ(sentNackBitrate, 0u);

    EXPECT_EQ(0, ViE.base->StopReceive(tbChannel.videoChannel));

    AutoTestSleep(2000);

    EXPECT_EQ(0, ViE.rtp_rtcp->GetSentRTCPStatistics(
        tbChannel.videoChannel, sentFractionsLost, sentCumulativeLost,
        sentExtendedMax, sentJitter, sentRttMs));
    EXPECT_GT(sentCumulativeLost, 0u);
    EXPECT_GT(sentExtendedMax, startSequenceNumber);
    EXPECT_GT(sentJitter, 0u);
    EXPECT_GT(sentRttMs, 0);

    EXPECT_EQ(0, ViE.rtp_rtcp->GetReceivedRTCPStatistics(
        tbChannel.videoChannel, recFractionsLost, recCumulativeLost,
        recExtendedMax, recJitter, recRttMs));

    EXPECT_GT(recCumulativeLost, 0u);
    EXPECT_GT(recExtendedMax, startSequenceNumber);
    EXPECT_GT(recJitter, 0u);
    EXPECT_GT(recRttMs, 0);

    unsigned int estimated_bandwidth = 0;
    EXPECT_EQ(0, ViE.rtp_rtcp->GetEstimatedSendBandwidth(
        tbChannel.videoChannel,
        &estimated_bandwidth));
    EXPECT_GT(estimated_bandwidth, 0u);

    if (FLAGS_include_timing_dependent_tests) {
      EXPECT_EQ(0, ViE.rtp_rtcp->GetEstimatedReceiveBandwidth(
          tbChannel.videoChannel,
          &estimated_bandwidth));
      EXPECT_GT(estimated_bandwidth, 0u);
    }

    // Check that rec stats extended max is greater than what we've sent.
    EXPECT_GE(recExtendedMax, sentExtendedMax);
    EXPECT_EQ(0, ViE.base->StopSend(tbChannel.videoChannel));

    //
    // Test bandwidth statistics with NACK and FEC separately
    //

    myTransport.ClearStats();
    network.packet_loss_rate = kPacketLossRate;
    myTransport.SetNetworkParameters(network);

    EXPECT_EQ(0, ViE.rtp_rtcp->SetFECStatus(
        tbChannel.videoChannel, true, 96, 97));
    EXPECT_EQ(0, ViE.base->StartReceive(tbChannel.videoChannel));
    EXPECT_EQ(0, ViE.base->StartSend(tbChannel.videoChannel));

    AutoTestSleep(kAutoTestSleepTimeMs);

    EXPECT_EQ(0, ViE.rtp_rtcp->GetBandwidthUsage(
        tbChannel.videoChannel, sentTotalBitrate, sentVideoBitrate,
         sentFecBitrate, sentNackBitrate));

    if (FLAGS_include_timing_dependent_tests) {
      EXPECT_GT(sentTotalBitrate, 0u);
      EXPECT_GE(sentFecBitrate, 10u);
      EXPECT_EQ(sentNackBitrate, 0u);
    }

    EXPECT_EQ(0, ViE.base->StopSend(tbChannel.videoChannel));
    EXPECT_EQ(0, ViE.rtp_rtcp->SetFECStatus(
        tbChannel.videoChannel, false, 96, 97));
    EXPECT_EQ(0, ViE.rtp_rtcp->SetNACKStatus(tbChannel.videoChannel, true));
    EXPECT_EQ(0, ViE.base->StartSend(tbChannel.videoChannel));

    AutoTestSleep(kAutoTestSleepTimeMs);

    EXPECT_EQ(0, ViE.rtp_rtcp->GetBandwidthUsage(
        tbChannel.videoChannel, sentTotalBitrate, sentVideoBitrate,
        sentFecBitrate, sentNackBitrate));

    // TODO(holmer): Write a non-flaky verification of this API.
    // numberOfErrors += ViETest::TestError(sentTotalBitrate > 0 &&
    //                                      sentFecBitrate == 0 &&
    //                                      sentNackBitrate > 0,
    //                                      "ERROR: %s at line %d",
    //                                      __FUNCTION__, __LINE__);

    EXPECT_EQ(0, ViE.base->StopReceive(tbChannel.videoChannel));
    EXPECT_EQ(0, ViE.base->StopSend(tbChannel.videoChannel));
    EXPECT_EQ(0, ViE.rtp_rtcp->SetNACKStatus(tbChannel.videoChannel, false));


    // Test to set SSRC
    network.packet_loss_rate = 0;
    myTransport.SetNetworkParameters(network);
    myTransport.ClearStats();

    unsigned int setSSRC = 0x01234567;
    ViETest::Log("Set SSRC %u", setSSRC);
    EXPECT_EQ(0, ViE.rtp_rtcp->SetLocalSSRC(tbChannel.videoChannel, setSSRC));
    EXPECT_EQ(0, ViE.base->StartReceive(tbChannel.videoChannel));
    EXPECT_EQ(0, ViE.base->StartSend(tbChannel.videoChannel));

    myTransport.EnableSSRCCheck();

    AutoTestSleep(2000);
    unsigned int receivedSSRC = myTransport.ReceivedSSRC();
    ViETest::Log("Received SSRC %u\n", receivedSSRC);

    if (FLAGS_include_timing_dependent_tests) {
      EXPECT_EQ(setSSRC, receivedSSRC);

      unsigned int localSSRC = 0;
      EXPECT_EQ(0, ViE.rtp_rtcp->GetLocalSSRC(
          tbChannel.videoChannel, localSSRC));
      EXPECT_EQ(setSSRC, localSSRC);

      unsigned int remoteSSRC = 0;
      EXPECT_EQ(0, ViE.rtp_rtcp->GetRemoteSSRC(
          tbChannel.videoChannel, remoteSSRC));
      EXPECT_EQ(setSSRC, remoteSSRC);
    }

    EXPECT_EQ(0, ViE.base->StopSend(tbChannel.videoChannel));

    ViETest::Log("Testing RTP dump...\n");

    std::string inDumpName =
        ViETest::GetResultOutputPath() + "IncomingRTPDump.rtp";
    std::string outDumpName =
        ViETest::GetResultOutputPath() + "OutgoingRTPDump.rtp";
    EXPECT_EQ(0, ViE.rtp_rtcp->StartRTPDump(
        tbChannel.videoChannel, inDumpName.c_str(), webrtc::kRtpIncoming));
    EXPECT_EQ(0, ViE.rtp_rtcp->StartRTPDump(
        tbChannel.videoChannel, outDumpName.c_str(), webrtc::kRtpOutgoing));

    EXPECT_EQ(0, ViE.base->StartSend(tbChannel.videoChannel));

    AutoTestSleep(kAutoTestSleepTimeMs);

    EXPECT_EQ(0, ViE.base->StopSend(tbChannel.videoChannel));

    AutoTestSleep(1000);

    EXPECT_EQ(0, ViE.rtp_rtcp->StopRTPDump(
        tbChannel.videoChannel, webrtc::kRtpIncoming));
    EXPECT_EQ(0, ViE.rtp_rtcp->StopRTPDump(
        tbChannel.videoChannel, webrtc::kRtpOutgoing));

    // Make sure data was actually saved to the file and we stored the same
    // amount of data in both files
    FILE* inDump = fopen(inDumpName.c_str(), "r");
    fseek(inDump, 0L, SEEK_END);
    long inEndPos = ftell(inDump);
    fclose(inDump);
    FILE* outDump = fopen(outDumpName.c_str(), "r");
    fseek(outDump, 0L, SEEK_END);
    // long outEndPos = ftell(outDump);
    fclose(outDump);

    EXPECT_GT(inEndPos, 0);

    // TODO(phoglund): This is flaky for some reason. Are the sleeps too
    // short above?
    // EXPECT_LT(inEndPos, outEndPos + 100);

    // Deregister external transport
    EXPECT_EQ(0, ViE.network->DeregisterSendTransport(tbChannel.videoChannel));


    //***************************************************************
    //  Testing finished. Tear down Video Engine
    //***************************************************************
}

void ViEAutoTest::ViERtpRtcpExtendedTest()
{
    //***************************************************************
    //  Begin create/initialize WebRTC Video Engine for testing
    //***************************************************************
    // Create VIE
    TbInterfaces ViE("ViERtpRtcpExtendedTest");
    // Create a video channel
    TbVideoChannel tbChannel(ViE, webrtc::kVideoCodecVP8);
    // Create a capture device
    TbCaptureDevice tbCapture(ViE);
    tbCapture.ConnectTo(tbChannel.videoChannel);

    //tbChannel.StartReceive(rtpPort);
    //tbChannel.StartSend(rtpPort);
    TbExternalTransport myTransport(*(ViE.network), tbChannel.videoChannel,
                                    NULL);

    EXPECT_EQ(0, ViE.network->RegisterSendTransport(
        tbChannel.videoChannel, myTransport));
    EXPECT_EQ(0, ViE.base->StartReceive(tbChannel.videoChannel));
    EXPECT_EQ(0, ViE.base->StartSend(tbChannel.videoChannel));

    //***************************************************************
    //  Engine ready. Begin testing class
    //***************************************************************

    //
    // Application specific RTCP
    //
    //

    ViERtcpObserver rtcpObserver;
    EXPECT_EQ(0, ViE.rtp_rtcp->RegisterRTCPObserver(
        tbChannel.videoChannel, rtcpObserver));

    unsigned char subType = 3;
    unsigned int name = static_cast<unsigned int> (0x41424344); // 'ABCD';
    const char* data = "ViEAutoTest Data of length 32 -\0";
    const unsigned short numBytes = 32;

    EXPECT_EQ(0, ViE.rtp_rtcp->SendApplicationDefinedRTCPPacket(
        tbChannel.videoChannel, subType, name, data, numBytes));

    ViETest::Log("Sending RTCP application data...\n");
    AutoTestSleep(kAutoTestSleepTimeMs);

    EXPECT_EQ(subType, rtcpObserver._subType);
    EXPECT_STRCASEEQ(data, rtcpObserver._data);
    EXPECT_EQ(name, rtcpObserver._name);
    EXPECT_EQ(numBytes, rtcpObserver._dataLength);

    ViETest::Log("\t RTCP application data received\n");

    //***************************************************************
    //  Testing finished. Tear down Video Engine
    //***************************************************************
    EXPECT_EQ(0, ViE.base->StopReceive(tbChannel.videoChannel));
    EXPECT_EQ(0, ViE.base->StopSend(tbChannel.videoChannel));

    EXPECT_EQ(0, ViE.network->DeregisterSendTransport(tbChannel.videoChannel));
}

void ViEAutoTest::ViERtpRtcpAPITest()
{
    //***************************************************************
    //  Begin create/initialize WebRTC Video Engine for testing
    //***************************************************************
    // Create VIE
    TbInterfaces ViE("ViERtpRtcpAPITest");

    // Verify that we can set the bandwidth estimation mode, as that API only
    // is valid to call before creating channels.
    EXPECT_EQ(0, ViE.rtp_rtcp->SetBandwidthEstimationMode(
        webrtc::kViESingleStreamEstimation));
    EXPECT_EQ(0, ViE.rtp_rtcp->SetBandwidthEstimationMode(
        webrtc::kViEMultiStreamEstimation));

    // Create a video channel
    TbVideoChannel tbChannel(ViE, webrtc::kVideoCodecVP8);

    EXPECT_EQ(-1, ViE.rtp_rtcp->SetBandwidthEstimationMode(
        webrtc::kViESingleStreamEstimation));
    EXPECT_EQ(-1, ViE.rtp_rtcp->SetBandwidthEstimationMode(
        webrtc::kViEMultiStreamEstimation));

    // Create a capture device
    TbCaptureDevice tbCapture(ViE);
    tbCapture.ConnectTo(tbChannel.videoChannel);

    //***************************************************************
    //  Engine ready. Begin testing class
    //***************************************************************

    //
    // Check different RTCP modes
    //
    webrtc::ViERTCPMode rtcpMode = webrtc::kRtcpNone;
    EXPECT_EQ(0, ViE.rtp_rtcp->GetRTCPStatus(
        tbChannel.videoChannel, rtcpMode));
    EXPECT_EQ(webrtc::kRtcpCompound_RFC4585, rtcpMode);
    EXPECT_EQ(0, ViE.rtp_rtcp->SetRTCPStatus(
        tbChannel.videoChannel, webrtc::kRtcpCompound_RFC4585));
    EXPECT_EQ(0, ViE.rtp_rtcp->GetRTCPStatus(
        tbChannel.videoChannel, rtcpMode));
    EXPECT_EQ(webrtc::kRtcpCompound_RFC4585, rtcpMode);
    EXPECT_EQ(0, ViE.rtp_rtcp->SetRTCPStatus(
        tbChannel.videoChannel, webrtc::kRtcpNonCompound_RFC5506));
    EXPECT_EQ(0, ViE.rtp_rtcp->GetRTCPStatus(
        tbChannel.videoChannel, rtcpMode));
    EXPECT_EQ(webrtc::kRtcpNonCompound_RFC5506, rtcpMode);
    EXPECT_EQ(0, ViE.rtp_rtcp->SetRTCPStatus(
        tbChannel.videoChannel, webrtc::kRtcpNone));
    EXPECT_EQ(0, ViE.rtp_rtcp->GetRTCPStatus(
        tbChannel.videoChannel, rtcpMode));
    EXPECT_EQ(webrtc::kRtcpNone, rtcpMode);
    EXPECT_EQ(0, ViE.rtp_rtcp->SetRTCPStatus(
        tbChannel.videoChannel, webrtc::kRtcpCompound_RFC4585));

    //
    // CName is testedn in SimpleTest
    // Start sequence number is tested in SimplTEst
    //
    const char* testCName = "ViEAutotestCName";
    EXPECT_EQ(0, ViE.rtp_rtcp->SetRTCPCName(
        tbChannel.videoChannel, testCName));

    char returnCName[256];
    memset(returnCName, 0, 256);
    EXPECT_EQ(0, ViE.rtp_rtcp->GetRTCPCName(
        tbChannel.videoChannel, returnCName));
    EXPECT_STRCASEEQ(testCName, returnCName);

    //
    // SSRC
    //
    EXPECT_EQ(0, ViE.rtp_rtcp->SetLocalSSRC(
        tbChannel.videoChannel, 0x01234567));
    EXPECT_EQ(0, ViE.rtp_rtcp->SetLocalSSRC(
        tbChannel.videoChannel, 0x76543210));

    unsigned int ssrc = 0;
    EXPECT_EQ(0, ViE.rtp_rtcp->GetLocalSSRC(tbChannel.videoChannel, ssrc));
    EXPECT_EQ(0, ViE.rtp_rtcp->SetStartSequenceNumber(
        tbChannel.videoChannel, 1000));
    tbChannel.StartSend();
    EXPECT_NE(0, ViE.rtp_rtcp->SetStartSequenceNumber(
        tbChannel.videoChannel, 12345));
    tbChannel.StopSend();

    //
    // Start sequence number
    //
    EXPECT_EQ(0, ViE.rtp_rtcp->SetStartSequenceNumber(
        tbChannel.videoChannel, 12345));
    EXPECT_EQ(0, ViE.rtp_rtcp->SetStartSequenceNumber(
        tbChannel.videoChannel, 1000));
    tbChannel.StartSend();
    EXPECT_NE(0, ViE.rtp_rtcp->SetStartSequenceNumber(
        tbChannel.videoChannel, 12345));
    tbChannel.StopSend();

    //
    // Application specific RTCP
    //
    {
        unsigned char subType = 3;
        unsigned int name = static_cast<unsigned int> (0x41424344); // 'ABCD';
        const char* data = "ViEAutoTest Data of length 32 --";
        const unsigned short numBytes = 32;

        tbChannel.StartSend();
        EXPECT_EQ(0, ViE.rtp_rtcp->SendApplicationDefinedRTCPPacket(
            tbChannel.videoChannel, subType, name, data, numBytes));
        EXPECT_NE(0, ViE.rtp_rtcp->SendApplicationDefinedRTCPPacket(
            tbChannel.videoChannel, subType, name, NULL, numBytes)) <<
                "Should fail on NULL input.";
        EXPECT_NE(0, ViE.rtp_rtcp->SendApplicationDefinedRTCPPacket(
            tbChannel.videoChannel, subType, name, data, numBytes - 1)) <<
                "Should fail on incorrect length.";

        EXPECT_EQ(0, ViE.rtp_rtcp->GetRTCPStatus(
            tbChannel.videoChannel, rtcpMode));
        EXPECT_EQ(0, ViE.rtp_rtcp->SendApplicationDefinedRTCPPacket(
            tbChannel.videoChannel, subType, name, data, numBytes));
        EXPECT_EQ(0, ViE.rtp_rtcp->SetRTCPStatus(
            tbChannel.videoChannel, webrtc::kRtcpCompound_RFC4585));
        tbChannel.StopSend();
        EXPECT_NE(0, ViE.rtp_rtcp->SendApplicationDefinedRTCPPacket(
            tbChannel.videoChannel, subType, name, data, numBytes));
    }

    //
    // Statistics
    //
    // Tested in SimpleTest(), we'll get errors if we haven't received a RTCP
    // packet.

    //
    // RTP Dump
    //
    {
        std::string output_file = webrtc::test::OutputPath() +
            "DumpFileName.rtp";
        const char* dumpName = output_file.c_str();

        EXPECT_EQ(0, ViE.rtp_rtcp->StartRTPDump(
            tbChannel.videoChannel, dumpName, webrtc::kRtpIncoming));
        EXPECT_EQ(0, ViE.rtp_rtcp->StopRTPDump(
            tbChannel.videoChannel, webrtc::kRtpIncoming));
        EXPECT_NE(0, ViE.rtp_rtcp->StopRTPDump(
            tbChannel.videoChannel, webrtc::kRtpIncoming));
        EXPECT_EQ(0, ViE.rtp_rtcp->StartRTPDump(
            tbChannel.videoChannel, dumpName, webrtc::kRtpOutgoing));
        EXPECT_EQ(0, ViE.rtp_rtcp->StopRTPDump(
            tbChannel.videoChannel, webrtc::kRtpOutgoing));
        EXPECT_NE(0, ViE.rtp_rtcp->StopRTPDump(
            tbChannel.videoChannel, webrtc::kRtpOutgoing));
        EXPECT_NE(0, ViE.rtp_rtcp->StartRTPDump(
            tbChannel.videoChannel, dumpName, (webrtc::RTPDirections) 3));
    }
    //
    // RTP/RTCP Observers
    //
    {
        ViERtpObserver rtpObserver;
        EXPECT_EQ(0, ViE.rtp_rtcp->RegisterRTPObserver(
            tbChannel.videoChannel, rtpObserver));
        EXPECT_NE(0, ViE.rtp_rtcp->RegisterRTPObserver(
            tbChannel.videoChannel, rtpObserver));
        EXPECT_EQ(0, ViE.rtp_rtcp->DeregisterRTPObserver(
            tbChannel.videoChannel));
        EXPECT_NE(0, ViE.rtp_rtcp->DeregisterRTPObserver(
            tbChannel.videoChannel));

        ViERtcpObserver rtcpObserver;
        EXPECT_EQ(0, ViE.rtp_rtcp->RegisterRTCPObserver(
            tbChannel.videoChannel, rtcpObserver));
        EXPECT_NE(0, ViE.rtp_rtcp->RegisterRTCPObserver(
            tbChannel.videoChannel, rtcpObserver));
        EXPECT_EQ(0, ViE.rtp_rtcp->DeregisterRTCPObserver(
            tbChannel.videoChannel));
        EXPECT_NE(0, ViE.rtp_rtcp->DeregisterRTCPObserver(
            tbChannel.videoChannel));
    }
    //
    // PLI
    //
    {
        EXPECT_EQ(0, ViE.rtp_rtcp->SetKeyFrameRequestMethod(
            tbChannel.videoChannel, webrtc::kViEKeyFrameRequestPliRtcp));
        EXPECT_EQ(0, ViE.rtp_rtcp->SetKeyFrameRequestMethod(
            tbChannel.videoChannel, webrtc::kViEKeyFrameRequestPliRtcp));
        EXPECT_EQ(0, ViE.rtp_rtcp->SetKeyFrameRequestMethod(
            tbChannel.videoChannel, webrtc::kViEKeyFrameRequestNone));
        EXPECT_EQ(0, ViE.rtp_rtcp->SetKeyFrameRequestMethod(
            tbChannel.videoChannel, webrtc::kViEKeyFrameRequestNone));
    }
    //
    // NACK
    //
    {
      EXPECT_EQ(0, ViE.rtp_rtcp->SetNACKStatus(tbChannel.videoChannel, true));
    }

    // Timsetamp offset extension.
    // Valid range is 1 to 14 inclusive.
    EXPECT_EQ(-1, ViE.rtp_rtcp->SetSendTimestampOffsetStatus(
        tbChannel.videoChannel, true, 0));
    EXPECT_EQ(-1, ViE.rtp_rtcp->SetSendTimestampOffsetStatus(
        tbChannel.videoChannel, true, 15));
    EXPECT_EQ(0, ViE.rtp_rtcp->SetSendTimestampOffsetStatus(
        tbChannel.videoChannel, true, 3));
    EXPECT_EQ(0, ViE.rtp_rtcp->SetSendTimestampOffsetStatus(
        tbChannel.videoChannel, true, 3));
    EXPECT_EQ(0, ViE.rtp_rtcp->SetSendTimestampOffsetStatus(
            tbChannel.videoChannel, false, 3));
    EXPECT_EQ(0, ViE.rtp_rtcp->SetSendTimestampOffsetStatus(
        tbChannel.videoChannel, true, 3));
    EXPECT_EQ(0, ViE.rtp_rtcp->SetSendTimestampOffsetStatus(
              tbChannel.videoChannel, false, 3));
    EXPECT_EQ(0, ViE.rtp_rtcp->SetSendTimestampOffsetStatus(
            tbChannel.videoChannel, false, 3));

    EXPECT_EQ(-1, ViE.rtp_rtcp->SetReceiveTimestampOffsetStatus(
        tbChannel.videoChannel, true, 0));
    EXPECT_EQ(-1, ViE.rtp_rtcp->SetReceiveTimestampOffsetStatus(
        tbChannel.videoChannel, true, 15));
    EXPECT_EQ(0, ViE.rtp_rtcp->SetReceiveTimestampOffsetStatus(
        tbChannel.videoChannel, true, 3));
    EXPECT_EQ(-1, ViE.rtp_rtcp->SetReceiveTimestampOffsetStatus(
        tbChannel.videoChannel, true, 3));
    EXPECT_EQ(0, ViE.rtp_rtcp->SetReceiveTimestampOffsetStatus(
            tbChannel.videoChannel, false, 3));
    EXPECT_EQ(0, ViE.rtp_rtcp->SetReceiveTimestampOffsetStatus(
        tbChannel.videoChannel, true, 3));
    EXPECT_EQ(0, ViE.rtp_rtcp->SetReceiveTimestampOffsetStatus(
              tbChannel.videoChannel, false, 3));
    EXPECT_EQ(0, ViE.rtp_rtcp->SetReceiveTimestampOffsetStatus(
            tbChannel.videoChannel, false, 3));

    // Transmission smoothening.
    const int invalid_channel_id = 17;
    EXPECT_EQ(-1, ViE.rtp_rtcp->SetTransmissionSmoothingStatus(
        invalid_channel_id, true));
    EXPECT_EQ(0, ViE.rtp_rtcp->SetTransmissionSmoothingStatus(
        tbChannel.videoChannel, true));
    EXPECT_EQ(0, ViE.rtp_rtcp->SetTransmissionSmoothingStatus(
        tbChannel.videoChannel, true));
    EXPECT_EQ(0, ViE.rtp_rtcp->SetTransmissionSmoothingStatus(
        tbChannel.videoChannel, false));
    EXPECT_EQ(0, ViE.rtp_rtcp->SetTransmissionSmoothingStatus(
        tbChannel.videoChannel, false));

    //***************************************************************
    //  Testing finished. Tear down Video Engine
    //***************************************************************
}
