/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "mp4_demuxer/MP4Metadata.h"

#include <stdint.h>
#include <stdio.h>
#include <vector>

extern "C" bool read_box_from_buffer(uint8_t *buffer, size_t size);

using namespace mp4_demuxer;
using namespace mozilla;

TEST(rust, MP4MetadataEmpty)
{
  bool rv;
  rv = read_box_from_buffer(nullptr, 0);
  EXPECT_EQ(rv, false);

  size_t len = 4097;
  rv = read_box_from_buffer(nullptr, len);
  EXPECT_EQ(rv, false);

  std::vector<uint8_t> buf;
  rv = read_box_from_buffer(buf.data(), buf.size());
  EXPECT_EQ(rv, false);

  buf.reserve(len);
  rv = read_box_from_buffer(buf.data(), buf.size());
  EXPECT_EQ(rv, false);
}

TEST(rust, MP4Metadata)
{
  FILE* f = fopen("street.mp4", "rb");
  ASSERT_TRUE(f != nullptr);

  size_t len = 4096;
  std::vector<uint8_t> buf(len);
  size_t read = fread(buf.data(), sizeof(decltype(buf)::value_type), buf.size(), f);
  buf.resize(read);
  fclose(f);

  bool rv = read_box_from_buffer(buf.data(), buf.size());
  EXPECT_EQ(rv, true);
}
