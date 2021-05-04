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

#include "jxl/encode.h"

#include "gtest/gtest.h"
#include "jxl/encode_cxx.h"
#include "lib/extras/codec.h"
#include "lib/jxl/dec_file.h"
#include "lib/jxl/enc_butteraugli_comparator.h"
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
  jxl::CodecInOut orig_io;
  ASSERT_TRUE(
      SetFromBytes(jxl::Span<const uint8_t>(orig), &orig_io, /*pool=*/nullptr));

  JxlEncoderOptions* options = JxlEncoderOptionsCreate(enc.get(), NULL);

  JxlBasicInfo basic_info;
  basic_info.exponent_bits_per_sample = 0;
  basic_info.bits_per_sample = 8;
  basic_info.alpha_bits = 0;
  basic_info.alpha_exponent_bits = 0;
  basic_info.xsize = orig_io.xsize();
  basic_info.ysize = orig_io.ysize();
  basic_info.uses_original_profile = true;
  EXPECT_EQ(JXL_ENC_SUCCESS, JxlEncoderSetBasicInfo(enc.get(), &basic_info));
  JxlColorEncoding color_encoding;
  JxlColorEncodingSetToSRGB(&color_encoding, /*is_gray=*/false);
  EXPECT_EQ(JXL_ENC_SUCCESS,
            JxlEncoderSetColorEncoding(enc.get(), &color_encoding));
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
  basic_info.uses_original_profile = false;
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

  jxl::ButteraugliParams ba;
  EXPECT_LE(ButteraugliDistance(input_io, decoded_io, ba,
                                /*distmap=*/nullptr, nullptr),
            3.0f);
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
    EXPECT_EQ(JXL_ENC_SUCCESS, JxlEncoderOptionsSetEffort(options, 5));
    VerifyFrameEncoding(enc.get(), options);
    EXPECT_EQ(jxl::SpeedTier::kHare, enc->last_used_cparams.speed_tier);
  }

  {
    JxlEncoderPtr enc = JxlEncoderMake(nullptr);
    EXPECT_NE(nullptr, enc.get());
    JxlEncoderOptions* options = JxlEncoderOptionsCreate(enc.get(), NULL);
    // Lower than currently supported values
    EXPECT_EQ(JXL_ENC_ERROR, JxlEncoderOptionsSetEffort(options, 2));
    // Higher than currently supported values
    EXPECT_EQ(JXL_ENC_ERROR, JxlEncoderOptionsSetEffort(options, 10));
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
    EXPECT_EQ(JXL_ENC_SUCCESS, JxlEncoderOptionsSetDecodingSpeed(options, 2));
    VerifyFrameEncoding(enc.get(), options);
    EXPECT_EQ(2, enc->last_used_cparams.decoding_speed_tier);
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

  // If successfull, returns true and sets `in` to be the rest data (if any).
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
  EXPECT_EQ(JXL_ENC_SUCCESS, JxlEncoderUseContainer(enc.get(),
			  true));
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
  EXPECT_EQ(JXL_ENC_SUCCESS, JxlEncoderSetColorEncoding(enc.get(), &color_encoding));
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
  EXPECT_EQ(0, encoded_span.size());
  EXPECT_EQ(0, memcmp("jxlc", container.boxes[0].type, 4));
  EXPECT_EQ(true, container.boxes[0].data_size_given);
}

