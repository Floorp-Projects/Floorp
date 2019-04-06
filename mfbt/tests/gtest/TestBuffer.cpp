/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include "mozilla/Buffer.h"
#include "mozilla/Array.h"

using namespace mozilla;

TEST(Buffer, TestBufferInfallible)
{
  const size_t LEN = 8;
  Array<int32_t, LEN> arr = {1, 2, 3, 4, 5, 6, 7, 8};
  Buffer<int32_t> buf(arr);

  for (size_t i = 0; i < LEN; i++) {
    ASSERT_EQ(buf[i], arr[i]);
  }

  auto iter = buf.begin();
  auto end = buf.end();
  for (size_t i = 0; i < LEN; i++) {
    ASSERT_EQ(*iter, arr[i]);
    iter++;
  }
  ASSERT_EQ(iter, end);

  Span<int32_t> span = buf;
  for (size_t i = 0; i < LEN; i++) {
    ASSERT_EQ(span[i], arr[i]);
  }

  auto spanIter = span.begin();
  auto spanEnd = span.end();
  for (size_t i = 0; i < LEN; i++) {
    ASSERT_EQ(*spanIter, arr[i]);
    spanIter++;
  }
  ASSERT_EQ(spanIter, spanEnd);

  span[3] = 42;
  ASSERT_EQ(buf[3], 42);

  Buffer<int32_t> another(std::move(buf));
  ASSERT_EQ(another[3], 42);
  ASSERT_EQ(buf.Length(), 0U);
}

TEST(Buffer, TestBufferFallible)
{
  const size_t LEN = 8;
  Array<int32_t, LEN> arr = {1, 2, 3, 4, 5, 6, 7, 8};
  auto maybe = Buffer<int32_t>::CopyFrom(arr);
  ASSERT_TRUE(maybe.isSome());
  Buffer<int32_t> buf(std::move(*maybe));

  for (size_t i = 0; i < LEN; i++) {
    ASSERT_EQ(buf[i], arr[i]);
  }

  auto iter = buf.begin();
  auto end = buf.end();
  for (size_t i = 0; i < LEN; i++) {
    ASSERT_EQ(*iter, arr[i]);
    iter++;
  }
  ASSERT_EQ(iter, end);

  Span<int32_t> span = buf;
  for (size_t i = 0; i < LEN; i++) {
    ASSERT_EQ(span[i], arr[i]);
  }

  auto spanIter = span.begin();
  auto spanEnd = span.end();
  for (size_t i = 0; i < LEN; i++) {
    ASSERT_EQ(*spanIter, arr[i]);
    spanIter++;
  }
  ASSERT_EQ(spanIter, spanEnd);

  span[3] = 42;
  ASSERT_EQ(buf[3], 42);

  Buffer<int32_t> another(std::move(buf));
  ASSERT_EQ(another[3], 42);
  ASSERT_EQ(buf.Length(), 0U);
}

TEST(Buffer, TestBufferElements)
{
  ASSERT_EQ(Buffer<int32_t>().Elements(),
            reinterpret_cast<int32_t*>(alignof(int32_t)));
}
