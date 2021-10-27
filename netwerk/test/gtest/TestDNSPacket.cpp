#include "gtest/gtest.h"

#include "mozilla/net/DNSPacket.h"
#include "mozilla/Preferences.h"

using namespace mozilla;
using namespace mozilla::net;

void AssertDnsPadding(uint32_t PaddingLength, unsigned int WithPadding,
                      unsigned int WithoutPadding, bool DisableEcn,
                      const nsCString& host) {
  DNSPacket encoder;
  nsCString buf;

  ASSERT_EQ(Preferences::SetUint("network.trr.padding.length", PaddingLength),
            NS_OK);

  ASSERT_EQ(Preferences::SetBool("network.trr.padding", true), NS_OK);
  ASSERT_EQ(encoder.EncodeRequest(buf, host, 1, DisableEcn), NS_OK);
  ASSERT_EQ(buf.Length(), WithPadding);

  ASSERT_EQ(Preferences::SetBool("network.trr.padding", false), NS_OK);
  ASSERT_EQ(encoder.EncodeRequest(buf, host, 1, DisableEcn), NS_OK);
  ASSERT_EQ(buf.Length(), WithoutPadding);
}

TEST(TestDNSPacket, PaddingLenEcn)
{
  AssertDnsPadding(16, 48, 41, true, "a.de"_ns);
  AssertDnsPadding(16, 48, 42, true, "ab.de"_ns);
  AssertDnsPadding(16, 48, 43, true, "abc.de"_ns);
  AssertDnsPadding(16, 48, 44, true, "abcd.de"_ns);
  AssertDnsPadding(16, 64, 45, true, "abcde.de"_ns);
  AssertDnsPadding(16, 64, 46, true, "abcdef.de"_ns);
  AssertDnsPadding(16, 64, 47, true, "abcdefg.de"_ns);
  AssertDnsPadding(16, 64, 48, true, "abcdefgh.de"_ns);
}

TEST(TestDNSPacket, PaddingLenDisableEcn)
{
  AssertDnsPadding(16, 48, 22, false, "a.de"_ns);
  AssertDnsPadding(16, 48, 23, false, "ab.de"_ns);
  AssertDnsPadding(16, 48, 24, false, "abc.de"_ns);
  AssertDnsPadding(16, 48, 25, false, "abcd.de"_ns);
  AssertDnsPadding(16, 48, 26, false, "abcde.de"_ns);
  AssertDnsPadding(16, 48, 27, false, "abcdef.de"_ns);
  AssertDnsPadding(16, 48, 32, false, "abcdefghijk.de"_ns);
  AssertDnsPadding(16, 48, 33, false, "abcdefghijkl.de"_ns);
  AssertDnsPadding(16, 64, 34, false, "abcdefghijklm.de"_ns);
  AssertDnsPadding(16, 64, 35, false, "abcdefghijklmn.de"_ns);
}

TEST(TestDNSPacket, PaddingLengths)
{
  AssertDnsPadding(0, 45, 41, true, "a.de"_ns);
  AssertDnsPadding(1, 45, 41, true, "a.de"_ns);
  AssertDnsPadding(2, 46, 41, true, "a.de"_ns);
  AssertDnsPadding(3, 45, 41, true, "a.de"_ns);
  AssertDnsPadding(4, 48, 41, true, "a.de"_ns);
  AssertDnsPadding(16, 48, 41, true, "a.de"_ns);
  AssertDnsPadding(32, 64, 41, true, "a.de"_ns);
  AssertDnsPadding(42, 84, 41, true, "a.de"_ns);
  AssertDnsPadding(52, 52, 41, true, "a.de"_ns);
  AssertDnsPadding(80, 80, 41, true, "a.de"_ns);
  AssertDnsPadding(128, 128, 41, true, "a.de"_ns);
  AssertDnsPadding(256, 256, 41, true, "a.de"_ns);
  AssertDnsPadding(1024, 1024, 41, true, "a.de"_ns);
  AssertDnsPadding(1025, 1024, 41, true, "a.de"_ns);
}
