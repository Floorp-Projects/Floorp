/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/test/testsupport/frame_writer.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/test/testsupport/fileutils.h"

namespace webrtc {
namespace test {

const std::string kOutputFilename = "temp_outputfile.tmp";
const size_t kFrameLength = 1000;

class FrameWriterTest: public testing::Test {
 protected:
  FrameWriterTest() {}
  virtual ~FrameWriterTest() {}
  void SetUp() {
    // Cleanup any previous output file.
    remove(kOutputFilename.c_str());
    frame_writer_ = new FrameWriterImpl(kOutputFilename, kFrameLength);
    ASSERT_TRUE(frame_writer_->Init());
  }
  void TearDown() {
    delete frame_writer_;
    // Cleanup the temporary file.
    remove(kOutputFilename.c_str());
  }
  FrameWriter* frame_writer_;
};

TEST_F(FrameWriterTest, InitSuccess) {
  FrameWriterImpl frame_writer(kOutputFilename, kFrameLength);
  ASSERT_TRUE(frame_writer.Init());
  ASSERT_EQ(kFrameLength, frame_writer.FrameLength());
}

TEST_F(FrameWriterTest, WriteFrame) {
  uint8_t buffer[kFrameLength];
  memset(buffer, 9, kFrameLength);  // Write lots of 9s to the buffer
  bool result = frame_writer_->WriteFrame(buffer);
  ASSERT_TRUE(result);  // success
  // Close the file and verify the size.
  frame_writer_->Close();
  ASSERT_EQ(kFrameLength, GetFileSize(kOutputFilename));
}

TEST_F(FrameWriterTest, WriteFrameUninitialized) {
  uint8_t buffer[3];
  FrameWriterImpl frame_writer(kOutputFilename, kFrameLength);
  ASSERT_FALSE(frame_writer.WriteFrame(buffer));
}

}  // namespace test
}  // namespace webrtc
