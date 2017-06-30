// Copyright 2015 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/fxcrt/fx_bidi.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const FX_WCHAR kNeutralChar = 32;
const FX_WCHAR kLeftChar = 65;
const FX_WCHAR kRightChar = 1424;

}  // namespace

TEST(fxcrt, BidiCharEmpty) {
  CFX_BidiChar bidi;
  CFX_BidiChar::Segment info;

  info = bidi.GetSegmentInfo();
  EXPECT_EQ(CFX_BidiChar::NEUTRAL, info.direction);
  EXPECT_EQ(0, info.start);
  EXPECT_EQ(0, info.count);
  EXPECT_FALSE(bidi.EndChar());
}

TEST(fxcrt, BidiCharLeft) {
  CFX_BidiChar bidi;
  CFX_BidiChar::Segment info;

  EXPECT_TRUE(bidi.AppendChar(kLeftChar));
  info = bidi.GetSegmentInfo();
  EXPECT_EQ(0, info.start);
  EXPECT_EQ(0, info.count);

  EXPECT_FALSE(bidi.AppendChar(kLeftChar));
  EXPECT_FALSE(bidi.AppendChar(kLeftChar));

  info = bidi.GetSegmentInfo();
  EXPECT_EQ(CFX_BidiChar::NEUTRAL, info.direction);
  EXPECT_EQ(0, info.start);
  EXPECT_EQ(0, info.count);

  EXPECT_TRUE(bidi.EndChar());
  info = bidi.GetSegmentInfo();
  EXPECT_EQ(CFX_BidiChar::LEFT, info.direction);
  EXPECT_EQ(0, info.start);
  EXPECT_EQ(3, info.count);

  EXPECT_FALSE(bidi.EndChar());
}

TEST(fxcrt, BidiCharLeftNeutralRight) {
  CFX_BidiChar bidi;
  CFX_BidiChar::Segment info;

  EXPECT_TRUE(bidi.AppendChar(kLeftChar));
  info = bidi.GetSegmentInfo();
  EXPECT_EQ(0, info.start);
  EXPECT_EQ(0, info.count);

  EXPECT_FALSE(bidi.AppendChar(kLeftChar));
  EXPECT_FALSE(bidi.AppendChar(kLeftChar));
  EXPECT_TRUE(bidi.AppendChar(kNeutralChar));
  info = bidi.GetSegmentInfo();
  EXPECT_EQ(0, info.start);
  EXPECT_EQ(3, info.count);

  EXPECT_FALSE(bidi.AppendChar(kNeutralChar));
  EXPECT_FALSE(bidi.AppendChar(kNeutralChar));
  EXPECT_FALSE(bidi.AppendChar(kNeutralChar));
  EXPECT_TRUE(bidi.AppendChar(kRightChar));
  info = bidi.GetSegmentInfo();
  EXPECT_EQ(CFX_BidiChar::NEUTRAL, info.direction);
  EXPECT_EQ(3, info.start);
  EXPECT_EQ(4, info.count);

  EXPECT_TRUE(bidi.EndChar());
  info = bidi.GetSegmentInfo();
  EXPECT_EQ(CFX_BidiChar::RIGHT, info.direction);
  EXPECT_EQ(7, info.start);
  EXPECT_EQ(1, info.count);

  EXPECT_FALSE(bidi.EndChar());
}

TEST(fxcrt, BidiCharLeftRightLeft) {
  CFX_BidiChar bidi;
  CFX_BidiChar::Segment info;

  EXPECT_TRUE(bidi.AppendChar(kLeftChar));
  info = bidi.GetSegmentInfo();
  EXPECT_EQ(0, info.start);
  EXPECT_EQ(0, info.count);

  EXPECT_FALSE(bidi.AppendChar(kLeftChar));
  EXPECT_FALSE(bidi.AppendChar(kLeftChar));
  EXPECT_TRUE(bidi.AppendChar(kRightChar));
  info = bidi.GetSegmentInfo();
  EXPECT_EQ(0, info.start);
  EXPECT_EQ(3, info.count);

  EXPECT_FALSE(bidi.AppendChar(kRightChar));
  EXPECT_FALSE(bidi.AppendChar(kRightChar));
  EXPECT_FALSE(bidi.AppendChar(kRightChar));
  EXPECT_TRUE(bidi.AppendChar(kLeftChar));
  info = bidi.GetSegmentInfo();
  EXPECT_EQ(CFX_BidiChar::RIGHT, info.direction);
  EXPECT_EQ(3, info.start);
  EXPECT_EQ(4, info.count);

  EXPECT_TRUE(bidi.EndChar());
  info = bidi.GetSegmentInfo();
  EXPECT_EQ(CFX_BidiChar::LEFT, info.direction);
  EXPECT_EQ(7, info.start);
  EXPECT_EQ(1, info.count);

  EXPECT_FALSE(bidi.EndChar());
}

