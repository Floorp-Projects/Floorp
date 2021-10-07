#include "gtest/gtest.h"

#include "mozilla/net/DNSPacket.h"

using namespace mozilla::net;

void AssertDnsPadding(unsigned int WithPadding, unsigned int WithoutPadding,
                      bool DisableEcn, const nsCString& host) {
  DNSPacket encoder;
  nsCString buf;

  Preferences::SetBool("network.trr.padding", true);

  ASSERT_EQ(encoder.EncodeRequest(buf, host, 1, DisableEcn), NS_OK);
  ASSERT_EQ(buf.Length(), WithPadding);

  Preferences::SetBool("network.trr.padding", false);

  ASSERT_EQ(encoder.EncodeRequest(buf, host, 1, DisableEcn), NS_OK);
  ASSERT_EQ(buf.Length(), WithoutPadding);
}

TEST(TestDNSPacket, PaddingLenEcn)
{
  AssertDnsPadding(48, 41, true, "a.de"_ns);
  AssertDnsPadding(48, 42, true, "ab.de"_ns);
  AssertDnsPadding(48, 43, true, "abc.de"_ns);
  AssertDnsPadding(48, 44, true, "abcd.de"_ns);
  AssertDnsPadding(64, 45, true, "abcde.de"_ns);
  AssertDnsPadding(64, 46, true, "abcdef.de"_ns);
  AssertDnsPadding(64, 47, true, "abcdefg.de"_ns);
  AssertDnsPadding(64, 48, true, "abcdefgh.de"_ns);
}

TEST(TestDNSPacket, PaddingLenDisableEcn)
{
  AssertDnsPadding(48, 22, false, "a.de"_ns);
  AssertDnsPadding(48, 23, false, "ab.de"_ns);
  AssertDnsPadding(48, 24, false, "abc.de"_ns);
  AssertDnsPadding(48, 25, false, "abcd.de"_ns);
  AssertDnsPadding(48, 26, false, "abcde.de"_ns);
  AssertDnsPadding(48, 27, false, "abcdef.de"_ns);
  AssertDnsPadding(48, 32, false, "abcdefghijk.de"_ns);
  AssertDnsPadding(48, 33, false, "abcdefghijkl.de"_ns);
  AssertDnsPadding(64, 34, false, "abcdefghijklm.de"_ns);
  AssertDnsPadding(64, 35, false, "abcdefghijklmn.de"_ns);
}
