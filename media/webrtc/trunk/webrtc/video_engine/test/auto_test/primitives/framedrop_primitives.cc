/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <cassert>
#include <cmath>
#include <sstream>
#include <string>

#include "webrtc/modules/video_capture/include/video_capture_factory.h"
#include "system_wrappers/interface/tick_util.h"
#include "testsupport/fileutils.h"
#include "testsupport/frame_reader.h"
#include "testsupport/frame_writer.h"
#include "testsupport/perf_test.h"
#include "video_engine/test/auto_test/interface/vie_autotest.h"
#include "video_engine/test/auto_test/interface/vie_autotest_defines.h"
#include "video_engine/test/auto_test/primitives/framedrop_primitives.h"
#include "video_engine/test/auto_test/primitives/general_primitives.h"
#include "video_engine/test/libvietest/include/tb_interfaces.h"
#include "video_engine/test/libvietest/include/tb_external_transport.h"
#include "video_engine/test/libvietest/include/vie_external_render_filter.h"
#include "video_engine/test/libvietest/include/vie_to_file_renderer.h"

enum { kWaitTimeForFinalDecodeMs = 100 };

// Writes the frames to be encoded to file and tracks which frames are sent in
// external transport on the local side and reports them to the
// FrameDropDetector class.
class LocalRendererEffectFilter : public webrtc::ExternalRendererEffectFilter {
 public:
  LocalRendererEffectFilter(webrtc::ExternalRenderer* renderer,
                            FrameDropDetector* frame_drop_detector)
      : ExternalRendererEffectFilter(renderer),
        frame_drop_detector_(frame_drop_detector) {}
  int Transform(int size, unsigned char* frameBuffer,
                unsigned int timeStamp90KHz, unsigned int width,
                unsigned int height) {
    frame_drop_detector_->ReportFrameState(
        FrameDropDetector::kCreated,
        timeStamp90KHz,
        webrtc::TickTime::MicrosecondTimestamp());
    return webrtc::ExternalRendererEffectFilter::Transform(
        size, frameBuffer, timeStamp90KHz, width, height);
  }
 private:
  FrameDropDetector* frame_drop_detector_;
};

// Tracks which frames are sent in external transport on the local side
// and reports them to the FrameDropDetector class.
class FrameSentCallback : public SendFrameCallback {
 public:
  explicit FrameSentCallback(FrameDropDetector* frame_drop_detector)
      : frame_drop_detector_(frame_drop_detector) {}
  virtual ~FrameSentCallback() {}
  virtual void FrameSent(unsigned int rtp_timestamp) {
    frame_drop_detector_->ReportFrameState(
        FrameDropDetector::kSent,
        rtp_timestamp,
        webrtc::TickTime::MicrosecondTimestamp());
  }

 private:
  FrameDropDetector* frame_drop_detector_;
};

// Tracks which frames are received in external transport on the remote side
// and reports them to the FrameDropDetector class.
class FrameReceivedCallback : public ReceiveFrameCallback {
 public:
  explicit FrameReceivedCallback(FrameDropDetector* frame_drop_detector)
      : frame_drop_detector_(frame_drop_detector) {}
  virtual ~FrameReceivedCallback() {}
  virtual void FrameReceived(unsigned int rtp_timestamp) {
    frame_drop_detector_->ReportFrameState(
        FrameDropDetector::kReceived,
        rtp_timestamp,
        webrtc::TickTime::MicrosecondTimestamp());
  }

 private:
  FrameDropDetector* frame_drop_detector_;
};

// Tracks when frames are decoded on the remote side (received from the
// jitter buffer) and reports them to the FrameDropDetector class.
class DecodedTimestampEffectFilter : public webrtc::ViEEffectFilter {
 public:
  explicit DecodedTimestampEffectFilter(FrameDropDetector* frame_drop_detector)
      : frame_drop_detector_(frame_drop_detector) {}
  virtual ~DecodedTimestampEffectFilter() {}
  virtual int Transform(int size, unsigned char* frameBuffer,
                        unsigned int timeStamp90KHz, unsigned int width,
                        unsigned int height) {
    frame_drop_detector_->ReportFrameState(
        FrameDropDetector::kDecoded,
        timeStamp90KHz,
        webrtc::TickTime::MicrosecondTimestamp());
    return 0;
  }

 private:
  FrameDropDetector* frame_drop_detector_;
};

class Statistics {
 public:
  Statistics() : sum_(0.0f), sum_squared_(0.0f), count_(0) {};

