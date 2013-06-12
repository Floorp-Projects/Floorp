/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <cstdio>
#include <string>

#include "common_video/interface/video_image.h"
#include "common_video/jpeg/include/jpeg.h"
#include "common_video/libyuv/include/webrtc_libyuv.h"
#include "gtest/gtest.h"
#include "testsupport/fileutils.h"
#include "modules/interface/module_common_types.h"

namespace webrtc {

const int kImageWidth = 640;
const int kImageHeight = 480;

class JpegTest: public testing::Test {
 protected:
  JpegTest()
      : input_filename_(webrtc::test::ProjectRootPath() +
                       "data/common_video/jpeg/webrtc_logo.jpg"),
        decoded_filename_(webrtc::test::OutputPath() + "TestJpegDec.yuv"),
        encoded_filename_(webrtc::test::OutputPath() + "TestJpegEnc.jpg"),
        encoded_buffer_(NULL) {}
  virtual ~JpegTest() {}

  void SetUp() {
    encoder_ = new JpegEncoder();
  }

  void TearDown() {
    if (encoded_buffer_ != NULL) {
      if (encoded_buffer_->_buffer != NULL) {
        delete [] encoded_buffer_->_buffer;
      }
      delete encoded_buffer_;
    }
    delete encoder_;
  }

  // Reads an encoded image. Caller will have to deallocate the memory of this
  // object and it's _buffer byte array.
  EncodedImage* ReadEncodedImage(std::string input_filename) {
    FILE* open_file = fopen(input_filename.c_str(), "rb");
    assert(open_file != NULL);
    size_t length = webrtc::test::GetFileSize(input_filename);
    EncodedImage* encoded_buffer = new EncodedImage();
    encoded_buffer->_buffer = new WebRtc_UWord8[length];
    encoded_buffer->_size = length;
    encoded_buffer->_length = length;
    if (fread(encoded_buffer->_buffer, 1, length, open_file) != length) {
      ADD_FAILURE() << "Error reading file:" << input_filename;
    }
    fclose(open_file);
    return encoded_buffer;
  }

  std::string input_filename_;
  std::string decoded_filename_;
  std::string encoded_filename_;
  EncodedImage* encoded_buffer_;
  JpegEncoder* encoder_;
};

TEST_F(JpegTest, Decode) {
  encoded_buffer_ = ReadEncodedImage(input_filename_);
  I420VideoFrame image_buffer;
  EXPECT_EQ(0, ConvertJpegToI420(*encoded_buffer_, &image_buffer));
  EXPECT_FALSE(image_buffer.IsZeroSize());
  EXPECT_EQ(kImageWidth, image_buffer.width());
  EXPECT_EQ(kImageHeight, image_buffer.height());
}

TEST_F(JpegTest, EncodeInvalidInputs) {
  I420VideoFrame empty;
  empty.set_width(164);
  empty.set_height(164);
  EXPECT_EQ(-1, encoder_->SetFileName(0));
  // Test empty (null) frame.
  EXPECT_EQ(-1, encoder_->Encode(empty));
  // Create empty frame (allocate memory) - arbitrary dimensions.
  empty.CreateEmptyFrame(10, 10, 10, 5, 5);
  empty.ResetSize();
  EXPECT_EQ(-1, encoder_->Encode(empty));
}

TEST_F(JpegTest, Encode) {
  // Decode our input image then encode it again to a new file:
  encoded_buffer_ = ReadEncodedImage(input_filename_);
  I420VideoFrame image_buffer;
  EXPECT_EQ(0, ConvertJpegToI420(*encoded_buffer_, &image_buffer));

  EXPECT_EQ(0, encoder_->SetFileName(encoded_filename_.c_str()));
  EXPECT_EQ(0, encoder_->Encode(image_buffer));

  // Save decoded image to file.
  FILE* save_file = fopen(decoded_filename_.c_str(), "wb");
  if (PrintI420VideoFrame(image_buffer, save_file)) {
    return;
  }
  fclose(save_file);

}

}  // namespace webrtc
