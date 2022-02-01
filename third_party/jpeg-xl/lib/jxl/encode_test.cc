// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "jxl/encode.h"

#include "gtest/gtest.h"
#include "jxl/decode.h"
#include "jxl/decode_cxx.h"
#include "jxl/encode_cxx.h"
#include "lib/extras/codec.h"
#include "lib/jxl/dec_file.h"
#include "lib/jxl/enc_butteraugli_pnorm.h"
#include "lib/jxl/encode_internal.h"
#include "lib/jxl/jpeg/dec_jpeg_data.h"
#include "lib/jxl/jpeg/dec_jpeg_data_writer.h"
#include "lib/jxl/test_utils.h"
#include "lib/jxl/testdata.h"

TEST(EncodeTest, AddFrameAfterCloseInputTest) {
  JxlEncoderPtr enc = JxlEncoderMake(nullptr);
  EXPECT_NE(nullptr, enc.get());

  JxlEncoderCloseInput(enc.get());

  size_t xsize = 64;
  size_t ysize = 64;
  JxlPixelFormat pixel_format = {4, JXL_TYPE_UINT16, JXL_BIG_ENDIAN, 0};
  std::vector<uint8_t> pixels = jxl::test::GetSomeTestImage(xsize, ysize, 4, 0);

  jxl::CodecInOut input_io =
      jxl::test::SomeTestImageToCodecInOut(pixels, 4, xsize, ysize);

  JxlBasicInfo basic_info;
  jxl::test::JxlBasicInfoSetFromPixelFormat(&basic_info, &pixel_format);
  basic_info.xsize = xsize;
  basic_info.ysize = ysize;
  basic_info.uses_original_profile = false;
  EXPECT_EQ(JXL_ENC_SUCCESS, JxlEncoderSetBasicInfo(enc.get(), &basic_info));
  JxlColorEncoding color_encoding;
  JxlColorEncodingSetToSRGB(&color_encoding,
                            /*is_gray=*/pixel_format.num_channels < 3);
  EXPECT_EQ(JXL_ENC_SUCCESS,
            JxlEncoderSetColorEncoding(enc.get(), &color_encoding));
  JxlEncoderOptions* options = JxlEncoderOptionsCreate(enc.get(), NULL);
  EXPECT_EQ(JXL_ENC_ERROR,
            JxlEncoderAddImageFrame(options, &pixel_format, pixels.data(),
                                    pixels.size()));
}

TEST(EncodeTest, AddJPEGAfterCloseTest) {
  JxlEncoderPtr enc = JxlEncoderMake(nullptr);
  EXPECT_NE(nullptr, enc.get());

  JxlEncoderCloseInput(enc.get());

  const std::string jpeg_path =
      "imagecompression.info/flower_foveon.png.im_q85_420.jpg";
  const jxl::PaddedBytes orig = jxl::ReadTestData(jpeg_path);

  JxlEncoderOptions* options = JxlEncoderOptionsCreate(enc.get(), NULL);

  EXPECT_EQ(JXL_ENC_ERROR,
            JxlEncoderAddJPEGFrame(options, orig.data(), orig.size()));
}

TEST(EncodeTest, AddFrameBeforeColorEncodingTest) {
  JxlEncoderPtr enc = JxlEncoderMake(nullptr);
  EXPECT_NE(nullptr, enc.get());

  size_t xsize = 64;
  size_t ysize = 64;
  JxlPixelFormat pixel_format = {4, JXL_TYPE_UINT16, JXL_BIG_ENDIAN, 0};
  std::vector<uint8_t> pixels = jxl::test::GetSomeTestImage(xsize, ysize, 4, 0);

  jxl::CodecInOut input_io =
      jxl::test::SomeTestImageToCodecInOut(pixels, 4, xsize, ysize);

  JxlBasicInfo basic_info;
  jxl::test::JxlBasicInfoSetFromPixelFormat(&basic_info, &pixel_format);
  basic_info.xsize = xsize;
  basic_info.ysize = ysize;
  basic_info.uses_original_profile = true;
  EXPECT_EQ(JXL_ENC_SUCCESS, JxlEncoderSetBasicInfo(enc.get(), &basic_info));
  JxlEncoderOptions* options = JxlEncoderOptionsCreate(enc.get(), NULL);
  EXPECT_EQ(JXL_ENC_ERROR,
            JxlEncoderAddImageFrame(options, &pixel_format, pixels.data(),
                                    pixels.size()));
}

TEST(EncodeTest, AddFrameBeforeBasicInfoTest) {
  JxlEncoderPtr enc = JxlEncoderMake(nullptr);
  EXPECT_NE(nullptr, enc.get());

  size_t xsize = 64;
  size_t ysize = 64;
  JxlPixelFormat pixel_format = {4, JXL_TYPE_UINT16, JXL_BIG_ENDIAN, 0};
  std::vector<uint8_t> pixels = jxl::test::GetSomeTestImage(xsize, ysize, 4, 0);

  jxl::CodecInOut input_io =
      jxl::test::SomeTestImageToCodecInOut(pixels, 4, xsize, ysize);

  JxlColorEncoding color_encoding;
  JxlColorEncodingSetToSRGB(&color_encoding,
                            /*is_gray=*/pixel_format.num_channels < 3);
  EXPECT_EQ(JXL_ENC_SUCCESS,
            JxlEncoderSetColorEncoding(enc.get(), &color_encoding));
  JxlEncoderOptions* options = JxlEncoderOptionsCreate(enc.get(), NULL);
  EXPECT_EQ(JXL_ENC_ERROR,
            JxlEncoderAddImageFrame(options, &pixel_format, pixels.data(),
                                    pixels.size()));
}

TEST(EncodeTest, DefaultAllocTest) {
  JxlEncoder* enc = JxlEncoderCreate(nullptr);
  EXPECT_NE(nullptr, enc);
  JxlEncoderDestroy(enc);
}

