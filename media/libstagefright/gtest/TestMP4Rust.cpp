/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "mp4_demuxer/MP4Metadata.h"
#include "mp4parse.h"

#include <stdint.h>
#include <stdio.h>
#include <vector>

using namespace mp4_demuxer;
using namespace mozilla;

TEST(rust, MP4MetadataEmpty)
{
  int32_t rv;

  mp4parse_state* context = mp4parse_new();
  ASSERT_NE(context, nullptr);

  rv = mp4parse_read(nullptr, nullptr, 0);
  EXPECT_EQ(rv, MP4PARSE_ERROR_BADARG);
  rv = mp4parse_read(context, nullptr, 0);
  EXPECT_EQ(rv, MP4PARSE_ERROR_BADARG);

  size_t len = 4097;
  rv = mp4parse_read(nullptr, nullptr, len);
  EXPECT_EQ(rv, MP4PARSE_ERROR_BADARG);
  rv = mp4parse_read(context, nullptr, len);
  EXPECT_EQ(rv, MP4PARSE_ERROR_BADARG);

  std::vector<uint8_t> buf;
  rv = mp4parse_read(nullptr, &buf.front(), buf.size());
  EXPECT_EQ(rv, MP4PARSE_ERROR_BADARG);
  rv = mp4parse_read(context, &buf.front(), buf.size());
  EXPECT_EQ(rv, MP4PARSE_ERROR_BADARG);

  buf.reserve(len);
  rv = mp4parse_read(nullptr, &buf.front(), buf.size());
  EXPECT_EQ(rv, MP4PARSE_ERROR_BADARG);
  rv = mp4parse_read(context, &buf.front(), buf.size());
  EXPECT_EQ(rv, MP4PARSE_ERROR_BADARG);

  mp4parse_free(context);
}

TEST(rust, MP4Metadata)
{
  FILE* f = fopen("street.mp4", "rb");
  ASSERT_TRUE(f != nullptr);

  size_t len = 4096;
  std::vector<uint8_t> buf(len);
  size_t read = fread(&buf.front(), sizeof(decltype(buf)::value_type), buf.size(), f);
  buf.resize(read);
  fclose(f);

  mp4parse_state* context = mp4parse_new();
  ASSERT_NE(context, nullptr);

  int32_t rv = mp4parse_read(context, &buf.front(), buf.size());
  EXPECT_EQ(rv, MP4PARSE_OK);

  uint32_t tracks = mp4parse_get_track_count(context);
  EXPECT_EQ(tracks, 2U);

  mp4parse_free(context);
}
