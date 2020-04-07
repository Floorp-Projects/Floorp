#include "gtest/gtest.h"

#include "mozilla/NonBlockingAsyncInputStream.h"
#include "nsIAsyncInputStream.h"
#include "nsStreamUtils.h"
#include "nsString.h"
#include "nsStringStream.h"
#include "Helpers.h"

using mozilla::NonBlockingAsyncInputStream;
using mozilla::SpinEventLoopUntil;

TEST(TestNonBlockingAsyncInputStream, Simple)
{
  nsCString data;
  data.Assign("Hello world!");

  // It should not be async.
  bool nonBlocking = false;
  nsCOMPtr<nsIAsyncInputStream> async;

  {
    // Let's create a test string inputStream
    nsCOMPtr<nsIInputStream> stream;
    ASSERT_EQ(NS_OK, NS_NewCStringInputStream(getter_AddRefs(stream), data));

    async = do_QueryInterface(stream);
    ASSERT_EQ(nullptr, async);

    // It must be non-blocking
    ASSERT_EQ(NS_OK, stream->IsNonBlocking(&nonBlocking));
    ASSERT_TRUE(nonBlocking);

    // Here the non-blocking stream.
    ASSERT_EQ(NS_OK, NonBlockingAsyncInputStream::Create(
                         stream.forget(), getter_AddRefs(async)));
  }
  ASSERT_TRUE(!!async);

  // Still non-blocking
  ASSERT_EQ(NS_OK, async->IsNonBlocking(&nonBlocking));
  ASSERT_TRUE(nonBlocking);

  // Testing ::Available()
  uint64_t length;
  ASSERT_EQ(NS_OK, async->Available(&length));
  ASSERT_EQ(data.Length(), length);

  // Read works fine.
  char buffer[1024];
  uint32_t read = 0;
  ASSERT_EQ(NS_OK, async->Read(buffer, sizeof(buffer), &read));
  ASSERT_EQ(data.Length(), read);
  ASSERT_TRUE(data.Equals(nsCString(buffer, read)));
}

class ReadSegmentsData {
 public:
  ReadSegmentsData(nsIInputStream* aStream, char* aBuffer)
      : mStream(aStream), mBuffer(aBuffer) {}

  nsIInputStream* mStream;
  char* mBuffer;
};

static nsresult ReadSegmentsFunction(nsIInputStream* aInStr, void* aClosure,
                                     const char* aBuffer, uint32_t aOffset,
                                     uint32_t aCount, uint32_t* aCountWritten) {
  ReadSegmentsData* data = static_cast<ReadSegmentsData*>(aClosure);
  if (aInStr != data->mStream) return NS_ERROR_FAILURE;
  memcpy(&data->mBuffer[aOffset], aBuffer, aCount);
  *aCountWritten = aCount;
  return NS_OK;
}

TEST(TestNonBlockingAsyncInputStream, ReadSegments)
{
  nsCString data;
  data.Assign("Hello world!");

  nsCOMPtr<nsIAsyncInputStream> async;
  {
    // Let's create a test string inputStream
    nsCOMPtr<nsIInputStream> stream;
    ASSERT_EQ(NS_OK, NS_NewCStringInputStream(getter_AddRefs(stream), data));

    // Here the non-blocking stream.
    ASSERT_EQ(NS_OK, NonBlockingAsyncInputStream::Create(
                         stream.forget(), getter_AddRefs(async)));
  }

  // Read works fine.
  char buffer[1024];
  uint32_t read = 0;
  ReadSegmentsData closure(async, buffer);
  ASSERT_EQ(NS_OK, async->ReadSegments(ReadSegmentsFunction, &closure,
                                       sizeof(buffer), &read));
  ASSERT_EQ(data.Length(), read);
  ASSERT_TRUE(data.Equals(nsCString(buffer, read)));
}

TEST(TestNonBlockingAsyncInputStream, AsyncWait_Simple)
{
  nsCString data;
  data.Assign("Hello world!");

  nsCOMPtr<nsIAsyncInputStream> async;
  {
    // Let's create a test string inputStream
    nsCOMPtr<nsIInputStream> stream;
    ASSERT_EQ(NS_OK, NS_NewCStringInputStream(getter_AddRefs(stream), data));

    // Here the non-blocking stream.
    ASSERT_EQ(NS_OK, NonBlockingAsyncInputStream::Create(
                         stream.forget(), getter_AddRefs(async)));
  }
  ASSERT_TRUE(!!async);

  // Testing ::Available()
  uint64_t length;
  ASSERT_EQ(NS_OK, async->Available(&length));
  ASSERT_EQ(data.Length(), length);

  // Testing ::AsyncWait - without EventTarget
  RefPtr<testing::InputStreamCallback> cb = new testing::InputStreamCallback();

  ASSERT_EQ(NS_OK, async->AsyncWait(cb, 0, 0, nullptr));
  ASSERT_TRUE(cb->Called());

  // Testing ::AsyncWait - with EventTarget
  cb = new testing::InputStreamCallback();
  nsCOMPtr<nsIThread> thread = do_GetCurrentThread();

  ASSERT_EQ(NS_OK, async->AsyncWait(cb, 0, 0, thread));
  ASSERT_FALSE(cb->Called());
  MOZ_ALWAYS_TRUE(SpinEventLoopUntil([&]() { return cb->Called(); }));
  ASSERT_TRUE(cb->Called());

  // Read works fine.
  char buffer[1024];
  uint32_t read = 0;
  ASSERT_EQ(NS_OK, async->Read(buffer, sizeof(buffer), &read));
  ASSERT_EQ(data.Length(), read);
  ASSERT_TRUE(data.Equals(nsCString(buffer, read)));
}

