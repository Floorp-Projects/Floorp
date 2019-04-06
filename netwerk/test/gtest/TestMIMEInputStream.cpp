#include "gtest/gtest.h"

#include "Helpers.h"
#include "nsCOMPtr.h"
#include "nsIPipe.h"
#include "nsStreamUtils.h"
#include "nsString.h"
#include "nsStringStream.h"
#include "nsIMIMEInputStream.h"

using mozilla::GetCurrentThreadSerialEventTarget;
using mozilla::SpinEventLoopUntil;

namespace {

class SeekableLengthInputStream final : public testing::LengthInputStream,
                                        public nsISeekableStream {
 public:
  SeekableLengthInputStream(const nsACString& aBuffer,
                            bool aIsInputStreamLength,
                            bool aIsAsyncInputStreamLength,
                            nsresult aLengthRv = NS_OK,
                            bool aNegativeValue = false)
      : testing::LengthInputStream(aBuffer, aIsInputStreamLength,
                                   aIsAsyncInputStreamLength, aLengthRv,
                                   aNegativeValue) {}

  NS_DECL_ISUPPORTS_INHERITED

  NS_IMETHOD
  Seek(int32_t aWhence, int64_t aOffset) override {
    MOZ_CRASH("This method should not be called.");
    return NS_ERROR_FAILURE;
  }

  NS_IMETHOD
  Tell(int64_t* aResult) override {
    MOZ_CRASH("This method should not be called.");
    return NS_ERROR_FAILURE;
  }

  NS_IMETHOD
  SetEOF() override {
    MOZ_CRASH("This method should not be called.");
    return NS_ERROR_FAILURE;
  }

 private:
  ~SeekableLengthInputStream() = default;
};

NS_IMPL_ISUPPORTS_INHERITED(SeekableLengthInputStream,
                            testing::LengthInputStream, nsISeekableStream)

}  // namespace

// nsIInputStreamLength && nsIAsyncInputStreamLength

TEST(TestNsMIMEInputStream, QIInputStreamLength)
{
  nsCString buf;
  buf.AssignLiteral("Hello world");

  for (int i = 0; i < 4; i++) {
    nsCOMPtr<nsIInputStream> mis;
    {
      RefPtr<SeekableLengthInputStream> stream =
          new SeekableLengthInputStream(buf, i % 2, i > 1);

      nsresult rv;
      nsCOMPtr<nsIMIMEInputStream> m(
          do_CreateInstance("@mozilla.org/network/mime-input-stream;1", &rv));
      ASSERT_EQ(NS_OK, rv);

      rv = m->SetData(stream);
      ASSERT_EQ(NS_OK, rv);

      mis = m;
      ASSERT_TRUE(!!mis);
    }

    {
      nsCOMPtr<nsIInputStreamLength> qi = do_QueryInterface(mis);
      ASSERT_EQ(!!(i % 2), !!qi);
    }

    {
      nsCOMPtr<nsIAsyncInputStreamLength> qi = do_QueryInterface(mis);
      ASSERT_EQ(i > 1, !!qi);
    }
  }
}

TEST(TestNsMIMEInputStream, InputStreamLength)
{
  nsCString buf;
  buf.AssignLiteral("Hello world");

  nsCOMPtr<nsIInputStream> mis;
  {
    RefPtr<SeekableLengthInputStream> stream =
        new SeekableLengthInputStream(buf, true, false);

    nsresult rv;
    nsCOMPtr<nsIMIMEInputStream> m(
        do_CreateInstance("@mozilla.org/network/mime-input-stream;1", &rv));
    ASSERT_EQ(NS_OK, rv);

    rv = m->SetData(stream);
    ASSERT_EQ(NS_OK, rv);

    mis = m;
    ASSERT_TRUE(!!mis);
  }

  nsCOMPtr<nsIInputStreamLength> qi = do_QueryInterface(mis);
  ASSERT_TRUE(!!qi);

  int64_t size;
  nsresult rv = qi->Length(&size);
  ASSERT_EQ(NS_OK, rv);
  ASSERT_EQ(buf.Length(), size);
}

TEST(TestNsMIMEInputStream, NegativeInputStreamLength)
{
  nsCString buf;
  buf.AssignLiteral("Hello world");

  nsCOMPtr<nsIInputStream> mis;
  {
    RefPtr<SeekableLengthInputStream> stream =
        new SeekableLengthInputStream(buf, true, false, NS_OK, true);

    nsresult rv;
    nsCOMPtr<nsIMIMEInputStream> m(
        do_CreateInstance("@mozilla.org/network/mime-input-stream;1", &rv));
    ASSERT_EQ(NS_OK, rv);

    rv = m->SetData(stream);
    ASSERT_EQ(NS_OK, rv);

    mis = m;
    ASSERT_TRUE(!!mis);
  }

  nsCOMPtr<nsIInputStreamLength> qi = do_QueryInterface(mis);
  ASSERT_TRUE(!!qi);

  int64_t size;
  nsresult rv = qi->Length(&size);
  ASSERT_EQ(NS_OK, rv);
  ASSERT_EQ(-1, size);
}

