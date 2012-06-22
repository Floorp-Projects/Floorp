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

#include <stdlib.h>
#include <string.h>  // memset

#include <string>
#include <vector>

#include "gtest/gtest.h"

#include "modules/audio_coding/neteq/interface/webrtc_neteq.h"
#include "modules/audio_coding/neteq/interface/webrtc_neteq_help_macros.h"
#include "modules/audio_coding/neteq/interface/webrtc_neteq_internal.h"
#include "modules/audio_coding/neteq/test/NETEQTEST_CodecClass.h"
#include "modules/audio_coding/neteq/test/NETEQTEST_NetEQClass.h"
#include "modules/audio_coding/neteq/test/NETEQTEST_RTPpacket.h"
#include "testsupport/fileutils.h"
#include "typedefs.h"  // NOLINT(build/include)

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
  void WriteToFile(const WebRtcNetEQ_NetworkStatistics& stats);
  void ReadFromFileAndCompare(const WebRtcNetEQ_NetworkStatistics& stats);
  void WriteToFile(const WebRtcNetEQ_RTCPStat& stats);
  void ReadFromFileAndCompare(const WebRtcNetEQ_RTCPStat& stats);

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
    EXPECT_EQ(0, memcmp(&test_results, ref, sizeof(T) * length));
    delete [] ref;
  }
}

void RefFiles::WriteToFile(const WebRtcNetEQ_NetworkStatistics& stats) {
  if (output_fp_) {
    ASSERT_EQ(1u, fwrite(&stats, sizeof(WebRtcNetEQ_NetworkStatistics), 1,
                         output_fp_));
  }
}

void RefFiles::ReadFromFileAndCompare(
    const WebRtcNetEQ_NetworkStatistics& stats) {
  if (input_fp_) {
    // Read from ref file.
    size_t stat_size = sizeof(WebRtcNetEQ_NetworkStatistics);
    WebRtcNetEQ_NetworkStatistics ref_stats;
    ASSERT_EQ(1u, fread(&ref_stats, stat_size, 1, input_fp_));
    // Compare
    EXPECT_EQ(0, memcmp(&stats, &ref_stats, stat_size));
  }
}

void RefFiles::WriteToFile(const WebRtcNetEQ_RTCPStat& stats) {
  if (output_fp_) {
    ASSERT_EQ(1u, fwrite(&(stats.fraction_lost), sizeof(stats.fraction_lost), 1,
                         output_fp_));
    ASSERT_EQ(1u, fwrite(&(stats.cum_lost), sizeof(stats.cum_lost), 1,
                         output_fp_));
    ASSERT_EQ(1u, fwrite(&(stats.ext_max), sizeof(stats.ext_max), 1,
                         output_fp_));
    ASSERT_EQ(1u, fwrite(&(stats.jitter), sizeof(stats.jitter), 1,
                         output_fp_));
  }
}

