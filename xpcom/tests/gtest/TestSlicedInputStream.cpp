#include "gtest/gtest.h"

#include "nsCOMPtr.h"
#include "nsIInputStream.h"
#include "nsStreamUtils.h"
#include "nsString.h"
#include "nsStringStream.h"
#include "SlicedInputStream.h"

/* We want to ensure that sliced streams work with both seekable and
 * non-seekable input streams.  As our string streams are seekable, we need to
 * provide a string stream that doesn't permit seeking, so we can test the
 * logic that emulates seeking in sliced input streams.
 */
class NonSeekableStringStream final : public nsIInputStream
{
  nsCOMPtr<nsIInputStream> mStream;

public:
  NS_DECL_THREADSAFE_ISUPPORTS

  explicit NonSeekableStringStream(const nsACString& aBuffer)
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
    return mStream->ReadSegments(aWriter, aClosure, aCount, aResult);
  }

  NS_IMETHOD
  Close() override
  {
    return mStream->Close();
  }

  NS_IMETHOD
  IsNonBlocking(bool* aNonBlocking) override
  {
    return mStream->IsNonBlocking(aNonBlocking);
  }

private:
  ~NonSeekableStringStream() {}
};

NS_IMPL_ISUPPORTS(NonSeekableStringStream, nsIInputStream)

// Helper function for creating a seekable nsIInputStream + a SlicedInputStream.
SlicedInputStream*
CreateSeekableStreams(uint32_t aSize, uint64_t aStart, uint64_t aLength,
                      nsCString& aBuffer)
{
  aBuffer.SetLength(aSize);
  for (uint32_t i = 0; i < aSize; ++i) {
    aBuffer.BeginWriting()[i] = i % 10;
  }

  nsCOMPtr<nsIInputStream> stream;
  NS_NewCStringInputStream(getter_AddRefs(stream), aBuffer);
  return new SlicedInputStream(stream, aStart, aLength);
}

// Helper function for creating a non-seekable nsIInputStream + a
// SlicedInputStream.
SlicedInputStream*
CreateNonSeekableStreams(uint32_t aSize, uint64_t aStart, uint64_t aLength,
                         nsCString& aBuffer)
{
  aBuffer.SetLength(aSize);
  for (uint32_t i = 0; i < aSize; ++i) {
    aBuffer.BeginWriting()[i] = i % 10;
  }

  RefPtr<NonSeekableStringStream> stream = new NonSeekableStringStream(aBuffer);
  return new SlicedInputStream(stream, aStart, aLength);
}

// Same start, same length.
TEST(TestSlicedInputStream, Simple) {
  const size_t kBufSize = 4096;

  nsCString buf;
  RefPtr<SlicedInputStream> sis =
    CreateSeekableStreams(kBufSize, 0, kBufSize, buf);

  uint64_t length;
  ASSERT_EQ(NS_OK, sis->Available(&length));
  ASSERT_EQ((uint64_t)kBufSize, length);

  char buf2[kBufSize];
  uint32_t count;
  ASSERT_EQ(NS_OK, sis->Read(buf2, sizeof(buf2), &count));
  ASSERT_EQ(count, buf.Length());
  ASSERT_TRUE(nsCString(buf.get()).Equals(nsCString(buf2)));
}

// Simple sliced stream - seekable
TEST(TestSlicedInputStream, Sliced) {
  const size_t kBufSize = 4096;

  nsCString buf;
  RefPtr<SlicedInputStream> sis =
    CreateSeekableStreams(kBufSize, 10, 100, buf);

  uint64_t length;
  ASSERT_EQ(NS_OK, sis->Available(&length));
  ASSERT_EQ((uint64_t)100, length);

  char buf2[kBufSize / 2];
  uint32_t count;
  ASSERT_EQ(NS_OK, sis->Read(buf2, sizeof(buf2), &count));
  ASSERT_EQ((uint64_t)100, count);
  ASSERT_TRUE(nsCString(buf.get() + 10, count).Equals(nsCString(buf2, count)));
}

// Simple sliced stream - non seekable
TEST(TestSlicedInputStream, SlicedNoSeek) {
  const size_t kBufSize = 4096;

  nsCString buf;
  RefPtr<SlicedInputStream> sis =
    CreateNonSeekableStreams(kBufSize, 10, 100, buf);

  uint64_t length;
  ASSERT_EQ(NS_OK, sis->Available(&length));
  ASSERT_EQ((uint64_t)100, length);

  char buf2[kBufSize / 2];
  uint32_t count;
  ASSERT_EQ(NS_OK, sis->Read(buf2, sizeof(buf2), &count));
  ASSERT_EQ((uint64_t)100, count);
  ASSERT_TRUE(nsCString(buf.get() + 10, count).Equals(nsCString(buf2, count)));
}

