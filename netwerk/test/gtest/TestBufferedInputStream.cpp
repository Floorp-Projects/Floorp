#include "gtest/gtest.h"

#include "nsBufferedStreams.h"
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
  MOZ_ALWAYS_TRUE(mozilla::SpinEventLoopUntil([&]() { return cb->Called(); }));
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
  MOZ_ALWAYS_TRUE(mozilla::SpinEventLoopUntil([&]() { return cb->Called(); }));
  ASSERT_TRUE(cb->Called());
}