void RefFiles::ReadFromFileAndCompare(
    const WebRtcNetEQ_RTCPStat& stats) {
  if (input_fp_) {
    // Read from ref file.
    WebRtcNetEQ_RTCPStat ref_stats;
    ASSERT_EQ(1u, fread(&(ref_stats.fraction_lost),
                        sizeof(ref_stats.fraction_lost), 1, input_fp_));
    ASSERT_EQ(1u, fread(&(ref_stats.cum_lost), sizeof(ref_stats.cum_lost), 1,
                        input_fp_));
    ASSERT_EQ(1u, fread(&(ref_stats.ext_max), sizeof(ref_stats.ext_max), 1,
                        input_fp_));
    ASSERT_EQ(1u, fread(&(ref_stats.jitter), sizeof(ref_stats.jitter), 1,
                        input_fp_));
    // Compare
    EXPECT_EQ(ref_stats.fraction_lost, stats.fraction_lost);
    EXPECT_EQ(ref_stats.cum_lost, stats.cum_lost);
    EXPECT_EQ(ref_stats.ext_max, stats.ext_max);
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

  NetEqDecodingTest();
  virtual void SetUp();
  virtual void TearDown();
  void SelectDecoders(WebRtcNetEQDecoder* used_codec);
  void LoadDecoders();
  void OpenInputFile(const std::string &rtp_file);
  void Process(NETEQTEST_RTPpacket* rtp_ptr, int16_t* out_len);
  void DecodeAndCompare(const std::string &rtp_file,
                        const std::string &ref_file);
  void DecodeAndCheckStats(const std::string &rtp_file,
                           const std::string &stat_ref_file,
                           const std::string &rtcp_ref_file);
  static void PopulateRtpInfo(int frame_index,
                              int timestamp,
                              WebRtcNetEQ_RTPInfo* rtp_info);
  static void PopulateCng(int frame_index,
                          int timestamp,
                          WebRtcNetEQ_RTPInfo* rtp_info,
                          uint8_t* payload,
                          int* payload_len);

  NETEQTEST_NetEQClass* neteq_inst_;
  std::vector<NETEQTEST_Decoder*> dec_;
  FILE* rtp_fp_;
  unsigned int sim_clock_;
  int16_t out_data_[kMaxBlockSize];
};

NetEqDecodingTest::NetEqDecodingTest()
    : neteq_inst_(NULL),
      rtp_fp_(NULL),
      sim_clock_(0) {
  memset(out_data_, 0, sizeof(out_data_));
}

void NetEqDecodingTest::SetUp() {
  WebRtcNetEQDecoder usedCodec[kDecoderReservedEnd - 1];

  SelectDecoders(usedCodec);
  neteq_inst_ = new NETEQTEST_NetEQClass(usedCodec, dec_.size(), 8000,
                                         kTCPLargeJitter);
  ASSERT_TRUE(neteq_inst_);
  LoadDecoders();
}

void NetEqDecodingTest::TearDown() {
  if (neteq_inst_)
    delete neteq_inst_;
  for (size_t i = 0; i < dec_.size(); ++i) {
    if (dec_[i])
      delete dec_[i];
  }
  if (rtp_fp_)
    fclose(rtp_fp_);
}

void NetEqDecodingTest::SelectDecoders(WebRtcNetEQDecoder* used_codec) {
  *used_codec++ = kDecoderPCMu;
  dec_.push_back(new decoder_PCMU(0));
  *used_codec++ = kDecoderPCMa;
  dec_.push_back(new decoder_PCMA(8));
  *used_codec++ = kDecoderILBC;
  dec_.push_back(new decoder_ILBC(102));
  *used_codec++ = kDecoderISAC;
  dec_.push_back(new decoder_iSAC(103));
  *used_codec++ = kDecoderISACswb;
  dec_.push_back(new decoder_iSACSWB(104));
  *used_codec++ = kDecoderPCM16B;
  dec_.push_back(new decoder_PCM16B_NB(93));
  *used_codec++ = kDecoderPCM16Bwb;
  dec_.push_back(new decoder_PCM16B_WB(94));
  *used_codec++ = kDecoderPCM16Bswb32kHz;
  dec_.push_back(new decoder_PCM16B_SWB32(95));
  *used_codec++ = kDecoderCNG;
  dec_.push_back(new decoder_CNG(13, 8000));
  *used_codec++ = kDecoderCNG;
  dec_.push_back(new decoder_CNG(98, 16000));
}

void NetEqDecodingTest::LoadDecoders() {
  for (size_t i = 0; i < dec_.size(); ++i) {
    ASSERT_EQ(0, dec_[i]->loadToNetEQ(*neteq_inst_));
  }
}

void NetEqDecodingTest::OpenInputFile(const std::string &rtp_file) {
  rtp_fp_ = fopen(rtp_file.c_str(), "rb");
  ASSERT_TRUE(rtp_fp_ != NULL);
  ASSERT_EQ(0, NETEQTEST_RTPpacket::skipFileHeader(rtp_fp_));
}

void NetEqDecodingTest::Process(NETEQTEST_RTPpacket* rtp, int16_t* out_len) {
  // Check if time to receive.
  while ((sim_clock_ >= rtp->time()) &&
         (rtp->dataLen() >= 0)) {
    if (rtp->dataLen() > 0) {
      ASSERT_EQ(0, neteq_inst_->recIn(*rtp));
    }
    // Get next packet.
    ASSERT_NE(-1, rtp->readFromFile(rtp_fp_));
  }

  // RecOut
  *out_len = neteq_inst_->recOut(out_data_);
  ASSERT_TRUE((*out_len == kBlockSize8kHz) ||
              (*out_len == kBlockSize16kHz) ||
              (*out_len == kBlockSize32kHz));

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
  while (rtp.dataLen() >= 0) {
    int16_t out_len;
    Process(&rtp, &out_len);
    ref_files.ProcessReference(out_data_, out_len);
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
    int16_t out_len;
    Process(&rtp, &out_len);

    // Query the network statistics API once per second
    if (sim_clock_ % 1000 == 0) {
      // Process NetworkStatistics.
      WebRtcNetEQ_NetworkStatistics network_stats;
      ASSERT_EQ(0, WebRtcNetEQ_GetNetworkStatistics(neteq_inst_->instance(),
                                                    &network_stats));
      network_stat_files.ProcessReference(network_stats);

      // Process RTCPstat.
      WebRtcNetEQ_RTCPStat rtcp_stats;
      ASSERT_EQ(0, WebRtcNetEQ_GetRTCPStats(neteq_inst_->instance(),
                                            &rtcp_stats));
      rtcp_stat_files.ProcessReference(rtcp_stats);
    }
  }
}

void NetEqDecodingTest::PopulateRtpInfo(int frame_index,
                                        int timestamp,
                                        WebRtcNetEQ_RTPInfo* rtp_info) {
  rtp_info->sequenceNumber = frame_index;
  rtp_info->timeStamp = timestamp;
  rtp_info->SSRC = 0x1234;  // Just an arbitrary SSRC.
  rtp_info->payloadType = 94;  // PCM16b WB codec.
  rtp_info->markerBit = 0;
}

void NetEqDecodingTest::PopulateCng(int frame_index,
                                    int timestamp,
                                    WebRtcNetEQ_RTPInfo* rtp_info,
                                    uint8_t* payload,
                                    int* payload_len) {
  rtp_info->sequenceNumber = frame_index;
  rtp_info->timeStamp = timestamp;
  rtp_info->SSRC = 0x1234;  // Just an arbitrary SSRC.
  rtp_info->payloadType = 98;  // WB CNG.
  rtp_info->markerBit = 0;
  payload[0] = 64;  // Noise level -64 dBov, quite arbitrarily chosen.
  *payload_len = 1;  // Only noise level, no spectral parameters.
}

TEST_F(NetEqDecodingTest, TestBitExactness) {
  const std::string kInputRtpFile = webrtc::test::ProjectRootPath() +
      "resources/neteq_universal.rtp";
  const std::string kInputRefFile =
      webrtc::test::ResourcePath("neteq_universal_ref", "pcm");
  DecodeAndCompare(kInputRtpFile, kInputRefFile);
}

TEST_F(NetEqDecodingTest, TestNetworkStatistics) {
  const std::string kInputRtpFile = webrtc::test::ProjectRootPath() +
      "resources/neteq_universal.rtp";
  const std::string kNetworkStatRefFile =
      webrtc::test::ResourcePath("neteq_network_stats", "dat");
  const std::string kRtcpStatRefFile =
      webrtc::test::ResourcePath("neteq_rtcp_stats", "dat");
  DecodeAndCheckStats(kInputRtpFile, kNetworkStatRefFile, kRtcpStatRefFile);
}

TEST_F(NetEqDecodingTest, TestFrameWaitingTimeStatistics) {
  // Use fax mode to avoid time-scaling. This is to simplify the testing of
  // packet waiting times in the packet buffer.
  ASSERT_EQ(0,
            WebRtcNetEQ_SetPlayoutMode(neteq_inst_->instance(), kPlayoutFax));
  // Insert 30 dummy packets at once. Each packet contains 10 ms 16 kHz audio.
  int num_frames = 30;
  const int kSamples = 10 * 16;
  const int kPayloadBytes = kSamples * 2;
  for (int i = 0; i < num_frames; ++i) {
    uint16_t payload[kSamples] = {0};
    WebRtcNetEQ_RTPInfo rtp_info;
    rtp_info.sequenceNumber = i;
    rtp_info.timeStamp = i * kSamples;
    rtp_info.SSRC = 0x1234;  // Just an arbitrary SSRC.
    rtp_info.payloadType = 94;  // PCM16b WB codec.
    rtp_info.markerBit = 0;
    ASSERT_EQ(0, WebRtcNetEQ_RecInRTPStruct(neteq_inst_->instance(), &rtp_info,
                                            reinterpret_cast<uint8_t*>(payload),
                                            kPayloadBytes, 0));
  }
  // Pull out all data.
  for (int i = 0; i < num_frames; ++i) {
    ASSERT_TRUE(kBlockSize16kHz == neteq_inst_->recOut(out_data_));
  }
  const int kVecLen = 110;  // More than kLenWaitingTimes in mcu.h.
  int waiting_times[kVecLen];
  int len = WebRtcNetEQ_GetRawFrameWaitingTimes(neteq_inst_->instance(),
                                                kVecLen, waiting_times);
  EXPECT_EQ(num_frames, len);
  // Since all frames are dumped into NetEQ at once, but pulled out with 10 ms
  // spacing (per definition), we expect the delay to increase with 10 ms for
  // each packet.
  for (int i = 0; i < len; ++i) {
    EXPECT_EQ((i + 1) * 10, waiting_times[i]);
  }

  // Check statistics again and make sure it's been reset.
  EXPECT_EQ(0, WebRtcNetEQ_GetRawFrameWaitingTimes(neteq_inst_->instance(),
                                                   kVecLen, waiting_times));

  // Process > 100 frames, and make sure that that we get statistics
  // only for 100 frames. Note the new SSRC, causing NetEQ to reset.
  num_frames = 110;
  for (int i = 0; i < num_frames; ++i) {
    uint16_t payload[kSamples] = {0};
    WebRtcNetEQ_RTPInfo rtp_info;
    rtp_info.sequenceNumber = i;
    rtp_info.timeStamp = i * kSamples;
    rtp_info.SSRC = 0x1235;  // Just an arbitrary SSRC.
    rtp_info.payloadType = 94;  // PCM16b WB codec.
    rtp_info.markerBit = 0;
    ASSERT_EQ(0, WebRtcNetEQ_RecInRTPStruct(neteq_inst_->instance(), &rtp_info,
                                            reinterpret_cast<uint8_t*>(payload),
                                            kPayloadBytes, 0));
    ASSERT_TRUE(kBlockSize16kHz == neteq_inst_->recOut(out_data_));
  }

  len = WebRtcNetEQ_GetRawFrameWaitingTimes(neteq_inst_->instance(),
                                            kVecLen, waiting_times);
  EXPECT_EQ(100, len);
}

TEST_F(NetEqDecodingTest, TestAverageInterArrivalTimeNegative) {
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
      WebRtcNetEQ_RTPInfo rtp_info;
      PopulateRtpInfo(frame_index, frame_index * kSamples, &rtp_info);
      ASSERT_EQ(0,
                WebRtcNetEQ_RecInRTPStruct(neteq_inst_->instance(),
                                           &rtp_info,
                                           payload,
                                           kPayloadBytes, 0));
      ++frame_index;
    }

    // Pull out data once.
    ASSERT_TRUE(kBlockSize16kHz == neteq_inst_->recOut(out_data_));
  }

  WebRtcNetEQ_NetworkStatistics network_stats;
  ASSERT_EQ(0, WebRtcNetEQ_GetNetworkStatistics(neteq_inst_->instance(),
                                                &network_stats));
  EXPECT_EQ(-106911, network_stats.clockDriftPPM);
}