TEST(EncodeTest, CustomAllocTest) {
  struct CalledCounters {
    int allocs = 0;
    int frees = 0;
  } counters;

  JxlMemoryManager mm;
  mm.opaque = &counters;
  mm.alloc = [](void* opaque, size_t size) {
    reinterpret_cast<CalledCounters*>(opaque)->allocs++;
    return malloc(size);
  };
  mm.free = [](void* opaque, void* address) {
    reinterpret_cast<CalledCounters*>(opaque)->frees++;
    free(address);
  };

  {
    JxlEncoderPtr enc = JxlEncoderMake(&mm);
    EXPECT_NE(nullptr, enc.get());
    EXPECT_LE(1, counters.allocs);
    EXPECT_EQ(0, counters.frees);
  }
  EXPECT_LE(1, counters.frees);
}

TEST(EncodeTest, DefaultParallelRunnerTest) {
  JxlEncoderPtr enc = JxlEncoderMake(nullptr);
  EXPECT_NE(nullptr, enc.get());
  EXPECT_EQ(JXL_ENC_SUCCESS,
            JxlEncoderSetParallelRunner(enc.get(), nullptr, nullptr));
}

void VerifyFrameEncoding(size_t xsize, size_t ysize, JxlEncoder* enc,
                         const JxlEncoderOptions* options) {
  JxlPixelFormat pixel_format = {4, JXL_TYPE_UINT16, JXL_BIG_ENDIAN, 0};
  std::vector<uint8_t> pixels = jxl::test::GetSomeTestImage(xsize, ysize, 4, 0);

  jxl::CodecInOut input_io =
      jxl::test::SomeTestImageToCodecInOut(pixels, 4, xsize, ysize);

  JxlBasicInfo basic_info;
  jxl::test::JxlBasicInfoSetFromPixelFormat(&basic_info, &pixel_format);
  basic_info.xsize = xsize;
  basic_info.ysize = ysize;
  if (options->values.lossless) {
    basic_info.uses_original_profile = true;
  } else {
    basic_info.uses_original_profile = false;
  }
  EXPECT_EQ(JXL_ENC_SUCCESS, JxlEncoderSetBasicInfo(enc, &basic_info));
  JxlColorEncoding color_encoding;
  JxlColorEncodingSetToSRGB(&color_encoding,
                            /*is_gray=*/pixel_format.num_channels < 3);
  EXPECT_EQ(JXL_ENC_SUCCESS, JxlEncoderSetColorEncoding(enc, &color_encoding));
  EXPECT_EQ(JXL_ENC_SUCCESS,
            JxlEncoderAddImageFrame(options, &pixel_format, pixels.data(),
                                    pixels.size()));
  JxlEncoderCloseInput(enc);

  std::vector<uint8_t> compressed = std::vector<uint8_t>(64);
  uint8_t* next_out = compressed.data();
  size_t avail_out = compressed.size() - (next_out - compressed.data());
  JxlEncoderStatus process_result = JXL_ENC_NEED_MORE_OUTPUT;
  while (process_result == JXL_ENC_NEED_MORE_OUTPUT) {
    process_result = JxlEncoderProcessOutput(enc, &next_out, &avail_out);
    if (process_result == JXL_ENC_NEED_MORE_OUTPUT) {
      size_t offset = next_out - compressed.data();
      compressed.resize(compressed.size() * 2);
      next_out = compressed.data() + offset;
      avail_out = compressed.size() - offset;
    }
  }
  compressed.resize(next_out - compressed.data());
  EXPECT_EQ(JXL_ENC_SUCCESS, process_result);

  jxl::DecompressParams dparams;
  jxl::CodecInOut decoded_io;
  EXPECT_TRUE(jxl::DecodeFile(
      dparams, jxl::Span<const uint8_t>(compressed.data(), compressed.size()),
      &decoded_io, /*pool=*/nullptr));

  EXPECT_LE(ComputeDistance2(input_io.Main(), decoded_io.Main()), 1.8);
}

void VerifyFrameEncoding(JxlEncoder* enc, const JxlEncoderOptions* options) {
  VerifyFrameEncoding(63, 129, enc, options);
}

TEST(EncodeTest, FrameEncodingTest) {
  JxlEncoderPtr enc = JxlEncoderMake(nullptr);
  EXPECT_NE(nullptr, enc.get());
  VerifyFrameEncoding(enc.get(), JxlEncoderOptionsCreate(enc.get(), nullptr));
}

TEST(EncodeTest, EncoderResetTest) {
  JxlEncoderPtr enc = JxlEncoderMake(nullptr);
  EXPECT_NE(nullptr, enc.get());
  VerifyFrameEncoding(50, 200, enc.get(),
                      JxlEncoderOptionsCreate(enc.get(), nullptr));
  // Encoder should become reusable for a new image from scratch after using
  // reset.
  JxlEncoderReset(enc.get());
  VerifyFrameEncoding(157, 77, enc.get(),
                      JxlEncoderOptionsCreate(enc.get(), nullptr));
}

