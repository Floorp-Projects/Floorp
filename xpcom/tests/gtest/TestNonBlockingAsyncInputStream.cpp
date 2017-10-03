#include "gtest/gtest.h"

#include "mozilla/NonBlockingAsyncInputStream.h"
#include "nsIAsyncInputStream.h"
#include "nsStreamUtils.h"
#include "nsString.h"
#include "nsStringStream.h"
#include "Helpers.h"

TEST(TestNonBlockingAsyncInputStream, Simple) {
  nsCOMPtr<nsIInputStream> stream;

  nsCString data;
  data.Assign("Hello world!");

  // Let's create a test string inputStream
  ASSERT_EQ(NS_OK, NS_NewCStringInputStream(getter_AddRefs(stream), data));

  // It should not be async.
  nsCOMPtr<nsIAsyncInputStream> async = do_QueryInterface(stream);
  ASSERT_EQ(nullptr, async);

  // It must be non-blocking
  bool nonBlocking = false;
  ASSERT_EQ(NS_OK, stream->IsNonBlocking(&nonBlocking));
  ASSERT_TRUE(nonBlocking);

  // Here the non-blocking stream.
  ASSERT_EQ(NS_OK,
            NonBlockingAsyncInputStream::Create(stream, getter_AddRefs(async)));
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

TEST(TestNonBlockingAsyncInputStream, ReadSegments) {
  nsCOMPtr<nsIInputStream> stream;

  nsCString data;
  data.Assign("Hello world!");

  // Let's create a test string inputStream
  ASSERT_EQ(NS_OK, NS_NewCStringInputStream(getter_AddRefs(stream), data));

  // Here the non-blocking stream.
  nsCOMPtr<nsIAsyncInputStream> async;
  ASSERT_EQ(NS_OK,
            NonBlockingAsyncInputStream::Create(stream, getter_AddRefs(async)));

  // Read works fine.
  char buffer[1024];
  uint32_t read = 0;
  ASSERT_EQ(NS_OK, async->ReadSegments(NS_CopySegmentToBuffer, buffer,
                                       sizeof(buffer), &read));
  ASSERT_EQ(data.Length(), read);
  ASSERT_TRUE(data.Equals(nsCString(buffer, read)));
}

TEST(TestNonBlockingAsyncInputStream, AsyncWait_Simple) {
  nsCOMPtr<nsIInputStream> stream;

  nsCString data;
  data.Assign("Hello world!");

  // Let's create a test string inputStream
  ASSERT_EQ(NS_OK, NS_NewCStringInputStream(getter_AddRefs(stream), data));

  // Here the non-blocking stream.
  nsCOMPtr<nsIAsyncInputStream> async;
  ASSERT_EQ(NS_OK,
            NonBlockingAsyncInputStream::Create(stream, getter_AddRefs(async)));
  ASSERT_TRUE(!!async);

  // Testing ::Available()
  uint64_t length;
  ASSERT_EQ(NS_OK, async->Available(&length));
  ASSERT_EQ(data.Length(), length);

  // Testing ::AsyncWait - without EventTarget
  RefPtr<testing::InputStreamCallback> cb =
    new testing::InputStreamCallback();

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

TEST(TestNonBlockingAsyncInputStream, AsyncWait_ClosureOnly_withoutEventTarget) {
  nsCOMPtr<nsIInputStream> stream;

  nsCString data;
  data.Assign("Hello world!");

  // Let's create a test string inputStream
  ASSERT_EQ(NS_OK, NS_NewCStringInputStream(getter_AddRefs(stream), data));

  // Here the non-blocking stream.
  nsCOMPtr<nsIAsyncInputStream> async;
  ASSERT_EQ(NS_OK,
            NonBlockingAsyncInputStream::Create(stream, getter_AddRefs(async)));
  ASSERT_TRUE(!!async);

  // Testing ::AsyncWait - no eventTarget
  RefPtr<testing::InputStreamCallback> cb =
    new testing::InputStreamCallback();

  ASSERT_EQ(NS_OK, async->AsyncWait(cb, nsIAsyncInputStream::WAIT_CLOSURE_ONLY, 0, nullptr));

  ASSERT_FALSE(cb->Called());
  ASSERT_EQ(NS_OK, async->Close());
  ASSERT_TRUE(cb->Called());
}

TEST(TestNonBlockingAsyncInputStream, AsyncWait_ClosureOnly_withEventTarget) {
  nsCOMPtr<nsIInputStream> stream;

  nsCString data;
  data.Assign("Hello world!");

  // Let's create a test string inputStream
  ASSERT_EQ(NS_OK, NS_NewCStringInputStream(getter_AddRefs(stream), data));

  // Here the non-blocking stream.
  nsCOMPtr<nsIAsyncInputStream> async;
  ASSERT_EQ(NS_OK,
            NonBlockingAsyncInputStream::Create(stream, getter_AddRefs(async)));
  ASSERT_TRUE(!!async);

  // Testing ::AsyncWait - with EventTarget
  RefPtr<testing::InputStreamCallback> cb =
    new testing::InputStreamCallback();
  nsCOMPtr<nsIThread> thread = do_GetCurrentThread();

  ASSERT_EQ(NS_OK, async->AsyncWait(cb, nsIAsyncInputStream::WAIT_CLOSURE_ONLY, 0, thread));

  ASSERT_FALSE(cb->Called());
  ASSERT_EQ(NS_OK, async->Close());
  ASSERT_FALSE(cb->Called());

  MOZ_ALWAYS_TRUE(SpinEventLoopUntil([&]() { return cb->Called(); }));
  ASSERT_TRUE(cb->Called());
}

TEST(TestNonBlockingAsyncInputStream, Helper) {
  nsCOMPtr<nsIInputStream> stream;

  nsCString data;
  data.Assign("Hello world!");

  // Let's create a test string inputStream
  ASSERT_EQ(NS_OK, NS_NewCStringInputStream(getter_AddRefs(stream), data));

  // Here the non-blocking stream.
  nsCOMPtr<nsIAsyncInputStream> async;
  ASSERT_EQ(NS_OK,
            NonBlockingAsyncInputStream::Create(stream, getter_AddRefs(async)));
  ASSERT_TRUE(!!async);

  // This should return the same object because async is already non-blocking
  // and async.
  nsCOMPtr<nsIAsyncInputStream> result;
  ASSERT_EQ(NS_OK,
            NS_MakeAsyncNonBlockingInputStream(async, getter_AddRefs(result)));
  ASSERT_EQ(async, result);

  // This will use NonBlockingAsyncInputStream wrapper.
  ASSERT_EQ(NS_OK,
            NS_MakeAsyncNonBlockingInputStream(stream, getter_AddRefs(result)));
  ASSERT_TRUE(async != result);
  ASSERT_TRUE(async);
}

class QIInputStream final : public nsIInputStream
                          , public nsICloneableInputStream
                          , public nsIIPCSerializableInputStream
                          , public nsISeekableStream
{
public:
  NS_DECL_ISUPPORTS

  QIInputStream(bool aNonBlockingError, bool aCloneable,
                bool aIPCSerializable, bool aSeekable)
    : mNonBlockingError(aNonBlockingError)
    , mCloneable(aCloneable)
    , mIPCSerializable(aIPCSerializable)
    , mSeekable(aSeekable)
  {}

  // nsIInputStream
  NS_IMETHOD Close() override { return NS_ERROR_NOT_IMPLEMENTED; }
  NS_IMETHOD Available(uint64_t*) override { return NS_ERROR_NOT_IMPLEMENTED; }
  NS_IMETHOD Read(char*, uint32_t, uint32_t*) override { return NS_ERROR_NOT_IMPLEMENTED; }
  NS_IMETHOD ReadSegments(nsWriteSegmentFun, void*, uint32_t, uint32_t*) override { return NS_ERROR_NOT_IMPLEMENTED; }
  NS_IMETHOD IsNonBlocking(bool* aNonBlocking) override
  {
    *aNonBlocking = true;
    return mNonBlockingError ? NS_ERROR_FAILURE : NS_OK;
  }

  // nsICloneableInputStream
  NS_IMETHOD GetCloneable(bool*) override { return NS_ERROR_NOT_IMPLEMENTED; }
  NS_IMETHOD Clone(nsIInputStream**) override { return NS_ERROR_NOT_IMPLEMENTED; }

  // nsIIPCSerializableInputStream
  void Serialize(mozilla::ipc::InputStreamParams&, FileDescriptorArray&) override {}
  bool Deserialize(const mozilla::ipc::InputStreamParams&,
                   const FileDescriptorArray&) override { return false; }
  mozilla::Maybe<uint64_t> ExpectedSerializedLength() override { return mozilla::Nothing(); }

  // nsISeekableStream
  NS_IMETHOD Seek(int32_t, int64_t) override { return NS_ERROR_NOT_IMPLEMENTED; }
  NS_IMETHOD Tell(int64_t*) override { return NS_ERROR_NOT_IMPLEMENTED; }
  NS_IMETHOD SetEOF() override { return NS_ERROR_NOT_IMPLEMENTED; }

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
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsIIPCSerializableInputStream, mIPCSerializable)
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsISeekableStream, mSeekable)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIInputStream)
NS_INTERFACE_MAP_END

