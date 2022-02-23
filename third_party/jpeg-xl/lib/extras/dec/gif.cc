// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "lib/extras/dec/gif.h"

#include <gif_lib.h>
#include <string.h>

#include <utility>
#include <vector>

#include "lib/jxl/base/compiler_specific.h"
#include "lib/jxl/sanitizers.h"

namespace jxl {
namespace extras {

namespace {

struct ReadState {
  Span<const uint8_t> bytes;
};

struct DGifCloser {
  void operator()(GifFileType* const ptr) const { DGifCloseFile(ptr, nullptr); }
};
using GifUniquePtr = std::unique_ptr<GifFileType, DGifCloser>;

struct PackedRgba {
  uint8_t r, g, b, a;
};

// Gif does not support partial transparency, so this considers anything non-0
// as opaque.
bool AllOpaque(const PackedImage& color) {
  for (size_t y = 0; y < color.ysize; ++y) {
    const PackedRgba* const JXL_RESTRICT row =
        static_cast<const PackedRgba*>(color.pixels()) + y * color.xsize;
    for (size_t x = 0; x < color.xsize; ++x) {
      if (row[x].a == 0) {
        return false;
      }
    }
  }
  return true;
}

}  // namespace

Status DecodeImageGIF(Span<const uint8_t> bytes, const ColorHints& color_hints,
                      const SizeConstraints& constraints,
                      PackedPixelFile* ppf) {
  int error = GIF_OK;
  ReadState state = {bytes};
  const auto ReadFromSpan = [](GifFileType* const gif, GifByteType* const bytes,
                               int n) {
    ReadState* const state = reinterpret_cast<ReadState*>(gif->UserData);
    // giflib API requires the input size `n` to be signed int.
    if (static_cast<size_t>(n) > state->bytes.size()) {
      n = state->bytes.size();
    }
    memcpy(bytes, state->bytes.data(), n);
    state->bytes.remove_prefix(n);
    return n;
  };
  GifUniquePtr gif(DGifOpen(&state, ReadFromSpan, &error));
  if (gif == nullptr) {
    if (error == D_GIF_ERR_NOT_GIF_FILE) {
      // Not an error.
      return false;
    } else {
      return JXL_FAILURE("Failed to read GIF: %s", GifErrorString(error));
    }
  }
  error = DGifSlurp(gif.get());
  if (error != GIF_OK) {
    return JXL_FAILURE("Failed to read GIF: %s", GifErrorString(gif->Error));
  }

  msan::UnpoisonMemory(gif.get(), sizeof(*gif));
  if (gif->SColorMap) {
    msan::UnpoisonMemory(gif->SColorMap, sizeof(*gif->SColorMap));
    msan::UnpoisonMemory(
        gif->SColorMap->Colors,
        sizeof(*gif->SColorMap->Colors) * gif->SColorMap->ColorCount);
  }
  msan::UnpoisonMemory(gif->SavedImages,
                       sizeof(*gif->SavedImages) * gif->ImageCount);

  JXL_RETURN_IF_ERROR(
      VerifyDimensions<uint32_t>(&constraints, gif->SWidth, gif->SHeight));
  uint64_t total_pixel_count =
      static_cast<uint64_t>(gif->SWidth) * gif->SHeight;
  for (int i = 0; i < gif->ImageCount; ++i) {
    const SavedImage& image = gif->SavedImages[i];
    uint32_t w = image.ImageDesc.Width;
    uint32_t h = image.ImageDesc.Height;
    JXL_RETURN_IF_ERROR(VerifyDimensions<uint32_t>(&constraints, w, h));
    uint64_t pixel_count = static_cast<uint64_t>(w) * h;
    if (total_pixel_count + pixel_count < total_pixel_count) {
      return JXL_FAILURE("Image too big");
    }
    total_pixel_count += pixel_count;
    if (total_pixel_count > constraints.dec_max_pixels) {
      return JXL_FAILURE("Image too big");
    }
  }

  if (!gif->SColorMap) {
    for (int i = 0; i < gif->ImageCount; ++i) {
      if (!gif->SavedImages[i].ImageDesc.ColorMap) {
        return JXL_FAILURE("Missing GIF color map");
      }
    }
  }

  if (gif->ImageCount > 1) {
    ppf->info.have_animation = true;
    // Delays in GIF are specified in 100ths of a second.
    ppf->info.animation.tps_numerator = 100;
    ppf->info.animation.tps_denominator = 1;
  }

  ppf->frames.clear();
  ppf->frames.reserve(gif->ImageCount);

  ppf->info.xsize = gif->SWidth;
  ppf->info.ysize = gif->SHeight;
  ppf->info.bits_per_sample = 8;
  ppf->info.exponent_bits_per_sample = 0;
  // alpha_bits is later set to 8 if we find a frame with transparent pixels.
  ppf->info.alpha_bits = 0;
  ppf->info.alpha_exponent_bits = 0;
  JXL_RETURN_IF_ERROR(ApplyColorHints(color_hints, /*color_already_set=*/false,
                                      /*is_gray=*/false, ppf));

  ppf->info.num_color_channels = 3;

  const JxlPixelFormat format{
      /*num_channels=*/4u,
      /*data_type=*/JXL_TYPE_UINT8,
      /*endianness=*/JXL_NATIVE_ENDIAN,
      /*align=*/0,
  };

  GifColorType background_color;
  if (gif->SColorMap == nullptr ||
      gif->SBackGroundColor >= gif->SColorMap->ColorCount) {
    background_color = {0, 0, 0};
  } else {
    background_color = gif->SColorMap->Colors[gif->SBackGroundColor];
  }
  const PackedRgba background_rgba{background_color.Red, background_color.Green,
                                   background_color.Blue, 0};
  PackedFrame canvas(gif->SWidth, gif->SHeight, format);
  std::fill_n(static_cast<PackedRgba*>(canvas.color.pixels()),
              canvas.color.xsize * canvas.color.ysize, background_rgba);
  Rect canvas_rect{0, 0, canvas.color.xsize, canvas.color.ysize};

  Rect previous_rect_if_restore_to_background;

  bool has_alpha = false;
  bool replace = true;
  bool last_base_was_none = true;
  for (int i = 0; i < gif->ImageCount; ++i) {
    const SavedImage& image = gif->SavedImages[i];
    msan::UnpoisonMemory(image.RasterBits, sizeof(*image.RasterBits) *
                                               image.ImageDesc.Width *
                                               image.ImageDesc.Height);
    const Rect image_rect(image.ImageDesc.Left, image.ImageDesc.Top,
                          image.ImageDesc.Width, image.ImageDesc.Height);

    Rect total_rect;
    if (previous_rect_if_restore_to_background.xsize() != 0 ||
        previous_rect_if_restore_to_background.ysize() != 0) {
      const size_t xbegin = std::min(
          image_rect.x0(), previous_rect_if_restore_to_background.x0());
      const size_t ybegin = std::min(
          image_rect.y0(), previous_rect_if_restore_to_background.y0());
      const size_t xend =
          std::max(image_rect.x0() + image_rect.xsize(),
                   previous_rect_if_restore_to_background.x0() +
                       previous_rect_if_restore_to_background.xsize());
      const size_t yend =
          std::max(image_rect.y0() + image_rect.ysize(),
                   previous_rect_if_restore_to_background.y0() +
                       previous_rect_if_restore_to_background.ysize());
      total_rect = Rect(xbegin, ybegin, xend - xbegin, yend - ybegin);
      previous_rect_if_restore_to_background = Rect();
      replace = true;
    } else {
      total_rect = image_rect;
      replace = false;
    }
    if (!image_rect.IsInside(canvas_rect)) {
      return JXL_FAILURE("GIF frame extends outside of the canvas");
    }

    // Allocates the frame buffer.
    ppf->frames.emplace_back(total_rect.xsize(), total_rect.ysize(), format);
    auto* frame = &ppf->frames.back();

    const ColorMapObject* const color_map =
        image.ImageDesc.ColorMap ? image.ImageDesc.ColorMap : gif->SColorMap;
    JXL_CHECK(color_map);
    msan::UnpoisonMemory(color_map, sizeof(*color_map));
    msan::UnpoisonMemory(color_map->Colors,
                         sizeof(*color_map->Colors) * color_map->ColorCount);
    GraphicsControlBlock gcb;
    DGifSavedExtensionToGCB(gif.get(), i, &gcb);
    msan::UnpoisonMemory(&gcb, sizeof(gcb));

    if (ppf->info.have_animation) {
      frame->frame_info.duration = gcb.DelayTime;
      frame->x0 = image_rect.x0();
      frame->y0 = image_rect.y0();
      if (last_base_was_none) {
        replace = true;
      }
      frame->blend = !replace;
      // TODO(veluca): this could in principle be implemented.
      if (last_base_was_none &&
          (total_rect.x0() != 0 || total_rect.y0() != 0 ||
           total_rect.xsize() != canvas.color.xsize ||
           total_rect.ysize() != canvas.color.ysize || !replace)) {
        return JXL_FAILURE(
            "GIF with dispose-to-0 is not supported for non-full or "
            "blended frames");
      }
      switch (gcb.DisposalMode) {
        case DISPOSE_DO_NOT:
        case DISPOSE_BACKGROUND:
          frame->use_for_next_frame = true;
          last_base_was_none = false;
          break;
        case DISPOSE_PREVIOUS:
          frame->use_for_next_frame = false;
          break;
        default:
          frame->use_for_next_frame = false;
          last_base_was_none = true;
      }
    }

    // Update the canvas by creating a copy first.
    PackedImage new_canvas_image(canvas.color.xsize, canvas.color.ysize,
                                 canvas.color.format);
    memcpy(new_canvas_image.pixels(), canvas.color.pixels(),
           new_canvas_image.pixels_size);
    for (size_t y = 0, byte_index = 0; y < image_rect.ysize(); ++y) {
      // Assumes format.align == 0. row points to the beginning of the y row in
      // the image_rect.
      PackedRgba* row = static_cast<PackedRgba*>(new_canvas_image.pixels()) +
                        (y + image_rect.y0()) * new_canvas_image.xsize +
                        image_rect.x0();
      for (size_t x = 0; x < image_rect.xsize(); ++x, ++byte_index) {
        const GifByteType byte = image.RasterBits[byte_index];
        if (byte >= color_map->ColorCount) {
          return JXL_FAILURE("GIF color is out of bounds");
        }

        if (byte == gcb.TransparentColor) continue;
        GifColorType color = color_map->Colors[byte];
        row[x].r = color.Red;
        row[x].g = color.Green;
        row[x].b = color.Blue;
        row[x].a = 255;
      }
    }
    const PackedImage& sub_frame_image = frame->color;
    bool blend_alpha = false;
    if (replace) {
      // Copy from the new canvas image to the subframe
      for (size_t y = 0; y < total_rect.ysize(); ++y) {
        const PackedRgba* row_in =
            static_cast<const PackedRgba*>(new_canvas_image.pixels()) +
            (y + total_rect.y0()) * new_canvas_image.xsize + total_rect.x0();
        PackedRgba* row_out =
            static_cast<PackedRgba*>(sub_frame_image.pixels()) +
            y * sub_frame_image.xsize;
        memcpy(row_out, row_in, sub_frame_image.xsize * sizeof(PackedRgba));
      }
    } else {
      for (size_t y = 0, byte_index = 0; y < image_rect.ysize(); ++y) {
        // Assumes format.align == 0
        PackedRgba* row = static_cast<PackedRgba*>(sub_frame_image.pixels()) +
                          y * sub_frame_image.xsize;
        for (size_t x = 0; x < image_rect.xsize(); ++x, ++byte_index) {
          const GifByteType byte = image.RasterBits[byte_index];
          if (byte > color_map->ColorCount) {
            return JXL_FAILURE("GIF color is out of bounds");
          }
          if (byte == gcb.TransparentColor) {
            row[x].r = 0;
            row[x].g = 0;
            row[x].b = 0;
            row[x].a = 0;
            blend_alpha =
                true;  // need to use alpha channel if BlendMode blend is used
            continue;
          }
          GifColorType color = color_map->Colors[byte];
          row[x].r = color.Red;
          row[x].g = color.Green;
          row[x].b = color.Blue;
          row[x].a = 255;
        }
      }
    }

    if (!has_alpha && (!AllOpaque(sub_frame_image) || blend_alpha)) {
      has_alpha = true;
      ppf->info.alpha_bits = 8;
    }

    switch (gcb.DisposalMode) {
      case DISPOSE_DO_NOT:
        canvas.color = std::move(new_canvas_image);
        break;

      case DISPOSE_BACKGROUND:
        std::fill_n(static_cast<PackedRgba*>(canvas.color.pixels()),
                    canvas.color.xsize * canvas.color.ysize, background_rgba);
        previous_rect_if_restore_to_background = image_rect;
        break;

      case DISPOSE_PREVIOUS:
        break;

      case DISPOSAL_UNSPECIFIED:
      default:
        std::fill_n(static_cast<PackedRgba*>(canvas.color.pixels()),
                    canvas.color.xsize * canvas.color.ysize, background_rgba);
    }
  }

  return true;
}

}  // namespace extras
}  // namespace jxl
