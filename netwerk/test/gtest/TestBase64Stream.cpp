/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "mozilla/Base64.h"
#include "mozilla/gtest/MozAssertions.h"
#include "nsCOMPtr.h"
#include "nsIInputStream.h"
#include "nsStringStream.h"

namespace mozilla {
namespace net {

// An input stream whose ReadSegments method calls aWriter with writes of size
// aStep from the provided aInput in order to test edge-cases related to small
// buffers.
class TestStream final : public nsIInputStream {
 public:
  NS_DECL_ISUPPORTS;

  TestStream(const nsACString& aInput, uint32_t aStep)
      : mInput(aInput), mStep(aStep) {}

  NS_IMETHOD Close() override { MOZ_CRASH("This should not be called"); }

  NS_IMETHOD Available(uint64_t* aLength) override {
    *aLength = mInput.Length() - mPos;
    return NS_OK;
  }

  NS_IMETHOD StreamStatus() override { return NS_OK; }

  NS_IMETHOD Read(char* aBuffer, uint32_t aCount,
                  uint32_t* aReadCount) override {
    MOZ_CRASH("This should not be called");
  }

  NS_IMETHOD ReadSegments(nsWriteSegmentFun aWriter, void* aClosure,
                          uint32_t aCount, uint32_t* aResult) override {
    *aResult = 0;

    if (mPos == mInput.Length()) {
      return NS_OK;
    }

    while (aCount > 0) {
      uint32_t amt = std::min(mStep, (uint32_t)(mInput.Length() - mPos));

      uint32_t read = 0;
      nsresult rv =
          aWriter(this, aClosure, mInput.get() + mPos, *aResult, amt, &read);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      *aResult += read;
      aCount -= read;
      mPos += read;
    }

    return NS_OK;
  }

  NS_IMETHOD IsNonBlocking(bool* aNonBlocking) override {
    *aNonBlocking = true;
    return NS_OK;
  }

 private:
  ~TestStream() = default;

  nsCString mInput;
  const uint32_t mStep;
  uint32_t mPos = 0;
};

NS_IMPL_ISUPPORTS(TestStream, nsIInputStream)

// Test the base64 encoder with writer buffer sizes between 1 byte and the
// entire length of "Hello World!" in order to exercise various edge cases.
TEST(TestBase64Stream, Run)
{
  nsCString input;
  input.AssignLiteral("Hello World!");

  for (uint32_t step = 1; step <= input.Length(); ++step) {
    RefPtr<TestStream> ts = new TestStream(input, step);

    nsAutoString encodedData;
    nsresult rv = Base64EncodeInputStream(ts, encodedData, input.Length());
    ASSERT_NS_SUCCEEDED(rv);

    EXPECT_TRUE(encodedData.EqualsLiteral("SGVsbG8gV29ybGQh"));
  }
}

TEST(TestBase64Stream, VaryingCount)
{
  nsCString input;
  input.AssignLiteral("Hello World!");

  std::pair<size_t, nsCString> tests[] = {
      {0, "SGVsbG8gV29ybGQh"_ns},  {1, "SA=="_ns},
      {5, "SGVsbG8="_ns},          {11, "SGVsbG8gV29ybGQ="_ns},
      {12, "SGVsbG8gV29ybGQh"_ns}, {13, "SGVsbG8gV29ybGQh"_ns},
  };

  for (auto& [count, expected] : tests) {
    nsCOMPtr<nsIInputStream> is;
    nsresult rv = NS_NewCStringInputStream(getter_AddRefs(is), input);
    ASSERT_NS_SUCCEEDED(rv);

    nsAutoCString encodedData;
    rv = Base64EncodeInputStream(is, encodedData, count);
    ASSERT_NS_SUCCEEDED(rv);
    EXPECT_EQ(encodedData, expected) << "count: " << count;
  }
}

}  // namespace net
}  // namespace mozilla
