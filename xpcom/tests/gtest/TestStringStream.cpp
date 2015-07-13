/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "Helpers.h"
#include "nsICloneableInputStream.h"
#include "nsStringStream.h"

namespace {

static void TestStringStream(uint32_t aNumBytes)
{
  nsTArray<char> inputData;
  testing::CreateData(aNumBytes, inputData);
  nsDependentCSubstring inputString(inputData.Elements(), inputData.Length());

  nsCOMPtr<nsIInputStream> stream;
  nsresult rv = NS_NewCStringInputStream(getter_AddRefs(stream), inputString);
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  testing::ConsumeAndValidateStream(stream, inputString);
}

static void TestStringStreamClone(uint32_t aNumBytes)
{
  nsTArray<char> inputData;
  testing::CreateData(aNumBytes, inputData);
  nsDependentCSubstring inputString(inputData.Elements(), inputData.Length());

  nsCOMPtr<nsIInputStream> stream;
  nsresult rv = NS_NewCStringInputStream(getter_AddRefs(stream), inputString);
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  nsCOMPtr<nsICloneableInputStream> cloneable = do_QueryInterface(stream);
  ASSERT_TRUE(cloneable != nullptr);
  ASSERT_TRUE(cloneable->GetCloneable());

  nsCOMPtr<nsIInputStream> clone;
  rv = cloneable->Clone(getter_AddRefs(clone));

  testing::ConsumeAndValidateStream(stream, inputString);

  // Release the stream to verify that the clone's string survives correctly.
  stream = nullptr;

  testing::ConsumeAndValidateStream(clone, inputString);
}

} // namespace

TEST(StringStream, Simple_4k)
{
  TestStringStream(1024 * 4);
}

TEST(StringStream, Clone_4k)
{
  TestStringStreamClone(1024 * 4);
}