TEST(fxcrt, BidiStringEmpty) {
  CFX_BidiString bidi(L"");
  EXPECT_EQ(CFX_BidiChar::LEFT, bidi.OverallDirection());
  EXPECT_TRUE(bidi.begin() == bidi.end());
}

TEST(fxcrt, BidiStringAllNeutral) {
  {
    const FX_WCHAR str[] = {kNeutralChar, 0};
    CFX_BidiString bidi(str);
    EXPECT_EQ(CFX_BidiChar::LEFT, bidi.OverallDirection());

    auto it = bidi.begin();
    ASSERT_FALSE(it == bidi.end());
    EXPECT_EQ(0, it->start);
    EXPECT_EQ(1, it->count);
    EXPECT_EQ(CFX_BidiChar::NEUTRAL, it->direction);
    ++it;
    EXPECT_TRUE(it == bidi.end());
  }
  {
    const FX_WCHAR str[] = {kNeutralChar, kNeutralChar, kNeutralChar, 0};
    CFX_BidiString bidi(str);
    EXPECT_EQ(CFX_BidiChar::LEFT, bidi.OverallDirection());

    auto it = bidi.begin();
    ASSERT_FALSE(it == bidi.end());
    EXPECT_EQ(0, it->start);
    EXPECT_EQ(3, it->count);
    EXPECT_EQ(CFX_BidiChar::NEUTRAL, it->direction);
    ++it;
    EXPECT_TRUE(it == bidi.end());
  }
}

TEST(fxcrt, BidiStringAllLeft) {
  {
    const FX_WCHAR str[] = {kLeftChar, 0};
    CFX_BidiString bidi(str);
    EXPECT_EQ(CFX_BidiChar::LEFT, bidi.OverallDirection());

    auto it = bidi.begin();
    ASSERT_FALSE(it == bidi.end());
    EXPECT_EQ(0, it->start);
    EXPECT_EQ(0, it->count);
    EXPECT_EQ(CFX_BidiChar::NEUTRAL, it->direction);
    ASSERT_FALSE(it == bidi.end());

    ++it;
    EXPECT_EQ(0, it->start);
    EXPECT_EQ(1, it->count);
    EXPECT_EQ(CFX_BidiChar::LEFT, it->direction);
    ASSERT_FALSE(it == bidi.end());

    ++it;
    EXPECT_TRUE(it == bidi.end());
  }
  {
    const FX_WCHAR str[] = {kLeftChar, kLeftChar, kLeftChar, 0};
    CFX_BidiString bidi(str);
    EXPECT_EQ(CFX_BidiChar::LEFT, bidi.OverallDirection());

    auto it = bidi.begin();
    ASSERT_FALSE(it == bidi.end());
    EXPECT_EQ(0, it->start);
    EXPECT_EQ(0, it->count);
    EXPECT_EQ(CFX_BidiChar::NEUTRAL, it->direction);
    ASSERT_FALSE(it == bidi.end());

    ++it;
    EXPECT_EQ(0, it->start);
    EXPECT_EQ(3, it->count);
    EXPECT_EQ(CFX_BidiChar::LEFT, it->direction);
    ASSERT_FALSE(it == bidi.end());

    ++it;
    EXPECT_TRUE(it == bidi.end());
  }
}

TEST(fxcrt, BidiStringAllRight) {
  {
    const FX_WCHAR str[] = {kRightChar, 0};
    CFX_BidiString bidi(str);
    EXPECT_EQ(CFX_BidiChar::RIGHT, bidi.OverallDirection());

    auto it = bidi.begin();
    EXPECT_EQ(0, it->start);
    EXPECT_EQ(1, it->count);
    EXPECT_EQ(CFX_BidiChar::RIGHT, it->direction);
    ASSERT_FALSE(it == bidi.end());

    ++it;
    ASSERT_FALSE(it == bidi.end());
    EXPECT_EQ(0, it->start);
    EXPECT_EQ(0, it->count);
    EXPECT_EQ(CFX_BidiChar::NEUTRAL, it->direction);
    ASSERT_FALSE(it == bidi.end());

    ++it;
    EXPECT_TRUE(it == bidi.end());
  }
  {
    const FX_WCHAR str[] = {kRightChar, kRightChar, kRightChar, 0};
    CFX_BidiString bidi(str);
    EXPECT_EQ(CFX_BidiChar::RIGHT, bidi.OverallDirection());

    auto it = bidi.begin();
    EXPECT_EQ(0, it->start);
    EXPECT_EQ(3, it->count);
    EXPECT_EQ(CFX_BidiChar::RIGHT, it->direction);
    ASSERT_FALSE(it == bidi.end());

    ++it;
    ASSERT_FALSE(it == bidi.end());
    EXPECT_EQ(0, it->start);
    EXPECT_EQ(0, it->count);
    EXPECT_EQ(CFX_BidiChar::NEUTRAL, it->direction);
    ASSERT_FALSE(it == bidi.end());

    ++it;
    EXPECT_TRUE(it == bidi.end());
  }
}