TEST(TestNonBlockingAsyncInputStream, QI) {
  // Let's test ::Create() returning error.
  nsCOMPtr<nsIInputStream> stream = new QIInputStream(true, true, true, true);

  nsCOMPtr<nsIAsyncInputStream> async;
  ASSERT_EQ(NS_ERROR_FAILURE,
            NonBlockingAsyncInputStream::Create(stream, getter_AddRefs(async)));

  // Let's test the QIs
  for (int i = 0; i < 8; ++i) {
    bool shouldBeCloneable = !!(i & 0x01);
    bool shouldBeSerializable = !!(i & 0x02);
    bool shouldBeSeekable = !!(i & 0x04);

    stream = new QIInputStream(false, shouldBeCloneable, shouldBeSerializable,
                               shouldBeSeekable);
    ASSERT_EQ(NS_OK, NonBlockingAsyncInputStream::Create(stream, getter_AddRefs(async)));

    // The returned async stream should be cloneable only if the underlying
    // stream is.
    nsCOMPtr<nsICloneableInputStream> cloneable = do_QueryInterface(async);
    ASSERT_EQ(shouldBeCloneable, !!cloneable);
    cloneable = do_QueryInterface(stream);
    ASSERT_EQ(shouldBeCloneable, !!cloneable);

    // The returned async stream should be serializable only if the underlying
    // stream is.
    nsCOMPtr<nsIIPCSerializableInputStream> ipcSerializable = do_QueryInterface(async);
    ASSERT_EQ(shouldBeSerializable, !!ipcSerializable);
    ipcSerializable = do_QueryInterface(stream);
    ASSERT_EQ(shouldBeSerializable, !!ipcSerializable);

    // The returned async stream should be seekable only if the underlying
    // stream is.
    nsCOMPtr<nsISeekableStream> seekable = do_QueryInterface(async);
    ASSERT_EQ(shouldBeSeekable, !!seekable);
    seekable = do_QueryInterface(stream);
    ASSERT_EQ(shouldBeSeekable, !!seekable);
  }
}