TEST_F(NetEqDecodingTest, TestAverageInterArrivalTimePositive) {
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
      WebRtcNetEQ_RTPInfo rtp_info;
      PopulateRtpInfo(frame_index, frame_index * kSamples, &rtp_info);
      ASSERT_EQ(0,
                WebRtcNetEQ_RecInRTPStruct(neteq_inst_->instance(),
                                           &rtp_info,
                                           payload,
                                           kPayloadBytes, 0));
      ++frame_index;
    }

    // Pull out data once.
    ASSERT_TRUE(kBlockSize16kHz == neteq_inst_->recOut(out_data_));
  }

  WebRtcNetEQ_NetworkStatistics network_stats;
  ASSERT_EQ(0, WebRtcNetEQ_GetNetworkStatistics(neteq_inst_->instance(),
                                                &network_stats));
  EXPECT_EQ(108352, network_stats.clockDriftPPM);
}

TEST_F(NetEqDecodingTest, LongCngWithClockDrift) {
  uint16_t seq_no = 0;
  uint32_t timestamp = 0;
  const int kFrameSizeMs = 30;
  const int kSamples = kFrameSizeMs * 16;
  const int kPayloadBytes = kSamples * 2;
  // Apply a clock drift of -25 ms / s (sender faster than receiver).
  const double kDriftFactor = 1000.0 / (1000.0 + 25.0);
  double next_input_time_ms = 0.0;
  double t_ms;

  // Insert speech for 5 seconds.
  const int kSpeechDurationMs = 5000;
  for (t_ms = 0; t_ms < kSpeechDurationMs; t_ms += 10) {
    // Each turn in this for loop is 10 ms.
    while (next_input_time_ms <= t_ms) {
      // Insert one 30 ms speech frame.
      uint8_t payload[kPayloadBytes] = {0};
      WebRtcNetEQ_RTPInfo rtp_info;
      PopulateRtpInfo(seq_no, timestamp, &rtp_info);
      ASSERT_EQ(0,
                WebRtcNetEQ_RecInRTPStruct(neteq_inst_->instance(),
                                           &rtp_info,
                                           payload,
                                           kPayloadBytes, 0));
      ++seq_no;
      timestamp += kSamples;
      next_input_time_ms += static_cast<double>(kFrameSizeMs) * kDriftFactor;
    }
    // Pull out data once.
    ASSERT_TRUE(kBlockSize16kHz == neteq_inst_->recOut(out_data_));
  }

  EXPECT_EQ(kOutputNormal, neteq_inst_->getOutputType());
  int32_t delay_before = timestamp - neteq_inst_->getSpeechTimeStamp();

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
      WebRtcNetEQ_RTPInfo rtp_info;
      PopulateCng(seq_no, timestamp, &rtp_info, payload, &payload_len);
      ASSERT_EQ(0,
                WebRtcNetEQ_RecInRTPStruct(neteq_inst_->instance(),
                                           &rtp_info,
                                           payload,
                                           payload_len, 0));
      ++seq_no;
      timestamp += kCngPeriodSamples;
      next_input_time_ms += static_cast<double>(kCngPeriodMs) * kDriftFactor;
    }
    // Pull out data once.
    ASSERT_TRUE(kBlockSize16kHz == neteq_inst_->recOut(out_data_));
  }

  EXPECT_EQ(kOutputCNG, neteq_inst_->getOutputType());

  // Insert speech again until output type is speech.
  while (neteq_inst_->getOutputType() != kOutputNormal) {
    // Each turn in this for loop is 10 ms.
    while (next_input_time_ms <= t_ms) {
      // Insert one 30 ms speech frame.
      uint8_t payload[kPayloadBytes] = {0};
      WebRtcNetEQ_RTPInfo rtp_info;
      PopulateRtpInfo(seq_no, timestamp, &rtp_info);
      ASSERT_EQ(0,
                WebRtcNetEQ_RecInRTPStruct(neteq_inst_->instance(),
                                           &rtp_info,
                                           payload,
                                           kPayloadBytes, 0));
      ++seq_no;
      timestamp += kSamples;
      next_input_time_ms += static_cast<double>(kFrameSizeMs) * kDriftFactor;
    }
    // Pull out data once.
    ASSERT_TRUE(kBlockSize16kHz == neteq_inst_->recOut(out_data_));
    // Increase clock.
    t_ms += 10;
  }

  int32_t delay_after = timestamp - neteq_inst_->getSpeechTimeStamp();
  // Compare delay before and after, and make sure it differs less than 20 ms.
  EXPECT_LE(delay_after, delay_before + 20 * 16);
  EXPECT_GE(delay_after, delay_before - 20 * 16);
}

}  // namespace
