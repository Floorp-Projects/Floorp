#include "gtest/MozGTestBench.h"  // For MOZ_GTEST_BENCH
#include "gtest/gtest.h"

#include "nsURLHelper.h"

TEST(TestIsValidIp, IPV4Localhost)
{
  constexpr auto ip = "127.0.0.1"_ns;
  ASSERT_EQ(true, net_IsValidIPv4Addr(ip));
}

TEST(TestIsValidIp, IPV4Only0)
{
  constexpr auto ip = "0.0.0.0"_ns;
  ASSERT_EQ(true, net_IsValidIPv4Addr(ip));
}

TEST(TestIsValidIp, IPV4Max)
{
  constexpr auto ip = "255.255.255.255"_ns;
  ASSERT_EQ(true, net_IsValidIPv4Addr(ip));
}

TEST(TestIsValidIp, IPV4LeadingZero)
{
  constexpr auto ip = "055.225.255.255"_ns;
  ASSERT_EQ(false, net_IsValidIPv4Addr(ip));

  constexpr auto ip2 = "255.055.255.255"_ns;
  ASSERT_EQ(false, net_IsValidIPv4Addr(ip2));

  constexpr auto ip3 = "255.255.055.255"_ns;
  ASSERT_EQ(false, net_IsValidIPv4Addr(ip3));

  constexpr auto ip4 = "255.255.255.055"_ns;
  ASSERT_EQ(false, net_IsValidIPv4Addr(ip4));
}

TEST(TestIsValidIp, IPV4StartWithADot)
{
  constexpr auto ip = ".192.168.120.197"_ns;
  ASSERT_EQ(false, net_IsValidIPv4Addr(ip));
}

TEST(TestIsValidIp, IPV4StartWith4Digits)
{
  constexpr auto ip = "1927.168.120.197"_ns;
  ASSERT_EQ(false, net_IsValidIPv4Addr(ip));
}

TEST(TestIsValidIp, IPV4OutOfRange)
{
  constexpr auto invalid1 = "421.168.120.124"_ns;
  constexpr auto invalid2 = "192.997.120.124"_ns;
  constexpr auto invalid3 = "192.168.300.124"_ns;
  constexpr auto invalid4 = "192.168.120.256"_ns;

  ASSERT_EQ(false, net_IsValidIPv4Addr(invalid1));
  ASSERT_EQ(false, net_IsValidIPv4Addr(invalid2));
  ASSERT_EQ(false, net_IsValidIPv4Addr(invalid3));
  ASSERT_EQ(false, net_IsValidIPv4Addr(invalid4));
}

TEST(TestIsValidIp, IPV4EmptyDigits)
{
  constexpr auto invalid1 = "..0.0.0"_ns;
  constexpr auto invalid2 = "127..0.0"_ns;
  constexpr auto invalid3 = "127.0..0"_ns;
  constexpr auto invalid4 = "127.0.0."_ns;

  ASSERT_EQ(false, net_IsValidIPv4Addr(invalid1));
  ASSERT_EQ(false, net_IsValidIPv4Addr(invalid2));
  ASSERT_EQ(false, net_IsValidIPv4Addr(invalid3));
  ASSERT_EQ(false, net_IsValidIPv4Addr(invalid4));
}

TEST(TestIsValidIp, IPV4NonNumeric)
{
  constexpr auto invalid1 = "127.0.0.f"_ns;
  constexpr auto invalid2 = "127.0.0.!"_ns;
  constexpr auto invalid3 = "127#0.0.1"_ns;

  ASSERT_EQ(false, net_IsValidIPv4Addr(invalid1));
  ASSERT_EQ(false, net_IsValidIPv4Addr(invalid2));
  ASSERT_EQ(false, net_IsValidIPv4Addr(invalid3));
}

TEST(TestIsValidIp, IPV4TooManyDigits)
{
  constexpr auto ip = "127.0.0.1.2"_ns;
  ASSERT_EQ(false, net_IsValidIPv4Addr(ip));
}

TEST(TestIsValidIp, IPV4TooFewDigits)
{
  constexpr auto ip = "127.0.1"_ns;
  ASSERT_EQ(false, net_IsValidIPv4Addr(ip));
}

TEST(TestIsValidIp, IPV6WithIPV4Inside)
{
  constexpr auto ipv6 = "0123:4567:89ab:cdef:0123:4567:127.0.0.1"_ns;
  ASSERT_EQ(true, net_IsValidIPv6Addr(ipv6));
}

TEST(TestIsValidIp, IPv6FullForm)
{
  constexpr auto ipv6 = "0123:4567:89ab:cdef:0123:4567:890a:bcde"_ns;
  ASSERT_EQ(true, net_IsValidIPv6Addr(ipv6));
}

TEST(TestIsValidIp, IPv6TrimLeading0)
{
  constexpr auto ipv6 = "123:4567:0:0:123:4567:890a:bcde"_ns;
  ASSERT_EQ(true, net_IsValidIPv6Addr(ipv6));
}

TEST(TestIsValidIp, IPv6Collapsed)
{
  constexpr auto ipv6 = "FF01::101"_ns;
  ASSERT_EQ(true, net_IsValidIPv6Addr(ipv6));
}

TEST(TestIsValidIp, IPV6WithIPV4InsideCollapsed)
{
  constexpr auto ipv6 = "::FFFF:129.144.52.38"_ns;
  ASSERT_EQ(true, net_IsValidIPv6Addr(ipv6));
}

TEST(TestIsValidIp, IPV6Localhost)
{
  constexpr auto ipv6 = "::1"_ns;
  ASSERT_EQ(true, net_IsValidIPv6Addr(ipv6));
}

TEST(TestIsValidIp, IPV6LinkLocalPrefix)
{
  constexpr auto ipv6 = "fe80::"_ns;
  ASSERT_EQ(true, net_IsValidIPv6Addr(ipv6));
}

TEST(TestIsValidIp, IPV6GlobalUnicastPrefix)
{
  constexpr auto ipv6 = "2001::"_ns;
  ASSERT_EQ(true, net_IsValidIPv6Addr(ipv6));
}

TEST(TestIsValidIp, IPV6Unspecified)
{
  constexpr auto ipv6 = "::"_ns;
  ASSERT_EQ(true, net_IsValidIPv6Addr(ipv6));
}

TEST(TestIsValidIp, IPV6InvalidIPV4Inside)
{
  constexpr auto ipv6 = "0123:4567:89ab:cdef:0123:4567:127.0."_ns;
  ASSERT_EQ(false, net_IsValidIPv6Addr(ipv6));
}

TEST(TestIsValidIp, IPV6InvalidCharacters)
{
  constexpr auto ipv6 = "012g:4567:89ab:cdef:0123:4567:127.0.0.1"_ns;
  ASSERT_EQ(false, net_IsValidIPv6Addr(ipv6));

  constexpr auto ipv6pound = "0123:456#:89ab:cdef:0123:4567:127.0.0.1"_ns;
  ASSERT_EQ(false, net_IsValidIPv6Addr(ipv6pound));
}

TEST(TestIsValidIp, IPV6TooManyCharacters)
{
  constexpr auto ipv6 = "0123:45671:89ab:cdef:0123:4567:127.0.0.1"_ns;
  ASSERT_EQ(false, net_IsValidIPv6Addr(ipv6));
}
TEST(TestIsValidIp, IPV6DoubleDoubleDots)
{
  constexpr auto ipv6 = "0123::4567:890a::bcde:0123:4567"_ns;
  ASSERT_EQ(false, net_IsValidIPv6Addr(ipv6));
}
