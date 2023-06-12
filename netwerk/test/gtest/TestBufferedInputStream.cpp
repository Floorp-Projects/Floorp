#include "gtest/gtest.h"

#include "mozilla/SpinEventLoopUntil.h"
#include "nsBufferedStreams.h"
#include "nsIThread.h"
#include "nsNetUtil.h"
#include "nsStreamUtils.h"
#include "nsThreadUtils.h"
#include "Helpers.h"

// Helper function for creating a testing::AsyncStringStream
already_AddRefed<nsBufferedInputStream> CreateStream(uint32_t aSize,
                                                     nsCString& aBuffer) {
  aBuffer.SetLength(aSize);
  for (uint32_t i = 0; i < aSize; ++i) {
    aBuffer.BeginWriting()[i] = i % 10;
  }

  nsCOMPtr<nsIInputStream> stream = new testing::AsyncStringStream(aBuffer);

  RefPtr<nsBufferedInputStream> bis = new nsBufferedInputStream();
  bis->Init(stream, aSize);
  return bis.forget();
}

// Simple reading.
TEST(TestBufferedInputStream, SimpleRead)
{
  const size_t kBufSize = 10;

  nsCString buf;
  RefPtr<nsBufferedInputStream> bis = CreateStream(kBufSize, buf);

  uint64_t length;
  ASSERT_EQ(NS_OK, bis->Available(&length));
  ASSERT_EQ((uint64_t)kBufSize, length);

  char buf2[kBufSize];
  uint32_t count;
  ASSERT_EQ(NS_OK, bis->Read(buf2, sizeof(buf2), &count));
  ASSERT_EQ(count, buf.Length());
  ASSERT_TRUE(nsCString(buf.get(), kBufSize).Equals(nsCString(buf2, count)));
}

// Simple segment reading.
TEST(TestBufferedInputStream, SimpleReadSegments)
{
  const size_t kBufSize = 10;

  nsCString buf;
  RefPtr<nsBufferedInputStream> bis = CreateStream(kBufSize, buf);

  char buf2[kBufSize];
  uint32_t count;
  ASSERT_EQ(NS_OK, bis->ReadSegments(NS_CopySegmentToBuffer, buf2, sizeof(buf2),
                                     &count));
  ASSERT_EQ(count, buf.Length());
  ASSERT_TRUE(nsCString(buf.get(), kBufSize).Equals(nsCString(buf2, count)));
}

// AsyncWait - sync
TEST(TestBufferedInputStream, AsyncWait_sync)
{
  const size_t kBufSize = 10;

  nsCString buf;
  RefPtr<nsBufferedInputStream> bis = CreateStream(kBufSize, buf);

  RefPtr<testing::InputStreamCallback> cb = new testing::InputStreamCallback();

  ASSERT_EQ(NS_OK, bis->AsyncWait(cb, 0, 0, nullptr));

  // Immediatelly called
  ASSERT_TRUE(cb->Called());
}

// AsyncWait - async
TEST(TestBufferedInputStream, AsyncWait_async)
{
  const size_t kBufSize = 10;

  nsCString buf;
  RefPtr<nsBufferedInputStream> bis = CreateStream(kBufSize, buf);

  RefPtr<testing::InputStreamCallback> cb = new testing::InputStreamCallback();
  nsCOMPtr<nsIThread> thread = do_GetCurrentThread();

  ASSERT_EQ(NS_OK, bis->AsyncWait(cb, 0, 0, thread));

  ASSERT_FALSE(cb->Called());

  // Eventually it is called.
  MOZ_ALWAYS_TRUE(mozilla::SpinEventLoopUntil(
      "TEST(TestBufferedInputStream, AsyncWait_async)"_ns,
      [&]() { return cb->Called(); }));
  ASSERT_TRUE(cb->Called());
}

// AsyncWait - sync - closureOnly
TEST(TestBufferedInputStream, AsyncWait_sync_closureOnly)
{
  const size_t kBufSize = 10;

  nsCString buf;
  RefPtr<nsBufferedInputStream> bis = CreateStream(kBufSize, buf);

  RefPtr<testing::InputStreamCallback> cb = new testing::InputStreamCallback();

  ASSERT_EQ(NS_OK, bis->AsyncWait(cb, nsIAsyncInputStream::WAIT_CLOSURE_ONLY, 0,
                                  nullptr));
  ASSERT_FALSE(cb->Called());

  bis->CloseWithStatus(NS_ERROR_FAILURE);

  // Immediatelly called
  ASSERT_TRUE(cb->Called());
}

// AsyncWait - async
TEST(TestBufferedInputStream, AsyncWait_async_closureOnly)
{
  const size_t kBufSize = 10;

  nsCString buf;
  RefPtr<nsBufferedInputStream> bis = CreateStream(kBufSize, buf);

  RefPtr<testing::InputStreamCallback> cb = new testing::InputStreamCallback();
  nsCOMPtr<nsIThread> thread = do_GetCurrentThread();

  ASSERT_EQ(NS_OK, bis->AsyncWait(cb, nsIAsyncInputStream::WAIT_CLOSURE_ONLY, 0,
                                  thread));

  ASSERT_FALSE(cb->Called());
  bis->CloseWithStatus(NS_ERROR_FAILURE);
  ASSERT_FALSE(cb->Called());

  // Eventually it is called.
  MOZ_ALWAYS_TRUE(mozilla::SpinEventLoopUntil(
      "TEST(TestBufferedInputStream, AsyncWait_async_closureOnly)"_ns,
      [&]() { return cb->Called(); }));
  ASSERT_TRUE(cb->Called());
}