TEST(TestNonBlockingAsyncInputStream, AsyncWait_ClosureOnly_withoutEventTarget)
{
  nsCString data;
  data.Assign("Hello world!");

  nsCOMPtr<nsIAsyncInputStream> async;
  {
    // Let's create a test string inputStream
    nsCOMPtr<nsIInputStream> stream;
    ASSERT_EQ(NS_OK, NS_NewCStringInputStream(getter_AddRefs(stream), data));

    // Here the non-blocking stream.
    ASSERT_EQ(NS_OK, NonBlockingAsyncInputStream::Create(
                         stream.forget(), getter_AddRefs(async)));
  }
  ASSERT_TRUE(!!async);

  // Testing ::AsyncWait - no eventTarget
  RefPtr<testing::InputStreamCallback> cb = new testing::InputStreamCallback();

  ASSERT_EQ(NS_OK, async->AsyncWait(cb, nsIAsyncInputStream::WAIT_CLOSURE_ONLY,
                                    0, nullptr));

  ASSERT_FALSE(cb->Called());
  ASSERT_EQ(NS_OK, async->Close());
  ASSERT_TRUE(cb->Called());
}

TEST(TestNonBlockingAsyncInputStream, AsyncWait_ClosureOnly_withEventTarget)
{
  nsCString data;
  data.Assign("Hello world!");

  nsCOMPtr<nsIAsyncInputStream> async;
  {
    // Let's create a test string inputStream
    nsCOMPtr<nsIInputStream> stream;
    ASSERT_EQ(NS_OK, NS_NewCStringInputStream(getter_AddRefs(stream), data));

    // Here the non-blocking stream.
    ASSERT_EQ(NS_OK, NonBlockingAsyncInputStream::Create(
                         stream.forget(), getter_AddRefs(async)));
  }
  ASSERT_TRUE(!!async);

  // Testing ::AsyncWait - with EventTarget
  RefPtr<testing::InputStreamCallback> cb = new testing::InputStreamCallback();
  nsCOMPtr<nsIThread> thread = do_GetCurrentThread();

  ASSERT_EQ(NS_OK, async->AsyncWait(cb, nsIAsyncInputStream::WAIT_CLOSURE_ONLY,
                                    0, thread));

  ASSERT_FALSE(cb->Called());
  ASSERT_EQ(NS_OK, async->Close());
  ASSERT_FALSE(cb->Called());

  MOZ_ALWAYS_TRUE(SpinEventLoopUntil([&]() { return cb->Called(); }));
  ASSERT_TRUE(cb->Called());
}

TEST(TestNonBlockingAsyncInputStream, Helper)
{
  nsCString data;
  data.Assign("Hello world!");

  nsCOMPtr<nsIAsyncInputStream> async;
  {
    // Let's create a test string inputStream
    nsCOMPtr<nsIInputStream> stream;
    ASSERT_EQ(NS_OK, NS_NewCStringInputStream(getter_AddRefs(stream), data));

    // Here the non-blocking stream.
    ASSERT_EQ(NS_OK, NonBlockingAsyncInputStream::Create(
                         stream.forget(), getter_AddRefs(async)));
  }
  ASSERT_TRUE(!!async);

  // This should return the same object because async is already non-blocking
  // and async.
  nsCOMPtr<nsIAsyncInputStream> result;
  nsCOMPtr<nsIAsyncInputStream> asyncTmp = async;
  ASSERT_EQ(NS_OK, NS_MakeAsyncNonBlockingInputStream(asyncTmp.forget(),
                                                      getter_AddRefs(result)));
  ASSERT_EQ(async, result);

  // This will use NonBlockingAsyncInputStream wrapper.
  {
    nsCOMPtr<nsIInputStream> stream;
    ASSERT_EQ(NS_OK, NS_NewCStringInputStream(getter_AddRefs(stream), data));
    ASSERT_EQ(NS_OK, NS_MakeAsyncNonBlockingInputStream(
                         stream.forget(), getter_AddRefs(result)));
  }
  ASSERT_TRUE(async != result);
  ASSERT_TRUE(async);
}

