// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "jxl/gain_map.h"

#include <jxl/encode.h>
#include <stdint.h>

#include <fstream>

#include "lib/jxl/base/span.h"
#include "lib/jxl/base/status.h"
#include "lib/jxl/fields.h"
#include "lib/jxl/test_memory_manager.h"
#include "lib/jxl/test_utils.h"
#include "lib/jxl/testing.h"

namespace {
bool ColorEncodingsEqual(const JxlColorEncoding& lhs,
                         const JxlColorEncoding& rhs) {
  return lhs.color_space == rhs.color_space &&
         lhs.white_point == rhs.white_point &&
         std::memcmp(lhs.white_point_xy, rhs.white_point_xy,
                     sizeof(lhs.white_point_xy)) == 0 &&
         lhs.primaries == rhs.primaries &&
         std::memcmp(lhs.primaries_red_xy, rhs.primaries_red_xy,
                     sizeof(lhs.primaries_red_xy)) == 0 &&
         std::memcmp(lhs.primaries_green_xy, rhs.primaries_green_xy,
                     sizeof(lhs.primaries_green_xy)) == 0 &&
         std::memcmp(lhs.primaries_blue_xy, rhs.primaries_blue_xy,
                     sizeof(lhs.primaries_blue_xy)) == 0 &&
         lhs.transfer_function == rhs.transfer_function &&
         lhs.gamma == rhs.gamma && lhs.rendering_intent == rhs.rendering_intent;
}

std::vector<uint8_t> GoldenTestGainMap(bool has_icc, bool has_color_encoding) {
  // Define the parts of the gain map
  uint8_t jhgm_version = 0x00;
  std::vector<uint8_t> gain_map_metadata_size = {0x00, 0x58};  // 88 in decimal
  // TODO(firsching): Replace with more realistic data
  std::string first_placeholder =
      "placeholder gain map metadata, fill with actual example after (ISO "
      "21496-1) is finalized";

  uint8_t color_encoding_size = has_color_encoding ? 3 : 0;
  std::vector<uint8_t> color_encoding = {0x50, 0xb4, 0x00};

  std::vector<uint8_t> icc_size = {0x00, 0x00, 0x00, 0x00};
  if (has_icc) {
    icc_size = {0x00, 0x00, 0x01, 0x7A};  // 378 in decimal
  }
  std::vector<uint8_t> icc_data = jxl::test::GetCompressedIccTestProfile();
  std::string second_placeholder =
      "placeholder for an actual naked JPEG XL codestream";

  // Assemble the gain map
  std::vector<uint8_t> gain_map;
  gain_map.push_back(jhgm_version);
  gain_map.insert(gain_map.end(), gain_map_metadata_size.begin(),
                  gain_map_metadata_size.end());
  gain_map.insert(gain_map.end(), first_placeholder.begin(),
                  first_placeholder.end());
  gain_map.push_back(color_encoding_size);
  if (has_color_encoding) {
    gain_map.insert(gain_map.end(), color_encoding.begin(),
                    color_encoding.end());
  }
  gain_map.insert(gain_map.end(), icc_size.begin(), icc_size.end());
  if (has_icc) {
    gain_map.insert(gain_map.end(), icc_data.begin(), icc_data.end());
  }
  gain_map.insert(gain_map.end(), second_placeholder.begin(),
                  second_placeholder.end());

  return gain_map;
}

}  // namespace

