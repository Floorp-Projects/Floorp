/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Common.h"

#define EXPIRED_TIME_SEC     (PR_Now() / PR_USEC_PER_SEC - 3600)
#define NOTEXPIRED_TIME_SEC  (PR_Now() / PR_USEC_PER_SEC + 3600)

static void
SetupCacheEntry(LookupCacheV4* aLookupCache,
                const nsCString& aCompletion,
                bool aNegExpired = false,
                bool aPosExpired = false)
{
  FullHashResponseMap map;

  Prefix prefix;
  nsCOMPtr<nsICryptoHash> cryptoHash = do_CreateInstance(NS_CRYPTO_HASH_CONTRACTID);
  prefix.FromPlaintext(aCompletion, cryptoHash);

  CachedFullHashResponse* response = map.LookupOrAdd(prefix.ToUint32());

  response->negativeCacheExpirySec = aNegExpired ? EXPIRED_TIME_SEC : NOTEXPIRED_TIME_SEC;
  response->fullHashes.Put(GeneratePrefix(aCompletion, COMPLETE_SIZE),
                           aPosExpired ? EXPIRED_TIME_SEC : NOTEXPIRED_TIME_SEC);

  aLookupCache->AddFullHashResponseToCache(map);
}

void
TestCache(const Completion aCompletion,
          bool aExpectedHas,
          bool aExpectedConfirmed,
          bool aExpectedFromCache,
          LookupCacheV4* aCache = nullptr)
{
  bool has, fromCache, confirmed;
  uint32_t matchLength;
  TableFreshnessMap dummy;

  if (aCache) {
    aCache->Has(aCompletion, dummy, 0, &has, &matchLength, &confirmed, &fromCache);
  } else {
    _PrefixArray array = { GeneratePrefix(_Fragment("cache.notexpired.com/"), 10),
                           GeneratePrefix(_Fragment("cache.expired.com/"), 8),
                           GeneratePrefix(_Fragment("gound.com/"), 5),
                           GeneratePrefix(_Fragment("small.com/"), 4)
                         };

    UniquePtr<LookupCacheV4> cache = SetupLookupCacheV4(array);

    // Create an expired entry and a non-expired entry
    SetupCacheEntry(cache.get(), _Fragment("cache.notexpired.com/"));
    SetupCacheEntry(cache.get(), _Fragment("cache.expired.com/"), true, true);

    cache->Has(aCompletion, dummy, 0, &has, &matchLength, &confirmed, &fromCache);
  }

  EXPECT_EQ(has, aExpectedHas);
  EXPECT_EQ(confirmed, aExpectedConfirmed);
  EXPECT_EQ(fromCache, aExpectedFromCache);
}

void
TestCache(const _Fragment& aFragment,
          bool aExpectedHas,
          bool aExpectedConfirmed,
          bool aExpectedFromCache,
          LookupCacheV4* aCache = nullptr)
{
  Completion lookupHash;
  nsCOMPtr<nsICryptoHash> cryptoHash = do_CreateInstance(NS_CRYPTO_HASH_CONTRACTID);
  lookupHash.FromPlaintext(aFragment, cryptoHash);

  TestCache(lookupHash, aExpectedHas, aExpectedConfirmed, aExpectedFromCache, aCache);
}

// This testcase check the returned result of |Has| API if fullhash cannot match
// any prefix in the local database.
TEST(CachingV4, NotFound)
{
  TestCache(_Fragment("nomatch.com/"), false, false, false);
}

// This testcase check the returned result of |Has| API if fullhash find a match
// in the local database but not in the cache.
TEST(CachingV4, NotInCache)
{
  TestCache(_Fragment("gound.com/"), true, false, false);
}

// This testcase check the returned result of |Has| API if fullhash matches
// a cache entry in positive cache.
TEST(CachingV4, InPositiveCacheNotExpired)
{
  TestCache(_Fragment("cache.notexpired.com/"), true, true, true);
}

// This testcase check the returned result of |Has| API if fullhash matches
// a cache entry in positive cache but that it is expired.
TEST(CachingV4, InPositiveCacheExpired)
{
  TestCache(_Fragment("cache.expired.com/"), true, false, true);
}

// This testcase check the returned result of |Has| API if fullhash matches
// a cache entry in negative cache.
TEST(CachingV4, InNegativeCacheNotExpired)
{
  // Create a fullhash whose prefix matches the prefix in negative cache
  // but completion doesn't match any fullhash in positive cache.
  nsCOMPtr<nsICryptoHash> cryptoHash = do_CreateInstance(NS_CRYPTO_HASH_CONTRACTID);

  Completion prefix;
  prefix.FromPlaintext(_Fragment("cache.notexpired.com/"), cryptoHash);

  Completion fullhash;
  fullhash.FromPlaintext(_Fragment("firefox.com/"), cryptoHash);

  // Overwrite the 4-byte prefix of `fullhash` so that it conflicts with `prefix`.
  // Since "cache.notexpired.com" is added to database in TestCache as a
  // 10-byte prefix, we should copy more than 10 bytes to fullhash to ensure
  // it can match the prefix in database.
  memcpy(fullhash.buf, prefix.buf, 10);

  TestCache(fullhash, false, false, true);
}

