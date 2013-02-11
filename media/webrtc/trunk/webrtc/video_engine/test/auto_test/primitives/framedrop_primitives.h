/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VIDEO_ENGINE_TEST_AUTO_TEST_SOURCE_FRAMEDROP_PRIMITIVES_H_
#define WEBRTC_VIDEO_ENGINE_TEST_AUTO_TEST_SOURCE_FRAMEDROP_PRIMITIVES_H_

#include <map>
#include <vector>

#include "video_engine/include/vie_codec.h"
#include "video_engine/include/vie_image_process.h"
#include "video_engine/test/auto_test/interface/vie_autotest_defines.h"
#include "video_engine/test/libvietest/include/vie_to_file_renderer.h"

class FrameDropDetector;
struct NetworkParameters;
class TbInterfaces;

// Initializes the Video engine and its components, runs video playback using
// for KAutoTestSleepTimeMs milliseconds, then shuts down everything.
// The bit rate and packet loss parameters should be configured so that
// frames are dropped, in order to test the frame drop detection that is
// performed by the FrameDropDetector class.
void TestFullStack(const TbInterfaces& interfaces,
                   int capture_id,
                   int video_channel,
                   int width,
                   int height,
                   int bit_rate_kbps,
                   const NetworkParameters& network,
                   FrameDropDetector* frame_drop_detector,
                   ViEToFileRenderer* remote_file_renderer,
                   ViEToFileRenderer* local_file_renderer);

// A frame in a video file. The four different points in the stack when
// register the frame state are (in time order): created, transmitted, decoded,
// rendered.
class Frame {
 public:
  Frame(int number, unsigned int timestamp)
    : number_(number),
      frame_timestamp_(timestamp),
      created_timestamp_in_us_(-1),
      sent_timestamp_in_us_(-1),
      received_timestamp_in_us_(-1),
      decoded_timestamp_in_us_(-1),
      rendered_timestamp_in_us_(-1),
      dropped_at_send(false),
      dropped_at_receive(false),
      dropped_at_decode(false),
      dropped_at_render(false) {}

  // Frame number, starting at 0.
  int number_;

  // Frame timestamp, that is used by Video Engine and RTP headers and set when
  // the frame is sent into the stack.
  unsigned int frame_timestamp_;

  // Timestamps for our measurements of when the frame is in different states.
  int64_t created_timestamp_in_us_;
  int64_t sent_timestamp_in_us_;
  int64_t received_timestamp_in_us_;
  int64_t decoded_timestamp_in_us_;
  int64_t rendered_timestamp_in_us_;

  // Where the frame was dropped (more than one may be true).
  bool dropped_at_send;
  bool dropped_at_receive;
  bool dropped_at_decode;
  bool dropped_at_render;
};

// Fixes the output file by copying the last successful frame into the place
// where the dropped frame would be, for all dropped frames (if any).
// This method will not be able to fix data for the first frame if that is
// dropped, since there'll be no previous frame to copy. This case should never
// happen because of encoder frame dropping at least.
// Parameters:
//    output_file            The output file to modify (pad with frame copies
//                           for all dropped frames)
//    frame_length_in_bytes  Byte length of each frame.
//    frames                 A vector of all Frame objects. Must be sorted by
//                           frame number. If empty this method will do nothing.
void FixOutputFileForComparison(const std::string& output_file,
                                int frame_length_in_bytes,
                                const std::vector<Frame*>& frames);

// Handles statistics about dropped frames. Frames travel through the stack
// with different timestamps. The frames created and sent to the encoder have
// one timestamp on the sending side while the decoded/rendered frames have
// another timestamp on the receiving side. The difference between these
// timestamps is fixed, which we can use to identify the frames when they
// arrive, since the FrameDropDetector class gets data reported from both sides.
// The four different points in the stack when this class examines the frame
// states are (in time order): created, sent, received, decoded, rendered.
//
// The flow can be visualized like this:
//
//         Created        Sent        Received               Decoded   Rendered
// +-------+  |  +-------+ | +---------+ | +------+  +-------+  |  +--------+
// |Capture|  |  |Encoder| | |  Ext.   | | |Jitter|  |Decoder|  |  |  Ext.  |
// | device|---->|       |-->|transport|-->|buffer|->|       |---->|renderer|
// +-------+     +-------+   +---------+   +------+  +-------+     +--------+
//
// This class has no intention of being thread-safe.
class FrameDropDetector {
 public:
  enum State {
    // A frame being created, i.e. sent to the encoder; the first step of
    // a frame's life cycle. This timestamp becomes the frame timestamp in the
    // Frame objects.
    kCreated,
    // A frame being sent in external transport (to the simulated network). This
    // timestamp differs from the one in the Created state by a constant diff.
    kSent,
    // A frame being received in external transport (from the simulated
    // network). This timestamp differs from the one in the Created state by a
    // constant diff.
    kReceived,
    // A frame that has been decoded in the decoder. This timestamp differs
    // from the one in the Created state by a constant diff.
    kDecoded,
    // A frame that has been rendered; the last step of a frame's life cycle.
    // This timestamp differs from the one in the Created state by a constant
    // diff.
    kRendered
  };

