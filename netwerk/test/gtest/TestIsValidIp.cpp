#include "gtest/gtest.h"
#include "gtest/MozGTestBench.h"  // For MOZ_GTEST_BENCH

#include "nsURLHelper.h"

TEST(TestIsValidIp, IPV4Localhost)
{
  NS_NAMED_LITERAL_CSTRING(ip, "127.0.0.1");
  ASSERT_EQ(true, net_IsValidIPv4Addr(ip));
}

TEST(TestIsValidIp, IPV4Only0)
{
  NS_NAMED_LITERAL_CSTRING(ip, "0.0.0.0");
  ASSERT_EQ(true, net_IsValidIPv4Addr(ip));
}

TEST(TestIsValidIp, IPV4Max)
{
  NS_NAMED_LITERAL_CSTRING(ip, "255.255.255.255");
  ASSERT_EQ(true, net_IsValidIPv4Addr(ip));
}

TEST(TestIsValidIp, IPV4LeadingZero)
{
  NS_NAMED_LITERAL_CSTRING(ip, "055.225.255.255");
  ASSERT_EQ(false, net_IsValidIPv4Addr(ip));

  NS_NAMED_LITERAL_CSTRING(ip2, "255.055.255.255");
  ASSERT_EQ(false, net_IsValidIPv4Addr(ip2));

  NS_NAMED_LITERAL_CSTRING(ip3, "255.255.055.255");
  ASSERT_EQ(false, net_IsValidIPv4Addr(ip3));

  NS_NAMED_LITERAL_CSTRING(ip4, "255.255.255.055");
  ASSERT_EQ(false, net_IsValidIPv4Addr(ip4));
}

TEST(TestIsValidIp, IPV4StartWithADot)
{
  NS_NAMED_LITERAL_CSTRING(ip, ".192.168.120.197");
  ASSERT_EQ(false, net_IsValidIPv4Addr(ip));
}

TEST(TestIsValidIp, IPV4StartWith4Digits)
{
  NS_NAMED_LITERAL_CSTRING(ip, "1927.168.120.197");
  ASSERT_EQ(false, net_IsValidIPv4Addr(ip));
}

TEST(TestIsValidIp, IPV4OutOfRange)
{
  NS_NAMED_LITERAL_CSTRING(invalid1, "421.168.120.124");
  NS_NAMED_LITERAL_CSTRING(invalid2, "192.997.120.124");
  NS_NAMED_LITERAL_CSTRING(invalid3, "192.168.300.124");
  NS_NAMED_LITERAL_CSTRING(invalid4, "192.168.120.256");

  ASSERT_EQ(false, net_IsValidIPv4Addr(invalid1));
  ASSERT_EQ(false, net_IsValidIPv4Addr(invalid2));
  ASSERT_EQ(false, net_IsValidIPv4Addr(invalid3));
  ASSERT_EQ(false, net_IsValidIPv4Addr(invalid4));
}

TEST(TestIsValidIp, IPV4EmptyDigits)
{
  NS_NAMED_LITERAL_CSTRING(invalid1, "..0.0.0");
  NS_NAMED_LITERAL_CSTRING(invalid2, "127..0.0");
  NS_NAMED_LITERAL_CSTRING(invalid3, "127.0..0");
  NS_NAMED_LITERAL_CSTRING(invalid4, "127.0.0.");

  ASSERT_EQ(false, net_IsValidIPv4Addr(invalid1));
  ASSERT_EQ(false, net_IsValidIPv4Addr(invalid2));
  ASSERT_EQ(false, net_IsValidIPv4Addr(invalid3));
  ASSERT_EQ(false, net_IsValidIPv4Addr(invalid4));
}

TEST(TestIsValidIp, IPV4NonNumeric)
{
  NS_NAMED_LITERAL_CSTRING(invalid1, "127.0.0.f");
  NS_NAMED_LITERAL_CSTRING(invalid2, "127.0.0.!");
  NS_NAMED_LITERAL_CSTRING(invalid3, "127#0.0.1");

  ASSERT_EQ(false, net_IsValidIPv4Addr(invalid1));
  ASSERT_EQ(false, net_IsValidIPv4Addr(invalid2));
  ASSERT_EQ(false, net_IsValidIPv4Addr(invalid3));
}

TEST(TestIsValidIp, IPV4TooManyDigits)
{
  NS_NAMED_LITERAL_CSTRING(ip, "127.0.0.1.2");
  ASSERT_EQ(false, net_IsValidIPv4Addr(ip));
}

TEST(TestIsValidIp, IPV4TooFewDigits)
{
  NS_NAMED_LITERAL_CSTRING(ip, "127.0.1");
  ASSERT_EQ(false, net_IsValidIPv4Addr(ip));
}

TEST(TestIsValidIp, IPV6WithIPV4Inside)
{
  const char ipv6[] = "0123:4567:89ab:cdef:0123:4567:127.0.0.1";
  ASSERT_EQ(true, net_IsValidIPv6Addr(ipv6, 39));
}