namespace jxl {
namespace {

struct GainMapTestParams {
  bool has_color_encoding;
  std::vector<uint8_t> icc_data;
};

class GainMapTest : public ::testing::TestWithParam<GainMapTestParams> {};

TEST_P(GainMapTest, GainMapRoundtrip) {
  size_t bundle_size;
  const GainMapTestParams& params = GetParam();
  std::vector<uint8_t> golden_gain_map =
      GoldenTestGainMap(!params.icc_data.empty(), params.has_color_encoding);

  JxlGainMapBundle orig_bundle;
  // Initialize the bundle with some test data
  orig_bundle.jhgm_version = 0;
  const char* metadata_str =
      "placeholder gain map metadata, fill with actual example after (ISO "
      "21496-1) is finalized";
  std::vector<uint8_t> gain_map_metadata(metadata_str,
                                         metadata_str + strlen(metadata_str));
  orig_bundle.gain_map_metadata_size = gain_map_metadata.size();
  orig_bundle.gain_map_metadata = gain_map_metadata.data();

  // Use the ICC profile from the parameter
  orig_bundle.has_color_encoding = params.has_color_encoding;
  if (orig_bundle.has_color_encoding) {
    JxlColorEncoding color_encoding = {};
    JxlColorEncodingSetToLinearSRGB(&color_encoding, /*is_gray=*/JXL_FALSE);
    orig_bundle.color_encoding = color_encoding;
  }

  std::vector<uint8_t> alt_icc(params.icc_data.begin(), params.icc_data.end());
  orig_bundle.alt_icc = alt_icc.data();
  orig_bundle.alt_icc_size = alt_icc.size();

  const char* gain_map_str =
      "placeholder for an actual naked JPEG XL codestream";
  std::vector<uint8_t> gain_map(gain_map_str,
                                gain_map_str + strlen(gain_map_str));
  orig_bundle.gain_map_size = gain_map.size();
  orig_bundle.gain_map = gain_map.data();

  ASSERT_TRUE(JxlGainMapGetBundleSize(&orig_bundle, &bundle_size));
  EXPECT_EQ(bundle_size, golden_gain_map.size());

  std::vector<uint8_t> buffer(bundle_size);
  size_t bytes_written;
  ASSERT_TRUE(JxlGainMapWriteBundle(&orig_bundle, buffer.data(), buffer.size(),
                                    &bytes_written));
  EXPECT_EQ(bytes_written, bundle_size);
  std::ofstream dump("/tmp/gainmap.bin", std::ios::out);
  dump.write(reinterpret_cast<const char*>(buffer.data()), buffer.size());
  dump.close();
  EXPECT_EQ(buffer[0], orig_bundle.jhgm_version);
  EXPECT_EQ(buffer.size(), golden_gain_map.size());
  EXPECT_TRUE(
      std::equal(buffer.begin(), buffer.end(), golden_gain_map.begin()));

  JxlGainMapBundle output_bundle;
  size_t bytes_read;
  ASSERT_TRUE(JxlGainMapReadBundle(&output_bundle, buffer.data(), buffer.size(),
                                   &bytes_read));

  EXPECT_EQ(output_bundle.gain_map_size, orig_bundle.gain_map_size);
  EXPECT_EQ(output_bundle.gain_map_metadata_size,
            orig_bundle.gain_map_metadata_size);
  EXPECT_EQ(output_bundle.alt_icc_size, orig_bundle.alt_icc_size);
  EXPECT_EQ(output_bundle.has_color_encoding, params.has_color_encoding);
  EXPECT_EQ(output_bundle.jhgm_version, orig_bundle.jhgm_version);
  std::vector<uint8_t> output_gain_map_metadata(
      output_bundle.gain_map_metadata,
      output_bundle.gain_map_metadata + output_bundle.gain_map_metadata_size);
  std::vector<uint8_t> output_alt_icc(
      output_bundle.alt_icc,
      output_bundle.alt_icc + output_bundle.alt_icc_size);
  std::vector<uint8_t> output_gain_map(
      output_bundle.gain_map,
      output_bundle.gain_map + output_bundle.gain_map_size);
  EXPECT_TRUE(std::equal(output_gain_map_metadata.begin(),
                         output_gain_map_metadata.end(),
                         gain_map_metadata.begin()));
  EXPECT_TRUE(std::equal(output_alt_icc.begin(), output_alt_icc.end(),
                         alt_icc.begin()));
  EXPECT_TRUE(std::equal(output_gain_map.begin(), output_gain_map.end(),
                         gain_map.begin()));
}

JXL_GTEST_INSTANTIATE_TEST_SUITE_P(
    GainMapTestCases, GainMapTest,
    ::testing::Values(
        GainMapTestParams{true, std::vector<uint8_t>()},
        GainMapTestParams{true, test::GetCompressedIccTestProfile()},
        GainMapTestParams{false, test::GetCompressedIccTestProfile()},
        GainMapTestParams{false, std::vector<uint8_t>()}),
    [](const testing::TestParamInfo<GainMapTest::ParamType>& info) {
      std::string name =
          "HasColorEncoding" + std::to_string(info.param.has_color_encoding);

      name += "ICCSize" + std::to_string(info.param.icc_data.size());

      return name;
    });

}  // namespace
}  // namespace jxl