TEST(EncodeTest, OptionsTest) {
  {
    JxlEncoderPtr enc = JxlEncoderMake(nullptr);
    EXPECT_NE(nullptr, enc.get());
    JxlEncoderOptions* options = JxlEncoderOptionsCreate(enc.get(), NULL);
    EXPECT_EQ(JXL_ENC_SUCCESS,
              JxlEncoderOptionsSetInteger(options, JXL_ENC_OPTION_EFFORT, 5));
    VerifyFrameEncoding(enc.get(), options);
    EXPECT_EQ(jxl::SpeedTier::kHare, enc->last_used_cparams.speed_tier);
  }

  {
    JxlEncoderPtr enc = JxlEncoderMake(nullptr);
    EXPECT_NE(nullptr, enc.get());
    JxlEncoderOptions* options = JxlEncoderOptionsCreate(enc.get(), NULL);
    // Lower than currently supported values
    EXPECT_EQ(JXL_ENC_ERROR,
              JxlEncoderOptionsSetInteger(options, JXL_ENC_OPTION_EFFORT, 0));
    // Higher than currently supported values
    EXPECT_EQ(JXL_ENC_ERROR,
              JxlEncoderOptionsSetInteger(options, JXL_ENC_OPTION_EFFORT, 10));
  }

  {
    JxlEncoderPtr enc = JxlEncoderMake(nullptr);
    EXPECT_NE(nullptr, enc.get());
    JxlEncoderOptions* options = JxlEncoderOptionsCreate(enc.get(), NULL);
    EXPECT_EQ(JXL_ENC_SUCCESS, JxlEncoderOptionsSetLossless(options, JXL_TRUE));
    VerifyFrameEncoding(enc.get(), options);
    EXPECT_EQ(true, enc->last_used_cparams.IsLossless());
  }

  {
    JxlEncoderPtr enc = JxlEncoderMake(nullptr);
    EXPECT_NE(nullptr, enc.get());
    JxlEncoderOptions* options = JxlEncoderOptionsCreate(enc.get(), NULL);
    EXPECT_EQ(JXL_ENC_SUCCESS, JxlEncoderOptionsSetDistance(options, 0.5));
    VerifyFrameEncoding(enc.get(), options);
    EXPECT_EQ(0.5, enc->last_used_cparams.butteraugli_distance);
  }

  {
    JxlEncoderPtr enc = JxlEncoderMake(nullptr);
    JxlEncoderOptions* options = JxlEncoderOptionsCreate(enc.get(), NULL);
    // Disallowed negative distance
    EXPECT_EQ(JXL_ENC_ERROR, JxlEncoderOptionsSetDistance(options, -1));
  }

  {
    JxlEncoderPtr enc = JxlEncoderMake(nullptr);
    EXPECT_NE(nullptr, enc.get());
    JxlEncoderOptions* options = JxlEncoderOptionsCreate(enc.get(), NULL);
    EXPECT_EQ(JXL_ENC_SUCCESS, JxlEncoderOptionsSetInteger(
                                   options, JXL_ENC_OPTION_DECODING_SPEED, 2));
    VerifyFrameEncoding(enc.get(), options);
    EXPECT_EQ(2u, enc->last_used_cparams.decoding_speed_tier);
  }

  {
    JxlEncoderPtr enc = JxlEncoderMake(nullptr);
    EXPECT_NE(nullptr, enc.get());
    JxlEncoderOptions* options = JxlEncoderOptionsCreate(enc.get(), NULL);
    EXPECT_EQ(JXL_ENC_ERROR, JxlEncoderOptionsSetInteger(
                                 options, JXL_ENC_OPTION_GROUP_ORDER, 100));
    EXPECT_EQ(JXL_ENC_SUCCESS, JxlEncoderOptionsSetInteger(
                                   options, JXL_ENC_OPTION_GROUP_ORDER, 1));
    EXPECT_EQ(JXL_ENC_SUCCESS,
              JxlEncoderOptionsSetInteger(
                  options, JXL_ENC_OPTION_GROUP_ORDER_CENTER_X, 5));
    VerifyFrameEncoding(enc.get(), options);
    EXPECT_EQ(true, enc->last_used_cparams.centerfirst);
    EXPECT_EQ(5, enc->last_used_cparams.center_x);
  }

  {
    JxlEncoderPtr enc = JxlEncoderMake(nullptr);
    EXPECT_NE(nullptr, enc.get());
    JxlEncoderOptions* options = JxlEncoderOptionsCreate(enc.get(), NULL);
    EXPECT_EQ(JXL_ENC_SUCCESS, JxlEncoderOptionsSetInteger(
                                   options, JXL_ENC_OPTION_RESPONSIVE, 0));
    EXPECT_EQ(JXL_ENC_SUCCESS, JxlEncoderOptionsSetInteger(
                                   options, JXL_ENC_OPTION_PROGRESSIVE_AC, 1));
    EXPECT_EQ(JXL_ENC_SUCCESS,
              JxlEncoderOptionsSetInteger(options,
                                          JXL_ENC_OPTION_QPROGRESSIVE_AC, -1));
    EXPECT_EQ(JXL_ENC_SUCCESS, JxlEncoderOptionsSetInteger(
                                   options, JXL_ENC_OPTION_PROGRESSIVE_DC, 2));
    VerifyFrameEncoding(enc.get(), options);
    EXPECT_EQ(false, enc->last_used_cparams.responsive);
    EXPECT_EQ(true, enc->last_used_cparams.progressive_mode);
    EXPECT_EQ(2, enc->last_used_cparams.progressive_dc);
  }

  {
    JxlEncoderPtr enc = JxlEncoderMake(nullptr);
    EXPECT_NE(nullptr, enc.get());
    JxlEncoderOptions* options = JxlEncoderOptionsCreate(enc.get(), NULL);
    EXPECT_EQ(JXL_ENC_SUCCESS, JxlEncoderOptionsSetInteger(
                                   options, JXL_ENC_OPTION_PHOTON_NOISE, 1777));
    VerifyFrameEncoding(enc.get(), options);
    EXPECT_EQ(1777.0f, enc->last_used_cparams.photon_noise_iso);
  }

  {
    JxlEncoderPtr enc = JxlEncoderMake(nullptr);
    EXPECT_NE(nullptr, enc.get());
    JxlEncoderOptions* options = JxlEncoderOptionsCreate(enc.get(), NULL);
    EXPECT_EQ(JXL_ENC_SUCCESS,
              JxlEncoderOptionsSetInteger(
                  options, JXL_ENC_OPTION_CHANNEL_COLORS_GLOBAL_PERCENT, 55));
    EXPECT_EQ(JXL_ENC_SUCCESS,
              JxlEncoderOptionsSetInteger(
                  options, JXL_ENC_OPTION_CHANNEL_COLORS_GROUP_PERCENT, 25));
    EXPECT_EQ(JXL_ENC_SUCCESS,
              JxlEncoderOptionsSetInteger(
                  options, JXL_ENC_OPTION_PALETTE_COLORS, 70000));
    EXPECT_EQ(JXL_ENC_SUCCESS, JxlEncoderOptionsSetInteger(
                                   options, JXL_ENC_OPTION_LOSSY_PALETTE, 1));
    VerifyFrameEncoding(enc.get(), options);
    EXPECT_EQ(55.0f,
              enc->last_used_cparams.channel_colors_pre_transform_percent);
    EXPECT_EQ(25.0f, enc->last_used_cparams.channel_colors_percent);
    EXPECT_EQ(70000, enc->last_used_cparams.palette_colors);
    EXPECT_EQ(true, enc->last_used_cparams.lossy_palette);
  }

  {
    JxlEncoderPtr enc = JxlEncoderMake(nullptr);
    EXPECT_NE(nullptr, enc.get());
    JxlEncoderOptions* options = JxlEncoderOptionsCreate(enc.get(), NULL);
    EXPECT_EQ(JXL_ENC_SUCCESS,
              JxlEncoderOptionsSetInteger(
                  options, JXL_ENC_OPTION_MODULAR_COLOR_SPACE, 30));
    EXPECT_EQ(JXL_ENC_SUCCESS,
              JxlEncoderOptionsSetInteger(
                  options, JXL_ENC_OPTION_MODULAR_GROUP_SIZE, 2));
    EXPECT_EQ(JXL_ENC_SUCCESS,
              JxlEncoderOptionsSetInteger(
                  options, JXL_ENC_OPTION_MODULAR_PREDICTOR, 14));
    EXPECT_EQ(
        JXL_ENC_SUCCESS,
        JxlEncoderOptionsSetInteger(
            options, JXL_ENC_OPTION_MODULAR_MA_TREE_LEARNING_PERCENT, 77));
    EXPECT_EQ(JXL_ENC_SUCCESS,
              JxlEncoderOptionsSetInteger(
                  options, JXL_ENC_OPTION_MODULAR_NB_PREV_CHANNELS, 7));
    VerifyFrameEncoding(enc.get(), options);
    // It was set to 30, but becomes 32 because in the C++ implementation, the
    // numerical RCT values are shifted 2 compared to the specification. The
    // API uses the values seen in the JXL specification.
    EXPECT_EQ(32, enc->last_used_cparams.colorspace);
    EXPECT_EQ(2, enc->last_used_cparams.modular_group_size_shift);
    EXPECT_EQ(jxl::Predictor::Best, enc->last_used_cparams.options.predictor);
    EXPECT_EQ(0.77f, enc->last_used_cparams.options.nb_repeats);
    EXPECT_EQ(7, enc->last_used_cparams.options.max_properties);
  }

  {
    JxlEncoderPtr enc = JxlEncoderMake(nullptr);
    EXPECT_NE(nullptr, enc.get());
    JxlEncoderOptions* options = JxlEncoderOptionsCreate(enc.get(), NULL);
    EXPECT_EQ(JXL_ENC_SUCCESS, JxlEncoderOptionsSetInteger(
                                   options, JXL_ENC_OPTION_JPEG_RECON_CFL, 0));
    VerifyFrameEncoding(enc.get(), options);
    EXPECT_EQ(false, enc->last_used_cparams.force_cfl_jpeg_recompression);
  }

  {
    JxlEncoderPtr enc = JxlEncoderMake(nullptr);
    EXPECT_NE(nullptr, enc.get());
    JxlEncoderOptions* options = JxlEncoderOptionsCreate(enc.get(), NULL);
    EXPECT_EQ(JXL_ENC_SUCCESS, JxlEncoderOptionsSetInteger(
                                   options, JXL_ENC_OPTION_JPEG_RECON_CFL, 1));
    VerifyFrameEncoding(enc.get(), options);
    EXPECT_EQ(true, enc->last_used_cparams.force_cfl_jpeg_recompression);
  }
}

