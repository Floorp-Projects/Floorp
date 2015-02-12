/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "Helpers.h"
#include "mozilla/unused.h"
#include "nsICloneableInputStream.h"
#include "nsNetUtil.h"
#include "nsStreamUtils.h"
#include "nsStringStream.h"

TEST(CloneInputStream, CloneableInput)
{
  nsTArray<char> inputData;
  testing::CreateData(4 * 1024, inputData);
  nsDependentCSubstring inputString(inputData.Elements(), inputData.Length());

  nsCOMPtr<nsIInputStream> stream;
  nsresult rv = NS_NewCStringInputStream(getter_AddRefs(stream), inputString);
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  nsCOMPtr<nsIInputStream> clone;
  rv = NS_CloneInputStream(stream, getter_AddRefs(clone));
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  testing::ConsumeAndValidateStream(stream, inputString);
  testing::ConsumeAndValidateStream(clone, inputString);
}

TEST(CloneInputStream, NonCloneableInput_NoFallback)
{
  nsTArray<char> inputData;
  testing::CreateData(4 * 1024, inputData);
  nsDependentCSubstring inputString(inputData.Elements(), inputData.Length());

  nsCOMPtr<nsIInputStream> base;
  nsresult rv = NS_NewCStringInputStream(getter_AddRefs(base), inputString);
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  // Take advantage of nsBufferedInputStream being non-cloneable right
  // now.  If this changes in the future, then we need a different stream
  // type in this test.
  nsCOMPtr<nsIInputStream> stream;
  rv = NS_NewBufferedInputStream(getter_AddRefs(stream), base, 4096);
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  nsCOMPtr<nsICloneableInputStream> cloneable = do_QueryInterface(stream);
  ASSERT_TRUE(cloneable == nullptr);

  nsCOMPtr<nsIInputStream> clone;
  rv = NS_CloneInputStream(stream, getter_AddRefs(clone));
  ASSERT_TRUE(NS_FAILED(rv));
  ASSERT_TRUE(clone == nullptr);

  testing::ConsumeAndValidateStream(stream, inputString);
}

TEST(CloneInputStream, NonCloneableInput_Fallback)
{
  nsTArray<char> inputData;
  testing::CreateData(4 * 1024, inputData);
  nsDependentCSubstring inputString(inputData.Elements(), inputData.Length());

  nsCOMPtr<nsIInputStream> base;
  nsresult rv = NS_NewCStringInputStream(getter_AddRefs(base), inputString);
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  // Take advantage of nsBufferedInputStream being non-cloneable right
  // now.  If this changes in the future, then we need a different stream
  // type in this test.
  nsCOMPtr<nsIInputStream> stream;
  rv = NS_NewBufferedInputStream(getter_AddRefs(stream), base, 4096);
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  nsCOMPtr<nsICloneableInputStream> cloneable = do_QueryInterface(stream);
  ASSERT_TRUE(cloneable == nullptr);

  nsCOMPtr<nsIInputStream> clone;
  nsCOMPtr<nsIInputStream> replacement;
  rv = NS_CloneInputStream(stream, getter_AddRefs(clone),
                           getter_AddRefs(replacement));
  ASSERT_TRUE(NS_SUCCEEDED(rv));
  ASSERT_TRUE(clone != nullptr);
  ASSERT_TRUE(replacement != nullptr);
  ASSERT_TRUE(stream.get() != replacement.get());
  ASSERT_TRUE(clone.get() != replacement.get());

  stream = replacement.forget();

  // The stream is being copied asynchronously on the STS event target.  Spin
  // a yield loop here until the data is available.  Yes, this is a bit hacky,
  // but AFAICT, gtest does not support async test completion.
  uint64_t available;
  do {
    mozilla::unused << PR_Sleep(PR_INTERVAL_NO_WAIT);
    rv = stream->Available(&available);
    ASSERT_TRUE(NS_SUCCEEDED(rv));
  } while(available < inputString.Length());

  testing::ConsumeAndValidateStream(stream, inputString);
  testing::ConsumeAndValidateStream(clone, inputString);
}
