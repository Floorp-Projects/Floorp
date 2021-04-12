#include "gtest/gtest.h"

#include "Helpers.h"
#include "nsCOMPtr.h"
#include "nsStreamUtils.h"
#include "nsString.h"
#include "nsStringStream.h"
#include "mozilla/SpinEventLoopUntil.h"
#include "mozilla/net/PartiallySeekableInputStream.h"

using mozilla::GetCurrentSerialEventTarget;
using mozilla::MakeRefPtr;
using mozilla::SpinEventLoopUntil;
using mozilla::net::PartiallySeekableInputStream;

class NonSeekableStream final : public nsIInputStream {
  nsCOMPtr<nsIInputStream> mStream;

 public:
  NS_DECL_THREADSAFE_ISUPPORTS

  explicit NonSeekableStream(const nsACString& aBuffer) {
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
    return mStream->ReadSegments(aWriter, aClosure, aCount, aResult);
  }

  NS_IMETHOD
  Close() override { return mStream->Close(); }

  NS_IMETHOD
  IsNonBlocking(bool* aNonBlocking) override {
    return mStream->IsNonBlocking(aNonBlocking);
  }

 private:
  ~NonSeekableStream() = default;
};

NS_IMPL_ISUPPORTS(NonSeekableStream, nsIInputStream)

// Helper function for creating a non-seekable nsIInputStream + a
// PartiallySeekableInputStream.
PartiallySeekableInputStream* CreateStream(uint32_t aSize, uint64_t aStreamSize,
                                           nsCString& aBuffer) {
  aBuffer.SetLength(aSize);
  for (uint32_t i = 0; i < aSize; ++i) {
    aBuffer.BeginWriting()[i] = i % 10;
  }

  RefPtr<NonSeekableStream> stream = new NonSeekableStream(aBuffer);
  return new PartiallySeekableInputStream(stream.forget(), aStreamSize);
}

// Simple reading.
TEST(TestPartiallySeekableInputStream, SimpleRead)
{
  const size_t kBufSize = 10;

  nsCString buf;
  RefPtr<PartiallySeekableInputStream> psi = CreateStream(kBufSize, 5, buf);

  uint64_t length;
  ASSERT_EQ(NS_OK, psi->Available(&length));
  ASSERT_EQ((uint64_t)kBufSize, length);

  char buf2[kBufSize];
  uint32_t count;
  ASSERT_EQ(NS_OK, psi->Read(buf2, sizeof(buf2), &count));
  ASSERT_EQ(count, buf.Length());
  ASSERT_TRUE(nsCString(buf.get(), kBufSize).Equals(nsCString(buf2, count)));

  // At this point, after reading more than the buffer size, seek is not
  // allowed.
  ASSERT_EQ(NS_ERROR_NOT_IMPLEMENTED,
            psi->Seek(nsISeekableStream::NS_SEEK_SET, 0));

  ASSERT_EQ(NS_ERROR_NOT_IMPLEMENTED,
            psi->Seek(nsISeekableStream::NS_SEEK_END, 0));

  ASSERT_EQ(NS_ERROR_NOT_IMPLEMENTED,
            psi->Seek(nsISeekableStream::NS_SEEK_CUR, 0));

  // Position is at the end of the stream.
  int64_t pos;
  ASSERT_EQ(NS_OK, psi->Tell(&pos));
  ASSERT_EQ((int64_t)kBufSize, pos);
}

