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

#include "lib/jxl/image_bundle.h"

#include "gtest/gtest.h"
#include "lib/jxl/aux_out.h"

namespace jxl {
namespace {

TEST(ImageBundleTest, ExtraChannelName) {
  AuxOut aux_out;
  BitWriter writer;
  BitWriter::Allotment allotment(&writer, 99);

  ImageMetadata metadata;
  ExtraChannelInfo eci;
  eci.type = ExtraChannel::kBlack;
  eci.name = "testK";
  metadata.extra_channel_info.push_back(std::move(eci));
  ASSERT_TRUE(WriteImageMetadata(metadata, &writer, /*layer=*/0, &aux_out));
  writer.ZeroPadToByte();
  ReclaimAndCharge(&writer, &allotment, /*layer=*/0, &aux_out);

  BitReader reader(writer.GetSpan());
  ImageMetadata metadata_out;
  ASSERT_TRUE(ReadImageMetadata(&reader, &metadata_out));
  EXPECT_TRUE(reader.Close());
  EXPECT_EQ("testK", metadata_out.Find(ExtraChannel::kBlack)->name);
}

}  // namespace
}  // namespace jxl
