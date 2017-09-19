/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "nsComponentManagerUtils.h"
#include "nsCOMPtr.h"
#include "nsIInputStream.h"
#include "nsIMultiplexInputStream.h"
#include "nsISeekableStream.h"
#include "nsStringStream.h"

TEST(MultiplexInputStream, Seek_SET)
{
  nsCString buf1;
  nsCString buf2;
  nsCString buf3;
  buf1.Assign("Hello world");
  buf2.Assign("The quick brown fox jumped over the lazy dog");
  buf3.Assign("Foo bar");

  nsCOMPtr<nsIInputStream> inputStream1;
  nsCOMPtr<nsIInputStream> inputStream2;
  nsCOMPtr<nsIInputStream> inputStream3;
  nsresult rv = NS_NewCStringInputStream(getter_AddRefs(inputStream1), buf1);
  ASSERT_TRUE(NS_SUCCEEDED(rv));
  rv = NS_NewCStringInputStream(getter_AddRefs(inputStream2), buf2);
  ASSERT_TRUE(NS_SUCCEEDED(rv));
  rv = NS_NewCStringInputStream(getter_AddRefs(inputStream3), buf3);
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  nsCOMPtr<nsIMultiplexInputStream> multiplexStream =
    do_CreateInstance("@mozilla.org/io/multiplex-input-stream;1");
  ASSERT_TRUE(multiplexStream);
  nsCOMPtr<nsIInputStream> stream(do_QueryInterface(multiplexStream));
  ASSERT_TRUE(stream);

  rv = multiplexStream->AppendStream(inputStream1);
  ASSERT_TRUE(NS_SUCCEEDED(rv));
  rv = multiplexStream->AppendStream(inputStream2);
  ASSERT_TRUE(NS_SUCCEEDED(rv));
  rv = multiplexStream->AppendStream(inputStream3);
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  uint64_t length;
  uint32_t count;
  char readBuf[4096];
  nsCOMPtr<nsISeekableStream> seekStream = do_QueryInterface(multiplexStream);
  ASSERT_TRUE(seekStream);

  // Seek forward in first input stream
  rv = seekStream->Seek(nsISeekableStream::NS_SEEK_SET, 1);
  ASSERT_TRUE(NS_SUCCEEDED(rv));
  rv = stream->Available(&length);
  ASSERT_TRUE(NS_SUCCEEDED(rv));
  ASSERT_EQ((uint64_t)buf1.Length() + buf2.Length() + buf3.Length() - 1,
            length);

  // Check read is correct
  rv = stream->Read(readBuf, 3, &count);
  ASSERT_TRUE(NS_SUCCEEDED(rv));
  ASSERT_EQ((uint64_t)3, count);
  rv = stream->Available(&length);
  ASSERT_TRUE(NS_SUCCEEDED(rv));
  ASSERT_EQ((uint64_t)buf1.Length() + buf2.Length() + buf3.Length() - 4,
            length);
  ASSERT_EQ(0, strncmp(readBuf, "ell", count));

  // Seek to start of third input stream
  rv = seekStream->Seek(nsISeekableStream::NS_SEEK_SET,
                        buf1.Length() + buf2.Length());
  ASSERT_TRUE(NS_SUCCEEDED(rv));
  rv = stream->Available(&length);
  ASSERT_TRUE(NS_SUCCEEDED(rv));
  ASSERT_EQ((uint64_t)buf3.Length(), length);

  // Check read is correct
  rv = stream->Read(readBuf, 5, &count);
  ASSERT_TRUE(NS_SUCCEEDED(rv));
  ASSERT_EQ((uint64_t)5, count);
  rv = stream->Available(&length);
  ASSERT_TRUE(NS_SUCCEEDED(rv));
  ASSERT_EQ((uint64_t)buf3.Length() - 5, length);
  ASSERT_EQ(0, strncmp(readBuf, "Foo b", count));

  // Seek back to start of second stream (covers bug 1272371)
  rv = seekStream->Seek(nsISeekableStream::NS_SEEK_SET, buf1.Length());
  ASSERT_TRUE(NS_SUCCEEDED(rv));
  rv = stream->Available(&length);
  ASSERT_TRUE(NS_SUCCEEDED(rv));
  ASSERT_EQ((uint64_t)buf2.Length() + buf3.Length(), length);

  // Check read is correct
  rv = stream->Read(readBuf, 6, &count);
  ASSERT_TRUE(NS_SUCCEEDED(rv));
  ASSERT_EQ((uint64_t)6, count);
  rv = stream->Available(&length);
  ASSERT_TRUE(NS_SUCCEEDED(rv));
  ASSERT_EQ((uint64_t)buf2.Length() - 6 + buf3.Length(), length);
  ASSERT_EQ(0, strncmp(readBuf, "The qu", count));
}
