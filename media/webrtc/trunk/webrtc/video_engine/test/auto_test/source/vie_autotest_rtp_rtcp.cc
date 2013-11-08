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

#include "webrtc/engine_configurations.h"
#include "webrtc/test/testsupport/fileutils.h"
#include "webrtc/video_engine/test/auto_test/interface/vie_autotest.h"
#include "webrtc/video_engine/test/auto_test/interface/vie_autotest_defines.h"
#include "webrtc/video_engine/test/libvietest/include/tb_capture_device.h"
#include "webrtc/video_engine/test/libvietest/include/tb_external_transport.h"
#include "webrtc/video_engine/test/libvietest/include/tb_interfaces.h"
#include "webrtc/video_engine/test/libvietest/include/tb_video_channel.h"

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

    ViE.network->DeregisterSendTransport(tbChannel.videoChannel);
    EXPECT_EQ(0, ViE.network->RegisterSendTransport(
        tbChannel.videoChannel, myTransport));

    // ***************************************************************
    // Engine ready. Begin testing class
    // ***************************************************************
    unsigned short startSequenceNumber = 12345;
    ViETest::Log("Set start sequence number: %u", startSequenceNumber);
    EXPECT_EQ(0, ViE.rtp_rtcp->SetStartSequenceNumber(
        tbChannel.videoChannel, startSequenceNumber));
    const unsigned int kVideoSsrc = 123456;
    // Set an SSRC to avoid issues with collisions.
    EXPECT_EQ(0, ViE.rtp_rtcp->SetLocalSSRC(tbChannel.videoChannel, kVideoSsrc,
                                            webrtc::kViEStreamTypeNormal, 0));

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
    // Pacing
    //
    unsigned short recFractionsLost = 0;
    unsigned int recCumulativeLost = 0;
    unsigned int recExtendedMax = 0;
    unsigned int recJitter = 0;
    int recRttMs = 0;
    unsigned int sentTotalBitrate = 0;
    unsigned int sentVideoBitrate = 0;
    unsigned int sentFecBitrate = 0;
    unsigned int sentNackBitrate = 0;

    ViETest::Log("Testing Pacing\n");
    EXPECT_EQ(0, ViE.base->StopSend(tbChannel.videoChannel));
    EXPECT_EQ(0, ViE.base->StopReceive(tbChannel.videoChannel));

    myTransport.ClearStats();

    EXPECT_EQ(0, ViE.rtp_rtcp->SetNACKStatus(tbChannel.videoChannel, true));
    EXPECT_EQ(0, ViE.rtp_rtcp->SetTransmissionSmoothingStatus(
        tbChannel.videoChannel, true));
    EXPECT_EQ(0, ViE.base->StartReceive(tbChannel.videoChannel));
    EXPECT_EQ(0, ViE.base->StartSend(tbChannel.videoChannel));

    NetworkParameters network;
    network.packet_loss_rate = 0;
    network.loss_model = kUniformLoss;
    myTransport.SetNetworkParameters(network);

    AutoTestSleep(kAutoTestSleepTimeMs);

    EXPECT_EQ(0, ViE.rtp_rtcp->GetReceivedRTCPStatistics(
        tbChannel.videoChannel, recFractionsLost, recCumulativeLost,
        recExtendedMax, recJitter, recRttMs));
    EXPECT_EQ(0, ViE.rtp_rtcp->GetBandwidthUsage(
        tbChannel.videoChannel, sentTotalBitrate, sentVideoBitrate,
        sentFecBitrate, sentNackBitrate));

    int num_rtp_packets = 0;
    int num_dropped_packets = 0;
    int num_rtcp_packets = 0;
    std::map<uint8_t, int> packet_counters;
    myTransport.GetStats(num_rtp_packets, num_dropped_packets, num_rtcp_packets,
                         &packet_counters);
    EXPECT_GT(num_rtp_packets, 0);
    EXPECT_EQ(num_dropped_packets, 0);
    EXPECT_GT(num_rtcp_packets, 0);
    EXPECT_GT(sentTotalBitrate, 0u);
    EXPECT_EQ(sentNackBitrate, 0u);
    EXPECT_EQ(recCumulativeLost, 0u);

    //
    // RTX
    //
    ViETest::Log("Testing NACK over RTX\n");
    EXPECT_EQ(0, ViE.base->StopSend(tbChannel.videoChannel));
    EXPECT_EQ(0, ViE.base->StopReceive(tbChannel.videoChannel));

    myTransport.ClearStats();

    const uint8_t kRtxPayloadType = 96;
    EXPECT_EQ(0, ViE.rtp_rtcp->SetTransmissionSmoothingStatus(
        tbChannel.videoChannel, false));
    EXPECT_EQ(0, ViE.rtp_rtcp->SetNACKStatus(tbChannel.videoChannel, true));
    EXPECT_EQ(0, ViE.rtp_rtcp->SetRtxSendPayloadType(tbChannel.videoChannel,
                                                     kRtxPayloadType));
    EXPECT_EQ(0, ViE.rtp_rtcp->SetRtxReceivePayloadType(tbChannel.videoChannel,
                                                        kRtxPayloadType));
    EXPECT_EQ(0, ViE.rtp_rtcp->SetLocalSSRC(tbChannel.videoChannel, 1234,
                                            webrtc::kViEStreamTypeRtx, 0));
    EXPECT_EQ(0, ViE.rtp_rtcp->SetRemoteSSRCType(tbChannel.videoChannel,
                                                 webrtc::kViEStreamTypeRtx,
                                                 1234));
    EXPECT_EQ(0, ViE.rtp_rtcp->SetStartSequenceNumber(
        tbChannel.videoChannel, startSequenceNumber));
    EXPECT_EQ(0, ViE.base->StartReceive(tbChannel.videoChannel));
    EXPECT_EQ(0, ViE.base->StartSend(tbChannel.videoChannel));

    // Make sure the first key frame gets through.
    AutoTestSleep(100);
    const int kPacketLossRate = 20;
    network.packet_loss_rate = kPacketLossRate;
    network.loss_model = kUniformLoss;
    myTransport.SetNetworkParameters(network);
    AutoTestSleep(kAutoTestSleepTimeMs);

    EXPECT_EQ(0, ViE.rtp_rtcp->GetReceivedRTCPStatistics(
        tbChannel.videoChannel, recFractionsLost, recCumulativeLost,
        recExtendedMax, recJitter, recRttMs));
    EXPECT_EQ(0, ViE.rtp_rtcp->GetBandwidthUsage(
        tbChannel.videoChannel, sentTotalBitrate, sentVideoBitrate,
        sentFecBitrate, sentNackBitrate));

    packet_counters.clear();
    myTransport.GetStats(num_rtp_packets, num_dropped_packets, num_rtcp_packets,
                         &packet_counters);
    EXPECT_GT(num_rtp_packets, 0);
    EXPECT_GT(num_dropped_packets, 0);
    EXPECT_GT(num_rtcp_packets, 0);
    EXPECT_GT(packet_counters[kRtxPayloadType], 0);

    // Make sure we have lost packets and that they were retransmitted.
    // TODO(holmer): Disabled due to being flaky. Could be a bug in our stats.
    // EXPECT_GT(recCumulativeLost, 0u);
    EXPECT_GT(sentTotalBitrate, 0u);
    EXPECT_GT(sentNackBitrate, 0u);

    //
    //  Statistics
    //
    // Stop and restart to clear stats
    ViETest::Log("Testing statistics\n");
    EXPECT_EQ(0, ViE.rtp_rtcp->SetNACKStatus(tbChannel.videoChannel, false));
    EXPECT_EQ(0, ViE.base->StopReceive(tbChannel.videoChannel));
    EXPECT_EQ(0, ViE.base->StopSend(tbChannel.videoChannel));

    myTransport.ClearStats();
    network.packet_loss_rate = kPacketLossRate;
    network.loss_model = kUniformLoss;
    myTransport.SetNetworkParameters(network);

    // Start send to verify sending stats

    EXPECT_EQ(0, ViE.rtp_rtcp->SetStartSequenceNumber(
        tbChannel.videoChannel, startSequenceNumber));
    EXPECT_EQ(0, ViE.base->StartReceive(tbChannel.videoChannel));
    EXPECT_EQ(0, ViE.base->StartSend(tbChannel.videoChannel));

    AutoTestSleep(kAutoTestSleepTimeMs);

    unsigned short sentFractionsLost = 0;
    unsigned int sentCumulativeLost = 0;
    unsigned int sentExtendedMax = 0;
    unsigned int sentJitter = 0;
    int sentRttMs = 0;

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

      int passive_channel = -1;
      EXPECT_EQ(ViE.base->CreateReceiveChannel(passive_channel,
                                               tbChannel.videoChannel), 0);
      EXPECT_EQ(ViE.base->StartReceive(passive_channel), 0);
      EXPECT_EQ(
          ViE.rtp_rtcp->GetEstimatedReceiveBandwidth(passive_channel,
                                                     &estimated_bandwidth),
          0);
      EXPECT_EQ(estimated_bandwidth, 0u);
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
      EXPECT_GT(sentFecBitrate, 0u);
      EXPECT_EQ(sentNackBitrate, 0u);
    }

    EXPECT_EQ(0, ViE.base->StopSend(tbChannel.videoChannel));
    EXPECT_EQ(0, ViE.rtp_rtcp->SetFECStatus(
        tbChannel.videoChannel, false, 96, 97));
    EXPECT_EQ(0, ViE.rtp_rtcp->SetNACKStatus(tbChannel.videoChannel, true));
    EXPECT_EQ(0, ViE.base->StartSend(tbChannel.videoChannel));

    AutoTestSleep(4 * kAutoTestSleepTimeMs);

    EXPECT_EQ(0, ViE.rtp_rtcp->GetBandwidthUsage(
        tbChannel.videoChannel, sentTotalBitrate, sentVideoBitrate,
        sentFecBitrate, sentNackBitrate));

    if (FLAGS_include_timing_dependent_tests) {
      EXPECT_GT(sentTotalBitrate, 0u);
      EXPECT_EQ(sentFecBitrate, 0u);

      // TODO(holmer): Test disabled due to being too flaky on buildbots. Tests
      // for new API provide partial coverage.
      // EXPECT_GT(sentNackBitrate, 0u);
    }

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

    EXPECT_EQ(0, ViE.base->StopReceive(tbChannel.videoChannel));

    ViETest::Log("Testing Network Down...\n");

    EXPECT_EQ(0, ViE.rtp_rtcp->SetNACKStatus(tbChannel.videoChannel, true));
    EXPECT_EQ(0, ViE.rtp_rtcp->SetTransmissionSmoothingStatus(
        tbChannel.videoChannel, true));
    unsigned int bytes_sent_before = 0;
    unsigned int packets_sent_before = 0;
    unsigned int bytes_received_before = 0;
    unsigned int packets_received_before = 0;
    unsigned int bytes_sent_after = 0;
    unsigned int packets_sent_after = 0;
    unsigned int bytes_received_after = 0;
    unsigned int packets_received_after = 0;
    EXPECT_EQ(0, ViE.rtp_rtcp->GetRTPStatistics(tbChannel.videoChannel,
                                                bytes_sent_before,
                                                packets_sent_before,
                                                bytes_received_before,
                                                packets_received_before));
    EXPECT_EQ(0, ViE.base->StartReceive(tbChannel.videoChannel));
    EXPECT_EQ(0, ViE.base->StartSend(tbChannel.videoChannel));

    // Real-time mode.
    AutoTestSleep(kAutoTestSleepTimeMs);
    EXPECT_EQ(0, ViE.rtp_rtcp->GetRTPStatistics(tbChannel.videoChannel,
                                                bytes_sent_after,
                                                packets_sent_after,
                                                bytes_received_after,
                                                packets_received_after));
    if (FLAGS_include_timing_dependent_tests) {
      EXPECT_GT(bytes_received_after, bytes_received_before);
    }
    // Simulate lost reception and verify that nothing is sent during that time.
    ViE.network->SetNetworkTransmissionState(tbChannel.videoChannel, false);
    // Allow the encoder to finish the current frame before we expect that no
    // additional packets will be sent.
    AutoTestSleep(kAutoTestSleepTimeMs);
    bytes_received_before = bytes_received_after;
    ViETest::Log("Network Down...\n");
    AutoTestSleep(kAutoTestSleepTimeMs);
    EXPECT_EQ(0, ViE.rtp_rtcp->GetRTPStatistics(tbChannel.videoChannel,
                                                bytes_sent_after,
                                                packets_sent_after,
                                                bytes_received_after,
                                                packets_received_after));
    if (FLAGS_include_timing_dependent_tests) {
      EXPECT_EQ(bytes_received_before, bytes_received_after);
    }

    // Network reception back. Video should now be sent.
    ViE.network->SetNetworkTransmissionState(tbChannel.videoChannel, true);
    ViETest::Log("Network Up...\n");
    AutoTestSleep(kAutoTestSleepTimeMs);
    EXPECT_EQ(0, ViE.rtp_rtcp->GetRTPStatistics(tbChannel.videoChannel,
                                                bytes_sent_before,
                                                packets_sent_before,
                                                bytes_received_before,
                                                packets_received_before));
    if (FLAGS_include_timing_dependent_tests) {
      EXPECT_GT(bytes_received_before, bytes_received_after);
    }
    bytes_received_after = bytes_received_before;
    // Buffering mode.
    EXPECT_EQ(0, ViE.base->StopSend(tbChannel.videoChannel));
    EXPECT_EQ(0, ViE.base->StopReceive(tbChannel.videoChannel));
    ViE.rtp_rtcp->SetSenderBufferingMode(tbChannel.videoChannel,
                                         kAutoTestSleepTimeMs / 2);
    // Add extra delay to the receiver to make sure it doesn't flush due to
    // too old packets being received (as the down-time introduced is longer
    // than what we buffer at the sender).
    ViE.rtp_rtcp->SetReceiverBufferingMode(tbChannel.videoChannel,
                                           3 * kAutoTestSleepTimeMs / 2);
    EXPECT_EQ(0, ViE.base->StartReceive(tbChannel.videoChannel));
    EXPECT_EQ(0, ViE.base->StartSend(tbChannel.videoChannel));
    AutoTestSleep(kAutoTestSleepTimeMs);
    // Simulate lost reception and verify that nothing is sent during that time.
    ViETest::Log("Network Down...\n");
    ViE.network->SetNetworkTransmissionState(tbChannel.videoChannel, false);
    // Allow the encoder to finish the current frame before we expect that no
    // additional packets will be sent.
    AutoTestSleep(kAutoTestSleepTimeMs);
    EXPECT_EQ(0, ViE.rtp_rtcp->GetRTPStatistics(tbChannel.videoChannel,
                                                bytes_sent_before,
                                                packets_sent_before,
                                                bytes_received_before,
                                                packets_received_before));
    if (FLAGS_include_timing_dependent_tests) {
      EXPECT_GT(bytes_received_before, bytes_received_after);
    }
    bytes_received_after = bytes_received_before;
    AutoTestSleep(kAutoTestSleepTimeMs);
    EXPECT_EQ(0, ViE.rtp_rtcp->GetRTPStatistics(tbChannel.videoChannel,
                                                bytes_sent_after,
                                                packets_sent_after,
                                                bytes_received_after,
                                                packets_received_after));
    if (FLAGS_include_timing_dependent_tests) {
      EXPECT_EQ(bytes_received_after, bytes_received_before);
    }
    // Network reception back. Video should now be sent.
    ViETest::Log("Network Up...\n");
    ViE.network->SetNetworkTransmissionState(tbChannel.videoChannel, true);
    AutoTestSleep(kAutoTestSleepTimeMs);
    EXPECT_EQ(0, ViE.rtp_rtcp->GetRTPStatistics(tbChannel.videoChannel,
                                                bytes_sent_before,
                                                packets_sent_before,
                                                bytes_received_before,
                                                packets_received_before));
    if (FLAGS_include_timing_dependent_tests) {
      EXPECT_GT(bytes_received_before, bytes_received_after);
    }
    // TODO(holmer): Verify that the decoded framerate doesn't decrease on an
    // outage when in buffering mode. This isn't currently possible because we
    // don't have an API to get decoded framerate.

    EXPECT_EQ(0, ViE.base->StopSend(tbChannel.videoChannel));
    EXPECT_EQ(0, ViE.base->StopReceive(tbChannel.videoChannel));


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

    EXPECT_EQ(0, ViE.network->DeregisterSendTransport(tbChannel.videoChannel));
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

    // Create a video channel
    TbVideoChannel tbChannel(ViE, webrtc::kVideoCodecVP8);

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

    // Timestamp offset extension.
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
    EXPECT_EQ(0, ViE.rtp_rtcp->SetReceiveTimestampOffsetStatus(
        tbChannel.videoChannel, true, 3));
    EXPECT_EQ(0, ViE.rtp_rtcp->SetReceiveTimestampOffsetStatus(
            tbChannel.videoChannel, false, 3));
    EXPECT_EQ(0, ViE.rtp_rtcp->SetReceiveTimestampOffsetStatus(
        tbChannel.videoChannel, true, 3));
    EXPECT_EQ(0, ViE.rtp_rtcp->SetReceiveTimestampOffsetStatus(
              tbChannel.videoChannel, false, 3));
    EXPECT_EQ(0, ViE.rtp_rtcp->SetReceiveTimestampOffsetStatus(
            tbChannel.videoChannel, false, 3));

    // Absolute send time extension.
    // Valid range is 1 to 14 inclusive.
    EXPECT_EQ(-1, ViE.rtp_rtcp->SetSendAbsoluteSendTimeStatus(
        tbChannel.videoChannel, true, 0));
    EXPECT_EQ(-1, ViE.rtp_rtcp->SetSendAbsoluteSendTimeStatus(
        tbChannel.videoChannel, true, 15));
    EXPECT_EQ(0, ViE.rtp_rtcp->SetSendAbsoluteSendTimeStatus(
        tbChannel.videoChannel, true, 3));
    EXPECT_EQ(0, ViE.rtp_rtcp->SetSendAbsoluteSendTimeStatus(
        tbChannel.videoChannel, true, 3));
    EXPECT_EQ(0, ViE.rtp_rtcp->SetSendAbsoluteSendTimeStatus(
        tbChannel.videoChannel, false, 3));
    EXPECT_EQ(0, ViE.rtp_rtcp->SetSendAbsoluteSendTimeStatus(
        tbChannel.videoChannel, true, 3));
    EXPECT_EQ(0, ViE.rtp_rtcp->SetSendAbsoluteSendTimeStatus(
        tbChannel.videoChannel, false, 3));
    EXPECT_EQ(0, ViE.rtp_rtcp->SetSendAbsoluteSendTimeStatus(
        tbChannel.videoChannel, false, 3));

    EXPECT_EQ(-1, ViE.rtp_rtcp->SetReceiveAbsoluteSendTimeStatus(
        tbChannel.videoChannel, true, 0));
    EXPECT_EQ(-1, ViE.rtp_rtcp->SetReceiveAbsoluteSendTimeStatus(
        tbChannel.videoChannel, true, 15));
    EXPECT_EQ(0, ViE.rtp_rtcp->SetReceiveAbsoluteSendTimeStatus(
        tbChannel.videoChannel, true, 3));
    EXPECT_EQ(0, ViE.rtp_rtcp->SetReceiveAbsoluteSendTimeStatus(
        tbChannel.videoChannel, true, 3));
    EXPECT_EQ(0, ViE.rtp_rtcp->SetReceiveAbsoluteSendTimeStatus(
        tbChannel.videoChannel, false, 3));
    EXPECT_EQ(0, ViE.rtp_rtcp->SetReceiveAbsoluteSendTimeStatus(
        tbChannel.videoChannel, true, 3));
    EXPECT_EQ(0, ViE.rtp_rtcp->SetReceiveAbsoluteSendTimeStatus(
        tbChannel.videoChannel, false, 3));
    EXPECT_EQ(0, ViE.rtp_rtcp->SetReceiveAbsoluteSendTimeStatus(
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

    // Buffering mode - sender side.
    EXPECT_EQ(-1, ViE.rtp_rtcp->SetSenderBufferingMode(
        invalid_channel_id, 0));
    int invalid_delay = -1;
    EXPECT_EQ(-1, ViE.rtp_rtcp->SetSenderBufferingMode(
        tbChannel.videoChannel, invalid_delay));
    invalid_delay = 15000;
    EXPECT_EQ(-1, ViE.rtp_rtcp->SetSenderBufferingMode(
        tbChannel.videoChannel, invalid_delay));
    EXPECT_EQ(0, ViE.rtp_rtcp->SetSenderBufferingMode(
        tbChannel.videoChannel, 5000));

    // Buffering mode - receiver side.
    // Run without VoE to verify it that does not crash, but return an error.
    EXPECT_EQ(-1, ViE.rtp_rtcp->SetReceiverBufferingMode(
        tbChannel.videoChannel, 0));
    EXPECT_EQ(-1, ViE.rtp_rtcp->SetReceiverBufferingMode(
        tbChannel.videoChannel, 2000));

    // Set VoE (required to set up stream-sync).
    webrtc::VoiceEngine* voice_engine = webrtc::VoiceEngine::Create();
    EXPECT_TRUE(NULL != voice_engine);
    webrtc::VoEBase* voe_base = webrtc::VoEBase::GetInterface(voice_engine);
    EXPECT_TRUE(NULL != voe_base);
    EXPECT_EQ(0, voe_base->Init());
    int audio_channel = voe_base->CreateChannel();
    EXPECT_NE(-1, audio_channel);
    EXPECT_EQ(0, ViE.base->SetVoiceEngine(voice_engine));
    EXPECT_EQ(0, ViE.base->ConnectAudioChannel(tbChannel.videoChannel,
                                               audio_channel));

    EXPECT_EQ(-1, ViE.rtp_rtcp->SetReceiverBufferingMode(
        invalid_channel_id, 0));
    EXPECT_EQ(-1, ViE.rtp_rtcp->SetReceiverBufferingMode(
        tbChannel.videoChannel, invalid_delay));
    invalid_delay = 15000;
    EXPECT_EQ(-1, ViE.rtp_rtcp->SetReceiverBufferingMode(
        tbChannel.videoChannel, invalid_delay));
    EXPECT_EQ(0, ViE.rtp_rtcp->SetReceiverBufferingMode(
        tbChannel.videoChannel, 5000));

    // Real-time mode - sender side.
    EXPECT_EQ(0, ViE.rtp_rtcp->SetSenderBufferingMode(
        tbChannel.videoChannel, 0));
    // Real-time mode - receiver side.
    EXPECT_EQ(0, ViE.rtp_rtcp->SetReceiverBufferingMode(
        tbChannel.videoChannel, 0));

    EXPECT_EQ(0, ViE.base->DisconnectAudioChannel(tbChannel.videoChannel));
    EXPECT_EQ(0, ViE.base->SetVoiceEngine(NULL));
    EXPECT_EQ(0, voe_base->DeleteChannel(audio_channel));
    voe_base->Release();
    EXPECT_TRUE(webrtc::VoiceEngine::Delete(voice_engine));

    //***************************************************************
    //  Testing finished. Tear down Video Engine
    //***************************************************************
}
