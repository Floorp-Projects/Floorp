/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/video_engine/test/auto_test/primitives/framedrop_primitives.h"

#include <stdio.h>

#include <vector>

#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/test/testsupport/fileutils.h"
#include "webrtc/test/testsupport/frame_reader.h"
#include "webrtc/test/testsupport/frame_writer.h"

namespace webrtc {

const std::string kOutputFilename = "temp_outputfile.tmp";
const int kFrameLength = 1000;

class FrameDropPrimitivesTest: public testing::Test {
 protected:
  FrameDropPrimitivesTest() {}
  virtual ~FrameDropPrimitivesTest() {}
  void SetUp() {
    // Cleanup any previous output file.
    remove(kOutputFilename.c_str());
  }
  void TearDown() {
    // Cleanup the temporary file.
    remove(kOutputFilename.c_str());
  }
};

TEST_F(FrameDropPrimitivesTest, FixOutputFileForComparison) {
  // Create test frame objects, where the second and fourth frame is marked
  // as dropped at rendering.
  std::vector<Frame*> frames;
  Frame first_frame(0, kFrameLength);
  Frame second_frame(0, kFrameLength);
  Frame third_frame(0, kFrameLength);
  Frame fourth_frame(0, kFrameLength);

  second_frame.dropped_at_render = true;
  fourth_frame.dropped_at_render = true;

  frames.push_back(&first_frame);
  frames.push_back(&second_frame);
  frames.push_back(&third_frame);
  frames.push_back(&fourth_frame);

  // Prepare data for the first and third frames:
  uint8_t first_frame_data[kFrameLength];
  memset(first_frame_data, 5, kFrameLength);  // Fill it with 5's to identify.
  uint8_t third_frame_data[kFrameLength];
  memset(third_frame_data, 7, kFrameLength);  // Fill it with 7's to identify.

  // Write the first and third frames to the temporary file. This means the fix
  // method should add two frames of data by filling the file with data from
  // the first and third frames after executing.
  webrtc::test::FrameWriterImpl frame_writer(kOutputFilename, kFrameLength);
  EXPECT_TRUE(frame_writer.Init());
  EXPECT_TRUE(frame_writer.WriteFrame(first_frame_data));
  EXPECT_TRUE(frame_writer.WriteFrame(third_frame_data));
  frame_writer.Close();
  EXPECT_EQ(2 * kFrameLength,
            static_cast<int>(webrtc::test::GetFileSize(kOutputFilename)));

  FixOutputFileForComparison(kOutputFilename, kFrameLength, frames);

  // Verify that the output file has correct size.
  EXPECT_EQ(4 * kFrameLength,
            static_cast<int>(webrtc::test::GetFileSize(kOutputFilename)));

  webrtc::test::FrameReaderImpl frame_reader(kOutputFilename, kFrameLength);
  frame_reader.Init();
  uint8_t read_buffer[kFrameLength];
  EXPECT_TRUE(frame_reader.ReadFrame(read_buffer));
  EXPECT_EQ(0, memcmp(read_buffer, first_frame_data, kFrameLength));
  EXPECT_TRUE(frame_reader.ReadFrame(read_buffer));
  EXPECT_EQ(0, memcmp(read_buffer, first_frame_data, kFrameLength));

  EXPECT_TRUE(frame_reader.ReadFrame(read_buffer));
  EXPECT_EQ(0, memcmp(read_buffer, third_frame_data, kFrameLength));
  EXPECT_TRUE(frame_reader.ReadFrame(read_buffer));
  EXPECT_EQ(0, memcmp(read_buffer, third_frame_data, kFrameLength));

  frame_reader.Close();
}

}  // namespace webrtc