  void AddSample(float sample) {
    sum_ += sample;
    sum_squared_ += sample * sample;
    ++count_;
  }

  float Mean() {
    if (count_ == 0)
      return -1.0f;
    return sum_ / count_;
  }

  float Variance() {
    if (count_ == 0)
      return -1.0f;
    return  sum_squared_ / count_ - Mean() * Mean();
  }

  std::string AsString() {
    std::stringstream ss;
    ss << (Mean() >= 0 ? Mean() : -1) << ", " <<
        (Variance() >= 0 ? sqrt(Variance()) : -1);
    return ss.str();
  }

 private:
  float sum_;
  float sum_squared_;
  int count_;
};

void TestFullStack(const TbInterfaces& interfaces,
                   int capture_id,
                   int video_channel,
                   int width,
                   int height,
                   int bit_rate_kbps,
                   const NetworkParameters& network,
                   FrameDropDetector* frame_drop_detector,
                   ViEToFileRenderer* remote_file_renderer,
                   ViEToFileRenderer* local_file_renderer) {
  webrtc::VideoEngine *video_engine_interface = interfaces.video_engine;
  webrtc::ViEBase *base_interface = interfaces.base;
  webrtc::ViECapture *capture_interface = interfaces.capture;
  webrtc::ViERender *render_interface = interfaces.render;
  webrtc::ViECodec *codec_interface = interfaces.codec;
  webrtc::ViENetwork *network_interface = interfaces.network;

  // ***************************************************************
  // Engine ready. Begin testing class
  // ***************************************************************
  webrtc::VideoCodec video_codec;
  memset(&video_codec, 0, sizeof (webrtc::VideoCodec));

  // Set up all receive codecs. This basically setup the codec interface
  // to be able to recognize all receive codecs based on payload type.
  for (int idx = 0; idx < codec_interface->NumberOfCodecs(); idx++) {
    EXPECT_EQ(0, codec_interface->GetCodec(idx, video_codec));
    SetSuitableResolution(&video_codec, width, height);

    EXPECT_EQ(0, codec_interface->SetReceiveCodec(video_channel, video_codec));
  }

  // Configure External transport to simulate network interference:
  TbExternalTransport external_transport(*interfaces.network, video_channel,
                                         NULL);
  external_transport.SetNetworkParameters(network);

  FrameSentCallback frame_sent_callback(frame_drop_detector);
  FrameReceivedCallback frame_received_callback(frame_drop_detector);
  external_transport.RegisterSendFrameCallback(&frame_sent_callback);
  external_transport.RegisterReceiveFrameCallback(&frame_received_callback);
  EXPECT_EQ(0, network_interface->RegisterSendTransport(video_channel,
                                                        external_transport));
  RenderToFile(interfaces.render, video_channel, remote_file_renderer);
  EXPECT_EQ(0, base_interface->StartReceive(video_channel));

  // Setup only the VP8 codec, which is what we'll use.
  webrtc::VideoCodec codec;
  EXPECT_TRUE(FindSpecificCodec(webrtc::kVideoCodecVP8, codec_interface,
                                &codec));
  codec.startBitrate = bit_rate_kbps;
  codec.maxBitrate = bit_rate_kbps;
  codec.width = width;
  codec.height = height;
  EXPECT_EQ(0, codec_interface->SetSendCodec(video_channel, codec));

  webrtc::ViEImageProcess *image_process =
      webrtc::ViEImageProcess::GetInterface(video_engine_interface);
  EXPECT_TRUE(image_process);

  // Setup the effect filters.
  // Local rendering at the send-side is done in an effect filter to avoid
  // synchronization issues with the remote renderer.
  LocalRendererEffectFilter local_renderer_filter(local_file_renderer,
                                                  frame_drop_detector);
  EXPECT_EQ(0, image_process->RegisterSendEffectFilter(video_channel,
                                                       local_renderer_filter));
  DecodedTimestampEffectFilter decode_filter(frame_drop_detector);
  EXPECT_EQ(0, image_process->RegisterRenderEffectFilter(video_channel,
                                                         decode_filter));
  // Send video.
  EXPECT_EQ(0, base_interface->StartSend(video_channel));
  AutoTestSleep(kAutoTestSleepTimeMs);

  ViETest::Log("Done!");

  // ***************************************************************
  // Testing finished. Tear down Video Engine
  // ***************************************************************
  EXPECT_EQ(0, capture_interface->DisconnectCaptureDevice(video_channel));

  const int one_way_delay_99_percentile = network.mean_one_way_delay  +
        3 * network.std_dev_one_way_delay;

  // Wait for the last packet to arrive before we tear down the receiver.
  AutoTestSleep(2 * one_way_delay_99_percentile);
  EXPECT_EQ(0, base_interface->StopSend(video_channel));
  while (!external_transport.EmptyQueue()) {
    AutoTestSleep(one_way_delay_99_percentile);
  }
  EXPECT_EQ(0, base_interface->StopReceive(video_channel));
  EXPECT_EQ(0, network_interface->DeregisterSendTransport(video_channel));
  // Wait for the last frame to be decoded and rendered. There is no guarantee
  // this wait time will be long enough. Ideally we would wait for at least one
  // "receive-side delay", which is what the video coding module calculates
  // based on network statistics etc. We don't have access to that value here.
  AutoTestSleep(kWaitTimeForFinalDecodeMs);
  // Must stop the frame drop detectors in the right order to avoid getting
  // frames which for instance are rendered but not decoded.
  EXPECT_EQ(0, render_interface->StopRender(video_channel));
  EXPECT_EQ(0, render_interface->RemoveRenderer(video_channel));
  EXPECT_EQ(0, image_process->DeregisterRenderEffectFilter(video_channel));
  EXPECT_EQ(0, image_process->DeregisterSendEffectFilter(video_channel));
  image_process->Release();
  EXPECT_EQ(0, base_interface->DeleteChannel(video_channel));

  // Collect transport statistics.
  int32_t num_rtp_packets = 0;
  int32_t num_dropped_packets = 0;
  int32_t num_rtcp_packets = 0;
  external_transport.GetStats(num_rtp_packets, num_dropped_packets,
                              num_rtcp_packets);
  ViETest::Log("RTP packets    : %5d", num_rtp_packets);
  ViETest::Log("Dropped packets: %5d", num_dropped_packets);
  ViETest::Log("RTCP packets   : %5d", num_rtcp_packets);
}