TEST(fxcrt, BidiStringLeftNeutralLeftRight) {
  const FX_WCHAR str[] = {kLeftChar, kNeutralChar, kLeftChar, kRightChar, 0};
  CFX_BidiString bidi(str);
  EXPECT_EQ(CFX_BidiChar::LEFT, bidi.OverallDirection());

  auto it = bidi.begin();
  ASSERT_FALSE(it == bidi.end());
  EXPECT_EQ(0, it->start);
  EXPECT_EQ(0, it->count);
  EXPECT_EQ(CFX_BidiChar::NEUTRAL, it->direction);
  ASSERT_FALSE(it == bidi.end());

  ++it;
  EXPECT_EQ(0, it->start);
  EXPECT_EQ(1, it->count);
  EXPECT_EQ(CFX_BidiChar::LEFT, it->direction);
  ASSERT_FALSE(it == bidi.end());

  ++it;
  EXPECT_EQ(1, it->start);
  EXPECT_EQ(1, it->count);
  EXPECT_EQ(CFX_BidiChar::NEUTRAL, it->direction);
  ASSERT_FALSE(it == bidi.end());

  ++it;
  EXPECT_EQ(2, it->start);
  EXPECT_EQ(1, it->count);
  EXPECT_EQ(CFX_BidiChar::LEFT, it->direction);
  ASSERT_FALSE(it == bidi.end());

  ++it;
  EXPECT_EQ(3, it->start);
  EXPECT_EQ(1, it->count);
  EXPECT_EQ(CFX_BidiChar::RIGHT, it->direction);
  ASSERT_FALSE(it == bidi.end());

  ++it;
  EXPECT_TRUE(it == bidi.end());
}

TEST(fxcrt, BidiStringRightNeutralLeftRight) {
  const FX_WCHAR str[] = {kRightChar, kNeutralChar, kLeftChar, kRightChar, 0};
  CFX_BidiString bidi(str);
  EXPECT_EQ(CFX_BidiChar::RIGHT, bidi.OverallDirection());

  auto it = bidi.begin();
  EXPECT_EQ(3, it->start);
  EXPECT_EQ(1, it->count);
  EXPECT_EQ(CFX_BidiChar::RIGHT, it->direction);
  ASSERT_FALSE(it == bidi.end());

  ++it;
  EXPECT_EQ(2, it->start);
  EXPECT_EQ(1, it->count);
  EXPECT_EQ(CFX_BidiChar::LEFT, it->direction);
  ASSERT_FALSE(it == bidi.end());

  ++it;
  EXPECT_EQ(1, it->start);
  EXPECT_EQ(1, it->count);
  EXPECT_EQ(CFX_BidiChar::NEUTRAL, it->direction);
  ASSERT_FALSE(it == bidi.end());

  ++it;
  EXPECT_EQ(0, it->start);
  EXPECT_EQ(1, it->count);
  EXPECT_EQ(CFX_BidiChar::RIGHT, it->direction);
  ASSERT_FALSE(it == bidi.end());

  ++it;
  ASSERT_FALSE(it == bidi.end());
  EXPECT_EQ(0, it->start);
  EXPECT_EQ(0, it->count);
  EXPECT_EQ(CFX_BidiChar::NEUTRAL, it->direction);
  ASSERT_FALSE(it == bidi.end());

  ++it;
  EXPECT_TRUE(it == bidi.end());
}

TEST(fxcrt, BidiStringReverse) {
  const FX_WCHAR str[] = {kLeftChar, kNeutralChar, kRightChar, kLeftChar, 0};
  CFX_BidiString bidi(str);
  EXPECT_EQ(CFX_BidiChar::LEFT, bidi.OverallDirection());
  bidi.SetOverallDirectionRight();

  auto it = bidi.begin();
  ASSERT_FALSE(it == bidi.end());
  EXPECT_EQ(3, it->start);
  EXPECT_EQ(1, it->count);
  EXPECT_EQ(CFX_BidiChar::LEFT, it->direction);

  ++it;
  ASSERT_FALSE(it == bidi.end());
  EXPECT_EQ(2, it->start);
  EXPECT_EQ(1, it->count);
  EXPECT_EQ(CFX_BidiChar::RIGHT, it->direction);
  ASSERT_FALSE(it == bidi.end());

  ++it;
  ASSERT_FALSE(it == bidi.end());
  EXPECT_EQ(1, it->start);
  EXPECT_EQ(1, it->count);
  EXPECT_EQ(CFX_BidiChar::NEUTRAL, it->direction);

  ++it;
  ASSERT_FALSE(it == bidi.end());
  EXPECT_EQ(0, it->start);
  EXPECT_EQ(1, it->count);
  EXPECT_EQ(CFX_BidiChar::LEFT, it->direction);

  ++it;
  ASSERT_FALSE(it == bidi.end());
  EXPECT_EQ(0, it->start);
  EXPECT_EQ(0, it->count);
  EXPECT_EQ(CFX_BidiChar::NEUTRAL, it->direction);

  ++it;
  EXPECT_TRUE(it == bidi.end());
}
