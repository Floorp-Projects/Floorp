// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

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
