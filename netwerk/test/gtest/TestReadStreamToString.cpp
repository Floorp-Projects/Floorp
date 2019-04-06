#include "gtest/gtest.h"

#include "nsCOMPtr.h"
#include "nsNetUtil.h"

// Here we test the reading a pre-allocated size
TEST(TestReadStreamToString, SyncStreamPreAllocatedSize)
{
  nsCString buffer;
  buffer.AssignLiteral("Hello world!");

  nsCOMPtr<nsIInputStream> stream;
  ASSERT_EQ(NS_OK, NS_NewCStringInputStream(getter_AddRefs(stream), buffer));

  uint64_t written;
  nsAutoCString result;
  result.SetLength(5);

  void* ptr = result.BeginWriting();

  ASSERT_EQ(NS_OK, NS_ReadInputStreamToString(stream, result, 5, &written));
  ASSERT_EQ((uint64_t)5, written);
  ASSERT_TRUE(nsCString(buffer.get(), 5).Equals(result));

  // The pointer should be equal: no relocation.
  ASSERT_EQ(ptr, result.BeginWriting());
}

// Here we test the reading the full size of a sync stream
TEST(TestReadStreamToString, SyncStreamFullSize)
{
  nsCString buffer;
  buffer.AssignLiteral("Hello world!");

  nsCOMPtr<nsIInputStream> stream;
  ASSERT_EQ(NS_OK, NS_NewCStringInputStream(getter_AddRefs(stream), buffer));

  uint64_t written;
  nsAutoCString result;

  ASSERT_EQ(NS_OK, NS_ReadInputStreamToString(stream, result, buffer.Length(),
                                              &written));
  ASSERT_EQ(buffer.Length(), written);
  ASSERT_TRUE(buffer.Equals(result));
}

// Here we test the reading less than the full size of a sync stream
TEST(TestReadStreamToString, SyncStreamLessThan)
{
  nsCString buffer;
  buffer.AssignLiteral("Hello world!");

  nsCOMPtr<nsIInputStream> stream;
  ASSERT_EQ(NS_OK, NS_NewCStringInputStream(getter_AddRefs(stream), buffer));

  uint64_t written;
  nsAutoCString result;

  ASSERT_EQ(NS_OK, NS_ReadInputStreamToString(stream, result, 5, &written));
  ASSERT_EQ((uint64_t)5, written);
  ASSERT_TRUE(nsCString(buffer.get(), 5).Equals(result));
}

// Here we test the reading more than the full size of a sync stream
TEST(TestReadStreamToString, SyncStreamMoreThan)
{
  nsCString buffer;
  buffer.AssignLiteral("Hello world!");

  nsCOMPtr<nsIInputStream> stream;
  ASSERT_EQ(NS_OK, NS_NewCStringInputStream(getter_AddRefs(stream), buffer));

  uint64_t written;
  nsAutoCString result;

  // Reading more than the buffer size.
  ASSERT_EQ(NS_OK, NS_ReadInputStreamToString(stream, result,
                                              buffer.Length() + 5, &written));
  ASSERT_EQ(buffer.Length(), written);
  ASSERT_TRUE(buffer.Equals(result));
}

// Here we test the reading a sync stream without passing the size
TEST(TestReadStreamToString, SyncStreamUnknownSize)
{
  nsCString buffer;
  buffer.AssignLiteral("Hello world!");

  nsCOMPtr<nsIInputStream> stream;
  ASSERT_EQ(NS_OK, NS_NewCStringInputStream(getter_AddRefs(stream), buffer));

  uint64_t written;
  nsAutoCString result;

  // Reading all without passing the size
  ASSERT_EQ(NS_OK, NS_ReadInputStreamToString(stream, result, -1, &written));
  ASSERT_EQ(buffer.Length(), written);
  ASSERT_TRUE(buffer.Equals(result));
}

// Here we test the reading the full size of an async stream
TEST(TestReadStreamToString, AsyncStreamFullSize)
{
  nsCString buffer;
  buffer.AssignLiteral("Hello world!");

  nsCOMPtr<nsIInputStream> stream = new testing::AsyncStringStream(buffer);

  uint64_t written;
  nsAutoCString result;

  ASSERT_EQ(NS_OK, NS_ReadInputStreamToString(stream, result, buffer.Length(),
                                              &written));
  ASSERT_EQ(buffer.Length(), written);
  ASSERT_TRUE(buffer.Equals(result));
}

// Here we test the reading less than the full size of an async stream
TEST(TestReadStreamToString, AsyncStreamLessThan)
{
  nsCString buffer;
  buffer.AssignLiteral("Hello world!");

  nsCOMPtr<nsIInputStream> stream = new testing::AsyncStringStream(buffer);

  uint64_t written;
  nsAutoCString result;

  ASSERT_EQ(NS_OK, NS_ReadInputStreamToString(stream, result, 5, &written));
  ASSERT_EQ((uint64_t)5, written);
  ASSERT_TRUE(nsCString(buffer.get(), 5).Equals(result));
}

// Here we test the reading more than the full size of an async stream
TEST(TestReadStreamToString, AsyncStreamMoreThan)
{
  nsCString buffer;
  buffer.AssignLiteral("Hello world!");

  nsCOMPtr<nsIInputStream> stream = new testing::AsyncStringStream(buffer);

  uint64_t written;
  nsAutoCString result;

  // Reading more than the buffer size.
  ASSERT_EQ(NS_OK, NS_ReadInputStreamToString(stream, result,
                                              buffer.Length() + 5, &written));
  ASSERT_EQ(buffer.Length(), written);
  ASSERT_TRUE(buffer.Equals(result));
}

// Here we test the reading an async stream without passing the size
TEST(TestReadStreamToString, AsyncStreamUnknownSize)
{
  nsCString buffer;
  buffer.AssignLiteral("Hello world!");

  nsCOMPtr<nsIInputStream> stream = new testing::AsyncStringStream(buffer);

  uint64_t written;
  nsAutoCString result;

  // Reading all without passing the size
  ASSERT_EQ(NS_OK, NS_ReadInputStreamToString(stream, result, -1, &written));
  ASSERT_EQ(buffer.Length(), written);
  ASSERT_TRUE(buffer.Equals(result));
}

// Here we test the reading an async big stream without passing the size
TEST(TestReadStreamToString, AsyncStreamUnknownBigSize)
{
  nsCString buffer;

  buffer.SetLength(4096 * 2);
  for (uint32_t i = 0; i < 4096 * 2; ++i) {
    buffer.BeginWriting()[i] = i % 10;
  }

  nsCOMPtr<nsIInputStream> stream = new testing::AsyncStringStream(buffer);

  uint64_t written;
  nsAutoCString result;

  // Reading all without passing the size
  ASSERT_EQ(NS_OK, NS_ReadInputStreamToString(stream, result, -1, &written));
  ASSERT_EQ(buffer.Length(), written);
  ASSERT_TRUE(buffer.Equals(result));
}
