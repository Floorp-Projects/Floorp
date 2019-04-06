
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ProfileBufferEntry.h"
#include "ThreadInfo.h"

#include "gtest/gtest.h"

// Make sure we can record one entry and read it
TEST(ThreadProfile, InsertOneEntry)
{
  auto pb = MakeUnique<ProfileBuffer>(10);
  pb->AddEntry(ProfileBufferEntry::Time(123.1));
  ASSERT_TRUE(pb->GetEntry(pb->mRangeStart).IsTime());
  ASSERT_TRUE(pb->GetEntry(pb->mRangeStart).GetDouble() == 123.1);
}

// See if we can insert some entries
TEST(ThreadProfile, InsertEntriesNoWrap)
{
  auto pb = MakeUnique<ProfileBuffer>(100);
  int test_size = 50;
  for (int i = 0; i < test_size; i++) {
    pb->AddEntry(ProfileBufferEntry::Time(i));
  }
  uint64_t readPos = pb->mRangeStart;
  while (readPos != pb->mRangeEnd) {
    ASSERT_TRUE(pb->GetEntry(readPos).IsTime());
    ASSERT_TRUE(pb->GetEntry(readPos).GetDouble() == readPos);
    readPos++;
  }
}

// See if evicting works as it should in the basic case
TEST(ThreadProfile, InsertEntriesWrap)
{
  int entries = 32;
  auto pb = MakeUnique<ProfileBuffer>(entries);
  ASSERT_TRUE(pb->mRangeStart == 0);
  ASSERT_TRUE(pb->mRangeEnd == 0);
  int test_size = 43;
  for (int i = 0; i < test_size; i++) {
    pb->AddEntry(ProfileBufferEntry::Time(i));
  }
  // We inserted 11 more entries than fit in the buffer, so the first 11 entries
  // should have been evicted, and the range start should have increased to 11.
  ASSERT_TRUE(pb->mRangeStart == 11);
  uint64_t readPos = pb->mRangeStart;
  while (readPos != pb->mRangeEnd) {
    ASSERT_TRUE(pb->GetEntry(readPos).IsTime());
    ASSERT_TRUE(pb->GetEntry(readPos).GetDouble() == readPos);
    readPos++;
  }
}