// Simple seek
TEST(TestPartiallySeekableInputStream, SimpleSeek)
{
  const size_t kBufSize = 10;

  nsCString buf;
  RefPtr<PartiallySeekableInputStream> psi = CreateStream(kBufSize, 5, buf);

  uint64_t length;
  ASSERT_EQ(NS_OK, psi->Available(&length));
  ASSERT_EQ((uint64_t)kBufSize, length);

  uint32_t count;

  {
    char buf2[3];
    ASSERT_EQ(NS_OK, psi->Read(buf2, sizeof(buf2), &count));
    ASSERT_EQ(count, sizeof(buf2));
    ASSERT_TRUE(nsCString(buf.get(), sizeof(buf2))
                    .Equals(nsCString(buf2, sizeof(buf2))));

    int64_t pos;
    ASSERT_EQ(NS_OK, psi->Tell(&pos));
    ASSERT_EQ((int64_t)sizeof(buf2), pos);

    uint64_t length;
    ASSERT_EQ(NS_OK, psi->Available(&length));
    ASSERT_EQ((uint64_t)kBufSize - sizeof(buf2), length);
  }

  // Let's seek back to the beginning using NS_SEEK_SET
  ASSERT_EQ(NS_OK, psi->Seek(nsISeekableStream::NS_SEEK_SET, 0));

  {
    uint64_t length;
    ASSERT_EQ(NS_OK, psi->Available(&length));
    ASSERT_EQ((uint64_t)kBufSize, length);

    char buf2[3];
    ASSERT_EQ(NS_OK, psi->Read(buf2, sizeof(buf2), &count));
    ASSERT_EQ(count, sizeof(buf2));
    ASSERT_TRUE(nsCString(buf.get(), sizeof(buf2))
                    .Equals(nsCString(buf2, sizeof(buf2))));

    int64_t pos;
    ASSERT_EQ(NS_OK, psi->Tell(&pos));
    ASSERT_EQ((int64_t)sizeof(buf2), pos);

    ASSERT_EQ(NS_OK, psi->Available(&length));
    ASSERT_EQ((uint64_t)kBufSize - sizeof(buf2), length);
  }

  // Let's seek back of 2 bytes using NS_SEEK_CUR
  ASSERT_EQ(NS_OK, psi->Seek(nsISeekableStream::NS_SEEK_CUR, -2));

  {
    uint64_t length;
    ASSERT_EQ(NS_OK, psi->Available(&length));
    ASSERT_EQ((uint64_t)kBufSize - 1, length);

    char buf2[3];
    ASSERT_EQ(NS_OK, psi->Read(buf2, sizeof(buf2), &count));
    ASSERT_EQ(count, sizeof(buf2));
    ASSERT_TRUE(nsCString(buf.get() + 1, sizeof(buf2))
                    .Equals(nsCString(buf2, sizeof(buf2))));

    int64_t pos;
    ASSERT_EQ(NS_OK, psi->Tell(&pos));
    ASSERT_EQ((int64_t)sizeof(buf2) + 1, pos);

    ASSERT_EQ(NS_OK, psi->Available(&length));
    ASSERT_EQ((uint64_t)kBufSize - sizeof(buf2) - 1, length);
  }

  // Let's seek back to the beginning using NS_SEEK_SET
  ASSERT_EQ(NS_OK, psi->Seek(nsISeekableStream::NS_SEEK_SET, 0));

  {
    uint64_t length;
    ASSERT_EQ(NS_OK, psi->Available(&length));
    ASSERT_EQ((uint64_t)kBufSize, length);

    char buf2[kBufSize];
    ASSERT_EQ(NS_OK, psi->Read(buf2, sizeof(buf2), &count));
    ASSERT_EQ(count, buf.Length());
    ASSERT_TRUE(nsCString(buf.get(), kBufSize).Equals(nsCString(buf2, count)));
  }
}

// Full in cache
TEST(TestPartiallySeekableInputStream, FullCachedSeek)
{
  const size_t kBufSize = 10;

  nsCString buf;
  RefPtr<PartiallySeekableInputStream> psi = CreateStream(kBufSize, 4096, buf);

  uint64_t length;
  ASSERT_EQ(NS_OK, psi->Available(&length));
  ASSERT_EQ((uint64_t)kBufSize, length);

  char buf2[kBufSize];
  uint32_t count;
  ASSERT_EQ(NS_OK, psi->Read(buf2, sizeof(buf2), &count));
  ASSERT_EQ(count, buf.Length());
  ASSERT_TRUE(nsCString(buf.get(), kBufSize).Equals(nsCString(buf2, count)));

  ASSERT_EQ(NS_OK, psi->Available(&length));
  ASSERT_EQ((uint64_t)0, length);

  ASSERT_EQ(NS_OK, psi->Seek(nsISeekableStream::NS_SEEK_SET, 0));

  ASSERT_EQ(NS_OK, psi->Available(&length));
  ASSERT_EQ((uint64_t)kBufSize, length);

  ASSERT_EQ(NS_OK, psi->Read(buf2, sizeof(buf2), &count));
  ASSERT_EQ(count, buf.Length());
  ASSERT_TRUE(nsCString(buf.get(), kBufSize).Equals(nsCString(buf2, count)));

  ASSERT_EQ(NS_OK, psi->Available(&length));
  ASSERT_EQ((uint64_t)0, length);
}

