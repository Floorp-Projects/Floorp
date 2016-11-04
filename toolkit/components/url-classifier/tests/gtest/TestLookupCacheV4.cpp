/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "LookupCacheV4.h"
#include "Common.h"

#define GTEST_SAFEBROWSING_DIR NS_LITERAL_CSTRING("safebrowsing")
#define GTEST_TABLE NS_LITERAL_CSTRING("gtest-malware-proto")

typedef nsCString _Fragment;
typedef nsTArray<nsCString> _PrefixArray;

// Generate a hash prefix from string
static const nsCString
GeneratePrefix(const _Fragment& aFragment, uint8_t aLength)
{
  Completion complete;
  nsCOMPtr<nsICryptoHash> cryptoHash = do_CreateInstance(NS_CRYPTO_HASH_CONTRACTID);
  complete.FromPlaintext(aFragment, cryptoHash);

  nsCString hash;
  hash.Assign((const char *)complete.buf, aLength);
  return hash;
}

static UniquePtr<LookupCacheV4>
SetupLookupCacheV4(const _PrefixArray& prefixArray)
{
  nsCOMPtr<nsIFile> file;
  NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR, getter_AddRefs(file));

  file->AppendNative(GTEST_SAFEBROWSING_DIR);

  UniquePtr<LookupCacheV4> cache = MakeUnique<LookupCacheV4>(GTEST_TABLE, EmptyCString(), file);
  nsresult rv = cache->Init();
  EXPECT_EQ(rv, NS_OK);

  PrefixStringMap map;
  PrefixArrayToPrefixStringMap(prefixArray, map);
  rv = cache->Build(map);
  EXPECT_EQ(rv, NS_OK);

  return Move(cache);
}

void
TestHasPrefix(const _Fragment& aFragment, bool aExpectedHas, bool aExpectedComplete)
{
  _PrefixArray array = { GeneratePrefix(_Fragment("bravo.com/"), 32),
                         GeneratePrefix(_Fragment("browsing.com/"), 8),
                         GeneratePrefix(_Fragment("gound.com/"), 5),
                         GeneratePrefix(_Fragment("small.com/"), 4)
                       };

  RunTestInNewThread([&] () -> void {
    UniquePtr<LookupCache> cache = SetupLookupCacheV4(array);

    Completion lookupHash;
    nsCOMPtr<nsICryptoHash> cryptoHash = do_CreateInstance(NS_CRYPTO_HASH_CONTRACTID);
    lookupHash.FromPlaintext(aFragment, cryptoHash);

    bool has, complete;
    nsresult rv = cache->Has(lookupHash, &has, &complete);

    EXPECT_EQ(rv, NS_OK);
    EXPECT_EQ(has, aExpectedHas);
    EXPECT_EQ(complete, aExpectedComplete);

    cache->ClearAll();
  });

}

TEST(LookupCacheV4, HasComplete)
{
  TestHasPrefix(_Fragment("bravo.com/"), true, true);
}

TEST(LookupCacheV4, HasPrefix)
{
  TestHasPrefix(_Fragment("browsing.com/"), true, false);
}

TEST(LookupCacheV4, Nomatch)
{
  TestHasPrefix(_Fragment("nomatch.com/"), false, false);
}
