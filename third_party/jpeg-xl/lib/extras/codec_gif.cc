// Copyright (c) the JPEG XL Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "lib/extras/codec_gif.h"

#include <gif_lib.h>
#include <string.h>

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "lib/jxl/base/compiler_specific.h"
#include "lib/jxl/color_encoding_internal.h"
#include "lib/jxl/frame_header.h"
#include "lib/jxl/headers.h"
#include "lib/jxl/image.h"
#include "lib/jxl/image_bundle.h"
#include "lib/jxl/image_ops.h"
#include "lib/jxl/luminance.h"

#ifdef MEMORY_SANITIZER
#include "sanitizer/msan_interface.h"
#endif

namespace jxl {

namespace {

struct ReadState {
  Span<const uint8_t> bytes;
};

struct DGifCloser {
  void operator()(GifFileType* const ptr) const { DGifCloseFile(ptr, nullptr); }
};
using GifUniquePtr = std::unique_ptr<GifFileType, DGifCloser>;

// Gif does not support partial transparency, so this considers anything non-0
// as opaque.
bool AllOpaque(const ImageF& alpha) {
  for (size_t y = 0; y < alpha.ysize(); ++y) {
    const float* const JXL_RESTRICT row = alpha.ConstRow(y);
    for (size_t x = 0; x < alpha.xsize(); ++x) {
      if (row[x] == 0.f) {
        return false;
      }
    }
  }
  return true;
}

}  // namespace

Status DecodeImageGIF(Span<const uint8_t> bytes, ThreadPool* pool,
                      CodecInOut* io) {
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

#ifdef MEMORY_SANITIZER
  __msan_unpoison(gif.get(), sizeof(*gif));
  if (gif->SColorMap) {
    __msan_unpoison(gif->SColorMap, sizeof(*gif->SColorMap));
    __msan_unpoison(gif->SColorMap->Colors, sizeof(*gif->SColorMap->Colors) *
                                                gif->SColorMap->ColorCount);
  }
  __msan_unpoison(gif->SavedImages,
                  sizeof(*gif->SavedImages) * gif->ImageCount);
#endif

  const SizeConstraints* constraints = &io->constraints;

  JXL_RETURN_IF_ERROR(
      VerifyDimensions<uint32_t>(constraints, gif->SWidth, gif->SHeight));
  uint64_t total_pixel_count =
      static_cast<uint64_t>(gif->SWidth) * gif->SHeight;
  for (int i = 0; i < gif->ImageCount; ++i) {
    const SavedImage& image = gif->SavedImages[i];
    uint32_t w = image.ImageDesc.Width;
    uint32_t h = image.ImageDesc.Height;
    JXL_RETURN_IF_ERROR(VerifyDimensions<uint32_t>(constraints, w, h));
    uint64_t pixel_count = static_cast<uint64_t>(w) * h;
    if (total_pixel_count + pixel_count < total_pixel_count) {
      return JXL_FAILURE("Image too big");
    }
    total_pixel_count += pixel_count;
    if (total_pixel_count > constraints->dec_max_pixels) {
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
    io->metadata.m.have_animation = true;
    // Delays in GIF are specified in 100ths of a second.
    io->metadata.m.animation.tps_numerator = 100;
  }

  io->frames.clear();
  io->frames.reserve(gif->ImageCount);
  io->dec_pixels = 0;

  io->metadata.m.SetUintSamples(8);
  io->metadata.m.color_encoding = ColorEncoding::SRGB();
  io->metadata.m.SetAlphaBits(0);
  (void)io->dec_hints.Foreach(
      [](const std::string& key, const std::string& /*value*/) {
        JXL_WARNING("GIF decoder ignoring %s hint", key.c_str());
        return true;
      });

  Image3F canvas(gif->SWidth, gif->SHeight);
  io->SetSize(gif->SWidth, gif->SHeight);
  ImageF alpha(gif->SWidth, gif->SHeight);
  GifColorType background_color;
  if (gif->SColorMap == nullptr) {
    background_color = {0, 0, 0};
  } else {
    if (gif->SBackGroundColor >= gif->SColorMap->ColorCount) {
      return JXL_FAILURE("GIF specifies out-of-bounds background color");
    }
    background_color = gif->SColorMap->Colors[gif->SBackGroundColor];
  }
  FillPlane<float>(background_color.Red, &canvas.Plane(0));
  FillPlane<float>(background_color.Green, &canvas.Plane(1));
  FillPlane<float>(background_color.Blue, &canvas.Plane(2));
  ZeroFillImage(&alpha);

  Rect previous_rect_if_restore_to_background;

  bool has_alpha = false;
  bool replace = true;
  bool last_base_was_none = true;
  for (int i = 0; i < gif->ImageCount; ++i) {
    const SavedImage& image = gif->SavedImages[i];
#ifdef MEMORY_SANITIZER
    __msan_unpoison(image.RasterBits, sizeof(*image.RasterBits) *
                                          image.ImageDesc.Width *
                                          image.ImageDesc.Height);
#endif
    const Rect image_rect(image.ImageDesc.Left, image.ImageDesc.Top,
                          image.ImageDesc.Width, image.ImageDesc.Height);
    io->dec_pixels += image_rect.xsize() * image_rect.ysize();
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
    if (!image_rect.IsInside(canvas)) {
      return JXL_FAILURE("GIF frame extends outside of the canvas");
    }
    const ColorMapObject* const color_map =
        image.ImageDesc.ColorMap ? image.ImageDesc.ColorMap : gif->SColorMap;
    JXL_CHECK(color_map);
#ifdef MEMORY_SANITIZER
    __msan_unpoison(color_map, sizeof(*color_map));
    __msan_unpoison(color_map->Colors,
                    sizeof(*color_map->Colors) * color_map->ColorCount);
#endif
    GraphicsControlBlock gcb;
    DGifSavedExtensionToGCB(gif.get(), i, &gcb);
#ifdef MEMORY_SANITIZER
    __msan_unpoison(&gcb, sizeof(gcb));
#endif

    ImageBundle bundle(&io->metadata.m);
    if (io->metadata.m.have_animation) {
      bundle.duration = gcb.DelayTime;
      bundle.origin.x0 = total_rect.x0();
      bundle.origin.y0 = total_rect.y0();
      if (last_base_was_none) {
        replace = true;
      }
      bundle.blend = !replace;
      // TODO(veluca): this could in principle be implemented.
      if (last_base_was_none &&
          (total_rect.x0() != 0 || total_rect.y0() != 0 ||
           total_rect.xsize() != canvas.xsize() ||
           total_rect.ysize() != canvas.ysize() || !replace)) {
        return JXL_FAILURE(
            "GIF with dispose-to-0 is not supported for non-full or "
            "blended frames");
      }
      switch (gcb.DisposalMode) {
        case DISPOSE_DO_NOT:
        case DISPOSE_BACKGROUND:
          bundle.use_for_next_frame = true;
          last_base_was_none = false;
          break;
        case DISPOSE_PREVIOUS:
          bundle.use_for_next_frame = false;
          break;
        default:
          bundle.use_for_next_frame = false;
          last_base_was_none = true;
      }
    }
    Image3F frame = CopyImage(canvas);
    ImageF frame_alpha = CopyImage(alpha);
    for (size_t y = 0, byte_index = 0; y < image_rect.ysize(); ++y) {
      float* const JXL_RESTRICT row_r = image_rect.Row(&frame.Plane(0), y);
      float* const JXL_RESTRICT row_g = image_rect.Row(&frame.Plane(1), y);
      float* const JXL_RESTRICT row_b = image_rect.Row(&frame.Plane(2), y);
      float* const JXL_RESTRICT row_alpha = image_rect.Row(&frame_alpha, y);
      for (size_t x = 0; x < image_rect.xsize(); ++x, ++byte_index) {
        const GifByteType byte = image.RasterBits[byte_index];
        if (byte >= color_map->ColorCount) {
          return JXL_FAILURE("GIF color is out of bounds");
        }
        if (byte == gcb.TransparentColor) continue;
        GifColorType color = color_map->Colors[byte];
        row_alpha[x] = 1.f;
        row_r[x] = (1.f / 255) * color.Red;
        row_g[x] = (1.f / 255) * color.Green;
        row_b[x] = (1.f / 255) * color.Blue;
      }
    }
    Image3F sub_frame(total_rect.xsize(), total_rect.ysize());
    ImageF sub_frame_alpha(total_rect.xsize(), total_rect.ysize());
    bool blend_alpha = false;
    if (replace) {
      CopyImageTo(total_rect, frame, &sub_frame);
      CopyImageTo(total_rect, frame_alpha, &sub_frame_alpha);
    } else {
      for (size_t y = 0, byte_index = 0; y < image_rect.ysize(); ++y) {
        float* const JXL_RESTRICT row_r = sub_frame.PlaneRow(0, y);
        float* const JXL_RESTRICT row_g = sub_frame.PlaneRow(1, y);
        float* const JXL_RESTRICT row_b = sub_frame.PlaneRow(2, y);
        float* const JXL_RESTRICT row_alpha = sub_frame_alpha.Row(y);
        for (size_t x = 0; x < image_rect.xsize(); ++x, ++byte_index) {
          const GifByteType byte = image.RasterBits[byte_index];
          if (byte > color_map->ColorCount) {
            return JXL_FAILURE("GIF color is out of bounds");
          }
          if (byte == gcb.TransparentColor) {
            row_alpha[x] = 0;
            row_r[x] = 0;
            row_g[x] = 0;
            row_b[x] = 0;
            blend_alpha =
                true;  // need to use alpha channel if BlendMode blend is used
            continue;
          }
          GifColorType color = color_map->Colors[byte];
          row_alpha[x] = 1.f;
          row_r[x] = (1.f / 255) * color.Red;
          row_g[x] = (1.f / 255) * color.Green;
          row_b[x] = (1.f / 255) * color.Blue;
        }
      }
    }
    bundle.SetFromImage(std::move(sub_frame), ColorEncoding::SRGB());
    if (has_alpha || !AllOpaque(frame_alpha) || blend_alpha) {
      if (!has_alpha) {
        has_alpha = true;
        io->metadata.m.SetAlphaBits(8);
        for (ImageBundle& previous_frame : io->frames) {
          ImageF previous_alpha(previous_frame.xsize(), previous_frame.ysize());
          FillImage(1.f, &previous_alpha);
          previous_frame.SetAlpha(std::move(previous_alpha),
                                  /*alpha_is_premultiplied=*/false);
        }
      }
      bundle.SetAlpha(std::move(sub_frame_alpha),
                      /*alpha_is_premultiplied=*/false);
    }
    io->frames.push_back(std::move(bundle));
    switch (gcb.DisposalMode) {
      case DISPOSE_DO_NOT:
        canvas = std::move(frame);
        alpha = std::move(frame_alpha);
        break;

      case DISPOSE_BACKGROUND:
        FillPlane<float>((1.f / 255) * background_color.Red, &canvas.Plane(0),
                         image_rect);
        FillPlane<float>((1.f / 255) * background_color.Green, &canvas.Plane(1),
                         image_rect);
        FillPlane<float>((1.f / 255) * background_color.Blue, &canvas.Plane(2),
                         image_rect);
        FillPlane(0.f, &alpha, image_rect);
        previous_rect_if_restore_to_background = image_rect;
        break;

      case DISPOSE_PREVIOUS:
        break;

      case DISPOSAL_UNSPECIFIED:
      default:
        FillPlane<float>((1.f / 255) * background_color.Red, &canvas.Plane(0));
        FillPlane<float>((1.f / 255) * background_color.Green,
                         &canvas.Plane(1));
        FillPlane<float>((1.f / 255) * background_color.Blue, &canvas.Plane(2));
        ZeroFillImage(&alpha);
    }
  }

  SetIntensityTarget(io);
  return true;
}

}  // namespace jxl