void FixOutputFileForComparison(const std::string& output_file,
                                int frame_length_in_bytes,
                                const std::vector<Frame*>& frames) {
  webrtc::test::FrameReaderImpl frame_reader(output_file,
                                             frame_length_in_bytes);
  const std::string temp_file = output_file + ".fixed";
  webrtc::test::FrameWriterImpl frame_writer(temp_file, frame_length_in_bytes);
  frame_reader.Init();
  frame_writer.Init();

  ASSERT_FALSE(frames.front()->dropped_at_render) << "It should not be "
      "possible to drop the first frame. Both because we don't have anything "
      "useful to fill that gap with and it is impossible to detect it without "
      "any previous timestamps to compare with.";

  uint8_t* last_frame_data = new uint8_t[frame_length_in_bytes];

  // Process the file and write frame duplicates for all dropped frames.
  for (std::vector<Frame*>::const_iterator it = frames.begin();
       it != frames.end(); ++it) {
    if ((*it)->dropped_at_render) {
      // Write the previous frame to the output file:
      EXPECT_TRUE(frame_writer.WriteFrame(last_frame_data));
    } else {
      EXPECT_TRUE(frame_reader.ReadFrame(last_frame_data));
      EXPECT_TRUE(frame_writer.WriteFrame(last_frame_data));
    }
  }
  delete[] last_frame_data;
  frame_reader.Close();
  frame_writer.Close();
  ASSERT_EQ(0, std::remove(output_file.c_str()));
  ASSERT_EQ(0, std::rename(temp_file.c_str(), output_file.c_str()));
}

void FrameDropDetector::ReportFrameState(State state, unsigned int timestamp,
                                         int64_t report_time_us) {
  dirty_ = true;
  switch (state) {
    case kCreated: {
      int number = created_frames_vector_.size();
      Frame* frame = new Frame(number, timestamp);
      frame->created_timestamp_in_us_ = report_time_us;
      created_frames_vector_.push_back(frame);
      created_frames_[timestamp] = frame;
      num_created_frames_++;
      break;
    }
    case kSent:
      sent_frames_[timestamp] = report_time_us;
      if (timestamp_diff_ == 0) {
        // When the first created frame arrives we calculate the fixed
        // difference between the timestamps of the frames entering and leaving
        // the encoder. This diff is used to identify the frames from the
        // created_frames_ map.
        timestamp_diff_ =
            timestamp - created_frames_vector_.front()->frame_timestamp_;
      }
      num_sent_frames_++;
      break;
    case kReceived:
      received_frames_[timestamp] = report_time_us;
      num_received_frames_++;
      break;
    case kDecoded:
      decoded_frames_[timestamp] = report_time_us;
      num_decoded_frames_++;
      break;
    case kRendered:
      rendered_frames_[timestamp] = report_time_us;
      num_rendered_frames_++;
      break;
  }
}

