/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/test/testsupport/frame_reader.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/test/testsupport/fileutils.h"

namespace webrtc {
namespace test {

const std::string kInputFilename = "temp_inputfile.tmp";
const std::string kInputFileContents = "baz";
// Setting the kFrameLength value to a value much larger than the
// file to test causes the ReadFrame test to fail on Windows.
const size_t kFrameLength = 1000;

class FrameReaderTest: public testing::Test {
 protected:
  FrameReaderTest() {}
  virtual ~FrameReaderTest() {}
  void SetUp() {
    // Cleanup any previous dummy input file.
    remove(kInputFilename.c_str());

    // Create a dummy input file.
    FILE* dummy = fopen(kInputFilename.c_str(), "wb");
    fprintf(dummy, "%s", kInputFileContents.c_str());
    fclose(dummy);

    frame_reader_ = new FrameReaderImpl(kInputFilename, kFrameLength);
    ASSERT_TRUE(frame_reader_->Init());
  }
  void TearDown() {
    delete frame_reader_;
    // Cleanup the dummy input file.
    remove(kInputFilename.c_str());
  }
  FrameReader* frame_reader_;
};

TEST_F(FrameReaderTest, InitSuccess) {
  FrameReaderImpl frame_reader(kInputFilename, kFrameLength);
  ASSERT_TRUE(frame_reader.Init());
  ASSERT_EQ(kFrameLength, frame_reader.FrameLength());
  ASSERT_EQ(0, frame_reader.NumberOfFrames());
}

TEST_F(FrameReaderTest, ReadFrame) {
  uint8_t buffer[3];
  bool result = frame_reader_->ReadFrame(buffer);
  ASSERT_FALSE(result);  // No more files to read.
  ASSERT_EQ(kInputFileContents[0], buffer[0]);
  ASSERT_EQ(kInputFileContents[1], buffer[1]);
  ASSERT_EQ(kInputFileContents[2], buffer[2]);
}

TEST_F(FrameReaderTest, ReadFrameUninitialized) {
  uint8_t buffer[3];
  FrameReaderImpl file_reader(kInputFilename, kFrameLength);
  ASSERT_FALSE(file_reader.ReadFrame(buffer));
}

}  // namespace test
}  // namespace webrtc