TEST(TestNsMIMEInputStream, AsyncInputStreamLength)
{
  nsCString buf;
  buf.AssignLiteral("Hello world");

  nsCOMPtr<nsIInputStream> mis;
  {
    RefPtr<SeekableLengthInputStream> stream =
        new SeekableLengthInputStream(buf, false, true);

    nsresult rv;
    nsCOMPtr<nsIMIMEInputStream> m(
        do_CreateInstance("@mozilla.org/network/mime-input-stream;1", &rv));
    ASSERT_EQ(NS_OK, rv);

    rv = m->SetData(stream);
    ASSERT_EQ(NS_OK, rv);

    mis = m;
    ASSERT_TRUE(!!mis);
  }

  nsCOMPtr<nsIAsyncInputStreamLength> qi = do_QueryInterface(mis);
  ASSERT_TRUE(!!qi);

  RefPtr<testing::LengthCallback> callback = new testing::LengthCallback();

  nsresult rv =
      qi->AsyncLengthWait(callback, GetCurrentThreadSerialEventTarget());
  ASSERT_EQ(NS_OK, rv);

  MOZ_ALWAYS_TRUE(SpinEventLoopUntil([&]() { return callback->Called(); }));
  ASSERT_EQ(buf.Length(), callback->Size());
}

TEST(TestNsMIMEInputStream, NegativeAsyncInputStreamLength)
{
  nsCString buf;
  buf.AssignLiteral("Hello world");

  nsCOMPtr<nsIInputStream> mis;
  {
    RefPtr<SeekableLengthInputStream> stream =
        new SeekableLengthInputStream(buf, false, true, NS_OK, true);

    nsresult rv;
    nsCOMPtr<nsIMIMEInputStream> m(
        do_CreateInstance("@mozilla.org/network/mime-input-stream;1", &rv));
    ASSERT_EQ(NS_OK, rv);

    rv = m->SetData(stream);
    ASSERT_EQ(NS_OK, rv);

    mis = m;
    ASSERT_TRUE(!!mis);
  }

  nsCOMPtr<nsIAsyncInputStreamLength> qi = do_QueryInterface(mis);
  ASSERT_TRUE(!!qi);

  RefPtr<testing::LengthCallback> callback = new testing::LengthCallback();

  nsresult rv =
      qi->AsyncLengthWait(callback, GetCurrentThreadSerialEventTarget());
  ASSERT_EQ(NS_OK, rv);

  MOZ_ALWAYS_TRUE(SpinEventLoopUntil([&]() { return callback->Called(); }));
  ASSERT_EQ(-1, callback->Size());
}

TEST(TestNsMIMEInputStream, AbortLengthCallback)
{
  nsCString buf;
  buf.AssignLiteral("Hello world");

  nsCOMPtr<nsIInputStream> mis;
  {
    RefPtr<SeekableLengthInputStream> stream =
        new SeekableLengthInputStream(buf, false, true, NS_OK, true);

    nsresult rv;
    nsCOMPtr<nsIMIMEInputStream> m(
        do_CreateInstance("@mozilla.org/network/mime-input-stream;1", &rv));
    ASSERT_EQ(NS_OK, rv);

    rv = m->SetData(stream);
    ASSERT_EQ(NS_OK, rv);

    mis = m;
    ASSERT_TRUE(!!mis);
  }

  nsCOMPtr<nsIAsyncInputStreamLength> qi = do_QueryInterface(mis);
  ASSERT_TRUE(!!qi);

  RefPtr<testing::LengthCallback> callback1 = new testing::LengthCallback();
  nsresult rv =
      qi->AsyncLengthWait(callback1, GetCurrentThreadSerialEventTarget());
  ASSERT_EQ(NS_OK, rv);

  RefPtr<testing::LengthCallback> callback2 = new testing::LengthCallback();
  rv = qi->AsyncLengthWait(callback2, GetCurrentThreadSerialEventTarget());
  ASSERT_EQ(NS_OK, rv);

  MOZ_ALWAYS_TRUE(SpinEventLoopUntil([&]() { return callback2->Called(); }));
  ASSERT_TRUE(!callback1->Called());
  ASSERT_EQ(-1, callback2->Size());
}
