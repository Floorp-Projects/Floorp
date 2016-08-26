#include "Entries.h"
#include "mozilla/EndianUtils.h"

TEST(SafebrowsingHash, ToFromUint32)
{
  using namespace mozilla::safebrowsing;

  // typedef SafebrowsingHash<PREFIX_SIZE, PrefixComparator> Prefix;
  // typedef nsTArray<Prefix> PrefixArray;

  const char PREFIX_RAW[4] = { 0x1, 0x2, 0x3, 0x4 };
  uint32_t PREFIX_UINT32;
  memcpy(&PREFIX_UINT32, PREFIX_RAW, 4);

  Prefix p;
  p.Assign(nsCString(PREFIX_RAW, 4));
  ASSERT_EQ(p.ToUint32(), PREFIX_UINT32);

  p.FromUint32(PREFIX_UINT32);
  ASSERT_EQ(memcmp(PREFIX_RAW, p.buf, 4), 0);
}

TEST(SafebrowsingHash, Compare)
{
  using namespace mozilla;
  using namespace mozilla::safebrowsing;

  Prefix p1, p2, p3;

  // The order of p1,p2,p3 is "p1 == p3 < p2"
#if MOZ_LITTLE_ENDIAN
  p1.Assign(nsCString("\x01\x00\x00\x00", 4));
  p2.Assign(nsCString("\x00\x00\x00\x01", 4));
  p3.Assign(nsCString("\x01\x00\x00\x00", 4));
#else
  p1.Assign(nsCString("\x00\x00\x00\x01", 4));
  p2.Assign(nsCString("\x01\x00\x00\x00", 4));
  p3.Assign(nsCString("\x00\x00\x00\x01", 4));
#endif

  // Make sure "p1 == p3 < p2" is true
  // on both little and big endian machine.

  ASSERT_EQ(p1.Compare(p2), -1);
  ASSERT_EQ(p1.Compare(p1), 0);
  ASSERT_EQ(p2.Compare(p1), 1);
  ASSERT_EQ(p1.Compare(p3), 0);

  ASSERT_TRUE(p1 < p2);
  ASSERT_TRUE(p1 == p1);
  ASSERT_TRUE(p1 == p3);
}