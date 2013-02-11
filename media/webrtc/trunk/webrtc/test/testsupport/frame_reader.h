/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_TEST_TESTSUPPORT_FRAME_READER_H_
#define WEBRTC_TEST_TESTSUPPORT_FRAME_READER_H_

#include <cstdio>
#include <string>

#include "typedefs.h"

namespace webrtc {
namespace test {

// Handles reading of frames from video files.
class FrameReader {
 public:
  virtual ~FrameReader() {}

  // Initializes the frame reader, i.e. opens the input file.
  // This must be called before reading of frames has started.
  // Returns false if an error has occurred, in addition to printing to stderr.
  virtual bool Init() = 0;

  // Reads a frame into the supplied buffer, which must contain enough space
  // for the frame size.
  // Returns true if there are more frames to read, false if we've already
  // read the last frame (in the previous call).
  virtual bool ReadFrame(WebRtc_UWord8* source_buffer) = 0;

  // Closes the input file if open. Essentially makes this class impossible
  // to use anymore. Will also be invoked by the destructor.
  virtual void Close() = 0;

  // Frame length in bytes of a single frame image.
  virtual int FrameLength() = 0;
  // Total number of frames in the input video source.
  virtual int NumberOfFrames() = 0;
};

class FrameReaderImpl : public FrameReader {
 public:
  // Creates a file handler. The input file is assumed to exist and be readable.
  // Parameters:
  //   input_filename          The file to read from.
  //   frame_length_in_bytes   The size of each frame.
  //                           For YUV this is 3 * width * height / 2
  FrameReaderImpl(std::string input_filename, int frame_length_in_bytes);
  virtual ~FrameReaderImpl();
  bool Init();
  bool ReadFrame(WebRtc_UWord8* source_buffer);
  void Close();
  int FrameLength() { return frame_length_in_bytes_; }
  int NumberOfFrames() { return number_of_frames_; }

 private:
  std::string input_filename_;
  int frame_length_in_bytes_;
  int number_of_frames_;
  FILE* input_file_;
};

}  // namespace test
}  // namespace webrtc

#endif  // WEBRTC_TEST_TESTSUPPORT_FRAME_READER_H_
