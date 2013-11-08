/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 * This file includes unit tests for NetEQ.
 */

#include "webrtc/modules/audio_coding/neteq4/interface/neteq.h"

#include <stdlib.h>
#include <string.h>  // memset

#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "webrtc/modules/audio_coding/neteq4/test/NETEQTEST_RTPpacket.h"
#include "webrtc/test/testsupport/fileutils.h"
#include "webrtc/test/testsupport/gtest_disable.h"
#include "webrtc/typedefs.h"

namespace webrtc {

class RefFiles {
 public:
  RefFiles(const std::string& input_file, const std::string& output_file);
  ~RefFiles();
  template<class T> void ProcessReference(const T& test_results);
  template<typename T, size_t n> void ProcessReference(
      const T (&test_results)[n],
      size_t length);
  template<typename T, size_t n> void WriteToFile(
      const T (&test_results)[n],
      size_t length);
  template<typename T, size_t n> void ReadFromFileAndCompare(
      const T (&test_results)[n],
      size_t length);
  void WriteToFile(const NetEqNetworkStatistics& stats);
  void ReadFromFileAndCompare(const NetEqNetworkStatistics& stats);
  void WriteToFile(const RtcpStatistics& stats);
  void ReadFromFileAndCompare(const RtcpStatistics& stats);

  FILE* input_fp_;
  FILE* output_fp_;
};

RefFiles::RefFiles(const std::string &input_file,
                   const std::string &output_file)
    : input_fp_(NULL),
      output_fp_(NULL) {
  if (!input_file.empty()) {
    input_fp_ = fopen(input_file.c_str(), "rb");
    EXPECT_TRUE(input_fp_ != NULL);
  }
  if (!output_file.empty()) {
    output_fp_ = fopen(output_file.c_str(), "wb");
    EXPECT_TRUE(output_fp_ != NULL);
  }
}

RefFiles::~RefFiles() {
  if (input_fp_) {
    EXPECT_EQ(EOF, fgetc(input_fp_));  // Make sure that we reached the end.
    fclose(input_fp_);
  }
  if (output_fp_) fclose(output_fp_);
}

template<class T>
void RefFiles::ProcessReference(const T& test_results) {
  WriteToFile(test_results);
  ReadFromFileAndCompare(test_results);
}

template<typename T, size_t n>
void RefFiles::ProcessReference(const T (&test_results)[n], size_t length) {
  WriteToFile(test_results, length);
  ReadFromFileAndCompare(test_results, length);
}

template<typename T, size_t n>
void RefFiles::WriteToFile(const T (&test_results)[n], size_t length) {
  if (output_fp_) {
    ASSERT_EQ(length, fwrite(&test_results, sizeof(T), length, output_fp_));
  }
}

template<typename T, size_t n>
void RefFiles::ReadFromFileAndCompare(const T (&test_results)[n],
                                      size_t length) {
  if (input_fp_) {
    // Read from ref file.
    T* ref = new T[length];
    ASSERT_EQ(length, fread(ref, sizeof(T), length, input_fp_));
    // Compare
    ASSERT_EQ(0, memcmp(&test_results, ref, sizeof(T) * length));
    delete [] ref;
  }
}

void RefFiles::WriteToFile(const NetEqNetworkStatistics& stats) {
  if (output_fp_) {
    ASSERT_EQ(1u, fwrite(&stats, sizeof(NetEqNetworkStatistics), 1,
                         output_fp_));
  }
}

void RefFiles::ReadFromFileAndCompare(
    const NetEqNetworkStatistics& stats) {
  if (input_fp_) {
    // Read from ref file.
    size_t stat_size = sizeof(NetEqNetworkStatistics);
    NetEqNetworkStatistics ref_stats;
    ASSERT_EQ(1u, fread(&ref_stats, stat_size, 1, input_fp_));
    // Compare
    EXPECT_EQ(0, memcmp(&stats, &ref_stats, stat_size));
  }
}

void RefFiles::WriteToFile(const RtcpStatistics& stats) {
  if (output_fp_) {
    ASSERT_EQ(1u, fwrite(&(stats.fraction_lost), sizeof(stats.fraction_lost), 1,
                         output_fp_));
    ASSERT_EQ(1u, fwrite(&(stats.cumulative_lost),
                         sizeof(stats.cumulative_lost), 1, output_fp_));
    ASSERT_EQ(1u, fwrite(&(stats.extended_max), sizeof(stats.extended_max), 1,
                         output_fp_));
    ASSERT_EQ(1u, fwrite(&(stats.jitter), sizeof(stats.jitter), 1,
                         output_fp_));
  }
}

void RefFiles::ReadFromFileAndCompare(
    const RtcpStatistics& stats) {
  if (input_fp_) {
    // Read from ref file.
    RtcpStatistics ref_stats;
    ASSERT_EQ(1u, fread(&(ref_stats.fraction_lost),
                        sizeof(ref_stats.fraction_lost), 1, input_fp_));
    ASSERT_EQ(1u, fread(&(ref_stats.cumulative_lost),
                        sizeof(ref_stats.cumulative_lost), 1, input_fp_));
    ASSERT_EQ(1u, fread(&(ref_stats.extended_max),
                        sizeof(ref_stats.extended_max), 1, input_fp_));
    ASSERT_EQ(1u, fread(&(ref_stats.jitter), sizeof(ref_stats.jitter), 1,
                        input_fp_));
    // Compare
    EXPECT_EQ(ref_stats.fraction_lost, stats.fraction_lost);
    EXPECT_EQ(ref_stats.cumulative_lost, stats.cumulative_lost);
    EXPECT_EQ(ref_stats.extended_max, stats.extended_max);
    EXPECT_EQ(ref_stats.jitter, stats.jitter);
  }
}

class NetEqDecodingTest : public ::testing::Test {
 protected:
  // NetEQ must be polled for data once every 10 ms. Thus, neither of the
  // constants below can be changed.
  static const int kTimeStepMs = 10;
  static const int kBlockSize8kHz = kTimeStepMs * 8;
  static const int kBlockSize16kHz = kTimeStepMs * 16;
  static const int kBlockSize32kHz = kTimeStepMs * 32;
  static const int kMaxBlockSize = kBlockSize32kHz;
  static const int kInitSampleRateHz = 8000;

