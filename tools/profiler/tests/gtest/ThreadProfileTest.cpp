
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ProfileBuffer.h"
#include "ThreadInfo.h"

#include "mozilla/PowerOfTwo.h"
#include "mozilla/ProfileBufferChunkManagerWithLocalLimit.h"
#include "mozilla/ProfileChunkedBuffer.h"

#include "gtest/gtest.h"

// Make sure we can record one entry and read it
TEST(ThreadProfile, InsertOneEntry)
{
  mozilla::ProfileBufferChunkManagerWithLocalLimit chunkManager(
      2 * (1 + uint32_t(sizeof(ProfileBufferEntry))) * 4,
      2 * (1 + uint32_t(sizeof(ProfileBufferEntry))));
  mozilla::ProfileChunkedBuffer profileChunkedBuffer(
      mozilla::ProfileChunkedBuffer::ThreadSafety::WithMutex, chunkManager);
  auto pb = mozilla::MakeUnique<ProfileBuffer>(profileChunkedBuffer);
  pb->AddEntry(ProfileBufferEntry::Time(123.1));
  ProfileBufferEntry entry = pb->GetEntry(pb->BufferRangeStart());
  ASSERT_TRUE(entry.IsTime());
  ASSERT_EQ(123.1, entry.GetDouble());
}

// See if we can insert some entries
TEST(ThreadProfile, InsertEntriesNoWrap)
{
  mozilla::ProfileBufferChunkManagerWithLocalLimit chunkManager(
      100 * (1 + uint32_t(sizeof(ProfileBufferEntry))),
      100 * (1 + uint32_t(sizeof(ProfileBufferEntry))) / 4);
  mozilla::ProfileChunkedBuffer profileChunkedBuffer(
      mozilla::ProfileChunkedBuffer::ThreadSafety::WithMutex, chunkManager);
  auto pb = mozilla::MakeUnique<ProfileBuffer>(profileChunkedBuffer);
  const int test_size = 50;
  for (int i = 0; i < test_size; i++) {
    pb->AddEntry(ProfileBufferEntry::Time(i));
  }
  int times = 0;
  uint64_t readPos = pb->BufferRangeStart();
  while (readPos != pb->BufferRangeEnd()) {
    ProfileBufferEntry entry = pb->GetEntry(readPos);
    readPos++;
    if (entry.GetKind() == ProfileBufferEntry::Kind::INVALID) {
      continue;
    }
    ASSERT_TRUE(entry.IsTime());
    ASSERT_EQ(times, entry.GetDouble());
    times++;
  }
  ASSERT_EQ(test_size, times);
}
