// Copyright 2015 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/fpdfapi/page/cpdf_streamparser.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(cpdf_streamparser, ReadHexString) {
  {
    // Position out of bounds.
    uint8_t data[] = "12ab>";
    CPDF_StreamParser parser(data, 5);
    parser.SetPos(6);
    EXPECT_EQ("", parser.ReadHexString());
  }

  {
    // Regular conversion.
    uint8_t data[] = "1A2b>abcd";
    CPDF_StreamParser parser(data, 5);
    EXPECT_EQ("\x1a\x2b", parser.ReadHexString());
    EXPECT_EQ(5u, parser.GetPos());
  }

  {
    // Missing ending >
    uint8_t data[] = "1A2b";
    CPDF_StreamParser parser(data, 5);
    EXPECT_EQ("\x1a\x2b", parser.ReadHexString());
    EXPECT_EQ(5u, parser.GetPos());
  }

  {
    // Uneven number of bytes.
    uint8_t data[] = "1A2>asdf";
    CPDF_StreamParser parser(data, 5);
    EXPECT_EQ("\x1a\x20", parser.ReadHexString());
    EXPECT_EQ(4u, parser.GetPos());
  }

  {
    uint8_t data[] = ">";
    CPDF_StreamParser parser(data, 5);
    EXPECT_EQ("", parser.ReadHexString());
    EXPECT_EQ(1u, parser.GetPos());
  }
}