TEST(TestPartiallySeekableInputStream, QIInputStreamLength)
{
  nsCString buf;
  buf.AssignLiteral("Hello world");

  for (int i = 0; i < 4; i++) {
    nsCOMPtr<nsIInputStream> psis;
    {
      RefPtr<testing::LengthInputStream> stream =
          new testing::LengthInputStream(buf, i % 2, i > 1);
      psis = new PartiallySeekableInputStream(stream.forget());
    }

    {
      nsCOMPtr<nsIInputStreamLength> qi = do_QueryInterface(psis);
      ASSERT_EQ(!!(i % 2), !!qi);
    }

    {
      nsCOMPtr<nsIAsyncInputStreamLength> qi = do_QueryInterface(psis);
      ASSERT_EQ(i > 1, !!qi);
    }
  }
}

TEST(TestPartiallySeekableInputStream, InputStreamLength)
{
  nsCString buf;
  buf.AssignLiteral("Hello world");

  nsCOMPtr<nsIInputStream> psis;
  {
    RefPtr<testing::LengthInputStream> stream =
        new testing::LengthInputStream(buf, true, false);
    psis = new PartiallySeekableInputStream(stream.forget());
  }

  nsCOMPtr<nsIInputStreamLength> qi = do_QueryInterface(psis);
  ASSERT_TRUE(!!qi);

  int64_t size;
  nsresult rv = qi->Length(&size);
  ASSERT_EQ(NS_OK, rv);
  ASSERT_EQ(buf.Length(), size);
}

TEST(TestPartiallySeekableInputStream, NegativeInputStreamLength)
{
  nsCString buf;
  buf.AssignLiteral("Hello world");

  nsCOMPtr<nsIInputStream> psis;
  {
    RefPtr<testing::LengthInputStream> stream =
        new testing::LengthInputStream(buf, true, false, NS_OK, true);
    psis = new PartiallySeekableInputStream(stream.forget());
  }

  nsCOMPtr<nsIInputStreamLength> qi = do_QueryInterface(psis);
  ASSERT_TRUE(!!qi);

  int64_t size;
  nsresult rv = qi->Length(&size);
  ASSERT_EQ(NS_OK, rv);
  ASSERT_EQ(-1, size);
}

TEST(TestPartiallySeekableInputStream, AsyncInputStreamLength)
{
  nsCString buf;
  buf.AssignLiteral("Hello world");

  nsCOMPtr<nsIInputStream> psis;
  {
    RefPtr<testing::LengthInputStream> stream =
        new testing::LengthInputStream(buf, false, true);
    psis = new PartiallySeekableInputStream(stream.forget());
  }

  nsCOMPtr<nsIAsyncInputStreamLength> qi = do_QueryInterface(psis);
  ASSERT_TRUE(!!qi);

  RefPtr<testing::LengthCallback> callback = new testing::LengthCallback();

  nsresult rv = qi->AsyncLengthWait(callback, GetCurrentSerialEventTarget());
  ASSERT_EQ(NS_OK, rv);

  MOZ_ALWAYS_TRUE(SpinEventLoopUntil([&]() { return callback->Called(); }));
  ASSERT_EQ(buf.Length(), callback->Size());
}

