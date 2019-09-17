#include <arpa/inet.h>

#include "gtest/gtest.h"
#include "mozilla/SHA1.h"
#include "nsString.h"
#include "nsPrintfCString.h"
#include "mozilla/Logging.h"
#include "nsNetworkLinkService.h"

using namespace mozilla;

in6_addr StringToSockAddr(const std::string& str) {
  sockaddr_in6 ip;
  inet_pton(AF_INET6, str.c_str(), &(ip.sin6_addr));
  return ip.sin6_addr;
}

TEST(TestNetworkLinkIdHashingDarwin, Single)
{
  // Setup
  SHA1Sum expected_sha1;
  SHA1Sum::Hash expected_digest;

  in6_addr a1 = StringToSockAddr("2001:db8:8714:3a91::1");

  // Prefix
  expected_sha1.update(&a1, sizeof(in6_addr));
  // Netmask
  expected_sha1.update(&a1, sizeof(in6_addr));
  expected_sha1.finish(expected_digest);

  std::vector<prefix_and_netmask> prefixNetmaskStore;
  prefixNetmaskStore.push_back(std::make_pair(a1, a1));
  SHA1Sum actual_sha1;
  // Run
  nsNetworkLinkService::HashSortedPrefixesAndNetmasks(prefixNetmaskStore,
                                                      &actual_sha1);
  SHA1Sum::Hash actual_digest;
  actual_sha1.finish(actual_digest);

  // Assert
  ASSERT_EQ(0, memcmp(&expected_digest, &actual_digest, sizeof(SHA1Sum::Hash)));
}

TEST(TestNetworkLinkIdHashingDarwin, Multiple)
{
  // Setup
  SHA1Sum expected_sha1;
  SHA1Sum::Hash expected_digest;

  std::vector<in6_addr> addresses;
  addresses.push_back(StringToSockAddr("2001:db8:8714:3a91::1"));
  addresses.push_back(StringToSockAddr("2001:db8:8714:3a91::2"));
  addresses.push_back(StringToSockAddr("2001:db8:8714:3a91::3"));
  addresses.push_back(StringToSockAddr("2001:db8:8714:3a91::4"));

  for (const auto& address : addresses) {
    // Prefix
    expected_sha1.update(&address, sizeof(in6_addr));
    // Netmask
    expected_sha1.update(&address, sizeof(in6_addr));
  }
  expected_sha1.finish(expected_digest);

  // Ordered
  std::vector<prefix_and_netmask> ordered;
  for (const auto& address : addresses) {
    ordered.push_back(std::make_pair(address, address));
  }
  SHA1Sum ordered_sha1;

  // Unordered
  std::vector<prefix_and_netmask> reversed;
  for (auto it = addresses.rbegin(); it != addresses.rend(); ++it) {
    reversed.push_back(std::make_pair(*it, *it));
  }
  SHA1Sum reversed_sha1;

  // Run
  nsNetworkLinkService::HashSortedPrefixesAndNetmasks(ordered, &ordered_sha1);
  SHA1Sum::Hash ordered_digest;
  ordered_sha1.finish(ordered_digest);

  nsNetworkLinkService::HashSortedPrefixesAndNetmasks(reversed, &reversed_sha1);
  SHA1Sum::Hash reversed_digest;
  reversed_sha1.finish(reversed_digest);

  // Assert
  ASSERT_EQ(0,
            memcmp(&expected_digest, &ordered_digest, sizeof(SHA1Sum::Hash)));
  ASSERT_EQ(0,
            memcmp(&expected_digest, &reversed_digest, sizeof(SHA1Sum::Hash)));
}