  FrameDropDetector()
      : dirty_(true),
        dropped_frames_at_send_(0),
        dropped_frames_at_receive_(0),
        dropped_frames_at_decode_(0),
        dropped_frames_at_render_(0),
        num_created_frames_(0),
        num_sent_frames_(0),
        num_received_frames_(0),
        num_decoded_frames_(0),
        num_rendered_frames_(0),
        timestamp_diff_(0) {}

  // Reports a frame has reached a state in the frame life cycle.
  void ReportFrameState(State state, unsigned int timestamp,
                        int64_t report_time_us);

  // Uses all the gathered timestamp information to calculate which frames have
  // been dropped during the test and where they were dropped. Not until
  // this method has been executed, the Frame objects will have all fields
  // filled with the proper timestamp information.
  void CalculateResults();

  // Calculates the number of frames have been registered as dropped at the
  // specified state of the frame life cycle.
  // CalculateResults() must be called before calling this method.
  int GetNumberOfFramesDroppedAt(State state);

  // Gets a vector of all the created frames.
  // CalculateResults() must be called before calling this method to have all
  // fields of the Frame objects to represent the current state.
  const std::vector<Frame*>& GetAllFrames();

  // Prints a detailed report about all the different frame states and which
  // ones are detected as dropped, using ViETest::Log. Also prints
  // perf-formatted output and adds |test_label| as a modifier to the perf
  // output.
  // CalculateResults() must be called before calling this method.
  void PrintReport(const std::string& test_label);

  // Prints all the timestamp maps. Mainly used for debugging purposes to find
  // missing timestamps.
  void PrintDebugDump();
 private:
  // Will be false until CalculateResults() is called. Switches to true
  // as soon as new timestamps are reported using ReportFrameState().
  bool dirty_;

  // Map of frame creation timestamps to all Frame objects.
  std::map<unsigned int, Frame*> created_frames_;

  // Maps converted frame timestamps (differ from creation timestamp) to the
  // time they arrived in the different states of the frame's life cycle.
  std::map<unsigned int, int64_t> sent_frames_;
  std::map<unsigned int, int64_t> received_frames_;
  std::map<unsigned int, int64_t> decoded_frames_;
  std::map<unsigned int, int64_t> rendered_frames_;

  // A vector with the frames sorted in their created order.
  std::vector<Frame*> created_frames_vector_;

  // Statistics.
  int dropped_frames_at_send_;
  int dropped_frames_at_receive_;
  int dropped_frames_at_decode_;
  int dropped_frames_at_render_;

  int num_created_frames_;
  int num_sent_frames_;
  int num_received_frames_;
  int num_decoded_frames_;
  int num_rendered_frames_;

  // The constant diff between the created and transmitted frames, since their
  // timestamps are converted.
  unsigned int timestamp_diff_;
};

// Tracks which frames are received on the remote side and reports back to the
// FrameDropDetector class when they are rendered.
class FrameDropMonitoringRemoteFileRenderer : public ViEToFileRenderer {
 public:
  explicit FrameDropMonitoringRemoteFileRenderer(
      FrameDropDetector* frame_drop_detector)
      : frame_drop_detector_(frame_drop_detector) {}
  virtual ~FrameDropMonitoringRemoteFileRenderer() {}

  // Implementation of ExternalRenderer:
  int FrameSizeChange(unsigned int width, unsigned int height,
                      unsigned int number_of_streams);
  int DeliverFrame(unsigned char* buffer, int buffer_size,
                   uint32_t time_stamp,
                   int64_t render_time);
 private:
  FrameDropDetector* frame_drop_detector_;
};

#endif  // WEBRTC_VIDEO_ENGINE_TEST_AUTO_TEST_SOURCE_FRAMEDROP_PRIMITIVES_H_
