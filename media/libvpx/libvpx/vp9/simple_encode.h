/*
 *  Copyright (c) 2019 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VPX_VP9_SIMPLE_ENCODE_H_
#define VPX_VP9_SIMPLE_ENCODE_H_

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <vector>

namespace vp9 {

enum FrameType {
  kKeyFrame = 0,
  kInterFrame,
  kAlternateReference,
};

struct EncodeFrameResult {
  int show_idx;
  FrameType frame_type;
  size_t coding_data_bit_size;
  size_t coding_data_byte_size;
  // The EncodeFrame will allocate a buffer, write the coding data into the
  // buffer and give the ownership of the buffer to coding_data.
  std::unique_ptr<unsigned char[]> coding_data;
  double psnr;
  uint64_t sse;
  int quantize_index;
};

class SimpleEncode {
 public:
  SimpleEncode(int frame_width, int frame_height, int frame_rate_num,
               int frame_rate_den, int target_bitrate, int num_frames,
               const char *infile_path);
  ~SimpleEncode();
  SimpleEncode(SimpleEncode &) = delete;
  SimpleEncode &operator=(const SimpleEncode &) = delete;

  // Makes encoder compute the first pass stats and store it internally for
  // future encode.
  void ComputeFirstPassStats();

  // Outputs the first pass stats represented by a 2-D vector.
  // One can use the frame index at first dimension to retrieve the stats for
  // each video frame. The stats of each video frame is a vector of 25 double
  // values. For details, please check FIRSTPASS_STATS in vp9_firstpass.h
  std::vector<std::vector<double>> ObserveFirstPassStats();

  // Initializes the encoder for actual encoding.
  // This function should be called after ComputeFirstPassStats().
  void StartEncode();

  // Frees the encoder.
  // This function should be called after StartEncode() or EncodeFrame().
  void EndEncode();

  // Given a key_frame_index, computes this key frame group's size.
  // The key frame group size includes one key frame plus the number of
  // following inter frames. Note that the key frame group size only counts the
  // show frames. The number of no show frames like alternate refereces are not
  // counted.
  int GetKeyFrameGroupSize(int key_frame_index) const;

  // Encodes a frame
  // This function should be called after StartEncode() and before EndEncode().
  void EncodeFrame(EncodeFrameResult *encode_frame_result);

  // Encodes a frame with a specific quantize index.
  // This function should be called after StartEncode() and before EndEncode().
  void EncodeFrameWithQuantizeIndex(EncodeFrameResult *encode_frame_result,
                                    int quantize_index);

  // Gets the number of coding frames for the video. The coding frames include
  // show frame and no show frame.
  // This function should be called after ComputeFirstPassStats().
  int GetCodingFrameNum() const;

 private:
  class EncodeImpl;

  int frame_width_;
  int frame_height_;
  int frame_rate_num_;
  int frame_rate_den_;
  int target_bitrate_;
  int num_frames_;
  std::FILE *file_;
  std::unique_ptr<EncodeImpl> impl_ptr_;
};

}  // namespace vp9

#endif  // VPX_VP9_SIMPLE_ENCODE_H_
