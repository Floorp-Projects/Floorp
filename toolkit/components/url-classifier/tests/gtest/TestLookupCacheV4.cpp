/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Common.h"

void
TestHasPrefix(const _Fragment& aFragment, bool aExpectedHas, bool aExpectedComplete)
{
  _PrefixArray array = { GeneratePrefix(_Fragment("bravo.com/"), 32),
                         GeneratePrefix(_Fragment("browsing.com/"), 8),
                         GeneratePrefix(_Fragment("gound.com/"), 5),
                         GeneratePrefix(_Fragment("small.com/"), 4)
                       };

  RunTestInNewThread([&] () -> void {
    RefPtr<LookupCache> cache = SetupLookupCache<LookupCacheV4>(array);

    Completion lookupHash;
    lookupHash.FromPlaintext(aFragment);

    bool has, confirmed;
    uint32_t matchLength;
    // Freshness is not used in V4 so we just put dummy values here.
    TableFreshnessMap dummy;
    nsresult rv =
      cache->Has(lookupHash, &has, &matchLength, &confirmed);

    EXPECT_EQ(rv, NS_OK);
    EXPECT_EQ(has, aExpectedHas);
    EXPECT_EQ(matchLength == COMPLETE_SIZE, aExpectedComplete);
    EXPECT_EQ(confirmed, false);

    cache->ClearAll();
  });

}

TEST(UrlClassifierLookupCacheV4, HasComplete)
{
  TestHasPrefix(_Fragment("bravo.com/"), true, true);
}

TEST(UrlClassifierLookupCacheV4, HasPrefix)
{
  TestHasPrefix(_Fragment("browsing.com/"), true, false);
}

TEST(UrlClassifierLookupCacheV4, Nomatch)
{
  TestHasPrefix(_Fragment("nomatch.com/"), false, false);
}