namespace {
// Returns a copy of buf from offset to offset+size, or a new zeroed vector if
// the result would have been out of bounds taking integer overflow into
// account.
const std::vector<uint8_t> SliceSpan(const jxl::Span<const uint8_t>& buf,
                                     size_t offset, size_t size) {
  if (offset + size >= buf.size()) {
    return std::vector<uint8_t>(size, 0);
  }
  if (offset + size < offset) {
    return std::vector<uint8_t>(size, 0);
  }
  return std::vector<uint8_t>(buf.data() + offset, buf.data() + offset + size);
}

struct Box {
  // The type of the box.
  // If "uuid", use extended_type instead
  char type[4] = {0, 0, 0, 0};

  // The extended_type is only used when type == "uuid".
  // Extended types are not used in JXL. However, the box format itself
  // supports this so they are handled correctly.
  char extended_type[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

  // Box data.
  jxl::Span<const uint8_t> data = jxl::Span<const uint8_t>(nullptr, 0);

  // If the size is not given, the datasize extends to the end of the file.
  // If this field is false, the size field is not encoded when the box is
  // serialized.
  bool data_size_given = true;

  // If successful, returns true and sets `in` to be the rest data (if any).
  // If `in` contains a box with a size larger than `in.size()`, will not
  // modify `in`, and will return true but the data `Span<uint8_t>` will
  // remain set to nullptr.
  // If unsuccessful, returns error and doesn't modify `in`.
  jxl::Status Decode(jxl::Span<const uint8_t>* in) {
    // Total box_size including this header itself.
    uint64_t box_size = LoadBE32(SliceSpan(*in, 0, 4).data());
    size_t pos = 4;

    memcpy(type, SliceSpan(*in, pos, 4).data(), 4);
    pos += 4;

    if (box_size == 1) {
      // If the size is 1, it indicates extended size read from 64-bit integer.
      box_size = LoadBE64(SliceSpan(*in, pos, 8).data());
      pos += 8;
    }

    if (!memcmp("uuid", type, 4)) {
      memcpy(extended_type, SliceSpan(*in, pos, 16).data(), 16);
      pos += 16;
    }

    // This is the end of the box header, the box data begins here. Handle
    // the data size now.
    const size_t header_size = pos;

    if (box_size != 0) {
      if (box_size < header_size) {
        return JXL_FAILURE("Invalid box size");
      }
      if (box_size > in->size()) {
        // The box is fine, but the input is too short.
        return true;
      }
      data_size_given = true;
      data = jxl::Span<const uint8_t>(in->data() + header_size,
                                      box_size - header_size);
    } else {
      data_size_given = false;
      data = jxl::Span<const uint8_t>(in->data() + header_size,
                                      in->size() - header_size);
    }

    *in = jxl::Span<const uint8_t>(in->data() + header_size + data.size(),
                                   in->size() - header_size - data.size());
    return true;
  }
};

struct Container {
  std::vector<Box> boxes;

