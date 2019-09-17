#include <combaseapi.h>

#include "gtest/gtest.h"
#include "mozilla/SHA1.h"
#include "nsNotifyAddrListener.h"

using namespace mozilla;

GUID StringToGuid(const std::string& str) {
  GUID guid;
  sscanf(str.c_str(),
         "{%8lx-%4hx-%4hx-%2hhx%2hhx-%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx}",
         &guid.Data1, &guid.Data2, &guid.Data3, &guid.Data4[0], &guid.Data4[1],
         &guid.Data4[2], &guid.Data4[3], &guid.Data4[4], &guid.Data4[5],
         &guid.Data4[6], &guid.Data4[7]);

  return guid;
}

TEST(TestGuidHashWindows, Single)
{
  // Setup
  SHA1Sum expected_sha1;
  SHA1Sum::Hash expected_digest;

  GUID g1 = StringToGuid("264555b1-289c-4494-83d1-e158d1d95115");

  expected_sha1.update(&g1, sizeof(GUID));
  expected_sha1.finish(expected_digest);

  std::vector<GUID> nwGUIDS;
  nwGUIDS.push_back(g1);
  SHA1Sum actual_sha1;
  // Run
  nsNotifyAddrListener::HashSortedNetworkIds(nwGUIDS, actual_sha1);
  SHA1Sum::Hash actual_digest;
  actual_sha1.finish(actual_digest);

  // Assert
  ASSERT_EQ(0, memcmp(&expected_digest, &actual_digest, sizeof(SHA1Sum::Hash)));
}

TEST(TestNetworkLinkIdHashingWindows, Multiple)
{
  // Setup
  SHA1Sum expected_sha1;
  SHA1Sum::Hash expected_digest;

  std::vector<GUID> nwGUIDS;
  nwGUIDS.push_back(StringToGuid("00000000-0000-0000-0000-000000000004"));
  nwGUIDS.push_back(StringToGuid("00000000-0000-0000-0000-000000000003"));
  nwGUIDS.push_back(StringToGuid("00000000-0000-0000-0000-000000000002"));
  nwGUIDS.push_back(StringToGuid("00000000-0000-0000-0000-000000000001"));

  for (const auto& guid : nwGUIDS) {
    expected_sha1.update(&guid, sizeof(GUID));
  }
  expected_sha1.finish(expected_digest);

  // Ordered
  std::vector<GUID> ordered;
  for (const auto& guid : nwGUIDS) {
    ordered.push_back(guid);
  }
  SHA1Sum ordered_sha1;

  // Unordered
  std::vector<GUID> reversed;
  for (auto it = nwGUIDS.rbegin(); it != nwGUIDS.rend(); ++it) {
    reversed.push_back(*it);
  }
  SHA1Sum reversed_sha1;

  // Run
  nsNotifyAddrListener::HashSortedNetworkIds(ordered, ordered_sha1);
  SHA1Sum::Hash ordered_digest;
  ordered_sha1.finish(ordered_digest);

  nsNotifyAddrListener::HashSortedNetworkIds(reversed, reversed_sha1);
  SHA1Sum::Hash reversed_digest;
  reversed_sha1.finish(reversed_digest);

  // Assert
  ASSERT_EQ(0,
            memcmp(&expected_digest, &ordered_digest, sizeof(SHA1Sum::Hash)));
  ASSERT_EQ(0,
            memcmp(&expected_digest, &reversed_digest, sizeof(SHA1Sum::Hash)));
}
