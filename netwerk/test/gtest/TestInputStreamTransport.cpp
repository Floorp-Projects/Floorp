#include "gtest/gtest.h"

#include "nsIStreamTransportService.h"
#include "nsStreamUtils.h"
#include "nsThreadUtils.h"
#include "Helpers.h"
#include "nsNetCID.h"
#include "nsServiceManagerUtils.h"
#include "nsITransport.h"
#include "nsNetUtil.h"

static NS_DEFINE_CID(kStreamTransportServiceCID, NS_STREAMTRANSPORTSERVICE_CID);

void CreateStream(already_AddRefed<nsIInputStream> aSource,
                  nsIAsyncInputStream** aStream) {
  nsCOMPtr<nsIInputStream> source = std::move(aSource);

  nsresult rv;
  nsCOMPtr<nsIStreamTransportService> sts =
      do_GetService(kStreamTransportServiceCID, &rv);
  ASSERT_EQ(NS_OK, rv);

  nsCOMPtr<nsITransport> transport;
  rv = sts->CreateInputTransport(source, true, getter_AddRefs(transport));
  ASSERT_EQ(NS_OK, rv);

  nsCOMPtr<nsIInputStream> wrapper;
  rv = transport->OpenInputStream(0, 0, 0, getter_AddRefs(wrapper));
  ASSERT_EQ(NS_OK, rv);

  nsCOMPtr<nsIAsyncInputStream> asyncStream = do_QueryInterface(wrapper);
  MOZ_ASSERT(asyncStream);

  asyncStream.forget(aStream);
}

class BlockingSyncStream final : public nsIInputStream {
  nsCOMPtr<nsIInputStream> mStream;

 public:
  NS_DECL_THREADSAFE_ISUPPORTS

  explicit BlockingSyncStream(const nsACString& aBuffer) {
    NS_NewCStringInputStream(getter_AddRefs(mStream), aBuffer);
  }

  NS_IMETHOD
  Available(uint64_t* aLength) override { return mStream->Available(aLength); }

  NS_IMETHOD
  StreamStatus() override { return mStream->StreamStatus(); }

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
    *aNonBlocking = false;
    return NS_OK;
  }

 private:
  ~BlockingSyncStream() = default;
};

NS_IMPL_ISUPPORTS(BlockingSyncStream, nsIInputStream)

// Testing a simple blocking stream.
TEST(TestInputStreamTransport, BlockingNotAsync)
{
  RefPtr<BlockingSyncStream> stream = new BlockingSyncStream("Hello world"_ns);

  nsCOMPtr<nsIAsyncInputStream> ais;
  CreateStream(stream.forget(), getter_AddRefs(ais));
  ASSERT_TRUE(!!ais);

  nsAutoCString data;
  nsresult rv = NS_ReadInputStreamToString(ais, data, -1);
  ASSERT_EQ(NS_OK, rv);

  ASSERT_TRUE(data.EqualsLiteral("Hello world"));
}

class BlockingAsyncStream final : public nsIAsyncInputStream {
  nsCOMPtr<nsIInputStream> mStream;
  bool mPending;

 public:
  NS_DECL_THREADSAFE_ISUPPORTS

  explicit BlockingAsyncStream(const nsACString& aBuffer) : mPending(false) {
    NS_NewCStringInputStream(getter_AddRefs(mStream), aBuffer);
  }

  NS_IMETHOD
  Available(uint64_t* aLength) override {
    mStream->Available(aLength);

    // 1 char at the time, just to test the asyncWait+Read loop a bit more.
    if (*aLength > 0) {
      *aLength = 1;
    }

    return NS_OK;
  }

  NS_IMETHOD
  StreamStatus() override { return mStream->StreamStatus(); }

  NS_IMETHOD
  Read(char* aBuffer, uint32_t aCount, uint32_t* aReadCount) override {
    mPending = !mPending;
    if (mPending) {
      return NS_BASE_STREAM_WOULD_BLOCK;
    }

    // 1 char at the time, just to test the asyncWait+Read loop a bit more.
    aCount = 1;

    return mStream->Read(aBuffer, aCount, aReadCount);
  }

  NS_IMETHOD
  ReadSegments(nsWriteSegmentFun aWriter, void* aClosure, uint32_t aCount,
               uint32_t* aResult) override {
    mPending = !mPending;
    if (mPending) {
      return NS_BASE_STREAM_WOULD_BLOCK;
    }

    // 1 char at the time, just to test the asyncWait+Read loop a bit more.
    aCount = 1;

    return mStream->ReadSegments(aWriter, aClosure, aCount, aResult);
  }

  NS_IMETHOD
  Close() override { return mStream->Close(); }

  NS_IMETHOD
  IsNonBlocking(bool* aNonBlocking) override {
    *aNonBlocking = false;
    return NS_OK;
  }

  NS_IMETHOD
  CloseWithStatus(nsresult aStatus) override { return Close(); }

  NS_IMETHOD
  AsyncWait(nsIInputStreamCallback* aCallback, uint32_t aFlags,
            uint32_t aRequestedCount, nsIEventTarget* aEventTarget) override {
    if (!aCallback) {
      return NS_OK;
    }

    RefPtr<BlockingAsyncStream> self = this;
    nsCOMPtr<nsIInputStreamCallback> callback = aCallback;

    nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction(
        "gtest-asyncwait",
        [self, callback]() { callback->OnInputStreamReady(self); });

    if (aEventTarget) {
      aEventTarget->Dispatch(r.forget());
    } else {
      r->Run();
    }

    return NS_OK;
  }

 private:
  ~BlockingAsyncStream() = default;
};

NS_IMPL_ISUPPORTS(BlockingAsyncStream, nsIInputStream, nsIAsyncInputStream)

// Testing an async blocking stream.
TEST(TestInputStreamTransport, BlockingAsync)
{
  RefPtr<BlockingAsyncStream> stream =
      new BlockingAsyncStream("Hello world"_ns);

  nsCOMPtr<nsIAsyncInputStream> ais;
  CreateStream(stream.forget(), getter_AddRefs(ais));
  ASSERT_TRUE(!!ais);

  nsAutoCString data;
  nsresult rv = NS_ReadInputStreamToString(ais, data, -1);
  ASSERT_EQ(NS_OK, rv);

  ASSERT_TRUE(data.EqualsLiteral("Hello world"));
}