  // If successful, returns true and sets `in` to be the rest data (if any).
  // If unsuccessful, returns error and doesn't modify `in`.
  jxl::Status Decode(jxl::Span<const uint8_t>* in) {
    boxes.clear();

    Box signature_box;
    JXL_RETURN_IF_ERROR(signature_box.Decode(in));
    if (memcmp("JXL ", signature_box.type, 4) != 0) {
      return JXL_FAILURE("Invalid magic signature");
    }
    if (signature_box.data.size() != 4)
      return JXL_FAILURE("Invalid magic signature");
    if (signature_box.data[0] != 0xd || signature_box.data[1] != 0xa ||
        signature_box.data[2] != 0x87 || signature_box.data[3] != 0xa) {
      return JXL_FAILURE("Invalid magic signature");
    }

    Box ftyp_box;
    JXL_RETURN_IF_ERROR(ftyp_box.Decode(in));
    if (memcmp("ftyp", ftyp_box.type, 4) != 0) {
      return JXL_FAILURE("Invalid ftyp");
    }
    if (ftyp_box.data.size() != 12) return JXL_FAILURE("Invalid ftyp");
    const char* expected = "jxl \0\0\0\0jxl ";
    if (memcmp(expected, ftyp_box.data.data(), 12) != 0)
      return JXL_FAILURE("Invalid ftyp");

    while (in->size() > 0) {
      Box box = {};
      JXL_RETURN_IF_ERROR(box.Decode(in));
      if (box.data.data() == nullptr) {
        // The decoding encountered a box, but not enough data yet.
        return true;
      }
      boxes.emplace_back(box);
    }

    return true;
  }
};

}  // namespace

TEST(EncodeTest, SingleFrameBoundedJXLCTest) {
  JxlEncoderPtr enc = JxlEncoderMake(nullptr);
  EXPECT_NE(nullptr, enc.get());
  EXPECT_EQ(JXL_ENC_SUCCESS, JxlEncoderUseContainer(enc.get(), true));
  JxlEncoderOptions* options = JxlEncoderOptionsCreate(enc.get(), NULL);

  size_t xsize = 71;
  size_t ysize = 23;
  JxlPixelFormat pixel_format = {4, JXL_TYPE_UINT16, JXL_BIG_ENDIAN, 0};
  std::vector<uint8_t> pixels = jxl::test::GetSomeTestImage(xsize, ysize, 4, 0);

  JxlBasicInfo basic_info;
  jxl::test::JxlBasicInfoSetFromPixelFormat(&basic_info, &pixel_format);
  basic_info.xsize = xsize;
  basic_info.ysize = ysize;
  basic_info.uses_original_profile = false;
  EXPECT_EQ(JXL_ENC_SUCCESS, JxlEncoderSetBasicInfo(enc.get(), &basic_info));
  JxlColorEncoding color_encoding;
  JxlColorEncodingSetToSRGB(&color_encoding,
                            /*is_gray=*/false);
  EXPECT_EQ(JXL_ENC_SUCCESS,
            JxlEncoderSetColorEncoding(enc.get(), &color_encoding));
  EXPECT_EQ(JXL_ENC_SUCCESS,
            JxlEncoderAddImageFrame(options, &pixel_format, pixels.data(),
                                    pixels.size()));
  JxlEncoderCloseInput(enc.get());

  std::vector<uint8_t> compressed = std::vector<uint8_t>(64);
  uint8_t* next_out = compressed.data();
  size_t avail_out = compressed.size() - (next_out - compressed.data());
  JxlEncoderStatus process_result = JXL_ENC_NEED_MORE_OUTPUT;
  while (process_result == JXL_ENC_NEED_MORE_OUTPUT) {
    process_result = JxlEncoderProcessOutput(enc.get(), &next_out, &avail_out);
    if (process_result == JXL_ENC_NEED_MORE_OUTPUT) {
      size_t offset = next_out - compressed.data();
      compressed.resize(compressed.size() * 2);
      next_out = compressed.data() + offset;
      avail_out = compressed.size() - offset;
    }
  }
  compressed.resize(next_out - compressed.data());
  EXPECT_EQ(JXL_ENC_SUCCESS, process_result);

  Container container = {};
  jxl::Span<const uint8_t> encoded_span =
      jxl::Span<const uint8_t>(compressed.data(), compressed.size());
  EXPECT_TRUE(container.Decode(&encoded_span));
  EXPECT_EQ(0u, encoded_span.size());
  bool found_jxlc = false;
  bool found_jxlp = false;
  // The encoder is allowed to either emit a jxlc or one or more jxlp.
  for (size_t i = 0; i < container.boxes.size(); ++i) {
    if (memcmp("jxlc", container.boxes[i].type, 4) == 0) {
      EXPECT_EQ(false, found_jxlc);  // Max 1 jxlc
      EXPECT_EQ(false, found_jxlp);  // Can't mix jxlc and jxlp
      found_jxlc = true;
    }
    if (memcmp("jxlp", container.boxes[i].type, 4) == 0) {
      EXPECT_EQ(false, found_jxlc);  // Can't mix jxlc and jxlp
      found_jxlp = true;
    }
    // The encoder shouldn't create an unbounded box in this case, with the
    // single frame it knows the full size in time, so can help make decoding
    // more efficient by giving the full box size of the final box.
    EXPECT_EQ(true, container.boxes[i].data_size_given);
  }
  EXPECT_EQ(true, found_jxlc || found_jxlp);
}

TEST(EncodeTest, CodestreamLevelTest) {
  size_t xsize = 64;
  size_t ysize = 64;
  JxlPixelFormat pixel_format = {4, JXL_TYPE_UINT16, JXL_BIG_ENDIAN, 0};
  std::vector<uint8_t> pixels = jxl::test::GetSomeTestImage(xsize, ysize, 4, 0);

  jxl::CodecInOut input_io =
      jxl::test::SomeTestImageToCodecInOut(pixels, 4, xsize, ysize);

  JxlBasicInfo basic_info;
  jxl::test::JxlBasicInfoSetFromPixelFormat(&basic_info, &pixel_format);
  basic_info.xsize = xsize;
  basic_info.ysize = ysize;
  basic_info.uses_original_profile = false;

  JxlEncoderPtr enc = JxlEncoderMake(nullptr);
  EXPECT_EQ(JXL_ENC_SUCCESS, JxlEncoderSetCodestreamLevel(enc.get(), 10));
  JxlEncoderOptions* options = JxlEncoderOptionsCreate(enc.get(), NULL);

  EXPECT_EQ(JXL_ENC_SUCCESS, JxlEncoderSetBasicInfo(enc.get(), &basic_info));
  JxlColorEncoding color_encoding;
  JxlColorEncodingSetToSRGB(&color_encoding,
                            /*is_gray=*/pixel_format.num_channels < 3);
  EXPECT_EQ(JXL_ENC_SUCCESS,
            JxlEncoderSetColorEncoding(enc.get(), &color_encoding));
  EXPECT_EQ(JXL_ENC_SUCCESS,
            JxlEncoderAddImageFrame(options, &pixel_format, pixels.data(),
                                    pixels.size()));
  JxlEncoderCloseInput(enc.get());

  std::vector<uint8_t> compressed = std::vector<uint8_t>(64);
  uint8_t* next_out = compressed.data();
  size_t avail_out = compressed.size() - (next_out - compressed.data());
  JxlEncoderStatus process_result = JXL_ENC_NEED_MORE_OUTPUT;
  while (process_result == JXL_ENC_NEED_MORE_OUTPUT) {
    process_result = JxlEncoderProcessOutput(enc.get(), &next_out, &avail_out);
    if (process_result == JXL_ENC_NEED_MORE_OUTPUT) {
      size_t offset = next_out - compressed.data();
      compressed.resize(compressed.size() * 2);
      next_out = compressed.data() + offset;
      avail_out = compressed.size() - offset;
    }
  }
  compressed.resize(next_out - compressed.data());
  EXPECT_EQ(JXL_ENC_SUCCESS, process_result);

  Container container = {};
  jxl::Span<const uint8_t> encoded_span =
      jxl::Span<const uint8_t>(compressed.data(), compressed.size());
  EXPECT_TRUE(container.Decode(&encoded_span));
  EXPECT_EQ(0u, encoded_span.size());
  EXPECT_EQ(0, memcmp("jxll", container.boxes[0].type, 4));
}

TEST(EncodeTest, JXL_TRANSCODE_JPEG_TEST(JPEGReconstructionTest)) {
  const std::string jpeg_path =
      "imagecompression.info/flower_foveon.png.im_q85_420.jpg";
  const jxl::PaddedBytes orig = jxl::ReadTestData(jpeg_path);

  JxlEncoderPtr enc = JxlEncoderMake(nullptr);
  JxlEncoderOptions* options = JxlEncoderOptionsCreate(enc.get(), NULL);

  EXPECT_EQ(JXL_ENC_SUCCESS, JxlEncoderStoreJPEGMetadata(enc.get(), JXL_TRUE));
  EXPECT_EQ(JXL_ENC_SUCCESS,
            JxlEncoderAddJPEGFrame(options, orig.data(), orig.size()));
  JxlEncoderCloseInput(enc.get());

  std::vector<uint8_t> compressed = std::vector<uint8_t>(64);
  uint8_t* next_out = compressed.data();
  size_t avail_out = compressed.size() - (next_out - compressed.data());
  JxlEncoderStatus process_result = JXL_ENC_NEED_MORE_OUTPUT;
  while (process_result == JXL_ENC_NEED_MORE_OUTPUT) {
    process_result = JxlEncoderProcessOutput(enc.get(), &next_out, &avail_out);
    if (process_result == JXL_ENC_NEED_MORE_OUTPUT) {
      size_t offset = next_out - compressed.data();
      compressed.resize(compressed.size() * 2);
      next_out = compressed.data() + offset;
      avail_out = compressed.size() - offset;
    }
  }
  compressed.resize(next_out - compressed.data());
  EXPECT_EQ(JXL_ENC_SUCCESS, process_result);

  Container container = {};
  jxl::Span<const uint8_t> encoded_span =
      jxl::Span<const uint8_t>(compressed.data(), compressed.size());
  EXPECT_TRUE(container.Decode(&encoded_span));
  EXPECT_EQ(0u, encoded_span.size());
  bool found_jbrd = false;
  bool found_jxlc = false;
  bool found_jxlp = false;
  size_t jbrd_index = 0;
  std::vector<uint8_t> codestream_bytes;
  // The encoder is allowed to either emit a jxlc or one or more jxlp.
  for (size_t i = 0; i < container.boxes.size(); ++i) {
    if (memcmp("jbrd", container.boxes[i].type, 4) == 0) {
      EXPECT_EQ(false, found_jxlc);  // Max 1 jbrd
      found_jbrd = true;
      jbrd_index = i;
    }
    if (memcmp("jxlc", container.boxes[i].type, 4) == 0) {
      EXPECT_EQ(false, found_jxlc);  // Max 1 jxlc
      EXPECT_EQ(false, found_jxlp);  // Can't mix jxlc and jxlp
      found_jxlc = true;
      codestream_bytes.insert(
          codestream_bytes.end(), container.boxes[i].data.data(),
          container.boxes[i].data.data() + container.boxes[i].data.size());
    }
    if (memcmp("jxlp", container.boxes[i].type, 4) == 0) {
      EXPECT_EQ(false, found_jxlc);  // Can't mix jxlc and jxlp
      found_jxlp = true;
      // Append all data except the first 4 box content bytes which are the
      // jxpl box counter.
      codestream_bytes.insert(
          codestream_bytes.end(), container.boxes[i].data.data() + 4,
          container.boxes[i].data.data() + container.boxes[i].data.size());
    }
  }
  EXPECT_EQ(true, found_jbrd);
  EXPECT_EQ(true, found_jxlc || found_jxlp);

  jxl::CodecInOut decoded_io;
  decoded_io.Main().jpeg_data = jxl::make_unique<jxl::jpeg::JPEGData>();
  EXPECT_TRUE(jxl::jpeg::DecodeJPEGData(container.boxes[jbrd_index].data,
                                        decoded_io.Main().jpeg_data.get()));

  jxl::DecompressParams dparams;
  dparams.keep_dct = true;
  jxl::Span<const uint8_t> codestream_span = jxl::Span<const uint8_t>(
      codestream_bytes.data(), codestream_bytes.size());
  EXPECT_TRUE(jxl::DecodeFile(dparams, codestream_span, &decoded_io, nullptr));

  std::vector<uint8_t> decoded_jpeg_bytes;
  auto write = [&decoded_jpeg_bytes](const uint8_t* buf, size_t len) {
    decoded_jpeg_bytes.insert(decoded_jpeg_bytes.end(), buf, buf + len);
    return len;
  };
  EXPECT_TRUE(jxl::jpeg::WriteJpeg(*decoded_io.Main().jpeg_data, write));

  EXPECT_EQ(decoded_jpeg_bytes.size(), orig.size());
  EXPECT_EQ(0, memcmp(decoded_jpeg_bytes.data(), orig.data(), orig.size()));
}

static void ProcessEncoder(JxlEncoder* enc, std::vector<uint8_t>& compressed,
                           uint8_t*& next_out, size_t& avail_out) {
  JxlEncoderStatus process_result = JXL_ENC_NEED_MORE_OUTPUT;
  while (process_result == JXL_ENC_NEED_MORE_OUTPUT) {
    process_result = JxlEncoderProcessOutput(enc, &next_out, &avail_out);
    if (process_result == JXL_ENC_NEED_MORE_OUTPUT) {
      size_t offset = next_out - compressed.data();
      compressed.resize(compressed.size() * 2);
      next_out = compressed.data() + offset;
      avail_out = compressed.size() - offset;
    }
  }
  size_t offset = next_out - compressed.data();
  compressed.resize(next_out - compressed.data());
  next_out = compressed.data() + offset;
  avail_out = compressed.size() - offset;
  EXPECT_EQ(JXL_ENC_SUCCESS, process_result);
}

TEST(EncodeTest, BoxTest) {
  // Test with uncompressed boxes and with brob boxes
  for (int compress_box = 0; compress_box <= 1; ++compress_box) {
    // Tests adding two metadata boxes with the encoder: an exif box before the
    // image frame, and an xml box after the image frame. Then verifies the
    // decoder can decode them, they are in the expected place, and have the
    // correct content after decoding.
    JxlEncoderPtr enc = JxlEncoderMake(nullptr);
    EXPECT_NE(nullptr, enc.get());

    EXPECT_EQ(JXL_ENC_SUCCESS, JxlEncoderUseBoxes(enc.get()));

    JxlEncoderOptions* options = JxlEncoderOptionsCreate(enc.get(), NULL);
    size_t xsize = 50;
    size_t ysize = 17;
    JxlPixelFormat pixel_format = {4, JXL_TYPE_UINT16, JXL_BIG_ENDIAN, 0};
    std::vector<uint8_t> pixels =
        jxl::test::GetSomeTestImage(xsize, ysize, 4, 0);
    JxlBasicInfo basic_info;
    jxl::test::JxlBasicInfoSetFromPixelFormat(&basic_info, &pixel_format);
    basic_info.xsize = xsize;
    basic_info.ysize = ysize;
    basic_info.uses_original_profile = false;
    EXPECT_EQ(JXL_ENC_SUCCESS, JxlEncoderSetBasicInfo(enc.get(), &basic_info));
    JxlColorEncoding color_encoding;
    JxlColorEncodingSetToSRGB(&color_encoding,
                              /*is_gray=*/false);
    EXPECT_EQ(JXL_ENC_SUCCESS,
              JxlEncoderSetColorEncoding(enc.get(), &color_encoding));

    std::vector<uint8_t> compressed = std::vector<uint8_t>(64);
    uint8_t* next_out = compressed.data();
    size_t avail_out = compressed.size() - (next_out - compressed.data());

    // Add an early metadata box. Also add a valid 4-byte TIFF offset header
    // before the fake exif data of these box contents.
    constexpr const char* exif_test_string = "\0\0\0\0exif test data";
    const uint8_t* exif_data =
        reinterpret_cast<const uint8_t*>(exif_test_string);
    // Skip the 4 zeroes for strlen
    const size_t exif_size = 4 + strlen(exif_test_string + 4);
    JxlEncoderAddBox(enc.get(), "Exif", exif_data, exif_size, compress_box);

    // Write to output
    ProcessEncoder(enc.get(), compressed, next_out, avail_out);

    // Add image frame
    EXPECT_EQ(JXL_ENC_SUCCESS,
              JxlEncoderAddImageFrame(options, &pixel_format, pixels.data(),
                                      pixels.size()));
    // Indicate this is the last frame
    JxlEncoderCloseFrames(enc.get());

    // Write to output
    ProcessEncoder(enc.get(), compressed, next_out, avail_out);

    // Add a late metadata box
    constexpr const char* xml_test_string = "<some random xml data>";
    const uint8_t* xml_data = reinterpret_cast<const uint8_t*>(xml_test_string);
    size_t xml_size = strlen(xml_test_string);
    JxlEncoderAddBox(enc.get(), "XML ", xml_data, xml_size, compress_box);

    // Indicate this is the last box
    JxlEncoderCloseBoxes(enc.get());

    // Write to output
    ProcessEncoder(enc.get(), compressed, next_out, avail_out);

    // Decode to verify the boxes, we don't decode to pixels, only the boxes.
    JxlDecoderPtr dec = JxlDecoderMake(nullptr);
    EXPECT_NE(nullptr, dec.get());

    if (compress_box) {
      EXPECT_EQ(JXL_DEC_SUCCESS,
                JxlDecoderSetDecompressBoxes(dec.get(), JXL_TRUE));
    }

    EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderSubscribeEvents(
                                   dec.get(), JXL_DEC_FRAME | JXL_DEC_BOX));

    JxlDecoderSetInput(dec.get(), compressed.data(), compressed.size());
    JxlDecoderCloseInput(dec.get());

    std::vector<uint8_t> dec_exif_box(exif_size);
    std::vector<uint8_t> dec_xml_box(xml_size);

    for (bool post_frame = false;;) {
      JxlDecoderStatus status = JxlDecoderProcessInput(dec.get());
      if (status == JXL_DEC_ERROR) {
        FAIL();
      } else if (status == JXL_DEC_SUCCESS) {
        EXPECT_EQ(0, JxlDecoderReleaseBoxBuffer(dec.get()));
        break;
      } else if (status == JXL_DEC_FRAME) {
        post_frame = true;
      } else if (status == JXL_DEC_BOX) {
        // Since we gave the exif/xml box output buffer of the exact known
        // correct size, 0 bytes should be released. Same when no buffer was
        // set.
        EXPECT_EQ(0, JxlDecoderReleaseBoxBuffer(dec.get()));
        JxlBoxType type;
        EXPECT_EQ(JXL_DEC_SUCCESS, JxlDecoderGetBoxType(dec.get(), type, true));
        if (!memcmp(type, "Exif", 4)) {
          // This box should have been encoded before the image frame
          EXPECT_EQ(false, post_frame);
          JxlDecoderSetBoxBuffer(dec.get(), dec_exif_box.data(),
                                 dec_exif_box.size());
        } else if (!memcmp(type, "XML ", 4)) {
          // This box should have been encoded after the image frame
          EXPECT_EQ(true, post_frame);
          JxlDecoderSetBoxBuffer(dec.get(), dec_xml_box.data(),
                                 dec_xml_box.size());
        }
      } else {
        FAIL();  // unexpected status
      }
    }

    EXPECT_EQ(0, memcmp(exif_data, dec_exif_box.data(), exif_size));
    EXPECT_EQ(0, memcmp(xml_data, dec_xml_box.data(), xml_size));
  }
}

