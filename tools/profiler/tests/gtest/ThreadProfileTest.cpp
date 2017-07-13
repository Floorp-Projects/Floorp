
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include "ProfileBufferEntry.h"
#include "ThreadInfo.h"

// Make sure we can initialize our thread profile
TEST(ThreadProfile, Initialization) {
  Thread::tid_t tid = 1000;
  ThreadInfo info("testThread", tid, true, nullptr);
  info.StartProfiling();
}

// Make sure we can record one entry and read it
TEST(ThreadProfile, InsertOneEntry) {
  Thread::tid_t tid = 1000;
  ThreadInfo info("testThread", tid, true, nullptr);
  auto pb = MakeUnique<ProfileBuffer>(10);
  pb->addEntry(ProfileBufferEntry::Time(123.1));
  ASSERT_TRUE(pb->mEntries != nullptr);
  ASSERT_TRUE(pb->mEntries[pb->mReadPos].IsTime());
  ASSERT_TRUE(pb->mEntries[pb->mReadPos].u.mDouble == 123.1);
}

// See if we can insert some entries
TEST(ThreadProfile, InsertEntriesNoWrap) {
  Thread::tid_t tid = 1000;
  ThreadInfo info("testThread", tid, true, nullptr);
  auto pb = MakeUnique<ProfileBuffer>(100);
  int test_size = 50;
  for (int i = 0; i < test_size; i++) {
    pb->addEntry(ProfileBufferEntry::Time(i));
  }
  ASSERT_TRUE(pb->mEntries != nullptr);
  int readPos = pb->mReadPos;
  while (readPos != pb->mWritePos) {
    ASSERT_TRUE(pb->mEntries[readPos].IsTime());
    ASSERT_TRUE(pb->mEntries[readPos].u.mDouble == readPos);
    readPos = (readPos + 1) % pb->mEntrySize;
  }
}

// See if wrapping works as it should in the basic case
TEST(ThreadProfile, InsertEntriesWrap) {
  Thread::tid_t tid = 1000;
  // we can fit only 24 entries in this buffer because of the empty slot
  int entries = 24;
  int buffer_size = entries + 1;
  ThreadInfo info("testThread", tid, true, nullptr);
  auto pb = MakeUnique<ProfileBuffer>(buffer_size);
  int test_size = 43;
  for (int i = 0; i < test_size; i++) {
    pb->addEntry(ProfileBufferEntry::Time(i));
  }
  ASSERT_TRUE(pb->mEntries != nullptr);
  int readPos = pb->mReadPos;
  int ctr = 0;
  while (readPos != pb->mWritePos) {
    ASSERT_TRUE(pb->mEntries[readPos].IsTime());
    // the first few entries were discarded when we wrapped
    ASSERT_TRUE(pb->mEntries[readPos].u.mDouble == ctr + (test_size - entries));
    ctr++;
    readPos = (readPos + 1) % pb->mEntrySize;
  }
}