  NetEqDecodingTest();
  virtual void SetUp();
  virtual void TearDown();
  void SelectDecoders(NetEqDecoder* used_codec);
  void LoadDecoders();
  void OpenInputFile(const std::string &rtp_file);
  void Process(NETEQTEST_RTPpacket* rtp_ptr, int* out_len);
  void DecodeAndCompare(const std::string &rtp_file,
                        const std::string &ref_file);
  void DecodeAndCheckStats(const std::string &rtp_file,
                           const std::string &stat_ref_file,
                           const std::string &rtcp_ref_file);
  static void PopulateRtpInfo(int frame_index,
                              int timestamp,
                              WebRtcRTPHeader* rtp_info);
  static void PopulateCng(int frame_index,
                          int timestamp,
                          WebRtcRTPHeader* rtp_info,
                          uint8_t* payload,
                          int* payload_len);

  NetEq* neteq_;
  FILE* rtp_fp_;
  unsigned int sim_clock_;
  int16_t out_data_[kMaxBlockSize];
  int output_sample_rate_;
};

// Allocating the static const so that it can be passed by reference.
const int NetEqDecodingTest::kTimeStepMs;
const int NetEqDecodingTest::kBlockSize8kHz;
const int NetEqDecodingTest::kBlockSize16kHz;
const int NetEqDecodingTest::kBlockSize32kHz;
const int NetEqDecodingTest::kMaxBlockSize;
const int NetEqDecodingTest::kInitSampleRateHz;

NetEqDecodingTest::NetEqDecodingTest()
    : neteq_(NULL),
      rtp_fp_(NULL),
      sim_clock_(0),
      output_sample_rate_(kInitSampleRateHz) {
  memset(out_data_, 0, sizeof(out_data_));
}

void NetEqDecodingTest::SetUp() {
  neteq_ = NetEq::Create(kInitSampleRateHz);
  ASSERT_TRUE(neteq_);
  LoadDecoders();
}

void NetEqDecodingTest::TearDown() {
  delete neteq_;
  if (rtp_fp_)
    fclose(rtp_fp_);
}

void NetEqDecodingTest::LoadDecoders() {
  // Load PCMu.
  ASSERT_EQ(0, neteq_->RegisterPayloadType(kDecoderPCMu, 0));
  // Load PCMa.
  ASSERT_EQ(0, neteq_->RegisterPayloadType(kDecoderPCMa, 8));
#ifndef WEBRTC_ANDROID
  // Load iLBC.
  ASSERT_EQ(0, neteq_->RegisterPayloadType(kDecoderILBC, 102));
#endif  // WEBRTC_ANDROID
  // Load iSAC.
  ASSERT_EQ(0, neteq_->RegisterPayloadType(kDecoderISAC, 103));
  // Load iSAC SWB.
  ASSERT_EQ(0, neteq_->RegisterPayloadType(kDecoderISACswb, 104));
  // Load iSAC FB.
  ASSERT_EQ(0, neteq_->RegisterPayloadType(kDecoderISACfb, 105));
  // Load PCM16B nb.
  ASSERT_EQ(0, neteq_->RegisterPayloadType(kDecoderPCM16B, 93));
  // Load PCM16B wb.
  ASSERT_EQ(0, neteq_->RegisterPayloadType(kDecoderPCM16Bwb, 94));
  // Load PCM16B swb32.
  ASSERT_EQ(0, neteq_->RegisterPayloadType(kDecoderPCM16Bswb32kHz, 95));
  // Load CNG 8 kHz.
  ASSERT_EQ(0, neteq_->RegisterPayloadType(kDecoderCNGnb, 13));
  // Load CNG 16 kHz.
  ASSERT_EQ(0, neteq_->RegisterPayloadType(kDecoderCNGwb, 98));
}

void NetEqDecodingTest::OpenInputFile(const std::string &rtp_file) {
  rtp_fp_ = fopen(rtp_file.c_str(), "rb");
  ASSERT_TRUE(rtp_fp_ != NULL);
  ASSERT_EQ(0, NETEQTEST_RTPpacket::skipFileHeader(rtp_fp_));
}

void NetEqDecodingTest::Process(NETEQTEST_RTPpacket* rtp, int* out_len) {
  // Check if time to receive.
  while ((sim_clock_ >= rtp->time()) &&
         (rtp->dataLen() >= 0)) {
    if (rtp->dataLen() > 0) {
      WebRtcRTPHeader rtpInfo;
      rtp->parseHeader(&rtpInfo);
      ASSERT_EQ(0, neteq_->InsertPacket(
          rtpInfo,
          rtp->payload(),
          rtp->payloadLen(),
          rtp->time() * (output_sample_rate_ / 1000)));
    }
    // Get next packet.
    ASSERT_NE(-1, rtp->readFromFile(rtp_fp_));
  }

  // Get audio from NetEq.
  NetEqOutputType type;
  int num_channels;
  ASSERT_EQ(0, neteq_->GetAudio(kMaxBlockSize, out_data_, out_len,
                                &num_channels, &type));
  ASSERT_TRUE((*out_len == kBlockSize8kHz) ||
              (*out_len == kBlockSize16kHz) ||
              (*out_len == kBlockSize32kHz));
  output_sample_rate_ = *out_len / 10 * 1000;

  // Increase time.
  sim_clock_ += kTimeStepMs;
}

void NetEqDecodingTest::DecodeAndCompare(const std::string &rtp_file,
                                         const std::string &ref_file) {
  OpenInputFile(rtp_file);

  std::string ref_out_file = "";
  if (ref_file.empty()) {
    ref_out_file = webrtc::test::OutputPath() + "neteq_out.pcm";
  }
  RefFiles ref_files(ref_file, ref_out_file);

  NETEQTEST_RTPpacket rtp;
  ASSERT_GT(rtp.readFromFile(rtp_fp_), 0);
  int i = 0;
  while (rtp.dataLen() >= 0) {
    std::ostringstream ss;
    ss << "Lap number " << i++ << " in DecodeAndCompare while loop";
    SCOPED_TRACE(ss.str());  // Print out the parameter values on failure.
    int out_len;
    ASSERT_NO_FATAL_FAILURE(Process(&rtp, &out_len));
    ASSERT_NO_FATAL_FAILURE(ref_files.ProcessReference(out_data_, out_len));
  }
}

void NetEqDecodingTest::DecodeAndCheckStats(const std::string &rtp_file,
                                            const std::string &stat_ref_file,
                                            const std::string &rtcp_ref_file) {
  OpenInputFile(rtp_file);
  std::string stat_out_file = "";
  if (stat_ref_file.empty()) {
    stat_out_file = webrtc::test::OutputPath() +
        "neteq_network_stats.dat";
  }
  RefFiles network_stat_files(stat_ref_file, stat_out_file);

  std::string rtcp_out_file = "";
  if (rtcp_ref_file.empty()) {
    rtcp_out_file = webrtc::test::OutputPath() +
        "neteq_rtcp_stats.dat";
  }
  RefFiles rtcp_stat_files(rtcp_ref_file, rtcp_out_file);

  NETEQTEST_RTPpacket rtp;
  ASSERT_GT(rtp.readFromFile(rtp_fp_), 0);
  while (rtp.dataLen() >= 0) {
    int out_len;
    Process(&rtp, &out_len);

    // Query the network statistics API once per second
    if (sim_clock_ % 1000 == 0) {
      // Process NetworkStatistics.
      NetEqNetworkStatistics network_stats;
      ASSERT_EQ(0, neteq_->NetworkStatistics(&network_stats));
      network_stat_files.ProcessReference(network_stats);

      // Process RTCPstat.
      RtcpStatistics rtcp_stats;
      neteq_->GetRtcpStatistics(&rtcp_stats);
      rtcp_stat_files.ProcessReference(rtcp_stats);
    }
  }
}

void NetEqDecodingTest::PopulateRtpInfo(int frame_index,
                                        int timestamp,
                                        WebRtcRTPHeader* rtp_info) {
  rtp_info->header.sequenceNumber = frame_index;
  rtp_info->header.timestamp = timestamp;
  rtp_info->header.ssrc = 0x1234;  // Just an arbitrary SSRC.
  rtp_info->header.payloadType = 94;  // PCM16b WB codec.
  rtp_info->header.markerBit = 0;
}

void NetEqDecodingTest::PopulateCng(int frame_index,
                                    int timestamp,
                                    WebRtcRTPHeader* rtp_info,
                                    uint8_t* payload,
                                    int* payload_len) {
  rtp_info->header.sequenceNumber = frame_index;
  rtp_info->header.timestamp = timestamp;
  rtp_info->header.ssrc = 0x1234;  // Just an arbitrary SSRC.
  rtp_info->header.payloadType = 98;  // WB CNG.
  rtp_info->header.markerBit = 0;
  payload[0] = 64;  // Noise level -64 dBov, quite arbitrarily chosen.
  *payload_len = 1;  // Only noise level, no spectral parameters.
}

#if defined(_WIN32) && defined(WEBRTC_ARCH_64_BITS)
// Disabled for Windows 64-bit until webrtc:1458 is fixed.
#define MAYBE_TestBitExactness DISABLED_TestBitExactness
#else
#define MAYBE_TestBitExactness TestBitExactness
#endif

TEST_F(NetEqDecodingTest, DISABLED_ON_ANDROID(MAYBE_TestBitExactness)) {
  const std::string kInputRtpFile = webrtc::test::ProjectRootPath() +
      "resources/audio_coding/neteq_universal_new.rtp";
#if defined(_MSC_VER) && (_MSC_VER >= 1700)
  // For Visual Studio 2012 and later, we will have to use the generic reference
  // file, rather than the windows-specific one.
  const std::string kInputRefFile = webrtc::test::ProjectRootPath() +
      "resources/audio_coding/neteq_universal_ref.pcm";
#else
  const std::string kInputRefFile =
      webrtc::test::ResourcePath("audio_coding/neteq_universal_ref", "pcm");
#endif
  DecodeAndCompare(kInputRtpFile, kInputRefFile);
}

TEST_F(NetEqDecodingTest, DISABLED_ON_ANDROID(TestNetworkStatistics)) {
  const std::string kInputRtpFile = webrtc::test::ProjectRootPath() +
      "resources/audio_coding/neteq_universal_new.rtp";
#if defined(_MSC_VER) && (_MSC_VER >= 1700)
  // For Visual Studio 2012 and later, we will have to use the generic reference
  // file, rather than the windows-specific one.
  const std::string kNetworkStatRefFile = webrtc::test::ProjectRootPath() +
      "resources/audio_coding/neteq_network_stats.dat";
#else
  const std::string kNetworkStatRefFile =
      webrtc::test::ResourcePath("audio_coding/neteq_network_stats", "dat");
#endif
  const std::string kRtcpStatRefFile =
      webrtc::test::ResourcePath("audio_coding/neteq_rtcp_stats", "dat");
  DecodeAndCheckStats(kInputRtpFile, kNetworkStatRefFile, kRtcpStatRefFile);
}

// TODO(hlundin): Re-enable test once the statistics interface is up and again.
TEST_F(NetEqDecodingTest, DISABLED_ON_ANDROID(TestFrameWaitingTimeStatistics)) {
  // Use fax mode to avoid time-scaling. This is to simplify the testing of
  // packet waiting times in the packet buffer.
  neteq_->SetPlayoutMode(kPlayoutFax);
  ASSERT_EQ(kPlayoutFax, neteq_->PlayoutMode());
  // Insert 30 dummy packets at once. Each packet contains 10 ms 16 kHz audio.
  size_t num_frames = 30;
  const int kSamples = 10 * 16;
  const int kPayloadBytes = kSamples * 2;
  for (size_t i = 0; i < num_frames; ++i) {
    uint16_t payload[kSamples] = {0};
    WebRtcRTPHeader rtp_info;
    rtp_info.header.sequenceNumber = i;
    rtp_info.header.timestamp = i * kSamples;
    rtp_info.header.ssrc = 0x1234;  // Just an arbitrary SSRC.
    rtp_info.header.payloadType = 94;  // PCM16b WB codec.
    rtp_info.header.markerBit = 0;
    ASSERT_EQ(0, neteq_->InsertPacket(
        rtp_info,
        reinterpret_cast<uint8_t*>(payload),
        kPayloadBytes, 0));
  }
  // Pull out all data.
  for (size_t i = 0; i < num_frames; ++i) {
    int out_len;
    int num_channels;
    NetEqOutputType type;
    ASSERT_EQ(0, neteq_->GetAudio(kMaxBlockSize, out_data_, &out_len,
                                  &num_channels, &type));
    ASSERT_EQ(kBlockSize16kHz, out_len);
  }

  std::vector<int> waiting_times;
  neteq_->WaitingTimes(&waiting_times);
  int len = waiting_times.size();
  EXPECT_EQ(num_frames, waiting_times.size());
  // Since all frames are dumped into NetEQ at once, but pulled out with 10 ms
  // spacing (per definition), we expect the delay to increase with 10 ms for
  // each packet.
  for (size_t i = 0; i < waiting_times.size(); ++i) {
    EXPECT_EQ(static_cast<int>(i + 1) * 10, waiting_times[i]);
  }

  // Check statistics again and make sure it's been reset.
  neteq_->WaitingTimes(&waiting_times);
  len = waiting_times.size();
  EXPECT_EQ(0, len);

  // Process > 100 frames, and make sure that that we get statistics
  // only for 100 frames. Note the new SSRC, causing NetEQ to reset.
  num_frames = 110;
  for (size_t i = 0; i < num_frames; ++i) {
    uint16_t payload[kSamples] = {0};
    WebRtcRTPHeader rtp_info;
    rtp_info.header.sequenceNumber = i;
    rtp_info.header.timestamp = i * kSamples;
    rtp_info.header.ssrc = 0x1235;  // Just an arbitrary SSRC.
    rtp_info.header.payloadType = 94;  // PCM16b WB codec.
    rtp_info.header.markerBit = 0;
    ASSERT_EQ(0, neteq_->InsertPacket(
        rtp_info,
        reinterpret_cast<uint8_t*>(payload),
        kPayloadBytes, 0));
    int out_len;
    int num_channels;
    NetEqOutputType type;
    ASSERT_EQ(0, neteq_->GetAudio(kMaxBlockSize, out_data_, &out_len,
                                  &num_channels, &type));
    ASSERT_EQ(kBlockSize16kHz, out_len);
  }

  neteq_->WaitingTimes(&waiting_times);
  EXPECT_EQ(100u, waiting_times.size());
}

TEST_F(NetEqDecodingTest,
       DISABLED_ON_ANDROID(TestAverageInterArrivalTimeNegative)) {
  const int kNumFrames = 3000;  // Needed for convergence.
  int frame_index = 0;
  const int kSamples = 10 * 16;
  const int kPayloadBytes = kSamples * 2;
  while (frame_index < kNumFrames) {
    // Insert one packet each time, except every 10th time where we insert two
    // packets at once. This will create a negative clock-drift of approx. 10%.
    int num_packets = (frame_index % 10 == 0 ? 2 : 1);
    for (int n = 0; n < num_packets; ++n) {
      uint8_t payload[kPayloadBytes] = {0};
      WebRtcRTPHeader rtp_info;
      PopulateRtpInfo(frame_index, frame_index * kSamples, &rtp_info);
      ASSERT_EQ(0, neteq_->InsertPacket(rtp_info, payload, kPayloadBytes, 0));
      ++frame_index;
    }

    // Pull out data once.
    int out_len;
    int num_channels;
    NetEqOutputType type;
    ASSERT_EQ(0, neteq_->GetAudio(kMaxBlockSize, out_data_, &out_len,
                                  &num_channels, &type));
    ASSERT_EQ(kBlockSize16kHz, out_len);
  }

  NetEqNetworkStatistics network_stats;
  ASSERT_EQ(0, neteq_->NetworkStatistics(&network_stats));
  EXPECT_EQ(-103196, network_stats.clockdrift_ppm);
}

TEST_F(NetEqDecodingTest,
       DISABLED_ON_ANDROID(TestAverageInterArrivalTimePositive)) {
  const int kNumFrames = 5000;  // Needed for convergence.
  int frame_index = 0;
  const int kSamples = 10 * 16;
  const int kPayloadBytes = kSamples * 2;
  for (int i = 0; i < kNumFrames; ++i) {
    // Insert one packet each time, except every 10th time where we don't insert
    // any packet. This will create a positive clock-drift of approx. 11%.
    int num_packets = (i % 10 == 9 ? 0 : 1);
    for (int n = 0; n < num_packets; ++n) {
      uint8_t payload[kPayloadBytes] = {0};
      WebRtcRTPHeader rtp_info;
      PopulateRtpInfo(frame_index, frame_index * kSamples, &rtp_info);
      ASSERT_EQ(0, neteq_->InsertPacket(rtp_info, payload, kPayloadBytes, 0));
      ++frame_index;
    }

    // Pull out data once.
    int out_len;
    int num_channels;
    NetEqOutputType type;
    ASSERT_EQ(0, neteq_->GetAudio(kMaxBlockSize, out_data_, &out_len,
                                  &num_channels, &type));
    ASSERT_EQ(kBlockSize16kHz, out_len);
  }

  NetEqNetworkStatistics network_stats;
  ASSERT_EQ(0, neteq_->NetworkStatistics(&network_stats));
  EXPECT_EQ(110946, network_stats.clockdrift_ppm);
}

TEST_F(NetEqDecodingTest, DISABLED_ON_ANDROID(LongCngWithClockDrift)) {
  uint16_t seq_no = 0;
  uint32_t timestamp = 0;
  const int kFrameSizeMs = 30;
  const int kSamples = kFrameSizeMs * 16;
  const int kPayloadBytes = kSamples * 2;
  // Apply a clock drift of -25 ms / s (sender faster than receiver).
  const double kDriftFactor = 1000.0 / (1000.0 + 25.0);
  double next_input_time_ms = 0.0;
  double t_ms;
  NetEqOutputType type;

  // Insert speech for 5 seconds.
  const int kSpeechDurationMs = 5000;
  for (t_ms = 0; t_ms < kSpeechDurationMs; t_ms += 10) {
    // Each turn in this for loop is 10 ms.
    while (next_input_time_ms <= t_ms) {
      // Insert one 30 ms speech frame.
      uint8_t payload[kPayloadBytes] = {0};
      WebRtcRTPHeader rtp_info;
      PopulateRtpInfo(seq_no, timestamp, &rtp_info);
      ASSERT_EQ(0, neteq_->InsertPacket(rtp_info, payload, kPayloadBytes, 0));
      ++seq_no;
      timestamp += kSamples;
      next_input_time_ms += static_cast<double>(kFrameSizeMs) * kDriftFactor;
    }
    // Pull out data once.
    int out_len;
    int num_channels;
    ASSERT_EQ(0, neteq_->GetAudio(kMaxBlockSize, out_data_, &out_len,
                                  &num_channels, &type));
    ASSERT_EQ(kBlockSize16kHz, out_len);
  }

  EXPECT_EQ(kOutputNormal, type);
  int32_t delay_before = timestamp - neteq_->PlayoutTimestamp();

  // Insert CNG for 1 minute (= 60000 ms).
  const int kCngPeriodMs = 100;
  const int kCngPeriodSamples = kCngPeriodMs * 16;  // Period in 16 kHz samples.
  const int kCngDurationMs = 60000;
  for (; t_ms < kSpeechDurationMs + kCngDurationMs; t_ms += 10) {
    // Each turn in this for loop is 10 ms.
    while (next_input_time_ms <= t_ms) {
      // Insert one CNG frame each 100 ms.
      uint8_t payload[kPayloadBytes];
      int payload_len;
      WebRtcRTPHeader rtp_info;
      PopulateCng(seq_no, timestamp, &rtp_info, payload, &payload_len);
      ASSERT_EQ(0, neteq_->InsertPacket(rtp_info, payload, payload_len, 0));
      ++seq_no;
      timestamp += kCngPeriodSamples;
      next_input_time_ms += static_cast<double>(kCngPeriodMs) * kDriftFactor;
    }
    // Pull out data once.
    int out_len;
    int num_channels;
    ASSERT_EQ(0, neteq_->GetAudio(kMaxBlockSize, out_data_, &out_len,
                                  &num_channels, &type));
    ASSERT_EQ(kBlockSize16kHz, out_len);
  }

  EXPECT_EQ(kOutputCNG, type);

  // Insert speech again until output type is speech.
  while (type != kOutputNormal) {
    // Each turn in this for loop is 10 ms.
    while (next_input_time_ms <= t_ms) {
      // Insert one 30 ms speech frame.
      uint8_t payload[kPayloadBytes] = {0};
      WebRtcRTPHeader rtp_info;
      PopulateRtpInfo(seq_no, timestamp, &rtp_info);
      ASSERT_EQ(0, neteq_->InsertPacket(rtp_info, payload, kPayloadBytes, 0));
      ++seq_no;
      timestamp += kSamples;
      next_input_time_ms += static_cast<double>(kFrameSizeMs) * kDriftFactor;
    }
    // Pull out data once.
    int out_len;
    int num_channels;
    ASSERT_EQ(0, neteq_->GetAudio(kMaxBlockSize, out_data_, &out_len,
                                  &num_channels, &type));
    ASSERT_EQ(kBlockSize16kHz, out_len);
    // Increase clock.
    t_ms += 10;
  }

  int32_t delay_after = timestamp - neteq_->PlayoutTimestamp();
  // Compare delay before and after, and make sure it differs less than 20 ms.
  EXPECT_LE(delay_after, delay_before + 20 * 16);
  EXPECT_GE(delay_after, delay_before - 20 * 16);
}

TEST_F(NetEqDecodingTest, DISABLED_ON_ANDROID(UnknownPayloadType)) {
  const int kPayloadBytes = 100;
  uint8_t payload[kPayloadBytes] = {0};
  WebRtcRTPHeader rtp_info;
  PopulateRtpInfo(0, 0, &rtp_info);
  rtp_info.header.payloadType = 1;  // Not registered as a decoder.
  EXPECT_EQ(NetEq::kFail,
            neteq_->InsertPacket(rtp_info, payload, kPayloadBytes, 0));
  EXPECT_EQ(NetEq::kUnknownRtpPayloadType, neteq_->LastError());
}

TEST_F(NetEqDecodingTest, DISABLED_ON_ANDROID(OversizePacket)) {
  // Payload size is greater than packet buffer size
  const int kPayloadBytes = NetEq::kMaxBytesInBuffer + 1;
  uint8_t payload[kPayloadBytes] = {0};
  WebRtcRTPHeader rtp_info;
  PopulateRtpInfo(0, 0, &rtp_info);
  rtp_info.header.payloadType = 103;  // iSAC, no packet splitting.
  EXPECT_EQ(NetEq::kFail,
            neteq_->InsertPacket(rtp_info, payload, kPayloadBytes, 0));
  EXPECT_EQ(NetEq::kOversizePacket, neteq_->LastError());
}

TEST_F(NetEqDecodingTest, DISABLED_ON_ANDROID(DecoderError)) {
  const int kPayloadBytes = 100;
  uint8_t payload[kPayloadBytes] = {0};
  WebRtcRTPHeader rtp_info;
  PopulateRtpInfo(0, 0, &rtp_info);
  rtp_info.header.payloadType = 103;  // iSAC, but the payload is invalid.
  EXPECT_EQ(0, neteq_->InsertPacket(rtp_info, payload, kPayloadBytes, 0));
  NetEqOutputType type;
  // Set all of |out_data_| to 1, and verify that it was set to 0 by the call
  // to GetAudio.
  for (int i = 0; i < kMaxBlockSize; ++i) {
    out_data_[i] = 1;
  }
  int num_channels;
  int samples_per_channel;
  EXPECT_EQ(NetEq::kFail,
            neteq_->GetAudio(kMaxBlockSize, out_data_,
                             &samples_per_channel, &num_channels, &type));
  // Verify that there is a decoder error to check.
  EXPECT_EQ(NetEq::kDecoderErrorCode, neteq_->LastError());
  // Code 6730 is an iSAC error code.
  EXPECT_EQ(6730, neteq_->LastDecoderError());
  // Verify that the first 160 samples are set to 0, and that the remaining
  // samples are left unmodified.
  static const int kExpectedOutputLength = 160;  // 10 ms at 16 kHz sample rate.
  for (int i = 0; i < kExpectedOutputLength; ++i) {
    std::ostringstream ss;
    ss << "i = " << i;
    SCOPED_TRACE(ss.str());  // Print out the parameter values on failure.
    EXPECT_EQ(0, out_data_[i]);
  }
  for (int i = kExpectedOutputLength; i < kMaxBlockSize; ++i) {
    std::ostringstream ss;
    ss << "i = " << i;
    SCOPED_TRACE(ss.str());  // Print out the parameter values on failure.
    EXPECT_EQ(1, out_data_[i]);
  }
}

TEST_F(NetEqDecodingTest, DISABLED_ON_ANDROID(GetAudioBeforeInsertPacket)) {
  NetEqOutputType type;
  // Set all of |out_data_| to 1, and verify that it was set to 0 by the call
  // to GetAudio.
  for (int i = 0; i < kMaxBlockSize; ++i) {
    out_data_[i] = 1;
  }
  int num_channels;
  int samples_per_channel;
  EXPECT_EQ(0, neteq_->GetAudio(kMaxBlockSize, out_data_,
                                &samples_per_channel,
                                &num_channels, &type));
  // Verify that the first block of samples is set to 0.
  static const int kExpectedOutputLength =
      kInitSampleRateHz / 100;  // 10 ms at initial sample rate.
  for (int i = 0; i < kExpectedOutputLength; ++i) {
    std::ostringstream ss;
    ss << "i = " << i;
    SCOPED_TRACE(ss.str());  // Print out the parameter values on failure.
    EXPECT_EQ(0, out_data_[i]);
  }
}
}  // namespace
