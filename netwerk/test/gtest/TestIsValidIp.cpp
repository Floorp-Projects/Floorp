#include "gtest/MozGTestBench.h"  // For MOZ_GTEST_BENCH
#include "gtest/gtest.h"

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
  NS_NAMED_LITERAL_CSTRING(ipv6, "0123:4567:89ab:cdef:0123:4567:127.0.0.1");
  ASSERT_EQ(true, net_IsValidIPv6Addr(ipv6));
}

TEST(TestIsValidIp, IPv6FullForm)
{
  NS_NAMED_LITERAL_CSTRING(ipv6, "0123:4567:89ab:cdef:0123:4567:890a:bcde");
  ASSERT_EQ(true, net_IsValidIPv6Addr(ipv6));
}

TEST(TestIsValidIp, IPv6TrimLeading0)
{
  NS_NAMED_LITERAL_CSTRING(ipv6, "123:4567:0:0:123:4567:890a:bcde");
  ASSERT_EQ(true, net_IsValidIPv6Addr(ipv6));
}

TEST(TestIsValidIp, IPv6Collapsed)
{
  NS_NAMED_LITERAL_CSTRING(ipv6, "FF01::101");
  ASSERT_EQ(true, net_IsValidIPv6Addr(ipv6));
}

TEST(TestIsValidIp, IPV6WithIPV4InsideCollapsed)
{
  NS_NAMED_LITERAL_CSTRING(ipv6, "::FFFF:129.144.52.38");
  ASSERT_EQ(true, net_IsValidIPv6Addr(ipv6));
}

TEST(TestIsValidIp, IPV6Localhost)
{
  NS_NAMED_LITERAL_CSTRING(ipv6, "::1");
  ASSERT_EQ(true, net_IsValidIPv6Addr(ipv6));
}

TEST(TestIsValidIp, IPV6LinkLocalPrefix)
{
  NS_NAMED_LITERAL_CSTRING(ipv6, "fe80::");
  ASSERT_EQ(true, net_IsValidIPv6Addr(ipv6));
}

TEST(TestIsValidIp, IPV6GlobalUnicastPrefix)
{
  NS_NAMED_LITERAL_CSTRING(ipv6, "2001::");
  ASSERT_EQ(true, net_IsValidIPv6Addr(ipv6));
}

TEST(TestIsValidIp, IPV6Unspecified)
{
  NS_NAMED_LITERAL_CSTRING(ipv6, "::");
  ASSERT_EQ(true, net_IsValidIPv6Addr(ipv6));
}

TEST(TestIsValidIp, IPV6InvalidIPV4Inside)
{
  NS_NAMED_LITERAL_CSTRING(ipv6, "0123:4567:89ab:cdef:0123:4567:127.0.");
  ASSERT_EQ(false, net_IsValidIPv6Addr(ipv6));
}

TEST(TestIsValidIp, IPV6InvalidCharacters)
{
  NS_NAMED_LITERAL_CSTRING(ipv6, "012g:4567:89ab:cdef:0123:4567:127.0.0.1");
  ASSERT_EQ(false, net_IsValidIPv6Addr(ipv6));

  NS_NAMED_LITERAL_CSTRING(ipv6pound,
                           "0123:456#:89ab:cdef:0123:4567:127.0.0.1");
  ASSERT_EQ(false, net_IsValidIPv6Addr(ipv6pound));
}

TEST(TestIsValidIp, IPV6TooManyCharacters)
{
  NS_NAMED_LITERAL_CSTRING(ipv6, "0123:45671:89ab:cdef:0123:4567:127.0.0.1");
  ASSERT_EQ(false, net_IsValidIPv6Addr(ipv6));
}
TEST(TestIsValidIp, IPV6DoubleDoubleDots)
{
  NS_NAMED_LITERAL_CSTRING(ipv6, "0123::4567:890a::bcde:0123:4567");
  ASSERT_EQ(false, net_IsValidIPv6Addr(ipv6));
}
