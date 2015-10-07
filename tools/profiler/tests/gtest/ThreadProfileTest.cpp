/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include "ProfileEntry.h"
#include "ThreadProfile.h"

// Make sure we can initialize our ThreadProfile
TEST(ThreadProfile, Initialization) {
  PseudoStack* stack = PseudoStack::create();
  Thread::tid_t tid = 1000;
  ThreadInfo info("testThread", tid, true, stack, nullptr);
  mozilla::RefPtr<ProfileBuffer> pb = new ProfileBuffer(10);
  ThreadProfile tp(&info, pb);
}

// Make sure we can record one tag and read it
TEST(ThreadProfile, InsertOneTag) {
  PseudoStack* stack = PseudoStack::create();
  Thread::tid_t tid = 1000;
  ThreadInfo info("testThread", tid, true, stack, nullptr);
  mozilla::RefPtr<ProfileBuffer> pb = new ProfileBuffer(10);
  pb->addTag(ProfileEntry('t', 123.1));
  ASSERT_TRUE(pb->mEntries != nullptr);
  ASSERT_TRUE(pb->mEntries[pb->mReadPos].mTagName == 't');
  ASSERT_TRUE(pb->mEntries[pb->mReadPos].mTagDouble == 123.1);
}

// See if we can insert some tags
TEST(ThreadProfile, InsertTagsNoWrap) {
  PseudoStack* stack = PseudoStack::create();
  Thread::tid_t tid = 1000;
  ThreadInfo info("testThread", tid, true, stack, nullptr);
  mozilla::RefPtr<ProfileBuffer> pb = new ProfileBuffer(100);
  int test_size = 50;
  for (int i = 0; i < test_size; i++) {
    pb->addTag(ProfileEntry('t', i));
  }
  ASSERT_TRUE(pb->mEntries != nullptr);
  int readPos = pb->mReadPos;
  while (readPos != pb->mWritePos) {
    ASSERT_TRUE(pb->mEntries[readPos].mTagName == 't');
    ASSERT_TRUE(pb->mEntries[readPos].mTagInt == readPos);
    readPos = (readPos + 1) % pb->mEntrySize;
  }
}

// See if wrapping works as it should in the basic case
TEST(ThreadProfile, InsertTagsWrap) {
  PseudoStack* stack = PseudoStack::create();
  Thread::tid_t tid = 1000;
  // we can fit only 24 tags in this buffer because of the empty slot
  int tags = 24;
  int buffer_size = tags + 1;
  ThreadInfo info("testThread", tid, true, stack, nullptr);
  mozilla::RefPtr<ProfileBuffer> pb = new ProfileBuffer(buffer_size);
  int test_size = 43;
  for (int i = 0; i < test_size; i++) {
    pb->addTag(ProfileEntry('t', i));
  }
  ASSERT_TRUE(pb->mEntries != nullptr);
  int readPos = pb->mReadPos;
  int ctr = 0;
  while (readPos != pb->mWritePos) {
    ASSERT_TRUE(pb->mEntries[readPos].mTagName == 't');
    // the first few tags were discarded when we wrapped
    ASSERT_TRUE(pb->mEntries[readPos].mTagInt == ctr + (test_size - tags));
    ctr++;
    readPos = (readPos + 1) % pb->mEntrySize;
  }
}

