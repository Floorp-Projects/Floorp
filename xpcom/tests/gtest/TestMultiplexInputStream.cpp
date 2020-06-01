/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "nsIAsyncInputStream.h"
#include "nsComponentManagerUtils.h"
#include "nsIInputStream.h"
#include "nsIMultiplexInputStream.h"
#include "nsISeekableStream.h"
#include "nsStreamUtils.h"
#include "nsThreadUtils.h"
#include "Helpers.h"

using mozilla::GetCurrentThreadSerialEventTarget;
using mozilla::SpinEventLoopUntil;

TEST(MultiplexInputStream, Seek_SET)
{
  nsCString buf1;
  nsCString buf2;
  nsCString buf3;
  buf1.AssignLiteral("Hello world");
  buf2.AssignLiteral("The quick brown fox jumped over the lazy dog");
  buf3.AssignLiteral("Foo bar");

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

static already_AddRefed<nsIInputStream> CreateStreamHelper() {
  nsCOMPtr<nsIMultiplexInputStream> multiplexStream =
      do_CreateInstance("@mozilla.org/io/multiplex-input-stream;1");

  nsCString buf1;
  buf1.AssignLiteral("Hello");

  nsCOMPtr<nsIInputStream> inputStream1 = new testing::AsyncStringStream(buf1);
  multiplexStream->AppendStream(inputStream1);

  nsCString buf2;
  buf2.AssignLiteral(" ");

  nsCOMPtr<nsIInputStream> inputStream2 = new testing::AsyncStringStream(buf2);
  multiplexStream->AppendStream(inputStream2);

  nsCString buf3;
  buf3.AssignLiteral("World!");

  nsCOMPtr<nsIInputStream> inputStream3 = new testing::AsyncStringStream(buf3);
  multiplexStream->AppendStream(inputStream3);

  nsCOMPtr<nsIInputStream> stream(do_QueryInterface(multiplexStream));
  return stream.forget();
}

// AsyncWait - without EventTarget
TEST(TestMultiplexInputStream, AsyncWait_withoutEventTarget)
{
  nsCOMPtr<nsIInputStream> is = CreateStreamHelper();

  nsCOMPtr<nsIAsyncInputStream> ais = do_QueryInterface(is);
  ASSERT_TRUE(!!ais);

  RefPtr<testing::InputStreamCallback> cb = new testing::InputStreamCallback();

  ASSERT_EQ(NS_OK, ais->AsyncWait(cb, 0, 0, nullptr));
  ASSERT_TRUE(cb->Called());
}

// AsyncWait - with EventTarget
TEST(TestMultiplexInputStream, AsyncWait_withEventTarget)
{
  nsCOMPtr<nsIInputStream> is = CreateStreamHelper();

  nsCOMPtr<nsIAsyncInputStream> ais = do_QueryInterface(is);
  ASSERT_TRUE(!!ais);

  RefPtr<testing::InputStreamCallback> cb = new testing::InputStreamCallback();
  nsCOMPtr<nsIThread> thread = do_GetCurrentThread();

  ASSERT_EQ(NS_OK, ais->AsyncWait(cb, 0, 0, thread));

  ASSERT_FALSE(cb->Called());

  // Eventually it is called.
  MOZ_ALWAYS_TRUE(mozilla::SpinEventLoopUntil([&]() { return cb->Called(); }));
  ASSERT_TRUE(cb->Called());
}

// AsyncWait - without EventTarget - closureOnly
TEST(TestMultiplexInputStream, AsyncWait_withoutEventTarget_closureOnly)
{
  nsCOMPtr<nsIInputStream> is = CreateStreamHelper();

  nsCOMPtr<nsIAsyncInputStream> ais = do_QueryInterface(is);
  ASSERT_TRUE(!!ais);

  RefPtr<testing::InputStreamCallback> cb = new testing::InputStreamCallback();

  ASSERT_EQ(NS_OK, ais->AsyncWait(cb, nsIAsyncInputStream::WAIT_CLOSURE_ONLY, 0,
                                  nullptr));
  ASSERT_FALSE(cb->Called());

  ais->CloseWithStatus(NS_ERROR_FAILURE);
  ASSERT_TRUE(cb->Called());
}

// AsyncWait - withEventTarget - closureOnly
TEST(TestMultiplexInputStream, AsyncWait_withEventTarget_closureOnly)
{
  nsCOMPtr<nsIInputStream> is = CreateStreamHelper();

  nsCOMPtr<nsIAsyncInputStream> ais = do_QueryInterface(is);
  ASSERT_TRUE(!!ais);

  RefPtr<testing::InputStreamCallback> cb = new testing::InputStreamCallback();
  nsCOMPtr<nsIThread> thread = do_GetCurrentThread();

  ASSERT_EQ(NS_OK, ais->AsyncWait(cb, nsIAsyncInputStream::WAIT_CLOSURE_ONLY, 0,
                                  thread));

  ASSERT_FALSE(cb->Called());
  ais->CloseWithStatus(NS_ERROR_FAILURE);
  ASSERT_FALSE(cb->Called());

  // Eventually it is called.
  MOZ_ALWAYS_TRUE(mozilla::SpinEventLoopUntil([&]() { return cb->Called(); }));
  ASSERT_TRUE(cb->Called());
}

class ClosedStream final : public nsIInputStream {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS

  ClosedStream() = default;

  NS_IMETHOD
  Available(uint64_t* aLength) override { return NS_BASE_STREAM_CLOSED; }

  NS_IMETHOD
  Read(char* aBuffer, uint32_t aCount, uint32_t* aReadCount) override {
    MOZ_CRASH("This should not be called!");
    return NS_OK;
  }

  NS_IMETHOD
  ReadSegments(nsWriteSegmentFun aWriter, void* aClosure, uint32_t aCount,
               uint32_t* aResult) override {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  NS_IMETHOD
  Close() override { return NS_OK; }

  NS_IMETHOD
  IsNonBlocking(bool* aNonBlocking) override {
    *aNonBlocking = true;
    return NS_OK;
  }

 private:
  ~ClosedStream() = default;
};

NS_IMPL_ISUPPORTS(ClosedStream, nsIInputStream)

class AsyncStream final : public nsIAsyncInputStream {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS

  explicit AsyncStream(int64_t aSize) : mState(eBlocked), mSize(aSize) {}

  void Unblock() { mState = eUnblocked; }

  NS_IMETHOD
  Available(uint64_t* aLength) override {
    *aLength = mState == eBlocked ? 0 : mSize;
    return mState == eClosed ? NS_BASE_STREAM_CLOSED : NS_OK;
  }

  NS_IMETHOD
  Read(char* aBuffer, uint32_t aCount, uint32_t* aReadCount) override {
    MOZ_CRASH("This should not be called!");
    return NS_OK;
  }

  NS_IMETHOD
  ReadSegments(nsWriteSegmentFun aWriter, void* aClosure, uint32_t aCount,
               uint32_t* aResult) override {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  NS_IMETHOD
  Close() override {
    mState = eClosed;
    return NS_OK;
  }

  NS_IMETHOD
  IsNonBlocking(bool* aNonBlocking) override {
    *aNonBlocking = true;
    return NS_OK;
  }

  NS_IMETHOD
  AsyncWait(nsIInputStreamCallback* aCallback, uint32_t aFlags,
            uint32_t aRequestedCount, nsIEventTarget* aEventTarget) override {
    MOZ_CRASH("This should not be called!");
    return NS_OK;
  }

  NS_IMETHOD
  CloseWithStatus(nsresult aStatus) override { return NS_OK; }

 private:
  ~AsyncStream() = default;

  enum { eBlocked, eUnblocked, eClosed } mState;

  uint64_t mSize;
};

NS_IMPL_ISUPPORTS(AsyncStream, nsIInputStream, nsIAsyncInputStream)

TEST(TestMultiplexInputStream, Available)
{
  nsCOMPtr<nsIMultiplexInputStream> multiplexStream =
      do_CreateInstance("@mozilla.org/io/multiplex-input-stream;1");

  nsCOMPtr<nsIInputStream> s = do_QueryInterface(multiplexStream);
  ASSERT_TRUE(!!s);

  nsCOMPtr<nsIAsyncInputStream> as = do_QueryInterface(multiplexStream);
  ASSERT_TRUE(!as);

  uint64_t length;

  // The stream returns NS_BASE_STREAM_CLOSED if there are no substreams.
  nsresult rv = s->Available(&length);
  ASSERT_EQ(NS_BASE_STREAM_CLOSED, rv);

  rv = multiplexStream->AppendStream(new ClosedStream());
  ASSERT_EQ(NS_OK, rv);

  uint64_t asyncSize = 2;
  RefPtr<AsyncStream> asyncStream = new AsyncStream(2);
  rv = multiplexStream->AppendStream(asyncStream);
  ASSERT_EQ(NS_OK, rv);

  nsCString buffer;
  buffer.Assign("World!!!");

  nsCOMPtr<nsIInputStream> stringStream;
  rv = NS_NewCStringInputStream(getter_AddRefs(stringStream), buffer);
  ASSERT_EQ(NS_OK, rv);

  rv = multiplexStream->AppendStream(stringStream);
  ASSERT_EQ(NS_OK, rv);

  // Now we are async.
  as = do_QueryInterface(multiplexStream);
  ASSERT_TRUE(!!as);

  // Available should skip the closed stream and return 0 because the
  // asyncStream returns 0 and it's async.
  rv = s->Available(&length);
  ASSERT_EQ(NS_OK, rv);
  ASSERT_EQ((uint64_t)0, length);

  asyncStream->Unblock();

  // Now we should return only the size of the async stream because we don't
  // know when this is completed.
  rv = s->Available(&length);
  ASSERT_EQ(NS_OK, rv);
  ASSERT_EQ(asyncSize, length);

  asyncStream->Close();

  rv = s->Available(&length);
  ASSERT_EQ(NS_OK, rv);
  ASSERT_EQ(buffer.Length(), length);
}

class NonBufferableStringStream final : public nsIInputStream {
  nsCOMPtr<nsIInputStream> mStream;

 public:
  NS_DECL_THREADSAFE_ISUPPORTS

  explicit NonBufferableStringStream(const nsACString& aBuffer) {
    NS_NewCStringInputStream(getter_AddRefs(mStream), aBuffer);
  }

  NS_IMETHOD
  Available(uint64_t* aLength) override { return mStream->Available(aLength); }

  NS_IMETHOD
  Read(char* aBuffer, uint32_t aCount, uint32_t* aReadCount) override {
    return mStream->Read(aBuffer, aCount, aReadCount);
  }

  NS_IMETHOD
  ReadSegments(nsWriteSegmentFun aWriter, void* aClosure, uint32_t aCount,
               uint32_t* aResult) override {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  NS_IMETHOD
  Close() override { return mStream->Close(); }

  NS_IMETHOD
  IsNonBlocking(bool* aNonBlocking) override {
    return mStream->IsNonBlocking(aNonBlocking);
  }

 private:
  ~NonBufferableStringStream() = default;
};

NS_IMPL_ISUPPORTS(NonBufferableStringStream, nsIInputStream)

TEST(TestMultiplexInputStream, Bufferable)
{
  nsCOMPtr<nsIMultiplexInputStream> multiplexStream =
      do_CreateInstance("@mozilla.org/io/multiplex-input-stream;1");

  nsCOMPtr<nsIInputStream> s = do_QueryInterface(multiplexStream);
  ASSERT_TRUE(!!s);

  nsCString buf1;
  buf1.AssignLiteral("Hello ");
  nsCOMPtr<nsIInputStream> inputStream1;
  nsresult rv = NS_NewCStringInputStream(getter_AddRefs(inputStream1), buf1);
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  nsCString buf2;
  buf2.AssignLiteral("world");
  nsCOMPtr<nsIInputStream> inputStream2 = new NonBufferableStringStream(buf2);

  rv = multiplexStream->AppendStream(inputStream1);
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  rv = multiplexStream->AppendStream(inputStream2);
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  nsCOMPtr<nsIInputStream> stream(do_QueryInterface(multiplexStream));
  ASSERT_TRUE(!!stream);

  char buf3[1024];
  uint32_t size = 0;
  rv = stream->ReadSegments(NS_CopySegmentToBuffer, buf3, sizeof(buf3), &size);
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  ASSERT_EQ(size, buf1.Length() + buf2.Length());
  ASSERT_TRUE(!strncmp(buf3, "Hello world", size));
}

TEST(TestMultiplexInputStream, QILengthInputStream)
{
  nsCString buf;
  buf.AssignLiteral("Hello world");

  nsCOMPtr<nsIMultiplexInputStream> multiplexStream =
      do_CreateInstance("@mozilla.org/io/multiplex-input-stream;1");

  // nsMultiplexInputStream doesn't expose nsIInputStreamLength if there are
  // no nsIInputStreamLength sub streams.
  {
    nsCOMPtr<nsIInputStream> inputStream;
    nsresult rv = NS_NewCStringInputStream(getter_AddRefs(inputStream), buf);
    ASSERT_TRUE(NS_SUCCEEDED(rv));

    rv = multiplexStream->AppendStream(inputStream);
    ASSERT_TRUE(NS_SUCCEEDED(rv));

    nsCOMPtr<nsIInputStreamLength> fsis = do_QueryInterface(multiplexStream);
    ASSERT_TRUE(!fsis);

    nsCOMPtr<nsIAsyncInputStreamLength> afsis =
        do_QueryInterface(multiplexStream);
    ASSERT_TRUE(!afsis);
  }

  // nsMultiplexInputStream exposes nsIInputStreamLength if there is one or
  // more nsIInputStreamLength sub streams.
  {
    RefPtr<testing::LengthInputStream> inputStream =
        new testing::LengthInputStream(buf, true, false);

    nsresult rv = multiplexStream->AppendStream(inputStream);
    ASSERT_TRUE(NS_SUCCEEDED(rv));

    nsCOMPtr<nsIInputStreamLength> fsis = do_QueryInterface(multiplexStream);
    ASSERT_TRUE(!!fsis);

    nsCOMPtr<nsIAsyncInputStreamLength> afsis =
        do_QueryInterface(multiplexStream);
    ASSERT_TRUE(!afsis);
  }

  // nsMultiplexInputStream exposes nsIAsyncInputStreamLength if there is one
  // or more nsIAsyncInputStreamLength sub streams.
  {
    RefPtr<testing::LengthInputStream> inputStream =
        new testing::LengthInputStream(buf, true, true);

    nsresult rv = multiplexStream->AppendStream(inputStream);
    ASSERT_TRUE(NS_SUCCEEDED(rv));

    nsCOMPtr<nsIInputStreamLength> fsis = do_QueryInterface(multiplexStream);
    ASSERT_TRUE(!!fsis);

    nsCOMPtr<nsIAsyncInputStreamLength> afsis =
        do_QueryInterface(multiplexStream);
    ASSERT_TRUE(!!afsis);
  }
}

TEST(TestMultiplexInputStream, LengthInputStream)
{
  nsCOMPtr<nsIMultiplexInputStream> multiplexStream =
      do_CreateInstance("@mozilla.org/io/multiplex-input-stream;1");

  // First stream is a a simple one.
  nsCString buf;
  buf.AssignLiteral("Hello world");

  nsCOMPtr<nsIInputStream> inputStream;
  nsresult rv = NS_NewCStringInputStream(getter_AddRefs(inputStream), buf);
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  rv = multiplexStream->AppendStream(inputStream);
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  // A LengthInputStream, non-async.
  RefPtr<testing::LengthInputStream> lengthStream =
      new testing::LengthInputStream(buf, true, false);

  rv = multiplexStream->AppendStream(lengthStream);
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  nsCOMPtr<nsIInputStreamLength> fsis = do_QueryInterface(multiplexStream);
  ASSERT_TRUE(!!fsis);

  // Size is the sum of the 2 streams.
  int64_t length;
  rv = fsis->Length(&length);
  ASSERT_TRUE(NS_SUCCEEDED(rv));
  ASSERT_EQ(buf.Length() * 2, length);

  // An async LengthInputStream.
  RefPtr<testing::LengthInputStream> asyncLengthStream =
      new testing::LengthInputStream(buf, true, true,
                                     NS_BASE_STREAM_WOULD_BLOCK);

  rv = multiplexStream->AppendStream(asyncLengthStream);
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  nsCOMPtr<nsIAsyncInputStreamLength> afsis =
      do_QueryInterface(multiplexStream);
  ASSERT_TRUE(!!afsis);

  // Now it would block.
  rv = fsis->Length(&length);
  ASSERT_EQ(NS_BASE_STREAM_WOULD_BLOCK, rv);

  // Let's read the size async.
  RefPtr<testing::LengthCallback> callback = new testing::LengthCallback();
  rv = afsis->AsyncLengthWait(callback, GetCurrentThreadSerialEventTarget());
  ASSERT_EQ(NS_OK, rv);

  MOZ_ALWAYS_TRUE(SpinEventLoopUntil([&]() { return callback->Called(); }));
  ASSERT_EQ(buf.Length() * 3, callback->Size());

  // Now a negative stream
  lengthStream = new testing::LengthInputStream(buf, true, false, NS_OK, true);

  rv = multiplexStream->AppendStream(lengthStream);
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  rv = fsis->Length(&length);
  ASSERT_TRUE(NS_SUCCEEDED(rv));
  ASSERT_EQ(-1, length);

  // Another async LengthInputStream.
  asyncLengthStream = new testing::LengthInputStream(
      buf, true, true, NS_BASE_STREAM_WOULD_BLOCK);

  rv = multiplexStream->AppendStream(asyncLengthStream);
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  afsis = do_QueryInterface(multiplexStream);
  ASSERT_TRUE(!!afsis);

  // Let's read the size async.
  RefPtr<testing::LengthCallback> callback1 = new testing::LengthCallback();
  rv = afsis->AsyncLengthWait(callback1, GetCurrentThreadSerialEventTarget());
  ASSERT_EQ(NS_OK, rv);

  RefPtr<testing::LengthCallback> callback2 = new testing::LengthCallback();
  rv = afsis->AsyncLengthWait(callback2, GetCurrentThreadSerialEventTarget());
  ASSERT_EQ(NS_OK, rv);

  MOZ_ALWAYS_TRUE(SpinEventLoopUntil([&]() { return callback2->Called(); }));
  ASSERT_FALSE(callback1->Called());
  ASSERT_TRUE(callback2->Called());
}
