/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdlib.h>
#include "nsIStorageStream.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"
#include "nsCOMPtr.h"
#include "gtest/gtest.h"

TEST(TestStorageStreams, Main)
{
  char kData[4096];
  memset(kData, 0, sizeof(kData));

  nsresult rv;
  nsCOMPtr<nsIStorageStream> stor;

  rv = NS_NewStorageStream(4096, UINT32_MAX, getter_AddRefs(stor));
  EXPECT_TRUE(NS_SUCCEEDED(rv));

  nsCOMPtr<nsIOutputStream> out;
  rv = stor->GetOutputStream(0, getter_AddRefs(out));
  EXPECT_TRUE(NS_SUCCEEDED(rv));

  uint32_t n;

  rv = out->Write(kData, sizeof(kData), &n);
  EXPECT_TRUE(NS_SUCCEEDED(rv));

  rv = out->Write(kData, sizeof(kData), &n);
  EXPECT_TRUE(NS_SUCCEEDED(rv));

  rv = out->Close();
  EXPECT_TRUE(NS_SUCCEEDED(rv));

  out = nullptr;

  nsCOMPtr<nsIInputStream> in;
  rv = stor->NewInputStream(0, getter_AddRefs(in));
  EXPECT_TRUE(NS_SUCCEEDED(rv));

  char buf[4096];

  // consume contents of input stream
  do {
    rv = in->Read(buf, sizeof(buf), &n);
    EXPECT_TRUE(NS_SUCCEEDED(rv));
  } while (n != 0);

  rv = in->Close();
  EXPECT_TRUE(NS_SUCCEEDED(rv));
  in = nullptr;

  // now, write 3 more full 4k segments + 11 bytes, starting at 8192
  // total written equals 20491 bytes

  rv = stor->GetOutputStream(8192, getter_AddRefs(out));
  EXPECT_TRUE(NS_SUCCEEDED(rv));

  rv = out->Write(kData, sizeof(kData), &n);
  EXPECT_TRUE(NS_SUCCEEDED(rv));

  rv = out->Write(kData, sizeof(kData), &n);
  EXPECT_TRUE(NS_SUCCEEDED(rv));

  rv = out->Write(kData, sizeof(kData), &n);
  EXPECT_TRUE(NS_SUCCEEDED(rv));

  rv = out->Write(kData, 11, &n);
  EXPECT_TRUE(NS_SUCCEEDED(rv));

  rv = out->Close();
  EXPECT_TRUE(NS_SUCCEEDED(rv));

  out = nullptr;

  // now, read all
  rv = stor->NewInputStream(0, getter_AddRefs(in));
  EXPECT_TRUE(NS_SUCCEEDED(rv));

  // consume contents of input stream
  do {
    rv = in->Read(buf, sizeof(buf), &n);
    EXPECT_TRUE(NS_SUCCEEDED(rv));
  } while (n != 0);

  rv = in->Close();
  EXPECT_TRUE(NS_SUCCEEDED(rv));
  in = nullptr;
}
