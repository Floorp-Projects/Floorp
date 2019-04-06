#include "gtest/gtest.h"

#include "mozilla/InputStreamLengthHelper.h"
#include "nsCOMPtr.h"
#include "nsIInputStream.h"
#include "nsIRunnable.h"
#include "nsIThreadManager.h"
#include "nsStreamUtils.h"
#include "nsString.h"
#include "nsStringStream.h"
#include "nsThreadUtils.h"
#include "nsXPCOM.h"
#include "Helpers.h"

using namespace mozilla;

TEST(TestInputStreamLengthHelper, NonLengthStream)
{
  nsCString buf;
  buf.AssignLiteral("Hello world");

  nsCOMPtr<nsIInputStream> stream;
  NS_NewCStringInputStream(getter_AddRefs(stream), buf);

  bool called = false;
  InputStreamLengthHelper::GetAsyncLength(stream, [&](int64_t aLength) {
    ASSERT_EQ(buf.Length(), aLength);
    called = true;
  });

  MOZ_ALWAYS_TRUE(SpinEventLoopUntil([&]() { return called; }));
}

class LengthStream final : public nsIInputStreamLength,
                           public nsIAsyncInputStreamLength,
                           public nsIInputStream {
 public:
  NS_DECL_ISUPPORTS

  LengthStream(int64_t aLength, nsresult aLengthRv, uint64_t aAvailable,
               bool aIsAsyncLength)
      : mLength(aLength),
        mLengthRv(aLengthRv),
        mAvailable(aAvailable),
        mIsAsyncLength(aIsAsyncLength) {}

  NS_IMETHOD Close(void) override { MOZ_CRASH("Invalid call!"); }
  NS_IMETHOD Read(char* aBuf, uint32_t aCount, uint32_t* _retval) override {
    MOZ_CRASH("Invalid call!");
  }
  NS_IMETHOD ReadSegments(nsWriteSegmentFun aWriter, void* aClosure,
                          uint32_t aCount, uint32_t* _retval) override {
    MOZ_CRASH("Invalid call!");
  }
  NS_IMETHOD IsNonBlocking(bool* _retval) override {
    MOZ_CRASH("Invalid call!");
  }

  NS_IMETHOD Length(int64_t* aLength) override {
    *aLength = mLength;
    return mLengthRv;
  }

  NS_IMETHOD AsyncLengthWait(nsIInputStreamLengthCallback* aCallback,
                             nsIEventTarget* aEventTarget) override {
    aCallback->OnInputStreamLengthReady(this, mLength);
    return NS_OK;
  }

  NS_IMETHOD Available(uint64_t* aAvailable) override {
    *aAvailable = mAvailable;
    return NS_OK;
  }

 private:
  ~LengthStream() = default;

  int64_t mLength;
  nsresult mLengthRv;
  uint64_t mAvailable;

  bool mIsAsyncLength;
};

NS_IMPL_ADDREF(LengthStream);
NS_IMPL_RELEASE(LengthStream);

NS_INTERFACE_MAP_BEGIN(LengthStream)
  NS_INTERFACE_MAP_ENTRY(nsIInputStream)
  NS_INTERFACE_MAP_ENTRY(nsIInputStreamLength)
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsIAsyncInputStreamLength, mIsAsyncLength)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIInputStream)
NS_INTERFACE_MAP_END

TEST(TestInputStreamLengthHelper, LengthStream)
{
  nsCOMPtr<nsIInputStream> stream = new LengthStream(42, NS_OK, 0, false);

  bool called = false;
  InputStreamLengthHelper::GetAsyncLength(stream, [&](int64_t aLength) {
    ASSERT_EQ(42, aLength);
    called = true;
  });

  MOZ_ALWAYS_TRUE(SpinEventLoopUntil([&]() { return called; }));
}

TEST(TestInputStreamLengthHelper, InvalidLengthStream)
{
  nsCOMPtr<nsIInputStream> stream =
      new LengthStream(42, NS_ERROR_NOT_AVAILABLE, 0, false);

  bool called = false;
  InputStreamLengthHelper::GetAsyncLength(stream, [&](int64_t aLength) {
    ASSERT_EQ(-1, aLength);
    called = true;
  });

  MOZ_ALWAYS_TRUE(SpinEventLoopUntil([&]() { return called; }));
}

TEST(TestInputStreamLengthHelper, AsyncLengthStream)
{
  nsCOMPtr<nsIInputStream> stream =
      new LengthStream(22, NS_BASE_STREAM_WOULD_BLOCK, 123, true);

  bool called = false;
  InputStreamLengthHelper::GetAsyncLength(stream, [&](int64_t aLength) {
    ASSERT_EQ(22, aLength);
    called = true;
  });

  MOZ_ALWAYS_TRUE(SpinEventLoopUntil([&]() { return called; }));
}

TEST(TestInputStreamLengthHelper, FallbackLengthStream)
{
  nsCOMPtr<nsIInputStream> stream =
      new LengthStream(-1, NS_BASE_STREAM_WOULD_BLOCK, 123, false);

  bool called = false;
  InputStreamLengthHelper::GetAsyncLength(stream, [&](int64_t aLength) {
    ASSERT_EQ(123, aLength);
    called = true;
  });

  MOZ_ALWAYS_TRUE(SpinEventLoopUntil([&]() { return called; }));
}