TEST(EncodeTest, JPEGReconstructionTest) {
  const std::string jpeg_path =
      "imagecompression.info/flower_foveon.png.im_q85_420.jpg";
  const jxl::PaddedBytes orig = jxl::ReadTestData(jpeg_path);
  jxl::CodecInOut orig_io;
  ASSERT_TRUE(
      SetFromBytes(jxl::Span<const uint8_t>(orig), &orig_io, /*pool=*/nullptr));

  JxlEncoderPtr enc = JxlEncoderMake(nullptr);
  JxlEncoderOptions* options = JxlEncoderOptionsCreate(enc.get(), NULL);

  JxlBasicInfo basic_info;
  basic_info.exponent_bits_per_sample = 0;
  basic_info.bits_per_sample = 8;
  basic_info.alpha_bits = 0;
  basic_info.alpha_exponent_bits = 0;
  basic_info.xsize = orig_io.xsize();
  basic_info.ysize = orig_io.ysize();
  basic_info.uses_original_profile = true;
  EXPECT_EQ(JXL_ENC_SUCCESS, JxlEncoderSetBasicInfo(enc.get(), &basic_info));
  JxlColorEncoding color_encoding;
  JxlColorEncodingSetToSRGB(&color_encoding, /*is_gray=*/false);
  EXPECT_EQ(JXL_ENC_SUCCESS,
            JxlEncoderSetColorEncoding(enc.get(), &color_encoding));
  EXPECT_EQ(JXL_ENC_SUCCESS, JxlEncoderUseContainer(enc.get(), JXL_TRUE));
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
  EXPECT_EQ(0, encoded_span.size());
  EXPECT_EQ(0, memcmp("jbrd", container.boxes[0].type, 4));
  EXPECT_EQ(0, memcmp("jxlc", container.boxes[1].type, 4));

  jxl::CodecInOut decoded_io;
  decoded_io.Main().jpeg_data = jxl::make_unique<jxl::jpeg::JPEGData>();
  EXPECT_TRUE(jxl::jpeg::DecodeJPEGData(container.boxes[0].data,
                                        decoded_io.Main().jpeg_data.get()));

  jxl::DecompressParams dparams;
  dparams.keep_dct = true;
  EXPECT_TRUE(
      jxl::DecodeFile(dparams, container.boxes[1].data, &decoded_io, nullptr));

  std::vector<uint8_t> decoded_jpeg_bytes;
  auto write = [&decoded_jpeg_bytes](const uint8_t* buf, size_t len) {
    decoded_jpeg_bytes.insert(decoded_jpeg_bytes.end(), buf, buf + len);
    return len;
  };
  EXPECT_TRUE(jxl::jpeg::WriteJpeg(*decoded_io.Main().jpeg_data, write));

  EXPECT_EQ(decoded_jpeg_bytes.size(), orig.size());
  EXPECT_EQ(0, memcmp(decoded_jpeg_bytes.data(), orig.data(), orig.size()));
}

TEST(EncodeTest, JPEGFrameTest) {
  const std::string jpeg_path =
      "imagecompression.info/flower_foveon.png.im_q85_420.jpg";
  const jxl::PaddedBytes orig = jxl::ReadTestData(jpeg_path);
  jxl::CodecInOut orig_io;
  ASSERT_TRUE(
      SetFromBytes(jxl::Span<const uint8_t>(orig), &orig_io, /*pool=*/nullptr));

  JxlEncoderPtr enc = JxlEncoderMake(nullptr);
  JxlEncoderOptions* options = JxlEncoderOptionsCreate(enc.get(), NULL);

  JxlBasicInfo basic_info;
  basic_info.exponent_bits_per_sample = 0;
  basic_info.bits_per_sample = 8;
  basic_info.alpha_bits = 0;
  basic_info.alpha_exponent_bits = 0;
  basic_info.xsize = orig_io.xsize();
  basic_info.ysize = orig_io.ysize();
  basic_info.uses_original_profile = true;
  EXPECT_EQ(JXL_ENC_SUCCESS, JxlEncoderSetBasicInfo(enc.get(), &basic_info));
  JxlColorEncoding color_encoding;
  JxlColorEncodingSetToSRGB(&color_encoding, /*is_gray=*/false);
  EXPECT_EQ(JXL_ENC_SUCCESS,
            JxlEncoderSetColorEncoding(enc.get(), &color_encoding));
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

  jxl::DecompressParams dparams;
  jxl::CodecInOut decoded_io;
  EXPECT_TRUE(jxl::DecodeFile(
      dparams, jxl::Span<const uint8_t>(compressed.data(), compressed.size()),
      &decoded_io, /*pool=*/nullptr));

  jxl::ButteraugliParams ba;
  EXPECT_LE(ButteraugliDistance(orig_io, decoded_io, ba,
                                /*distmap=*/nullptr, nullptr),
            2.5f);
}
