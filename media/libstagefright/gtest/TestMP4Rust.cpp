/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "mp4parse.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <algorithm>
#include <vector>

static intptr_t
error_reader(uint8_t* buffer, uintptr_t size, void* userdata)
{
  return -1;
}

struct read_vector {
  explicit read_vector(FILE* file, size_t length);
  explicit read_vector(size_t length);

  size_t location;
  std::vector<uint8_t> buffer;
};

read_vector::read_vector(FILE* file, size_t length)
 : location(0)
{
  buffer.resize(length);
  size_t read = fread(buffer.data(), sizeof(decltype(buffer)::value_type),
      buffer.size(), file);
  buffer.resize(read);
}

read_vector::read_vector(size_t length)
  : location(0)
{
  buffer.resize(length, 0);
}

static intptr_t
vector_reader(uint8_t* buffer, uintptr_t size, void* userdata)
{
  if (!buffer || !userdata) {
    return -1;
  }

  auto source = reinterpret_cast<read_vector*>(userdata);
  if (source->location > source->buffer.size()) {
    return -1;
  }
  uintptr_t available = source->buffer.size() - source->location;
  uintptr_t length = std::min(available, size);
  memcpy(buffer, source->buffer.data() + source->location, length);
  source->location += length;
  return length;
}

TEST(rust, MP4MetadataEmpty)
{
  mp4parse_error rv;
  mp4parse_io io;

  // Shouldn't be able to read with no context.
  rv = mp4parse_read(nullptr);
  EXPECT_EQ(rv, MP4PARSE_ERROR_BADARG);

  // Shouldn't be able to wrap an mp4parse_io with null members.
  io = { nullptr, nullptr };
  mp4parse_parser* context = mp4parse_new(&io);
  EXPECT_EQ(context, nullptr);

  io = { nullptr, &io };
  context = mp4parse_new(&io);
  EXPECT_EQ(context, nullptr);

  // FIXME: this should probably be accepted.
  io = { error_reader, nullptr };
  context = mp4parse_new(&io);
  EXPECT_EQ(context, nullptr);

  // Read method errors should propagate.
  io = { error_reader, &io };
  context = mp4parse_new(&io);
  ASSERT_NE(context, nullptr);
  rv = mp4parse_read(context);
  EXPECT_EQ(rv, MP4PARSE_ERROR_IO);
  mp4parse_free(context);

  // Short buffers should fail.
  read_vector buf(0);
  io = { vector_reader, &buf };
  context = mp4parse_new(&io);
  ASSERT_NE(context, nullptr);
  rv = mp4parse_read(context);
  EXPECT_EQ(rv, MP4PARSE_ERROR_INVALID);
  mp4parse_free(context);

  buf.buffer.reserve(4097);
  context = mp4parse_new(&io);
  ASSERT_NE(context, nullptr);
  rv = mp4parse_read(context);
  EXPECT_EQ(rv, MP4PARSE_ERROR_INVALID);
  mp4parse_free(context);

  // Empty buffers should fail.
  buf.buffer.resize(4097, 0);
  context = mp4parse_new(&io);
  rv = mp4parse_read(context);
  EXPECT_EQ(rv, MP4PARSE_ERROR_UNSUPPORTED);
  mp4parse_free(context);
}

TEST(rust, MP4Metadata)
{
  FILE* f = fopen("street.mp4", "rb");
  ASSERT_TRUE(f != nullptr);
  // Read just the moov header to work around the parser
  // treating mid-box eof as an error.
  //read_vector reader = read_vector(f, 1061);
  struct stat s;
  ASSERT_EQ(0, fstat(fileno(f), &s));
  read_vector reader = read_vector(f, s.st_size);
  fclose(f);

  mp4parse_io io = { vector_reader, &reader };
  mp4parse_parser* context = mp4parse_new(&io);
  ASSERT_NE(nullptr, context);

  mp4parse_error rv = mp4parse_read(context);
  EXPECT_EQ(MP4PARSE_OK, rv);

  uint32_t tracks = 0;
  rv = mp4parse_get_track_count(context, &tracks);
  EXPECT_EQ(MP4PARSE_OK, rv);
  EXPECT_EQ(2U, tracks);

  mp4parse_free(context);
}
