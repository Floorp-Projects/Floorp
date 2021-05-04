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

#include "lib/extras/codec_psd.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <numeric>
#include <string>
#include <utility>
#include <vector>

#include "lib/jxl/base/bits.h"
#include "lib/jxl/base/byte_order.h"
#include "lib/jxl/base/compiler_specific.h"
#include "lib/jxl/base/file_io.h"
#include "lib/jxl/color_management.h"
#include "lib/jxl/common.h"
#include "lib/jxl/fields.h"  // AllDefault
#include "lib/jxl/image.h"
#include "lib/jxl/image_bundle.h"
#include "lib/jxl/image_ops.h"
#include "lib/jxl/luminance.h"

namespace jxl {
namespace {

uint64_t get_be_int(int bytes, const uint8_t*& pos, const uint8_t* maxpos) {
  uint64_t r = 0;
  if (pos + bytes <= maxpos) {
    if (bytes == 1) {
      r = *pos;
    } else if (bytes == 2) {
      r = LoadBE16(pos);
    } else if (bytes == 4) {
      r = LoadBE32(pos);
    } else if (bytes == 8) {
      r = LoadBE64(pos);
    }
  }
  pos += bytes;
  return r;
}

// Copies up to n bytes, without reading from maxpos (the STL-style end).
void safe_copy(const uint8_t* JXL_RESTRICT pos,
               const uint8_t* JXL_RESTRICT maxpos, char* JXL_RESTRICT out,
               size_t n) {
  for (size_t i = 0; i < n; ++i) {
    if (pos + i >= maxpos) return;
    out[i] = pos[i];
  }
}

// maxpos is the STL-style end! The valid range is up to [pos, maxpos).
int safe_strncmp(const uint8_t* pos, const uint8_t* maxpos, const char* s2,
                 size_t n) {
  if (pos + n > maxpos) return 1;
  return strncmp((const char*)pos, s2, n);
}
constexpr int PSD_VERBOSITY = 1;

Status decode_layer(const uint8_t*& pos, const uint8_t* maxpos,
                    ImageBundle& layer, std::vector<int> chans,
                    std::vector<bool> invert, int w, int h, int version,
                    int colormodel, bool is_layer, int depth) {
  int compression_method = 2;
  int nb_channels = chans.size();
  JXL_DEBUG_V(PSD_VERBOSITY,
              "Trying to decode layer with dimensions %ix%i and %i channels", w,
              h, nb_channels);
  if (w <= 0 || h <= 0) return JXL_FAILURE("PSD: empty layer");
  for (int c = 0; c < nb_channels; c++) {
    // skip nop byte padding
    while (pos < maxpos && *pos == 128) pos++;
    JXL_DEBUG_V(PSD_VERBOSITY, "Channel %i (pos %zu)", c, (size_t)pos);
    // Merged image stores all channels together (same compression method)
    // Layers store channel per channel
    if (is_layer || c == 0) {
      compression_method = get_be_int(2, pos, maxpos);
      JXL_DEBUG_V(PSD_VERBOSITY, "compression method: %i", compression_method);
      if (compression_method > 1 || compression_method < 0) {
        return JXL_FAILURE("PSD: can't handle compression method %i",
                           compression_method);
      }
    }

    if (!is_layer && c < colormodel) {
      // skip to the extra channels
      if (compression_method == 0) {
        pos += w * h * (depth >> 3) * colormodel;
        c = colormodel - 1;
        continue;
      }
      size_t skip_amount = 0;
      for (int i = 0; i < nb_channels; i++) {
        if (i < colormodel) {
          for (int y = 0; y < h; y++) {
            skip_amount += get_be_int(2 * version, pos, maxpos);
          }
        } else {
          pos += h * 2 * version;
        }
      }
      pos += skip_amount;
      c = colormodel - 1;
      continue;
    }
    if (is_layer || c == 0) {
      // skip the line-counts, we don't need them
      if (compression_method == 1) {
        pos += h * (is_layer ? 1 : nb_channels) * 2 *
               version;  // PSB uses 4 bytes per rowsize instead of 2
      }
    }
    int c_id = chans[c];
    if (c_id < 0) continue;  // skip
    if (static_cast<unsigned int>(c_id) >= 3 + layer.extra_channels().size())
      return JXL_FAILURE("PSD: can't handle channel id %i", c_id);
    ImageF& ch = (c_id < 3 ? layer.color()->Plane(c_id)
                           : layer.extra_channels()[c_id - 3]);

    for (int y = 0; y < h; y++) {
      if (pos > maxpos) return JXL_FAILURE("PSD: premature end of input");
      float* const JXL_RESTRICT row = ch.Row(y);
      if (compression_method == 0) {
        // uncompressed is easy
        if (depth == 8) {
          for (int x = 0; x < w; x++) {
            row[x] = get_be_int(1, pos, maxpos) * (1.f / 255.f);
          }
        } else if (depth == 16) {
          for (int x = 0; x < w; x++) {
            row[x] = get_be_int(2, pos, maxpos) * (1.f / 65535.f);
          }
        } else if (depth == 32) {
          for (int x = 0; x < w; x++) {
            uint32_t f = get_be_int(4, pos, maxpos);
            memcpy(&row[x], &f, 4);
          }
        }
      } else {
        // RLE is not that hard
        if (depth != 8)
          return JXL_FAILURE("PSD: did not expect RLE with depth>1");
        for (int x = 0; x < w;) {
          if (pos >= maxpos) return JXL_FAILURE("PSD: out of bounds");
          int8_t rle = *pos++;
          if (rle <= 0) {
            if (rle == -128) continue;  // nop
            int count = 1 - rle;
            float v = get_be_int(1, pos, maxpos) * (1.f / 255.f);
            while (count && x < w) {
              row[x] = v;
              count--;
              x++;
            }
            if (count) return JXL_FAILURE("PSD: row overflow");
          } else {
            int count = 1 + rle;
            while (count && x < w) {
              row[x] = get_be_int(1, pos, maxpos) * (1.f / 255.f);
              count--;
              x++;
            }
            if (count) return JXL_FAILURE("PSD: row overflow");
          }
        }
      }
      if (invert[c]) {
        // sometimes 0 means full ink
        for (int x = 0; x < w; x++) {
          row[x] = 1.f - row[x];
        }
      }
    }
    JXL_DEBUG_V(PSD_VERBOSITY, "Channel %i read.", c);
  }

  return true;
}

}  // namespace

Status DecodeImagePSD(const Span<const uint8_t> bytes, ThreadPool* pool,
                      CodecInOut* io) {
  const uint8_t* pos = bytes.data();
  const uint8_t* maxpos = bytes.data() + bytes.size();
  if (safe_strncmp(pos, maxpos, "8BPS", 4)) return false;  // not a PSD file
  JXL_DEBUG_V(PSD_VERBOSITY, "trying psd decode");
  pos += 4;
  int version = get_be_int(2, pos, maxpos);
  JXL_DEBUG_V(PSD_VERBOSITY, "Version=%i", version);
  if (version < 1 || version > 2)
    return JXL_FAILURE("PSD: unknown format version");
  // PSD = version 1, PSB = version 2
  pos += 6;
  int nb_channels = get_be_int(2, pos, maxpos);
  size_t ysize = get_be_int(4, pos, maxpos);
  size_t xsize = get_be_int(4, pos, maxpos);
  const SizeConstraints* constraints = &io->constraints;
  JXL_RETURN_IF_ERROR(VerifyDimensions(constraints, xsize, ysize));
  uint64_t total_pixel_count = static_cast<uint64_t>(xsize) * ysize;
  int bitdepth = get_be_int(2, pos, maxpos);
  if (bitdepth != 8 && bitdepth != 16 && bitdepth != 32) {
    return JXL_FAILURE("PSD: bit depth %i invalid or not supported", bitdepth);
  }
  if (bitdepth == 32) {
    io->metadata.m.SetFloat32Samples();
  } else {
    io->metadata.m.SetUintSamples(bitdepth);
  }
  int colormodel = get_be_int(2, pos, maxpos);
  // 1 = Grayscale, 3 = RGB, 4 = CMYK
  if (colormodel != 1 && colormodel != 3 && colormodel != 4)
    return JXL_FAILURE("PSD: unsupported color model");

  int real_nb_channels = colormodel;
  std::vector<std::vector<float>> spotcolor;

  if (get_be_int(4, pos, maxpos))
    return JXL_FAILURE("PSD: Unsupported color mode section");

  bool hasmergeddata = true;
  bool have_alpha = false;
  bool merged_has_alpha = false;
  size_t metalength = get_be_int(4, pos, maxpos);
  const uint8_t* metaoffset = pos;
  while (pos < metaoffset + metalength) {
    char header[5] = "????";
    safe_copy(pos, maxpos, header, 4);
    if (memcmp(header, "8BIM", 4) != 0) {
      return JXL_FAILURE("PSD: Unexpected image resource header: %s", header);
    }
    pos += 4;
    int id = get_be_int(2, pos, maxpos);
    int namelength = get_be_int(1, pos, maxpos);
    pos += namelength;
    if (!(namelength & 1)) pos++;  // padding to even length
    size_t blocklength = get_be_int(4, pos, maxpos);
    // JXL_DEBUG_V(PSD_VERBOSITY, "block id: %i | block length: %zu",id,
    // blocklength);
    if (pos > maxpos) return JXL_FAILURE("PSD: Unexpected end of file");
    if (id == 1039) {  // ICC profile
      size_t delta = maxpos - pos;
      if (delta < blocklength) {
        return JXL_FAILURE("PSD: Invalid block length");
      }
      PaddedBytes icc;
      icc.resize(blocklength);
      memcpy(icc.data(), pos, blocklength);
      if (!io->metadata.m.color_encoding.SetICC(std::move(icc))) {
        return JXL_FAILURE("PSD: Invalid color profile");
      }
    } else if (id == 1057) {  // compatibility mode or not?
      if (get_be_int(4, pos, maxpos) != 1) {
        return JXL_FAILURE("PSD: expected version=1 in id=1057 resource block");
      }
      hasmergeddata = get_be_int(1, pos, maxpos);
      pos++;
      blocklength -= 6;       // already skipped these bytes
    } else if (id == 1077) {  // spot colors
      int version = get_be_int(4, pos, maxpos);
      if (version != 1) {
        return JXL_FAILURE(
            "PSD: expected DisplayInfo version 1, got version %i", version);
      }
      int spotcolorcount = nb_channels - colormodel;
      JXL_DEBUG_V(PSD_VERBOSITY, "Reading %i spot colors. %zu", spotcolorcount,
                  blocklength);
      for (int k = 0; k < spotcolorcount; k++) {
        int colorspace = get_be_int(2, pos, maxpos);
        if ((colormodel == 3 && colorspace != 0) ||
            (colormodel == 4 && colorspace != 2)) {
          return JXL_FAILURE(
              "PSD: cannot handle spot colors in different color spaces than "
              "image itself");
        }
        if (colorspace == 2) JXL_WARNING("PSD: K ignored in CMYK spot color");
        std::vector<float> color;
        color.push_back(get_be_int(2, pos, maxpos) / 65535.f);  // R or C
        color.push_back(get_be_int(2, pos, maxpos) / 65535.f);  // G or M
        color.push_back(get_be_int(2, pos, maxpos) / 65535.f);  // B or Y
        color.push_back(get_be_int(2, pos, maxpos) / 65535.f);  // ignored or K
        color.push_back(get_be_int(2, pos, maxpos) /
                        100.f);  // solidity (alpha, basically)
        int kind = get_be_int(1, pos, maxpos);
        JXL_DEBUG_V(PSD_VERBOSITY, "Kind=%i", kind);
        color.push_back(kind);
        spotcolor.push_back(color);
        if (kind == 2) {
          JXL_DEBUG_V(PSD_VERBOSITY, "Actual spot color");
        } else if (kind == 1) {
          JXL_DEBUG_V(PSD_VERBOSITY, "Mask (alpha) channel");
        } else if (kind == 0) {
          JXL_DEBUG_V(PSD_VERBOSITY, "Selection (alpha) channel");
        } else {
          return JXL_FAILURE("PSD: Unknown extra channel type");
        }
      }
      if (blocklength & 1) pos++;
      blocklength = 0;
    }
    pos += blocklength;
    if (blocklength & 1) pos++;  // padding again
  }

  size_t layerlength = get_be_int(4 * version, pos, maxpos);
  const uint8_t* after_layers_pos = pos + layerlength;
  if (after_layers_pos < pos) return JXL_FAILURE("PSD: invalid layer length");
  if (layerlength) {
    pos += 4 * version;  // don't care about layerinfolength
    JXL_DEBUG_V(PSD_VERBOSITY, "Layer section length: %zu", layerlength);
    int layercount = static_cast<int16_t>(get_be_int(2, pos, maxpos));
    JXL_DEBUG_V(PSD_VERBOSITY, "Layer count: %i", layercount);
    io->frames.clear();

    if (layercount == 0) {
      if (get_be_int(2, pos, maxpos) != 0) {
        return JXL_FAILURE(
            "PSD: Expected zero padding before additional layer info");
      }
      while (pos < after_layers_pos) {
        if (safe_strncmp(pos, maxpos, "8BIM", 4) &&
            safe_strncmp(pos, maxpos, "8B64", 4))
          return JXL_FAILURE("PSD: Unexpected layer info signature");
        pos += 4;
        const uint8_t* tpos = pos;
        pos += 4;
        size_t blocklength = get_be_int(4 * version, pos, maxpos);
        JXL_DEBUG_V(PSD_VERBOSITY, "Length=%zu", blocklength);
        if (blocklength > 0) {
          if (pos >= maxpos) return JXL_FAILURE("PSD: Unexpected end of file");
          size_t delta = maxpos - pos;
          if (delta < blocklength) {
            return JXL_FAILURE("PSD: Invalid block length");
          }
        }
        if (!safe_strncmp(tpos, maxpos, "Layr", 4) ||
            !safe_strncmp(tpos, maxpos, "Lr16", 4) ||
            !safe_strncmp(tpos, maxpos, "Lr32", 4)) {
          layercount = static_cast<int16_t>(get_be_int(2, pos, maxpos));
          if (layercount < 0) {
            return JXL_FAILURE("PSD: Invalid layer count");
          }
          JXL_DEBUG_V(PSD_VERBOSITY, "Real layer count: %i", layercount);
          break;
        }
        if (!safe_strncmp(tpos, maxpos, "Mtrn", 4) ||
            !safe_strncmp(tpos, maxpos, "Mt16", 4) ||
            !safe_strncmp(tpos, maxpos, "Mt32", 4)) {
          JXL_DEBUG_V(PSD_VERBOSITY, "Merged layer has transparency channel");
          if (nb_channels > real_nb_channels) {
            real_nb_channels++;
            have_alpha = true;
            merged_has_alpha = true;
          }
        }
        pos += blocklength;
      }
    } else if (layercount < 0) {
      // negative layer count indicates merged has alpha and it is to be shown
      if (nb_channels > real_nb_channels) {
        real_nb_channels++;
        have_alpha = true;
        merged_has_alpha = true;
      }
      layercount = -layercount;
    } else {
      // multiple layers implies there is alpha
      real_nb_channels++;
      have_alpha = true;
    }

    ExtraChannelInfo info;
    info.bit_depth.bits_per_sample = bitdepth;
    info.dim_shift = 0;

    if (colormodel == 4) {  // cmyk
      info.type = ExtraChannel::kBlack;
      io->metadata.m.extra_channel_info.push_back(info);
    }
    if (have_alpha) {
      JXL_DEBUG_V(PSD_VERBOSITY, "Have alpha");
      info.type = ExtraChannel::kAlpha;
      info.alpha_associated =
          false;  // true? PSD is not consistent with this, need to check
      io->metadata.m.extra_channel_info.push_back(info);
    }
    if (merged_has_alpha && !spotcolor.empty() && spotcolor[0][5] == 1) {
      // first alpha channel
      spotcolor.erase(spotcolor.begin());
    }
    for (size_t i = 0; i < spotcolor.size(); i++) {
      real_nb_channels++;
      if (spotcolor[i][5] == 2) {
        info.type = ExtraChannel::kSpotColor;
        info.spot_color[0] = spotcolor[i][0];
        info.spot_color[1] = spotcolor[i][1];
        info.spot_color[2] = spotcolor[i][2];
        info.spot_color[3] = spotcolor[i][4];
      } else if (spotcolor[i][5] == 1) {
        info.type = ExtraChannel::kAlpha;
      } else if (spotcolor[i][5] == 0) {
        info.type = ExtraChannel::kSelectionMask;
      } else
        return JXL_FAILURE("PSD: unhandled extra channel");
      io->metadata.m.extra_channel_info.push_back(info);
    }
    std::vector<std::vector<int>> layer_chan_id;
    std::vector<size_t> layer_offsets(layercount + 1, 0);
    std::vector<bool> is_real_layer(layercount, false);
    for (int l = 0; l < layercount; l++) {
      ImageBundle layer(&io->metadata.m);
      layer.duration = 0;
      layer.blend = (l > 0);

      layer.use_for_next_frame = (l + 1 < layercount);
      layer.origin.y0 = get_be_int(4, pos, maxpos);
      layer.origin.x0 = get_be_int(4, pos, maxpos);
      size_t height = get_be_int(4, pos, maxpos) - layer.origin.y0;
      size_t width = get_be_int(4, pos, maxpos) - layer.origin.x0;
      JXL_DEBUG_V(PSD_VERBOSITY, "Layer %i: %zu x %zu at origin (%i, %i)", l,
                  width, height, layer.origin.x0, layer.origin.y0);
      int nb_chs = get_be_int(2, pos, maxpos);
      JXL_DEBUG_V(PSD_VERBOSITY, "  channels: %i", nb_chs);
      std::vector<int> chan_ids;
      layer_offsets[l + 1] = layer_offsets[l];
      for (int lc = 0; lc < nb_chs; lc++) {
        int id = get_be_int(2, pos, maxpos);
        JXL_DEBUG_V(PSD_VERBOSITY, "    id=%i", id);
        if (id == 65535) {
          chan_ids.push_back(colormodel);  // alpha
        } else if (id == 65534) {
          chan_ids.push_back(-1);  // layer mask, ignored
        } else {
          chan_ids.push_back(id);  // color channel
        }
        layer_offsets[l + 1] += get_be_int(4 * version, pos, maxpos);
      }
      layer_chan_id.push_back(chan_ids);
      if (safe_strncmp(pos, maxpos, "8BIM", 4))
        return JXL_FAILURE("PSD: Layer %i: Unexpected signature (not 8BIM)", l);
      pos += 4;
      if (safe_strncmp(pos, maxpos, "norm", 4)) {
        return JXL_FAILURE(
            "PSD: Layer %i: Cannot handle non-default blend mode", l);
      }
      pos += 4;
      int opacity = get_be_int(1, pos, maxpos);
      if (opacity < 100) {
        JXL_WARNING(
            "PSD: ignoring opacity of semi-transparent layer %i (opacity=%i)",
            l, opacity);
      }
      pos++;  // clipping
      int flags = get_be_int(1, pos, maxpos);
      pos++;
      bool invisible = (flags & 2);
      if (invisible) {
        if (l + 1 < layercount) {
          layer.blend = false;
          layer.use_for_next_frame = false;
        } else {
          // TODO: instead add dummy last frame?
          JXL_WARNING("PSD: invisible top layer was made visible");
        }
      }
      size_t extradata = get_be_int(4, pos, maxpos);
      JXL_DEBUG_V(PSD_VERBOSITY, "  extradata: %zu bytes", extradata);
      const uint8_t* after_extra = pos + extradata;
      // TODO: deal with non-empty layer masks
      pos += get_be_int(4, pos, maxpos);  // skip layer mask data
      pos += get_be_int(4, pos, maxpos);  // skip layer blend range data
      size_t namelength = get_be_int(1, pos, maxpos);
      size_t delta = maxpos - pos;
      if (delta < namelength) return JXL_FAILURE("PSD: Invalid block length");
      char lname[256] = {};
      memcpy(lname, pos, namelength);
      lname[namelength] = 0;
      JXL_DEBUG_V(PSD_VERBOSITY, "  name: %s", lname);
      pos = after_extra;
      if (width == 0 || height == 0) {
        JXL_DEBUG_V(PSD_VERBOSITY,
                    "  NOT A REAL LAYER");  // probably layer group
        continue;
      }
      is_real_layer[l] = true;
      JXL_RETURN_IF_ERROR(VerifyDimensions(constraints, width, height));
      uint64_t pixel_count = static_cast<uint64_t>(width) * height;
      if (!SafeAdd(total_pixel_count, pixel_count, total_pixel_count)) {
        return JXL_FAILURE("Image too big");
      }
      if (total_pixel_count > constraints->dec_max_pixels) {
        return JXL_FAILURE("Image too big");
      }
      Image3F rgb(width, height);
      layer.SetFromImage(std::move(rgb), io->metadata.m.color_encoding);
      std::vector<ImageF> ec;
      for (const auto& ec_meta : layer.metadata()->extra_channel_info) {
        ImageF extra(width, height);
        if (ec_meta.type == ExtraChannel::kAlpha) {
          FillPlane(1.0f, &extra, Rect(extra));  // opaque
        } else {
          ZeroFillPlane(&extra, Rect(extra));  // zeroes
        }
        ec.push_back(std::move(extra));
      }
      if (!ec.empty()) layer.SetExtraChannels(std::move(ec));
      layer.name = lname;
      io->dec_pixels += layer.xsize() * layer.ysize();
      io->frames.push_back(std::move(layer));
    }

    std::vector<bool> invert(real_nb_channels, false);
    int il = 0;
    const uint8_t* bpos = pos;
    for (int l = 0; l < layercount; l++) {
      if (!is_real_layer[l]) continue;
      pos = bpos + layer_offsets[l];
      if (pos < bpos) return JXL_FAILURE("PSD: invalid layer offset");
      JXL_DEBUG_V(PSD_VERBOSITY, "At position %i (%zu)",
                  (int)(pos - bytes.data()), (size_t)pos);
      ImageBundle& layer = io->frames[il++];
      JXL_RETURN_IF_ERROR(decode_layer(pos, maxpos, layer, layer_chan_id[l],
                                       invert, layer.xsize(), layer.ysize(),
                                       version, colormodel, true, bitdepth));
    }
  } else
    return JXL_FAILURE("PSD: no layer data found");

  if (!hasmergeddata && !spotcolor.empty()) {
    return JXL_FAILURE("PSD: extra channel data declared but not found");
  }

  if (io->frames.empty()) return JXL_FAILURE("PSD: no layers");

  if (!spotcolor.empty()) {
    // PSD only has spot colors / extra alpha/mask data in the merged image
    // We don't redundantly store the merged image, so we put it in the first
    // layer (the next layers will kAdd zeroes to it)
    pos = after_layers_pos;
    ImageBundle& layer = io->frames[0];
    std::vector<int> chan_id(real_nb_channels);
    std::iota(chan_id.begin(), chan_id.end(), 0);
    std::vector<bool> invert(real_nb_channels, false);
    if (!merged_has_alpha) {
      chan_id.erase(chan_id.begin() + colormodel);
      invert.erase(invert.begin() + colormodel);
    } else
      colormodel++;
    for (size_t i = colormodel; i < invert.size(); i++) {
      if (spotcolor[i - colormodel][5] == 2) invert[i] = true;
      if (spotcolor[i - colormodel][5] == 0) invert[i] = true;
    }
    JXL_RETURN_IF_ERROR(decode_layer(pos, maxpos, layer, chan_id, invert,
                                     layer.xsize(), layer.ysize(), version,
                                     colormodel, false, bitdepth));
  }
  io->SetSize(xsize, ysize);

  SetIntensityTarget(io);

  return true;
}

Status EncodeImagePSD(const CodecInOut* io, const ColorEncoding& c_desired,
                      size_t bits_per_sample, ThreadPool* pool,
                      PaddedBytes* bytes) {
  return JXL_FAILURE("PSD encoding not yet implemented");
}

}  // namespace jxl
