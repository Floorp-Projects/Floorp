#include "gtest/gtest.h"

#include "nsBufferedStreams.h"
#include "nsCOMPtr.h"
#include "nsStreamUtils.h"
#include "nsString.h"
#include "nsStringStream.h"
#include "nsThreadUtils.h"
#include "Helpers.h"

class AsyncStringStream final : public nsIAsyncInputStream
{
  nsCOMPtr<nsIInputStream> mStream;

public:
  NS_DECL_THREADSAFE_ISUPPORTS

  explicit AsyncStringStream(const nsACString& aBuffer)
  {
    NS_NewCStringInputStream(getter_AddRefs(mStream), aBuffer);
  }

  NS_IMETHOD
  Available(uint64_t* aLength) override
  {
    return mStream->Available(aLength);
  }

  NS_IMETHOD
  Read(char* aBuffer, uint32_t aCount, uint32_t* aReadCount) override
  {
    return mStream->Read(aBuffer, aCount, aReadCount);
  }

  NS_IMETHOD
  ReadSegments(nsWriteSegmentFun aWriter, void* aClosure,
               uint32_t aCount, uint32_t *aResult) override
  {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  NS_IMETHOD
  Close() override
  {
    nsresult rv = mStream->Close();
    if (NS_SUCCEEDED(rv)) {
      MaybeExecCallback(mCallback, mCallbackEventTarget);
    }
    return rv;
  }

  NS_IMETHOD
  IsNonBlocking(bool* aNonBlocking) override
  {
    return mStream->IsNonBlocking(aNonBlocking);
  }

  NS_IMETHOD
  CloseWithStatus(nsresult aStatus) override
  {
    return Close();
  }

  NS_IMETHOD
  AsyncWait(nsIInputStreamCallback* aCallback,
            uint32_t aFlags, uint32_t aRequestedCount,
            nsIEventTarget* aEventTarget) override
  {
    if (aFlags & nsIAsyncInputStream::WAIT_CLOSURE_ONLY) {
      mCallback = aCallback;
      mCallbackEventTarget = aEventTarget;
      return NS_OK;
    }

    MaybeExecCallback(aCallback, aEventTarget);
    return NS_OK;
  }

  void
  MaybeExecCallback(nsIInputStreamCallback* aCallback,
                    nsIEventTarget* aEventTarget)
  {
    if (!aCallback) {
      return;
    }

    nsCOMPtr<nsIInputStreamCallback> callback = aCallback;
    nsCOMPtr<nsIAsyncInputStream> self = this;

    nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction(
      "AsyncWait", [callback, self]() { callback->OnInputStreamReady(self); });

    if (aEventTarget) {
      aEventTarget->Dispatch(r.forget());
    } else {
      r->Run();
    }
  }

private:
  ~AsyncStringStream() = default;

  nsCOMPtr<nsIInputStreamCallback> mCallback;
  nsCOMPtr<nsIEventTarget> mCallbackEventTarget;
};

NS_IMPL_ISUPPORTS(AsyncStringStream, nsIAsyncInputStream, nsIInputStream)

// Helper function for creating a AsyncStringStream
already_AddRefed<nsBufferedInputStream>
CreateStream(uint32_t aSize, nsCString& aBuffer)
{
  aBuffer.SetLength(aSize);
  for (uint32_t i = 0; i < aSize; ++i) {
    aBuffer.BeginWriting()[i] = i % 10;
  }

  nsCOMPtr<nsIInputStream> stream = new AsyncStringStream(aBuffer);

  RefPtr<nsBufferedInputStream> bis = new nsBufferedInputStream();
  bis->Init(stream, aSize);
  return bis.forget();
}

// Simple reading.
TEST(TestBufferedInputStream, SimpleRead) {
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
TEST(TestBufferedInputStream, SimpleReadSegments) {
  const size_t kBufSize = 10;

  nsCString buf;
  RefPtr<nsBufferedInputStream> bis = CreateStream(kBufSize, buf);

  char buf2[kBufSize];
  uint32_t count;
  ASSERT_EQ(NS_OK, bis->ReadSegments(NS_CopySegmentToBuffer, buf2, sizeof(buf2), &count));
  ASSERT_EQ(count, buf.Length());
  ASSERT_TRUE(nsCString(buf.get(), kBufSize).Equals(nsCString(buf2, count)));
}

// AsyncWait - sync
TEST(TestBufferedInputStream, AsyncWait_sync) {
  const size_t kBufSize = 10;

  nsCString buf;
  RefPtr<nsBufferedInputStream> bis = CreateStream(kBufSize, buf);

  RefPtr<testing::InputStreamCallback> cb =
    new testing::InputStreamCallback();

  ASSERT_EQ(NS_OK, bis->AsyncWait(cb, 0, 0, nullptr));

  // Immediatelly called
  ASSERT_TRUE(cb->Called());
}

// AsyncWait - async
TEST(TestBufferedInputStream, AsyncWait_async) {
  const size_t kBufSize = 10;

  nsCString buf;
  RefPtr<nsBufferedInputStream> bis = CreateStream(kBufSize, buf);

  RefPtr<testing::InputStreamCallback> cb =
    new testing::InputStreamCallback();
  nsCOMPtr<nsIThread> thread = do_GetCurrentThread();

  ASSERT_EQ(NS_OK, bis->AsyncWait(cb, 0, 0, thread));

  ASSERT_FALSE(cb->Called());

  // Eventually it is called.
  MOZ_ALWAYS_TRUE(mozilla::SpinEventLoopUntil([&]() { return cb->Called(); }));
  ASSERT_TRUE(cb->Called());
}

// AsyncWait - sync - closureOnly
TEST(TestBufferedInputStream, AsyncWait_sync_closureOnly) {
  const size_t kBufSize = 10;

  nsCString buf;
  RefPtr<nsBufferedInputStream> bis = CreateStream(kBufSize, buf);

  RefPtr<testing::InputStreamCallback> cb =
    new testing::InputStreamCallback();

  ASSERT_EQ(NS_OK, bis->AsyncWait(cb, nsIAsyncInputStream::WAIT_CLOSURE_ONLY,
                                  0, nullptr));
  ASSERT_FALSE(cb->Called());

  bis->CloseWithStatus(NS_ERROR_FAILURE);

  // Immediatelly called
  ASSERT_TRUE(cb->Called());
}

// AsyncWait - async
TEST(TestBufferedInputStream, AsyncWait_async_closureOnly) {
  const size_t kBufSize = 10;

  nsCString buf;
  RefPtr<nsBufferedInputStream> bis = CreateStream(kBufSize, buf);

  RefPtr<testing::InputStreamCallback> cb =
    new testing::InputStreamCallback();
  nsCOMPtr<nsIThread> thread = do_GetCurrentThread();

  ASSERT_EQ(NS_OK, bis->AsyncWait(cb, nsIAsyncInputStream::WAIT_CLOSURE_ONLY,
                                  0, thread));

  ASSERT_FALSE(cb->Called());
  bis->CloseWithStatus(NS_ERROR_FAILURE);
  ASSERT_FALSE(cb->Called());

  // Eventually it is called.
  MOZ_ALWAYS_TRUE(mozilla::SpinEventLoopUntil([&]() { return cb->Called(); }));
  ASSERT_TRUE(cb->Called());
}