void FrameDropDetector::CalculateResults() {
  // Fill in all fields of the Frame objects in the created_frames_ map.
  // Iterate over the maps from converted timestamps to the arrival timestamps.
  std::map<unsigned int, int64_t>::const_iterator it;
  for (it = sent_frames_.begin(); it != sent_frames_.end(); ++it) {
    unsigned int created_timestamp = it->first - timestamp_diff_;
    created_frames_[created_timestamp]->sent_timestamp_in_us_ = it->second;
  }
  for (it = received_frames_.begin(); it != received_frames_.end(); ++it) {
    unsigned int created_timestamp = it->first - timestamp_diff_;
    created_frames_[created_timestamp]->received_timestamp_in_us_ = it->second;
  }
  for (it = decoded_frames_.begin(); it != decoded_frames_.end(); ++it) {
    unsigned int created_timestamp = it->first - timestamp_diff_;
    created_frames_[created_timestamp]->decoded_timestamp_in_us_ =it->second;
  }
  for (it = rendered_frames_.begin(); it != rendered_frames_.end(); ++it) {
    unsigned int created_timestamp = it->first - timestamp_diff_;
    created_frames_[created_timestamp]->rendered_timestamp_in_us_ = it->second;
  }
  // Find out where the frames were not present in the different states.
  dropped_frames_at_send_ = 0;
  dropped_frames_at_receive_ = 0;
  dropped_frames_at_decode_ = 0;
  dropped_frames_at_render_ = 0;
  for (std::vector<Frame*>::const_iterator it = created_frames_vector_.begin();
       it != created_frames_vector_.end(); ++it) {
    int encoded_timestamp = (*it)->frame_timestamp_ + timestamp_diff_;
    if (sent_frames_.find(encoded_timestamp) == sent_frames_.end()) {
      (*it)->dropped_at_send = true;
      dropped_frames_at_send_++;
    }
    if (received_frames_.find(encoded_timestamp) == received_frames_.end()) {
      (*it)->dropped_at_receive = true;
      dropped_frames_at_receive_++;
    }
    if (decoded_frames_.find(encoded_timestamp) == decoded_frames_.end()) {
      (*it)->dropped_at_decode = true;
      dropped_frames_at_decode_++;
    }
    if (rendered_frames_.find(encoded_timestamp) == rendered_frames_.end()) {
      (*it)->dropped_at_render = true;
      dropped_frames_at_render_++;
    }
  }
  dirty_ = false;
}