TEST(TestBufferedInputStream, AsyncWait_after_close)
{
  const size_t kBufSize = 10;

  nsCString buf;
  RefPtr<nsBufferedInputStream> bis = CreateStream(kBufSize, buf);

  nsCOMPtr<nsIThread> eventTarget = do_GetCurrentThread();

  auto cb = mozilla::MakeRefPtr<testing::InputStreamCallback>();
  ASSERT_EQ(NS_OK, bis->AsyncWait(cb, 0, 0, eventTarget));
  MOZ_ALWAYS_TRUE(mozilla::SpinEventLoopUntil(
      "TEST(TestBufferedInputStream, AsyncWait_after_close) 1"_ns,
      [&]() { return cb->Called(); }));
  ASSERT_TRUE(cb->Called());

  ASSERT_EQ(NS_OK, bis->Close());

  cb = mozilla::MakeRefPtr<testing::InputStreamCallback>();
  ASSERT_EQ(NS_OK, bis->AsyncWait(cb, 0, 0, eventTarget));
  MOZ_ALWAYS_TRUE(mozilla::SpinEventLoopUntil(
      "TEST(TestBufferedInputStream, AsyncWait_after_close) 2"_ns,
      [&]() { return cb->Called(); }));
  ASSERT_TRUE(cb->Called());
}

TEST(TestBufferedInputStream, AsyncLengthWait_after_close)
{
  nsCString buf{"The Quick Brown Fox Jumps over the Lazy Dog"};
  const size_t kBufSize = 44;

  RefPtr<nsBufferedInputStream> bis = CreateStream(kBufSize, buf);

  nsCOMPtr<nsIThread> eventTarget = do_GetCurrentThread();

  auto cb = mozilla::MakeRefPtr<testing::LengthCallback>();
  ASSERT_EQ(NS_OK, bis->AsyncLengthWait(cb, eventTarget));
  MOZ_ALWAYS_TRUE(mozilla::SpinEventLoopUntil(
      "TEST(TestBufferedInputStream, AsyncLengthWait_after_close) 1"_ns,
      [&]() { return cb->Called(); }));
  ASSERT_TRUE(cb->Called());

  uint64_t length;
  ASSERT_EQ(NS_OK, bis->Available(&length));
  ASSERT_EQ((uint64_t)kBufSize, length);

  cb = mozilla::MakeRefPtr<testing::LengthCallback>();
  ASSERT_EQ(NS_OK, bis->AsyncLengthWait(cb, eventTarget));
  MOZ_ALWAYS_TRUE(mozilla::SpinEventLoopUntil(
      "TEST(TestBufferedInputStream, AsyncLengthWait_after_close) 2"_ns,
      [&]() { return cb->Called(); }));
  ASSERT_TRUE(cb->Called());
}

// This stream returns a few bytes on the first read, and error on the second.
class BrokenInputStream : public nsIInputStream {
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIINPUTSTREAM
 private:
  virtual ~BrokenInputStream() = default;
  bool mFirst = true;
};

NS_IMPL_ISUPPORTS(BrokenInputStream, nsIInputStream)

NS_IMETHODIMP BrokenInputStream::Close(void) { return NS_OK; }

NS_IMETHODIMP BrokenInputStream::Available(uint64_t* _retval) {
  *_retval = 100;
  return NS_OK;
}

NS_IMETHODIMP BrokenInputStream::StreamStatus(void) { return NS_OK; }

NS_IMETHODIMP BrokenInputStream::Read(char* aBuf, uint32_t aCount,
                                      uint32_t* _retval) {
  if (mFirst) {
    aBuf[0] = 'h';
    aBuf[1] = 'e';
    aBuf[2] = 'l';
    aBuf[3] = 0;
    *_retval = 4;
    mFirst = false;
    return NS_OK;
  }
  return NS_ERROR_CORRUPTED_CONTENT;
}

NS_IMETHODIMP BrokenInputStream::ReadSegments(nsWriteSegmentFun aWriter,
                                              void* aClosure, uint32_t aCount,
                                              uint32_t* _retval) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP BrokenInputStream::IsNonBlocking(bool* _retval) {
  *_retval = false;
  return NS_OK;
}

// Check that the error from BrokenInputStream::Read is propagated
// through NS_ReadInputStreamToString
TEST(TestBufferedInputStream, BrokenInputStreamToBuffer)
{
  nsAutoCString out;
  RefPtr<BrokenInputStream> stream = new BrokenInputStream();

  nsresult rv = NS_ReadInputStreamToString(stream, out, -1);
  ASSERT_EQ(rv, NS_ERROR_CORRUPTED_CONTENT);
}