#if JPEGXL_ENABLE_JPEG  // Loading .jpg files requires libjpeg support.
TEST(EncodeTest, JXL_TRANSCODE_JPEG_TEST(JPEGFrameTest)) {
  for (int skip_basic_info = 0; skip_basic_info < 2; skip_basic_info++) {
    for (int skip_color_encoding = 0; skip_color_encoding < 2;
         skip_color_encoding++) {
      const std::string jpeg_path =
          "imagecompression.info/flower_foveon_cropped.jpg";
      const jxl::PaddedBytes orig = jxl::ReadTestData(jpeg_path);
      jxl::CodecInOut orig_io;
      ASSERT_TRUE(SetFromBytes(jxl::Span<const uint8_t>(orig), &orig_io,
                               /*pool=*/nullptr));

      JxlEncoderPtr enc = JxlEncoderMake(nullptr);
      JxlEncoderOptions* options = JxlEncoderOptionsCreate(enc.get(), NULL);
      JxlEncoderOptionsSetInteger(options, JXL_ENC_OPTION_EFFORT, 1);
      if (!skip_basic_info) {
        JxlBasicInfo basic_info;
        JxlEncoderInitBasicInfo(&basic_info);
        basic_info.xsize = orig_io.xsize();
        basic_info.ysize = orig_io.ysize();
        basic_info.uses_original_profile = true;
        EXPECT_EQ(JXL_ENC_SUCCESS,
                  JxlEncoderSetBasicInfo(enc.get(), &basic_info));
      }
      if (!skip_color_encoding) {
        JxlColorEncoding color_encoding;
        JxlColorEncodingSetToSRGB(&color_encoding, /*is_gray=*/false);
        EXPECT_EQ(JXL_ENC_SUCCESS,
                  JxlEncoderSetColorEncoding(enc.get(), &color_encoding));
      }
      EXPECT_EQ(JXL_ENC_SUCCESS,
                JxlEncoderAddJPEGFrame(options, orig.data(), orig.size()));
      JxlEncoderCloseInput(enc.get());

      std::vector<uint8_t> compressed = std::vector<uint8_t>(64);
      uint8_t* next_out = compressed.data();
      size_t avail_out = compressed.size() - (next_out - compressed.data());
      JxlEncoderStatus process_result = JXL_ENC_NEED_MORE_OUTPUT;
      while (process_result == JXL_ENC_NEED_MORE_OUTPUT) {
        process_result =
            JxlEncoderProcessOutput(enc.get(), &next_out, &avail_out);
        if (process_result == JXL_ENC_NEED_MORE_OUTPUT) {
          size_t offset = next_out - compressed.data();
          compressed.resize(compressed.size() * 2);
          next_out = compressed.data() + offset;
          avail_out = compressed.size() - offset;
        }
      }
      compressed.resize(next_out - compressed.data());
      EXPECT_EQ(JXL_ENC_SUCCESS, process_result);

      jxl::DecompressParams dparams;
      jxl::CodecInOut decoded_io;
      EXPECT_TRUE(jxl::DecodeFile(
          dparams,
          jxl::Span<const uint8_t>(compressed.data(), compressed.size()),
          &decoded_io, /*pool=*/nullptr));

      EXPECT_LE(ComputeDistance2(orig_io.Main(), decoded_io.Main()), 3.5);
    }
  }
}
#endif  // JPEGXL_ENABLE_JPEG
