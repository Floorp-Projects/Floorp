/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/desktop_capture/differ.h"

#include "string.h"

#include "webrtc/modules/desktop_capture/differ_block.h"
#include "webrtc/system_wrappers/interface/logging.h"

namespace webrtc {

Differ::Differ(int width, int height, int bpp, int stride) {
  // Dimensions of screen.
  width_ = width;
  height_ = height;
  bytes_per_pixel_ = bpp;
  bytes_per_row_ = stride;

  // Calc number of blocks (full and partial) required to cover entire image.
  // One additional row/column is added as a boundary on the right & bottom.
  diff_info_width_ = ((width_ + kBlockSize - 1) / kBlockSize) + 1;
  diff_info_height_ = ((height_ + kBlockSize - 1) / kBlockSize) + 1;
  diff_info_size_ = diff_info_width_ * diff_info_height_ * sizeof(DiffInfo);
  diff_info_.reset(new DiffInfo[diff_info_size_]);
}

Differ::~Differ() {}

void Differ::CalcDirtyRegion(const void* prev_buffer, const void* curr_buffer,
                             DesktopRegion* region) {
  // Identify all the blocks that contain changed pixels.
  MarkDirtyBlocks(prev_buffer, curr_buffer);

  // Now that we've identified the blocks that have changed, merge adjacent
  // blocks to minimize the number of rects that we return.
  MergeBlocks(region);
}

void Differ::MarkDirtyBlocks(const void* prev_buffer, const void* curr_buffer) {
  memset(diff_info_.get(), 0, diff_info_size_);

  // Calc number of full blocks.
  int x_full_blocks = width_ / kBlockSize;
  int y_full_blocks = height_ / kBlockSize;

  // Calc size of partial blocks which may be present on right and bottom edge.
  int partial_column_width = width_ - (x_full_blocks * kBlockSize);
  int partial_row_height = height_ - (y_full_blocks * kBlockSize);

  // Offset from the start of one block-column to the next.
  int block_x_offset = bytes_per_pixel_ * kBlockSize;
  // Offset from the start of one block-row to the next.
  int block_y_stride = (width_ * bytes_per_pixel_) * kBlockSize;
  // Offset from the start of one diff_info row to the next.
  int diff_info_stride = diff_info_width_ * sizeof(DiffInfo);

  const uint8_t* prev_block_row_start =
      static_cast<const uint8_t*>(prev_buffer);
  const uint8_t* curr_block_row_start =
      static_cast<const uint8_t*>(curr_buffer);
  DiffInfo* diff_info_row_start = static_cast<DiffInfo*>(diff_info_.get());

  for (int y = 0; y < y_full_blocks; y++) {
    const uint8_t* prev_block = prev_block_row_start;
    const uint8_t* curr_block = curr_block_row_start;
    DiffInfo* diff_info = diff_info_row_start;

    for (int x = 0; x < x_full_blocks; x++) {
      // Mark this block as being modified so that it gets incorporated into
      // a dirty rect.
      *diff_info = BlockDifference(prev_block, curr_block, bytes_per_row_);
      prev_block += block_x_offset;
      curr_block += block_x_offset;
      diff_info += sizeof(DiffInfo);
    }

    // If there is a partial column at the end, handle it.
    // This condition should rarely, if ever, occur.
    if (partial_column_width != 0) {
      *diff_info = DiffPartialBlock(prev_block, curr_block, bytes_per_row_,
                                    partial_column_width, kBlockSize);
      diff_info += sizeof(DiffInfo);
    }

    // Update pointers for next row.
    prev_block_row_start += block_y_stride;
    curr_block_row_start += block_y_stride;
    diff_info_row_start += diff_info_stride;
  }

  // If the screen height is not a multiple of the block size, then this
  // handles the last partial row. This situation is far more common than the
  // 'partial column' case.
  if (partial_row_height != 0) {
    const uint8_t* prev_block = prev_block_row_start;
    const uint8_t* curr_block = curr_block_row_start;
    DiffInfo* diff_info = diff_info_row_start;
    for (int x = 0; x < x_full_blocks; x++) {
      *diff_info = DiffPartialBlock(prev_block, curr_block,
                                    bytes_per_row_,
                                    kBlockSize, partial_row_height);
      prev_block += block_x_offset;
      curr_block += block_x_offset;
      diff_info += sizeof(DiffInfo);
    }
    if (partial_column_width != 0) {
      *diff_info = DiffPartialBlock(prev_block, curr_block, bytes_per_row_,
                                    partial_column_width, partial_row_height);
      diff_info += sizeof(DiffInfo);
    }
  }
}

DiffInfo Differ::DiffPartialBlock(const uint8_t* prev_buffer,
                                  const uint8_t* curr_buffer,
                                  int stride, int width, int height) {
  int width_bytes = width * bytes_per_pixel_;
  for (int y = 0; y < height; y++) {
    if (memcmp(prev_buffer, curr_buffer, width_bytes) != 0)
      return 1;
    prev_buffer += bytes_per_row_;
    curr_buffer += bytes_per_row_;
  }
  return 0;
}

void Differ::MergeBlocks(DesktopRegion* region) {
  region->Clear();

  uint8_t* diff_info_row_start = static_cast<uint8_t*>(diff_info_.get());
  int diff_info_stride = diff_info_width_ * sizeof(DiffInfo);

  for (int y = 0; y < diff_info_height_; y++) {
    uint8_t* diff_info = diff_info_row_start;
    for (int x = 0; x < diff_info_width_; x++) {
      if (*diff_info != 0) {
        // We've found a modified block. Look at blocks to the right and below
        // to group this block with as many others as we can.
        int left = x * kBlockSize;
        int top = y * kBlockSize;
        int width = 1;
        int height = 1;
        *diff_info = 0;

        // Group with blocks to the right.
        // We can keep looking until we find an unchanged block because we
        // have a boundary block which is never marked as having diffs.
        uint8_t* right = diff_info + 1;
        while (*right) {
          *right++ = 0;
          width++;
        }

        // Group with blocks below.
        // The entire width of blocks that we matched above much match for
        // each row that we add.
        uint8_t* bottom = diff_info;
        bool found_new_row;
        do {
          found_new_row = true;
          bottom += diff_info_stride;
          right = bottom;
          for (int x2 = 0; x2 < width; x2++) {
            if (*right++ == 0) {
              found_new_row = false;
            }
          }

          if (found_new_row) {
            height++;

            // We need to go back and erase the diff markers so that we don't
            // try to add these blocks a second time.
            right = bottom;
            for (int x2 = 0; x2 < width; x2++) {
              *right++ = 0;
            }
          }
        } while (found_new_row);

        // Add rect to list of dirty rects.
        width *= kBlockSize;
        if (left + width > width_) {
          width = width_ - left;
        }
        height *= kBlockSize;
        if (top + height > height_) {
          height = height_ - top;
        }
        region->AddRect(DesktopRect::MakeXYWH(left, top, width, height));
      }

      // Increment to next block in this row.
      diff_info++;
    }

    // Go to start of next row.
    diff_info_row_start += diff_info_stride;
  }
}

}  // namespace webrtc