// This testcase check the returned result of |Has| API if fullhash matches
// a cache entry in negative cache but that entry is expired.
TEST(CachingV4, InNegativeCacheExpired)
{
  // Create a fullhash whose prefix is in the cache.
  nsCOMPtr<nsICryptoHash> cryptoHash = do_CreateInstance(NS_CRYPTO_HASH_CONTRACTID);

  Completion prefix;
  prefix.FromPlaintext(_Fragment("cache.expired.com/"), cryptoHash);

  Completion fullhash;
  fullhash.FromPlaintext(_Fragment("firefox.com/"), cryptoHash);

  memcpy(fullhash.buf, prefix.buf, 10);

  TestCache(fullhash, true, false, true);
}

#define CACHED_URL              _Fragment("cache.com/")
#define NEG_CACHE_EXPIRED_URL   _Fragment("cache.negExpired.com/")
#define POS_CACHE_EXPIRED_URL   _Fragment("cache.posExpired.com/")
#define BOTH_CACHE_EXPIRED_URL  _Fragment("cache.negAndposExpired.com/")

// This testcase create 4 cache entries.
// 1. unexpired entry.
// 2. an entry whose negative cache time is expired but whose positive cache
//    is not expired.
// 3. an entry whose positive cache time is expired
// 4. an entry whose negative cache time and positive cache time are expired
// After calling |InvalidateExpiredCacheEntry| API, entries with expired
// negative time should be removed from cache(2 & 4)
TEST(CachingV4, InvalidateExpiredCacheEntry)
{
  _PrefixArray array = { GeneratePrefix(CACHED_URL, 10),
                         GeneratePrefix(NEG_CACHE_EXPIRED_URL, 8),
                         GeneratePrefix(POS_CACHE_EXPIRED_URL, 5),
                         GeneratePrefix(BOTH_CACHE_EXPIRED_URL, 4)
                       };

  UniquePtr<LookupCacheV4> cache = SetupLookupCacheV4(array);

  SetupCacheEntry(cache.get(), CACHED_URL, false, false);
  SetupCacheEntry(cache.get(), NEG_CACHE_EXPIRED_URL, true, false);
  SetupCacheEntry(cache.get(), POS_CACHE_EXPIRED_URL, false, true);
  SetupCacheEntry(cache.get(), BOTH_CACHE_EXPIRED_URL, true, true);

  // Before invalidate
  TestCache(CACHED_URL, true, true, true, cache.get());
  TestCache(NEG_CACHE_EXPIRED_URL, true, true, true, cache.get());
  TestCache(POS_CACHE_EXPIRED_URL, true, false, true, cache.get());
  TestCache(BOTH_CACHE_EXPIRED_URL, true, false, true, cache.get());

  // Call InvalidateExpiredCacheEntry to remove cache entries whose negative cache
  // time is expired
  cache->InvalidateExpiredCacheEntry();

  // After invalidate, NEG_CACHE_EXPIRED_URL & BOTH_CACHE_EXPIRED_URL should
  // not be found in cache.
  TestCache(NEG_CACHE_EXPIRED_URL, true, false, false, cache.get());
  TestCache(BOTH_CACHE_EXPIRED_URL, true, false, false, cache.get());

  // Other entries should remain the same result.
  TestCache(CACHED_URL, true, true, true, cache.get());
  TestCache(POS_CACHE_EXPIRED_URL, true, false, true, cache.get());
}

// This testcase check if an cache entry whose negative cache time is expired
// and it doesn't have any postive cache entries in it, it should be removed
// from cache after calling |Has|.
TEST(CachingV4, NegativeCacheExpire)
{
  _PrefixArray array = { GeneratePrefix(NEG_CACHE_EXPIRED_URL, 8) };
  UniquePtr<LookupCacheV4> cache = SetupLookupCacheV4(array);

  FullHashResponseMap map;
  Prefix prefix;
  nsCOMPtr<nsICryptoHash> cryptoHash = do_CreateInstance(NS_CRYPTO_HASH_CONTRACTID);
  prefix.FromPlaintext(NEG_CACHE_EXPIRED_URL, cryptoHash);
  CachedFullHashResponse* response = map.LookupOrAdd(prefix.ToUint32());

  response->negativeCacheExpirySec = EXPIRED_TIME_SEC;

  cache->AddFullHashResponseToCache(map);

  // The first time we should found it in the cache but the result is not
  // confirmed(because it is expired).
  TestCache(NEG_CACHE_EXPIRED_URL, true, false, true, cache.get());

  // The second time it should not be found in the cache again
  TestCache(NEG_CACHE_EXPIRED_URL, true, false, false, cache.get());
}
