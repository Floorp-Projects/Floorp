/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "mp4_demuxer/MP4Metadata.h"

#include <cstdint>
#include <fstream>
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
  std::ifstream f("street.mp4");
  ASSERT_TRUE(f.is_open());

  size_t len = 4096;
  std::vector<uint8_t> buf;
  buf.reserve(len);
  f.read(reinterpret_cast<char*>(buf.data()), buf.size());
  bool rv = read_box_from_buffer(buf.data(), buf.size());
  ASSERT_FALSE(rv); // Expected fail: need to trap eof.
}
