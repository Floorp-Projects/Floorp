/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_DESKTOP_CAPTURE_DIFFER_H_
#define WEBRTC_MODULES_DESKTOP_CAPTURE_DIFFER_H_

#include <vector>

#include "webrtc/modules/desktop_capture/desktop_region.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"

namespace webrtc {

typedef uint8_t DiffInfo;

// TODO(sergeyu): Simplify differ now that we are working with DesktopRegion.
// diff_info_ should no longer be needed, as we can put our data directly into
// the region that we are calculating.
// http://crbug.com/92379
// TODO(sergeyu): Rename this class to something more sensible, e.g.
// ScreenCaptureFrameDifferencer.
class Differ {
 public:
  // Create a differ that operates on bitmaps with the specified width, height
  // and bytes_per_pixel.
  Differ(int width, int height, int bytes_per_pixel, int stride);
  ~Differ();

  int width() { return width_; }
  int height() { return height_; }
  int bytes_per_pixel() { return bytes_per_pixel_; }
  int bytes_per_row() { return bytes_per_row_; }

  // Given the previous and current screen buffer, calculate the dirty region
  // that encloses all of the changed pixels in the new screen.
  void CalcDirtyRegion(const void* prev_buffer, const void* curr_buffer,
                       DesktopRegion* region);

 private:
  // Allow tests to access our private parts.
  friend class DifferTest;

  // Identify all of the blocks that contain changed pixels.
  void MarkDirtyBlocks(const void* prev_buffer, const void* curr_buffer);

  // After the dirty blocks have been identified, this routine merges adjacent
  // blocks into a region.
  // The goal is to minimize the region that covers the dirty blocks.
  void MergeBlocks(DesktopRegion* region);

  // Check for diffs in upper-left portion of the block. The size of the portion
  // to check is specified by the |width| and |height| values.
  // Note that if we force the capturer to always return images whose width and
  // height are multiples of kBlockSize, then this will never be called.
  DiffInfo DiffPartialBlock(const uint8_t* prev_buffer,
                            const uint8_t* curr_buffer,
                            int stride,
                            int width, int height);

  // Dimensions of screen.
  int width_;
  int height_;

  // Number of bytes for each pixel in source and dest bitmap.
  // (Yes, they must match.)
  int bytes_per_pixel_;

  // Number of bytes in each row of the image (AKA: stride).
  int bytes_per_row_;

  // Diff information for each block in the image.
  scoped_array<DiffInfo> diff_info_;

  // Dimensions and total size of diff info array.
  int diff_info_width_;
  int diff_info_height_;
  int diff_info_size_;

  DISALLOW_COPY_AND_ASSIGN(Differ);
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_DESKTOP_CAPTURE_DIFFER_H_