void FrameDropDetector::PrintReport(const std::string& test_label) {
  assert(!dirty_);
  ViETest::Log("Frame Drop Detector report:");
  ViETest::Log("  Created  frames: %ld", created_frames_.size());
  ViETest::Log("  Sent     frames: %ld", sent_frames_.size());
  ViETest::Log("  Received frames: %ld", received_frames_.size());
  ViETest::Log("  Decoded  frames: %ld", decoded_frames_.size());
  ViETest::Log("  Rendered frames: %ld", rendered_frames_.size());

  // Display all frames and stats for them:
  long last_created = 0;
  long last_sent = 0;
  long last_received = 0;
  long last_decoded = 0;
  long last_rendered = 0;
  ViETest::Log("\nDeltas between sent frames and drop status:");
  ViETest::Log("Unit: Microseconds");
  ViETest::Log("Frame  Created    Sent    Received Decoded Rendered "
      "Dropped at  Dropped at  Dropped at  Dropped at");
  ViETest::Log(" nbr    delta     delta    delta    delta   delta   "
      " Send?       Receive?    Decode?     Render?");
  Statistics rendering_stats;
  for (std::vector<Frame*>::const_iterator it = created_frames_vector_.begin();
       it != created_frames_vector_.end(); ++it) {
    int created_delta =
        static_cast<int>((*it)->created_timestamp_in_us_ - last_created);
    int sent_delta = (*it)->dropped_at_send ? -1 :
        static_cast<int>((*it)->sent_timestamp_in_us_ - last_sent);
    int received_delta = (*it)->dropped_at_receive ? -1 :
        static_cast<int>((*it)->received_timestamp_in_us_ - last_received);
    int decoded_delta = (*it)->dropped_at_decode ? -1 :
        static_cast<int>((*it)->decoded_timestamp_in_us_ - last_decoded);
    int rendered_delta = (*it)->dropped_at_render ? -1 :
        static_cast<int>((*it)->rendered_timestamp_in_us_ - last_rendered);

    // Set values to -1 for the first frame:
    if ((*it)->number_ == 0) {
      created_delta = -1;
      sent_delta = -1;
      received_delta = -1;
      decoded_delta = -1;
      rendered_delta = -1;
    }
    ViETest::Log("%5d %8d %8d %8d %8d %8d %10s %10s %10s %10s",
                 (*it)->number_,
                 created_delta,
                 sent_delta,
                 received_delta,
                 decoded_delta,
                 rendered_delta,
                 (*it)->dropped_at_send ? "DROPPED" : "      ",
                 (*it)->dropped_at_receive ? "DROPPED" : "      ",
                 (*it)->dropped_at_decode ? "DROPPED" : "      ",
                 (*it)->dropped_at_render ? "DROPPED" : "      ");
    last_created = (*it)->created_timestamp_in_us_;
    if (!(*it)->dropped_at_send) {
      last_sent = (*it)->sent_timestamp_in_us_;
    }
     if (!(*it)->dropped_at_receive) {
      last_received = (*it)->received_timestamp_in_us_;
    }
    if (!(*it)->dropped_at_decode) {
      last_decoded = (*it)->decoded_timestamp_in_us_;
    }
    if (!(*it)->dropped_at_render) {
      last_rendered = (*it)->rendered_timestamp_in_us_;
      rendering_stats.AddSample(rendered_delta / 1000.0f);
    }
  }
  ViETest::Log("\nLatency between states (-1 means N/A because of drop):");
  ViETest::Log("Unit: Microseconds");
  ViETest::Log("Frame  Created    Sent      Received   Decoded      Total    "
      "   Total");
  ViETest::Log(" nbr   ->Sent  ->Received  ->Decoded ->Rendered    latency   "
      "  latency");
  ViETest::Log("                                               (incl network)"
      "(excl network)");
  Statistics latency_incl_network_stats;
  for (std::vector<Frame*>::const_iterator it = created_frames_vector_.begin();
       it != created_frames_vector_.end(); ++it) {
    int created_to_sent = (*it)->dropped_at_send ? -1 :
        static_cast<int>((*it)->sent_timestamp_in_us_ -
                         (*it)->created_timestamp_in_us_);
    int sent_to_received = (*it)->dropped_at_receive ? -1 :
        static_cast<int>((*it)->received_timestamp_in_us_ -
                         (*it)->sent_timestamp_in_us_);
    int received_to_decoded = (*it)->dropped_at_decode ? -1 :
        static_cast<int>((*it)->decoded_timestamp_in_us_ -
                         (*it)->received_timestamp_in_us_);
    int decoded_to_render = (*it)->dropped_at_render ? -1 :
        static_cast<int>((*it)->rendered_timestamp_in_us_ -
                         (*it)->decoded_timestamp_in_us_);
    int total_latency_incl_network = (*it)->dropped_at_render ? -1 :
        static_cast<int>((*it)->rendered_timestamp_in_us_ -
                         (*it)->created_timestamp_in_us_);
    int total_latency_excl_network = (*it)->dropped_at_render ? -1 :
        static_cast<int>((*it)->rendered_timestamp_in_us_ -
                         (*it)->created_timestamp_in_us_ - sent_to_received);
    if (total_latency_incl_network >= 0)
      latency_incl_network_stats.AddSample(total_latency_incl_network /
                                           1000.0f);
    ViETest::Log("%5d %9d %9d %9d %9d %12d %12d",
                 (*it)->number_,
                 created_to_sent,
                 sent_to_received,
                 received_to_decoded,
                 decoded_to_render,
                 total_latency_incl_network,
                 total_latency_excl_network);
  }

  // Plot all measurements in the same graph since they share the same value
  // range.
  webrtc::test::PrintResultMeanAndError(
      "total_delay_incl_network", "", test_label,
      latency_incl_network_stats.AsString(), "ms", false);
  webrtc::test::PrintResultMeanAndError(
      "time_between_rendered_frames", "", test_label,
      rendering_stats.AsString(), "ms", false);


  // Find and print the dropped frames.
  ViETest::Log("\nTotal # dropped frames at:");
  ViETest::Log("  Send   : %d", dropped_frames_at_send_);
  ViETest::Log("  Receive: %d", dropped_frames_at_receive_);
  ViETest::Log("  Decode : %d", dropped_frames_at_decode_);
  ViETest::Log("  Render : %d", dropped_frames_at_render_);
}

