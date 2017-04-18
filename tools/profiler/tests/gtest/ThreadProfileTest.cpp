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

// Make sure we can record one tag and read it
TEST(ThreadProfile, InsertOneTag) {
  Thread::tid_t tid = 1000;
  ThreadInfo info("testThread", tid, true, nullptr);
  ProfileBuffer* pb = new ProfileBuffer(10);
  pb->addTag(ProfileBufferEntry::Time(123.1));
  ASSERT_TRUE(pb->mEntries != nullptr);
  ASSERT_TRUE(pb->mEntries[pb->mReadPos].kind() ==
              ProfileBufferEntry::Kind::Time);
  ASSERT_TRUE(pb->mEntries[pb->mReadPos].mTagDouble == 123.1);
}

// See if we can insert some tags
TEST(ThreadProfile, InsertTagsNoWrap) {
  Thread::tid_t tid = 1000;
  ThreadInfo info("testThread", tid, true, nullptr);
  ProfileBuffer* pb = new ProfileBuffer(100);
  int test_size = 50;
  for (int i = 0; i < test_size; i++) {
    pb->addTag(ProfileBufferEntry::Time(i));
  }
  ASSERT_TRUE(pb->mEntries != nullptr);
  int readPos = pb->mReadPos;
  while (readPos != pb->mWritePos) {
    ASSERT_TRUE(pb->mEntries[readPos].kind() ==
                ProfileBufferEntry::Kind::Time);
    ASSERT_TRUE(pb->mEntries[readPos].mTagDouble == readPos);
    readPos = (readPos + 1) % pb->mEntrySize;
  }
}

// See if wrapping works as it should in the basic case
TEST(ThreadProfile, InsertTagsWrap) {
  Thread::tid_t tid = 1000;
  // we can fit only 24 tags in this buffer because of the empty slot
  int tags = 24;
  int buffer_size = tags + 1;
  ThreadInfo info("testThread", tid, true, nullptr);
  ProfileBuffer* pb = new ProfileBuffer(buffer_size);
  int test_size = 43;
  for (int i = 0; i < test_size; i++) {
    pb->addTag(ProfileBufferEntry::Time(i));
  }
  ASSERT_TRUE(pb->mEntries != nullptr);
  int readPos = pb->mReadPos;
  int ctr = 0;
  while (readPos != pb->mWritePos) {
    ASSERT_TRUE(pb->mEntries[readPos].kind() ==
                ProfileBufferEntry::Kind::Time);
    // the first few tags were discarded when we wrapped
    ASSERT_TRUE(pb->mEntries[readPos].mTagDouble == ctr + (test_size - tags));
    ctr++;
    readPos = (readPos + 1) % pb->mEntrySize;
  }
}

