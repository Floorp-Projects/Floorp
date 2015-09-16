/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdlib.h>
#include "gtest/gtest.h"
#include "Helpers.h"
#include "nsCOMPtr.h"
#include "nsICloneableInputStream.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"
#include "nsIStorageStream.h"

namespace {

void
WriteData(nsIOutputStream* aOut, nsTArray<char>& aData, uint32_t aNumBytes,
          nsACString& aDataWritten)
{
  uint32_t n;
  nsresult rv = aOut->Write(aData.Elements(), aNumBytes, &n);
  EXPECT_TRUE(NS_SUCCEEDED(rv));
  aDataWritten.Append(aData.Elements(), aNumBytes);
}

} // namespace

TEST(StorageStreams, Main)
{
  // generate some test data we will write in 4k chunks to the stream
  nsTArray<char> kData;
  testing::CreateData(4096, kData);

  // track how much data was written so we can compare at the end
  nsAutoCString dataWritten;

  nsresult rv;
  nsCOMPtr<nsIStorageStream> stor;

  rv = NS_NewStorageStream(kData.Length(), UINT32_MAX, getter_AddRefs(stor));
  EXPECT_TRUE(NS_SUCCEEDED(rv));

  nsCOMPtr<nsIOutputStream> out;
  rv = stor->GetOutputStream(0, getter_AddRefs(out));
  EXPECT_TRUE(NS_SUCCEEDED(rv));

  WriteData(out, kData, kData.Length(), dataWritten);
  WriteData(out, kData, kData.Length(), dataWritten);

  rv = out->Close();
  EXPECT_TRUE(NS_SUCCEEDED(rv));
  out = nullptr;

  nsCOMPtr<nsIInputStream> in;
  rv = stor->NewInputStream(0, getter_AddRefs(in));
  EXPECT_TRUE(NS_SUCCEEDED(rv));

  nsCOMPtr<nsICloneableInputStream> cloneable = do_QueryInterface(in);
  ASSERT_TRUE(cloneable != nullptr);
  ASSERT_TRUE(cloneable->GetCloneable());

  nsCOMPtr<nsIInputStream> clone;
  rv = cloneable->Clone(getter_AddRefs(clone));

  testing::ConsumeAndValidateStream(in, dataWritten);
  testing::ConsumeAndValidateStream(clone, dataWritten);
  in = nullptr;
  clone = nullptr;

  // now, write 3 more full 4k segments + 11 bytes, starting at 8192
  // total written equals 20491 bytes

  rv = stor->GetOutputStream(dataWritten.Length(), getter_AddRefs(out));
  EXPECT_TRUE(NS_SUCCEEDED(rv));

  WriteData(out, kData, kData.Length(), dataWritten);
  WriteData(out, kData, kData.Length(), dataWritten);
  WriteData(out, kData, kData.Length(), dataWritten);
  WriteData(out, kData, 11, dataWritten);

  rv = out->Close();
  EXPECT_TRUE(NS_SUCCEEDED(rv));
  out = nullptr;

  // now, read all
  rv = stor->NewInputStream(0, getter_AddRefs(in));
  EXPECT_TRUE(NS_SUCCEEDED(rv));

  testing::ConsumeAndValidateStream(in, dataWritten);
  in = nullptr;
}

TEST(StorageStreams, EarlyInputStream)
{
  // generate some test data we will write in 4k chunks to the stream
  nsTArray<char> kData;
  testing::CreateData(4096, kData);

  // track how much data was written so we can compare at the end
  nsAutoCString dataWritten;

  nsresult rv;
  nsCOMPtr<nsIStorageStream> stor;

  rv = NS_NewStorageStream(kData.Length(), UINT32_MAX, getter_AddRefs(stor));
  EXPECT_TRUE(NS_SUCCEEDED(rv));

  // Get input stream before writing data into the output stream
  nsCOMPtr<nsIInputStream> in;
  rv = stor->NewInputStream(0, getter_AddRefs(in));
  EXPECT_TRUE(NS_SUCCEEDED(rv));

  // Write data to output stream
  nsCOMPtr<nsIOutputStream> out;
  rv = stor->GetOutputStream(0, getter_AddRefs(out));
  EXPECT_TRUE(NS_SUCCEEDED(rv));

  WriteData(out, kData, kData.Length(), dataWritten);
  WriteData(out, kData, kData.Length(), dataWritten);

  rv = out->Close();
  EXPECT_TRUE(NS_SUCCEEDED(rv));
  out = nullptr;

  // Should be able to consume input stream
  testing::ConsumeAndValidateStream(in, dataWritten);
  in = nullptr;
}
