/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdlib.h>
#include "gtest/gtest.h"
#include "Helpers.h"
#include "mozilla/SeekableStreamWrapper.h"
#include "nsIPipe.h"

static void NewSeekablePipe(nsIInputStream** aReader,
                            nsIOutputStream** aWriter) {
  nsCOMPtr<nsIAsyncInputStream> reader;
  nsCOMPtr<nsIAsyncOutputStream> writer;

  nsresult rv =
      NS_NewPipe2(getter_AddRefs(reader), getter_AddRefs(writer), true, true);
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  nsCOMPtr<nsISeekableStream> seekable = do_QueryInterface(reader);
  EXPECT_FALSE(seekable);

  nsCOMPtr<nsIInputStream> wrapped;
  rv = mozilla::SeekableStreamWrapper::MaybeWrap(do_AddRef(reader),
                                                 getter_AddRefs(wrapped));
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  wrapped.forget(aReader);
  writer.forget(aWriter);
}

TEST(SeekableStreamWrapper, NoWrap)
{
  nsTArray<char> inputData;
  testing::CreateData(4096, inputData);
  nsDependentCSubstring inputString(inputData.Elements(), inputData.Length());

  nsCOMPtr<nsIInputStream> stream;
  nsresult rv = NS_NewCStringInputStream(getter_AddRefs(stream), inputString);
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  nsCOMPtr<nsIInputStream> wrapped;
  rv = mozilla::SeekableStreamWrapper::MaybeWrap(do_AddRef(stream),
                                                 getter_AddRefs(wrapped));
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  EXPECT_EQ(stream, wrapped);

  testing::ConsumeAndValidateStream(wrapped, inputString);
}

TEST(SeekableStreamWrapper, WrapPipe)
{
  nsCOMPtr<nsIInputStream> reader;
  nsCOMPtr<nsIOutputStream> writer;

  NewSeekablePipe(getter_AddRefs(reader), getter_AddRefs(writer));

  nsTArray<char> inputData;
  testing::CreateData(1024, inputData);

  uint32_t numWritten = 0;
  nsresult rv =
      writer->Write(inputData.Elements(), inputData.Length(), &numWritten);
  EXPECT_TRUE(NS_SUCCEEDED(rv));
  EXPECT_EQ(numWritten, 1024u);

  nsCOMPtr<nsISeekableStream> seekable = do_QueryInterface(reader);
  ASSERT_TRUE(seekable);

  nsCOMPtr<nsICloneableInputStream> cloneable = do_QueryInterface(reader);
  ASSERT_TRUE(cloneable);
  ASSERT_TRUE(cloneable->GetCloneable());

  nsCOMPtr<nsIInputStream> clone1;
  rv = cloneable->Clone(getter_AddRefs(clone1));
  ASSERT_TRUE(NS_SUCCEEDED(rv));
  ASSERT_TRUE(clone1);

  // Check that we can read the first 512 bytes.
  {
    char buf[512];
    uint32_t numRead = 0;
    rv = reader->Read(buf, 512, &numRead);
    EXPECT_TRUE(NS_SUCCEEDED(rv));
    EXPECT_EQ(numRead, 512u);
    EXPECT_EQ(mozilla::Span(inputData).First(512), mozilla::Span(buf));
  }

  // Check that we can read the second 512 bytes.
  {
    char buf[512];
    uint32_t numRead = 0;
    rv = reader->Read(buf, 512, &numRead);
    EXPECT_TRUE(NS_SUCCEEDED(rv));
    EXPECT_EQ(numRead, 512u);
    EXPECT_EQ(mozilla::Span(inputData).Last(512), mozilla::Span(buf));
  }

  // Should be at the end of the pipe
  {
    char buf[1];
    uint32_t numRead = 0;
    rv = reader->Read(buf, 1, &numRead);
    EXPECT_EQ(rv, NS_BASE_STREAM_WOULD_BLOCK);
  }

  // Re-read the second 512 bytes by seeking back.
  {
    rv = seekable->Seek(nsISeekableStream::NS_SEEK_CUR, -512);
    EXPECT_TRUE(NS_SUCCEEDED(rv));

    char buf[512];
    uint32_t numRead = 0;
    rv = reader->Read(buf, 512, &numRead);
    EXPECT_TRUE(NS_SUCCEEDED(rv));
    EXPECT_EQ(numRead, 512u);
    EXPECT_EQ(mozilla::Span(inputData).Last(512), mozilla::Span(buf));
  }

  // Re-read the last 256 bytes by seeking absolutely
  {
    rv = seekable->Seek(nsISeekableStream::NS_SEEK_SET, 768);
    EXPECT_TRUE(NS_SUCCEEDED(rv));

    char buf[256];
    uint32_t numRead = 0;
    rv = reader->Read(buf, 256, &numRead);
    EXPECT_TRUE(NS_SUCCEEDED(rv));
    EXPECT_EQ(numRead, 256u);
    EXPECT_EQ(mozilla::Span(inputData).Last(256), mozilla::Span(buf));
  }

  // Double-check that we haven't impacted our clone
  {
    char buf[1024];
    uint32_t numRead = 0;
    rv = clone1->Read(buf, 1024, &numRead);
    EXPECT_TRUE(NS_SUCCEEDED(rv));
    EXPECT_EQ(numRead, 1024u);
    EXPECT_EQ(mozilla::Span(inputData), mozilla::Span(buf));
  }

  // The clone should no longer be seekable once closed.
  rv = clone1->Close();
  EXPECT_TRUE(NS_SUCCEEDED(rv));
  nsCOMPtr<nsISeekableStream> clone1Seekable(do_QueryInterface(clone1));
  rv = clone1Seekable->Seek(nsISeekableStream::NS_SEEK_SET, 0);
  EXPECT_TRUE(NS_FAILED(rv));

  // Should still be open
  {
    uint64_t available = 0;
    rv = reader->Available(&available);
    EXPECT_TRUE(NS_SUCCEEDED(rv));
    EXPECT_EQ(available, uint64_t(0));
  }

  // Close the original output stream
  rv = writer->Close();
  EXPECT_TRUE(NS_SUCCEEDED(rv));

  // Should be closed
  {
    uint64_t available = 0;
    rv = reader->Available(&available);
    EXPECT_EQ(rv, NS_BASE_STREAM_CLOSED);
  }

  // Seeing back to the beginning should re-open the stream which had reached
  // the end.
  rv = seekable->Seek(nsISeekableStream::NS_SEEK_SET, 0);
  EXPECT_TRUE(NS_SUCCEEDED(rv));
  testing::ConsumeAndValidateStream(reader, inputData);
}

TEST(SeekableStreamWrapper, Interfaces)
{
  nsCOMPtr<nsIInputStream> reader;
  nsCOMPtr<nsIOutputStream> writer;

  NewSeekablePipe(getter_AddRefs(reader), getter_AddRefs(writer));

  nsCOMPtr<nsIAsyncInputStream> readerType1 = do_QueryInterface(reader);
  EXPECT_TRUE(readerType1);

  nsCOMPtr<nsITellableStream> readerType2 = do_QueryInterface(reader);
  EXPECT_TRUE(readerType2);

  nsCOMPtr<nsISeekableStream> readerType3 = do_QueryInterface(reader);
  EXPECT_TRUE(readerType3);

  nsCOMPtr<nsICloneableInputStream> readerType4 = do_QueryInterface(reader);
  EXPECT_TRUE(readerType4);

  nsCOMPtr<nsIBufferedInputStream> readerType5 = do_QueryInterface(reader);
  EXPECT_TRUE(readerType5);
}