class QIInputStream final : public nsIInputStream,
                            public nsICloneableInputStream,
                            public nsIIPCSerializableInputStream,
                            public nsISeekableStream {
 public:
  NS_DECL_ISUPPORTS

  QIInputStream(bool aNonBlockingError, bool aCloneable, bool aIPCSerializable,
                bool aSeekable)
      : mNonBlockingError(aNonBlockingError),
        mCloneable(aCloneable),
        mIPCSerializable(aIPCSerializable),
        mSeekable(aSeekable) {}

  // nsIInputStream
  NS_IMETHOD Close() override { return NS_ERROR_NOT_IMPLEMENTED; }
  NS_IMETHOD Available(uint64_t*) override { return NS_ERROR_NOT_IMPLEMENTED; }
  NS_IMETHOD Read(char*, uint32_t, uint32_t*) override {
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  NS_IMETHOD ReadSegments(nsWriteSegmentFun, void*, uint32_t,
                          uint32_t*) override {
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  NS_IMETHOD IsNonBlocking(bool* aNonBlocking) override {
    *aNonBlocking = true;
    return mNonBlockingError ? NS_ERROR_FAILURE : NS_OK;
  }

  // nsICloneableInputStream
  NS_IMETHOD GetCloneable(bool*) override { return NS_ERROR_NOT_IMPLEMENTED; }
  NS_IMETHOD Clone(nsIInputStream**) override {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  // nsIIPCSerializableInputStream
  void Serialize(mozilla::ipc::InputStreamParams&, FileDescriptorArray&, bool,
                 uint32_t, uint32_t*,
                 mozilla::ipc::ParentToChildStreamActorManager*) override {}
  void Serialize(mozilla::ipc::InputStreamParams&, FileDescriptorArray&, bool,
                 uint32_t, uint32_t*,
                 mozilla::ipc::ChildToParentStreamActorManager*) override {}
  bool Deserialize(const mozilla::ipc::InputStreamParams&,
                   const FileDescriptorArray&) override {
    return false;
  }

  // nsISeekableStream
  NS_IMETHOD Seek(int32_t, int64_t) override {
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  NS_IMETHOD SetEOF() override { return NS_ERROR_NOT_IMPLEMENTED; }

  // nsITellableStream
  NS_IMETHOD Tell(int64_t*) override { return NS_ERROR_NOT_IMPLEMENTED; }

 private:
  ~QIInputStream() = default;

  bool mNonBlockingError;
  bool mCloneable;
  bool mIPCSerializable;
  bool mSeekable;
};

NS_IMPL_ADDREF(QIInputStream);
NS_IMPL_RELEASE(QIInputStream);

NS_INTERFACE_MAP_BEGIN(QIInputStream)
  NS_INTERFACE_MAP_ENTRY(nsIInputStream)
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsICloneableInputStream, mCloneable)
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsIIPCSerializableInputStream,
                                     mIPCSerializable)
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsISeekableStream, mSeekable)
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsITellableStream, mSeekable)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIInputStream)
NS_INTERFACE_MAP_END

TEST(TestNonBlockingAsyncInputStream, QI)
{
  // Let's test ::Create() returning error.

  nsCOMPtr<nsIAsyncInputStream> async;
  {
    nsCOMPtr<nsIInputStream> stream = new QIInputStream(true, true, true, true);

    ASSERT_EQ(NS_ERROR_FAILURE, NonBlockingAsyncInputStream::Create(
                                    stream.forget(), getter_AddRefs(async)));
  }

  // Let's test the QIs
  for (int i = 0; i < 8; ++i) {
    bool shouldBeCloneable = !!(i & 0x01);
    bool shouldBeSerializable = !!(i & 0x02);
    bool shouldBeSeekable = !!(i & 0x04);

    nsCOMPtr<nsICloneableInputStream> cloneable;
    nsCOMPtr<nsIIPCSerializableInputStream> ipcSerializable;
    nsCOMPtr<nsISeekableStream> seekable;

    {
      nsCOMPtr<nsIInputStream> stream = new QIInputStream(
          false, shouldBeCloneable, shouldBeSerializable, shouldBeSeekable);

      cloneable = do_QueryInterface(stream);
      ASSERT_EQ(shouldBeCloneable, !!cloneable);

      ipcSerializable = do_QueryInterface(stream);
      ASSERT_EQ(shouldBeSerializable, !!ipcSerializable);

      seekable = do_QueryInterface(stream);
      ASSERT_EQ(shouldBeSeekable, !!seekable);

      ASSERT_EQ(NS_OK, NonBlockingAsyncInputStream::Create(
                           stream.forget(), getter_AddRefs(async)));
    }

    // The returned async stream should be cloneable only if the underlying
    // stream is.
    cloneable = do_QueryInterface(async);
    ASSERT_EQ(shouldBeCloneable, !!cloneable);

    // The returned async stream should be serializable only if the underlying
    // stream is.
    ipcSerializable = do_QueryInterface(async);
    ASSERT_EQ(shouldBeSerializable, !!ipcSerializable);

    // The returned async stream should be seekable only if the underlying
    // stream is.
    seekable = do_QueryInterface(async);
    ASSERT_EQ(shouldBeSeekable, !!seekable);
  }
}