void FrameDropDetector::PrintDebugDump() {
  assert(!dirty_);
  ViETest::Log("\nPrintDebugDump: Frame objects:");
  ViETest::Log("Frame FrTimeStamp Created       Sent      Received    Decoded"
      "    Rendered ");
  for (std::vector<Frame*>::const_iterator it = created_frames_vector_.begin();
       it != created_frames_vector_.end(); ++it) {
    ViETest::Log("%5d %11u %11lld %11lld %11lld %11lld %11lld",
                 (*it)->number_,
                 (*it)->frame_timestamp_,
                 (*it)->created_timestamp_in_us_,
                 (*it)->sent_timestamp_in_us_,
                 (*it)->received_timestamp_in_us_,
                 (*it)->decoded_timestamp_in_us_,
                 (*it)->rendered_timestamp_in_us_);
  }
  std::vector<int> mismatch_frame_num_list;
  for (std::vector<Frame*>::const_iterator it = created_frames_vector_.begin();
       it != created_frames_vector_.end(); ++it) {
    if ((*it)->dropped_at_render != (*it)->dropped_at_decode) {
      mismatch_frame_num_list.push_back((*it)->number_);
    }
  }
  if (mismatch_frame_num_list.size() > 0) {
    ViETest::Log("\nDecoded/Rendered mismatches:");
    ViETest::Log("Frame FrTimeStamp    Created       Sent      Received    "
        "Decoded    Rendered ");
    for (std::vector<int>::const_iterator it = mismatch_frame_num_list.begin();
         it != mismatch_frame_num_list.end(); ++it) {
      Frame* frame = created_frames_vector_[*it];
      ViETest::Log("%5d %11u %11lld %11lld %11lld %11lld %11lld",
                 frame->number_,
                 frame->frame_timestamp_,
                 frame->created_timestamp_in_us_,
                 frame->sent_timestamp_in_us_,
                 frame->received_timestamp_in_us_,
                 frame->decoded_timestamp_in_us_,
                 frame->rendered_timestamp_in_us_);
    }
  }

  ViETest::Log("\nReportFrameState method invocations:");
  ViETest::Log("  Created : %d", num_created_frames_);
  ViETest::Log("  Send    : %d", num_sent_frames_);
  ViETest::Log("  Received: %d", num_received_frames_);
  ViETest::Log("  Decoded : %d", num_decoded_frames_);
  ViETest::Log("  Rendered: %d", num_rendered_frames_);
}

const std::vector<Frame*>& FrameDropDetector::GetAllFrames() {
  assert(!dirty_);
  return created_frames_vector_;
}

int FrameDropDetector::GetNumberOfFramesDroppedAt(State state) {
  assert(!dirty_);
  switch (state) {
    case kSent:
      return dropped_frames_at_send_;
    case kReceived:
      return dropped_frames_at_receive_;
    case kDecoded:
      return dropped_frames_at_decode_;
    case kRendered:
      return dropped_frames_at_render_;
    default:
      return 0;
  }
}

int FrameDropMonitoringRemoteFileRenderer::DeliverFrame(
    unsigned char *buffer, int buffer_size, uint32_t time_stamp,
    int64_t render_time) {
  // |render_time| provides the ideal render time for this frame. If that time
  // has already passed we will render it immediately.
  int64_t report_render_time_us = render_time * 1000;
  int64_t time_now_us = webrtc::TickTime::MicrosecondTimestamp();
  if (render_time < (time_now_us + 500) / 1000) {
    report_render_time_us = time_now_us;
  }
  // Register that this frame has been rendered.
  frame_drop_detector_->ReportFrameState(FrameDropDetector::kRendered,
                                         time_stamp, report_render_time_us);
  return ViEToFileRenderer::DeliverFrame(buffer, buffer_size,
                                         time_stamp, render_time);
}

int FrameDropMonitoringRemoteFileRenderer::FrameSizeChange(
    unsigned int width, unsigned int height, unsigned int number_of_streams) {
  return ViEToFileRenderer::FrameSizeChange(width, height, number_of_streams);
}
