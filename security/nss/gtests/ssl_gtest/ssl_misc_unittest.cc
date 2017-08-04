/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "sslexp.h"

#include "gtest_utils.h"

namespace nss_test {

class MiscTest : public ::testing::Test {};

TEST_F(MiscTest, NonExistentExperimentalAPI) {
  EXPECT_EQ(nullptr, SSL_GetExperimentalAPI("blah"));
  EXPECT_EQ(SSL_ERROR_UNSUPPORTED_EXPERIMENTAL_API, PORT_GetError());
}

}  // namespace nss_test
