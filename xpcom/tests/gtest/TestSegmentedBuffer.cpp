/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "../../io/nsSegmentedBuffer.h"
#include "nsIEventTarget.h"

using namespace mozilla;

TEST(SegmentedBuffer, AppendAndDelete)
{
  auto buf = MakeUnique<nsSegmentedBuffer>();
  buf->Init(4);
  char* seg;
  bool empty;
  seg = buf->AppendNewSegment();
  EXPECT_TRUE(seg) << "AppendNewSegment failed";
  seg = buf->AppendNewSegment();
  EXPECT_TRUE(seg) << "AppendNewSegment failed";
  seg = buf->AppendNewSegment();
  EXPECT_TRUE(seg) << "AppendNewSegment failed";
  empty = buf->DeleteFirstSegment();
  EXPECT_TRUE(!empty) << "DeleteFirstSegment failed";
  empty = buf->DeleteFirstSegment();
  EXPECT_TRUE(!empty) << "DeleteFirstSegment failed";
  seg = buf->AppendNewSegment();
  EXPECT_TRUE(seg) << "AppendNewSegment failed";
  seg = buf->AppendNewSegment();
  EXPECT_TRUE(seg) << "AppendNewSegment failed";
  seg = buf->AppendNewSegment();
  EXPECT_TRUE(seg) << "AppendNewSegment failed";
  empty = buf->DeleteFirstSegment();
  EXPECT_TRUE(!empty) << "DeleteFirstSegment failed";
  empty = buf->DeleteFirstSegment();
  EXPECT_TRUE(!empty) << "DeleteFirstSegment failed";
  empty = buf->DeleteFirstSegment();
  EXPECT_TRUE(!empty) << "DeleteFirstSegment failed";
  empty = buf->DeleteFirstSegment();
  EXPECT_TRUE(empty) << "DeleteFirstSegment failed";
}