TEST(TestPartiallySeekableInputStream, NegativeAsyncInputStreamLength)
{
  nsCString buf;
  buf.AssignLiteral("Hello world");

  nsCOMPtr<nsIInputStream> psis;
  {
    RefPtr<testing::LengthInputStream> stream =
        new testing::LengthInputStream(buf, false, true, NS_OK, true);
    psis = new PartiallySeekableInputStream(stream.forget());
  }

  nsCOMPtr<nsIAsyncInputStreamLength> qi = do_QueryInterface(psis);
  ASSERT_TRUE(!!qi);

  RefPtr<testing::LengthCallback> callback = new testing::LengthCallback();

  nsresult rv = qi->AsyncLengthWait(callback, GetCurrentSerialEventTarget());
  ASSERT_EQ(NS_OK, rv);

  MOZ_ALWAYS_TRUE(SpinEventLoopUntil([&]() { return callback->Called(); }));
  ASSERT_EQ(-1, callback->Size());
}

TEST(TestPartiallySeekableInputStream, AbortLengthCallback)
{
  nsCString buf;
  buf.AssignLiteral("Hello world");

  nsCOMPtr<nsIInputStream> psis;
  {
    RefPtr<testing::LengthInputStream> stream =
        new testing::LengthInputStream(buf, false, true, NS_OK, true);
    psis = new PartiallySeekableInputStream(stream.forget());
  }

  nsCOMPtr<nsIAsyncInputStreamLength> qi = do_QueryInterface(psis);
  ASSERT_TRUE(!!qi);

  RefPtr<testing::LengthCallback> callback1 = new testing::LengthCallback();
  nsresult rv = qi->AsyncLengthWait(callback1, GetCurrentSerialEventTarget());
  ASSERT_EQ(NS_OK, rv);

  RefPtr<testing::LengthCallback> callback2 = new testing::LengthCallback();
  rv = qi->AsyncLengthWait(callback2, GetCurrentSerialEventTarget());
  ASSERT_EQ(NS_OK, rv);

  MOZ_ALWAYS_TRUE(SpinEventLoopUntil([&]() { return callback2->Called(); }));
  ASSERT_TRUE(!callback1->Called());
  ASSERT_EQ(-1, callback2->Size());
}

TEST(TestPartiallySeekableInputStream, AsyncWaitAfterConsumed)
{
  nsCString buf{"The Quick Brown Fox Jumps over the Lazy Dog"};
  const size_t bufsize = 44;

  auto stream = MakeRefPtr<testing::AsyncStringStream>(buf);
  nsCOMPtr<nsIAsyncInputStream> psis =
      new PartiallySeekableInputStream(stream.forget(), bufsize);
  ASSERT_TRUE(psis);

  auto callback = MakeRefPtr<testing::InputStreamCallback>();

  nsresult rv = psis->AsyncWait(callback, 0, 0, GetCurrentSerialEventTarget());
  ASSERT_EQ(NS_OK, rv);

  MOZ_ALWAYS_TRUE(SpinEventLoopUntil([&]() { return callback->Called(); }));

  char rdbuf[bufsize] = {'\0'};
  uint32_t count;
  ASSERT_EQ(NS_OK, psis->Read(rdbuf, sizeof(rdbuf), &count));
  ASSERT_STREQ(rdbuf, buf.Data());

  callback = MakeRefPtr<testing::InputStreamCallback>();

  rv = psis->AsyncWait(callback, 0, 0, GetCurrentSerialEventTarget());
  ASSERT_EQ(NS_OK, rv);

  MOZ_ALWAYS_TRUE(SpinEventLoopUntil([&]() { return callback->Called(); }));

  memset(rdbuf, 0x0, bufsize);
  ASSERT_EQ(NS_OK, psis->Read(rdbuf, sizeof(rdbuf), &count));
  ASSERT_EQ(0U, count);
}

