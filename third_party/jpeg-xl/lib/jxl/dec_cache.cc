// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "lib/jxl/dec_cache.h"

#include "lib/jxl/dec_reconstruct.h"

namespace jxl {

void PassesDecoderState::EnsureBordersStorage() {
  if (!EagerFinalizeImageRect()) return;
  size_t padding = FinalizeRectPadding();
  size_t bordery = 2 * padding;
  size_t borderx = padding + group_border_assigner.PaddingX(padding);
  Rect horizontal = Rect(0, 0, shared->frame_dim.xsize_padded,
                         bordery * shared->frame_dim.ysize_groups * 2);
  if (!SameSize(horizontal, borders_horizontal)) {
    borders_horizontal = Image3F(horizontal.xsize(), horizontal.ysize());
  }
  Rect vertical = Rect(0, 0, borderx * shared->frame_dim.xsize_groups * 2,
                       shared->frame_dim.ysize_padded);
  if (!SameSize(vertical, borders_vertical)) {
    borders_vertical = Image3F(vertical.xsize(), vertical.ysize());
  }
}

namespace {
void SaveBorders(const Rect& block_rect, size_t hshift, size_t vshift,
                 size_t padding, const ImageF& plane_in,
                 ImageF* border_storage_h, ImageF* border_storage_v) {
  constexpr size_t kGroupDataXBorder = PassesDecoderState::kGroupDataXBorder;
  constexpr size_t kGroupDataYBorder = PassesDecoderState::kGroupDataYBorder;
  size_t x0 = DivCeil(block_rect.x0() * kBlockDim, 1 << hshift);
  size_t x1 =
      DivCeil((block_rect.x0() + block_rect.xsize()) * kBlockDim, 1 << hshift);
  size_t y0 = DivCeil(block_rect.y0() * kBlockDim, 1 << vshift);
  size_t y1 =
      DivCeil((block_rect.y0() + block_rect.ysize()) * kBlockDim, 1 << vshift);
  size_t gy = block_rect.y0() / kGroupDimInBlocks;
  size_t gx = block_rect.x0() / kGroupDimInBlocks;
  // TODO(veluca): this is too much with chroma upsampling. It's just
  // inefficient though.
  size_t borderx = GroupBorderAssigner::PaddingX(padding);
  size_t bordery = padding;
  size_t borderx_write = padding + borderx;
  size_t bordery_write = padding + bordery;
  CopyImageTo(
      Rect(kGroupDataXBorder, kGroupDataYBorder, x1 - x0, bordery_write),
      plane_in, Rect(x0, (gy * 2) * bordery_write, x1 - x0, bordery_write),
      border_storage_h);
  CopyImageTo(
      Rect(kGroupDataXBorder, kGroupDataYBorder + y1 - y0 - bordery_write,
           x1 - x0, bordery_write),
      plane_in, Rect(x0, (gy * 2 + 1) * bordery_write, x1 - x0, bordery_write),
      border_storage_h);
  CopyImageTo(
      Rect(kGroupDataXBorder, kGroupDataYBorder, borderx_write, y1 - y0),
      plane_in, Rect((gx * 2) * borderx_write, y0, borderx_write, y1 - y0),
      border_storage_v);
  CopyImageTo(Rect(kGroupDataXBorder + x1 - x0 - borderx_write,
                   kGroupDataYBorder, borderx_write, y1 - y0),
              plane_in,
              Rect((gx * 2 + 1) * borderx_write, y0, borderx_write, y1 - y0),
              border_storage_v);
}

void LoadBorders(const Rect& block_rect, size_t hshift, size_t vshift,
                 const FrameDimensions& frame_dim, size_t padding,
                 const ImageF& border_storage_h, const ImageF& border_storage_v,
                 const Rect& r, ImageF* plane_out) {
  constexpr size_t kGroupDataXBorder = PassesDecoderState::kGroupDataXBorder;
  constexpr size_t kGroupDataYBorder = PassesDecoderState::kGroupDataYBorder;
  size_t x0 = DivCeil(block_rect.x0() * kBlockDim, 1 << hshift);
  size_t x1 =
      DivCeil((block_rect.x0() + block_rect.xsize()) * kBlockDim, 1 << hshift);
  size_t y0 = DivCeil(block_rect.y0() * kBlockDim, 1 << vshift);
  size_t y1 =
      DivCeil((block_rect.y0() + block_rect.ysize()) * kBlockDim, 1 << vshift);
  size_t gy = block_rect.y0() / kGroupDimInBlocks;
  size_t gx = block_rect.x0() / kGroupDimInBlocks;
  size_t borderx = GroupBorderAssigner::PaddingX(padding);
  size_t bordery = padding;
  size_t borderx_write = padding + borderx;
  size_t bordery_write = padding + bordery;
  // Limits of the area to copy from, in image coordinates.
  JXL_DASSERT(r.x0() == 0 || r.x0() >= borderx);
  size_t x0src = DivCeil(r.x0() == 0 ? r.x0() : r.x0() - borderx, 1 << hshift);
  size_t x1src =
      DivCeil(r.x0() + r.xsize() +
                  (r.x0() + r.xsize() == frame_dim.xsize_padded ? 0 : borderx),
              1 << hshift);
  JXL_DASSERT(r.y0() == 0 || r.y0() >= bordery);
  size_t y0src = DivCeil(r.y0() == 0 ? r.y0() : r.y0() - bordery, 1 << vshift);
  size_t y1src =
      DivCeil(r.y0() + r.ysize() +
                  (r.y0() + r.ysize() == frame_dim.ysize_padded ? 0 : bordery),
              1 << vshift);
  // Copy other groups' borders from the border storage.
  if (y0src < y0) {
    CopyImageTo(
        Rect(x0src, (gy * 2 - 1) * bordery_write, x1src - x0src, bordery_write),
        border_storage_h,
        Rect(kGroupDataXBorder + x0src - x0, kGroupDataYBorder - bordery_write,
             x1src - x0src, bordery_write),
        plane_out);
  }
  if (y1src > y1) {
    CopyImageTo(
        Rect(x0src, (gy * 2 + 2) * bordery_write, x1src - x0src, bordery_write),
        border_storage_h,
        Rect(kGroupDataXBorder + x0src - x0, kGroupDataYBorder + y1 - y0,
             x1src - x0src, bordery_write),
        plane_out);
  }
  if (x0src < x0) {
    CopyImageTo(
        Rect((gx * 2 - 1) * borderx_write, y0src, borderx_write, y1src - y0src),
        border_storage_v,
        Rect(kGroupDataXBorder - borderx_write, kGroupDataYBorder + y0src - y0,
             borderx_write, y1src - y0src),
        plane_out);
  }
  if (x1src > x1) {
    CopyImageTo(
        Rect((gx * 2 + 2) * borderx_write, y0src, borderx_write, y1src - y0src),
        border_storage_v,
        Rect(kGroupDataXBorder + x1 - x0, kGroupDataYBorder + y0src - y0,
             borderx_write, y1src - y0src),
        plane_out);
  }
}

}  // namespace

Status PassesDecoderState::FinalizeGroup(size_t group_idx, size_t thread,
                                         Image3F* pixel_data,
                                         ImageBundle* output) {
  // Copy the group borders to the border storage.
  const Rect block_rect = shared->BlockGroupRect(group_idx);
  const YCbCrChromaSubsampling& cs = shared->frame_header.chroma_subsampling;
  size_t padding = FinalizeRectPadding();
  for (size_t c = 0; c < 3; c++) {
    SaveBorders(block_rect, cs.HShift(c), cs.VShift(c), padding,
                pixel_data->Plane(c), &borders_horizontal.Plane(c),
                &borders_vertical.Plane(c));
  }
  Rect fir_rects[GroupBorderAssigner::kMaxToFinalize];
  size_t num_fir_rects = 0;
  group_border_assigner.GroupDone(group_idx, FinalizeRectPadding(), fir_rects,
                                  &num_fir_rects);
  for (size_t i = 0; i < num_fir_rects; i++) {
    const Rect& r = fir_rects[i];
    for (size_t c = 0; c < 3; c++) {
      LoadBorders(block_rect, cs.HShift(c), cs.VShift(c), shared->frame_dim,
                  padding, borders_horizontal.Plane(c),
                  borders_vertical.Plane(c), r, &pixel_data->Plane(c));
    }
    Rect pixel_data_rect(
        kGroupDataXBorder + r.x0() - block_rect.x0() * kBlockDim,
        kGroupDataYBorder + r.y0() - block_rect.y0() * kBlockDim, r.xsize(),
        r.ysize());
    JXL_RETURN_IF_ERROR(FinalizeImageRect(pixel_data, pixel_data_rect, {}, this,
                                          thread, output, r));
  }
  return true;
}

}  // namespace jxl