// Big inputStream - seekable
TEST(TestSlicedInputStream, BigSliced) {
  const size_t kBufSize = 4096 * 40;

  nsCString buf;
  RefPtr<SlicedInputStream> sis =
    CreateSeekableStreams(kBufSize, 4096 * 5, 4096 * 10, buf);

  uint64_t length;
  ASSERT_EQ(NS_OK, sis->Available(&length));
  ASSERT_EQ((uint64_t)4096 * 10, length);

  char buf2[kBufSize / 2];
  uint32_t count;
  ASSERT_EQ(NS_OK, sis->Read(buf2, sizeof(buf2), &count));
  ASSERT_EQ((uint64_t)4096 * 10, count);
  ASSERT_TRUE(nsCString(buf.get() + 4096 * 5, count).Equals(nsCString(buf2, count)));
}

// Big inputStream - non seekable
TEST(TestSlicedInputStream, BigSlicedNoSeek) {
  const size_t kBufSize = 4096 * 40;

  nsCString buf;
  RefPtr<SlicedInputStream> sis =
    CreateNonSeekableStreams(kBufSize, 4096 * 5, 4096 * 10, buf);

  uint64_t length;
  ASSERT_EQ(NS_OK, sis->Available(&length));
  ASSERT_EQ((uint64_t)4096 * 10, length);

  char buf2[kBufSize / 2];
  uint32_t count;
  ASSERT_EQ(NS_OK, sis->Read(buf2, sizeof(buf2), &count));
  ASSERT_EQ((uint64_t)4096 * 10, count);
  ASSERT_TRUE(nsCString(buf.get() + 4096 * 5, count).Equals(nsCString(buf2, count)));
}

// Available size.
TEST(TestSlicedInputStream, Available) {
  nsCString buf;
  RefPtr<SlicedInputStream> sis =
    CreateNonSeekableStreams(500000, 4, 400000, buf);

  uint64_t toRead = 400000;
  for (uint32_t i = 0; i < 400; ++i) {
    uint64_t length;
    ASSERT_EQ(NS_OK, sis->Available(&length));
    ASSERT_EQ(toRead, length);

    char buf2[1000];
    uint32_t count;
    ASSERT_EQ(NS_OK, sis->Read(buf2, sizeof(buf2), &count));
    ASSERT_EQ((uint64_t)1000, count);
    ASSERT_TRUE(nsCString(buf.get() + 4 + (1000 * i), count).Equals(nsCString(buf2, count)));

    toRead -= count;
  }

  uint64_t length;
  ASSERT_EQ(NS_OK, sis->Available(&length));
  ASSERT_EQ((uint64_t)0, length);

  char buf2[4096];
  uint32_t count;
  ASSERT_EQ(NS_OK, sis->Read(buf2, sizeof(buf2), &count));
  ASSERT_EQ((uint64_t)0, count);
}

// What if start is > then the size of the buffer?
TEST(TestSlicedInputStream, StartBiggerThan) {
  nsCString buf;
  RefPtr<SlicedInputStream> sis =
    CreateNonSeekableStreams(500, 4000, 1, buf);

  uint64_t length;
  ASSERT_EQ(NS_OK, sis->Available(&length));
  ASSERT_EQ((uint64_t)0, length);

  char buf2[4096];
  uint32_t count;
  ASSERT_EQ(NS_OK, sis->Read(buf2, sizeof(buf2), &count));
  ASSERT_EQ((uint64_t)0, count);
}

// What if the length is > than the size of the buffer?
TEST(TestSlicedInputStream, LengthBiggerThan) {
  nsCString buf;
  RefPtr<SlicedInputStream> sis =
    CreateNonSeekableStreams(500, 0, 500000, buf);

  uint64_t length;
  ASSERT_EQ(NS_OK, sis->Available(&length));
  ASSERT_EQ((uint64_t)500, length);

  char buf2[4096];
  uint32_t count;
  ASSERT_EQ(NS_OK, sis->Read(buf2, sizeof(buf2), &count));
  ASSERT_EQ((uint64_t)500, count);
}

// What if the length is 0?
TEST(TestSlicedInputStream, Length0) {
  nsCString buf;
  RefPtr<SlicedInputStream> sis =
    CreateNonSeekableStreams(500, 0, 0, buf);

  uint64_t length;
  ASSERT_EQ(NS_OK, sis->Available(&length));
  ASSERT_EQ((uint64_t)0, length);

  char buf2[4096];
  uint32_t count;
  ASSERT_EQ(NS_OK, sis->Read(buf2, sizeof(buf2), &count));
  ASSERT_EQ((uint64_t)0, count);
}