TEST(TestPartiallySeekableInputStream, AsyncWaitAfterClosed)
{
  nsCString buf{"The Quick Brown Fox Jumps over the Lazy Dog"};
  const size_t bufsize = 44;

  auto stream = MakeRefPtr<testing::AsyncStringStream>(buf);
  nsCOMPtr<nsIAsyncInputStream> psis =
      new PartiallySeekableInputStream(stream.forget(), bufsize);
  ASSERT_TRUE(psis);

  auto callback = MakeRefPtr<testing::InputStreamCallback>();

  nsresult rv = psis->AsyncWait(callback, 0, 0, GetCurrentSerialEventTarget());
  ASSERT_EQ(NS_OK, rv);

  MOZ_ALWAYS_TRUE(SpinEventLoopUntil([&]() { return callback->Called(); }));

  ASSERT_EQ(NS_OK, psis->Close());

  callback = MakeRefPtr<testing::InputStreamCallback>();

  rv = psis->AsyncWait(callback, 0, 0, GetCurrentSerialEventTarget());
  ASSERT_EQ(NS_OK, rv);

  MOZ_ALWAYS_TRUE(SpinEventLoopUntil([&]() { return callback->Called(); }));
}

TEST(TestPartiallySeekableInputStream, AsyncLengthWaitAfterClosed)
{
  nsCString buf{"The Quick Brown Fox Jumps over the Lazy Dog"};

  auto stream = MakeRefPtr<testing::LengthInputStream>(buf, false, true);
  nsCOMPtr<nsIInputStream> psis =
      new PartiallySeekableInputStream(stream.forget());

  nsCOMPtr<nsIAsyncInputStreamLength> qi = do_QueryInterface(psis);
  ASSERT_TRUE(qi);

  auto callback = MakeRefPtr<testing::LengthCallback>();

  nsresult rv = qi->AsyncLengthWait(callback, GetCurrentSerialEventTarget());
  ASSERT_EQ(NS_OK, rv);

  MOZ_ALWAYS_TRUE(SpinEventLoopUntil([&]() { return callback->Called(); }));
  ASSERT_EQ(buf.Length(), callback->Size());

  ASSERT_EQ(NS_OK, psis->Close());

  callback = MakeRefPtr<testing::LengthCallback>();

  rv = qi->AsyncLengthWait(callback, GetCurrentSerialEventTarget());
  ASSERT_EQ(NS_OK, rv);

  MOZ_ALWAYS_TRUE(SpinEventLoopUntil([&]() { return callback->Called(); }));
  ASSERT_EQ(-1, callback->Size());
}

TEST(TestPartiallySeekableInputStream, AsyncLengthWaitAfterConsumed)
{
  nsCString buf{"The Quick Brown Fox Jumps over the Lazy Dog"};
  const size_t bufsize = 44;

  auto stream = MakeRefPtr<testing::LengthInputStream>(buf, false, true);
  nsCOMPtr<nsIInputStream> psis =
      new PartiallySeekableInputStream(stream.forget());

  nsCOMPtr<nsIAsyncInputStreamLength> qi = do_QueryInterface(psis);
  ASSERT_TRUE(qi);

  auto callback = MakeRefPtr<testing::LengthCallback>();

  nsresult rv = qi->AsyncLengthWait(callback, GetCurrentSerialEventTarget());
  ASSERT_EQ(NS_OK, rv);

  MOZ_ALWAYS_TRUE(SpinEventLoopUntil([&]() { return callback->Called(); }));
  ASSERT_EQ(buf.Length(), callback->Size());

  char rdbuf[bufsize] = {'\0'};
  uint32_t count;
  ASSERT_EQ(NS_OK, psis->Read(rdbuf, sizeof(rdbuf), &count));
  ASSERT_STREQ(rdbuf, buf.Data());

  callback = MakeRefPtr<testing::LengthCallback>();

  rv = qi->AsyncLengthWait(callback, GetCurrentSerialEventTarget());
  ASSERT_EQ(NS_OK, rv);

  MOZ_ALWAYS_TRUE(SpinEventLoopUntil([&]() { return callback->Called(); }));
  ASSERT_EQ(0U, callback->Size());

  memset(rdbuf, 0x0, bufsize);
  ASSERT_EQ(NS_OK, psis->Read(rdbuf, sizeof(rdbuf), &count));
  ASSERT_EQ(0U, count);
}
