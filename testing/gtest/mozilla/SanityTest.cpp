/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * * This Source Code Form is subject to the terms of the Mozilla Public
 * * License, v. 2.0. If a copy of the MPL was not distributed with this
 * * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "gmock/gmock.h"

using ::testing::AtLeast;

// Sanity test to make sure that GTest is hooked into
// the mozilla build system correctly
TEST(MozillaGTestSanity, Runs) {
  EXPECT_EQ(1, 1);
}
namespace {
class TestMock {
public:
  TestMock() {}
  MOCK_METHOD0(MockedCall, void());
};
} // namespace
TEST(MozillaGMockSanity, Runs) {
  TestMock mockedClass;
  EXPECT_CALL(mockedClass, MockedCall())
    .Times(AtLeast(3));

  mockedClass.MockedCall();
  mockedClass.MockedCall();
  mockedClass.MockedCall();
}
