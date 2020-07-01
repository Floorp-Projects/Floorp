/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Common.h"

#define EXPIRED_TIME_SEC (PR_Now() / PR_USEC_PER_SEC - 3600)
#define NOTEXPIRED_TIME_SEC (PR_Now() / PR_USEC_PER_SEC + 3600)

#define CACHED_URL "cache.com/"_ns
#define NEG_CACHE_EXPIRED_URL "cache.negExpired.com/"_ns
#define POS_CACHE_EXPIRED_URL "cache.posExpired.com/"_ns
#define BOTH_CACHE_EXPIRED_URL "cache.negAndposExpired.com/"_ns

static void SetupCacheEntry(LookupCacheV2* aLookupCache,
                            const nsCString& aCompletion,
                            bool aNegExpired = false,
                            bool aPosExpired = false) {
  AddCompleteArray completes;
  AddCompleteArray emptyCompletes;
  MissPrefixArray misses;
  MissPrefixArray emptyMisses;

  AddComplete* add = completes.AppendElement(fallible);
  add->complete.FromPlaintext(aCompletion);

  Prefix* prefix = misses.AppendElement(fallible);
  prefix->FromPlaintext(aCompletion);

  // Setup positive cache first otherwise negative cache expiry will be
  // overwritten.
  int64_t posExpirySec = aPosExpired ? EXPIRED_TIME_SEC : NOTEXPIRED_TIME_SEC;
  aLookupCache->AddGethashResultToCache(completes, emptyMisses, posExpirySec);

  int64_t negExpirySec = aNegExpired ? EXPIRED_TIME_SEC : NOTEXPIRED_TIME_SEC;
  aLookupCache->AddGethashResultToCache(emptyCompletes, misses, negExpirySec);
}

static void SetupCacheEntry(LookupCacheV4* aLookupCache,
                            const nsCString& aCompletion,
                            bool aNegExpired = false,
                            bool aPosExpired = false) {
  FullHashResponseMap map;

  Prefix prefix;
  prefix.FromPlaintext(aCompletion);

  CachedFullHashResponse* response = map.LookupOrAdd(prefix.ToUint32());

  response->negativeCacheExpirySec =
      aNegExpired ? EXPIRED_TIME_SEC : NOTEXPIRED_TIME_SEC;
  response->fullHashes.Put(
      CreatePrefixFromURL(aCompletion, COMPLETE_SIZE),
      aPosExpired ? EXPIRED_TIME_SEC : NOTEXPIRED_TIME_SEC);

  aLookupCache->AddFullHashResponseToCache(map);
}

template <typename T>
static void TestCache(const Completion aCompletion, bool aExpectedHas,
                      bool aExpectedConfirmed, bool aExpectedInCache,
                      T* aCache = nullptr) {
  bool has, inCache, confirmed;
  uint32_t matchLength;

  if (aCache) {
    aCache->Has(aCompletion, &has, &matchLength, &confirmed);
    inCache = aCache->IsInCache(aCompletion.ToUint32());
  } else {
    _PrefixArray array = {CreatePrefixFromURL("cache.notexpired.com/", 10),
                          CreatePrefixFromURL("cache.expired.com/", 8),
                          CreatePrefixFromURL("gound.com/", 5),
                          CreatePrefixFromURL("small.com/", 4)};

    RefPtr<T> cache = SetupLookupCache<T>(array);

    // Create an expired entry and a non-expired entry
    SetupCacheEntry(cache, "cache.notexpired.com/"_ns);
    SetupCacheEntry(cache, "cache.expired.com/"_ns, true, true);

    cache->Has(aCompletion, &has, &matchLength, &confirmed);
    inCache = cache->IsInCache(aCompletion.ToUint32());
  }

  EXPECT_EQ(has, aExpectedHas);
  EXPECT_EQ(confirmed, aExpectedConfirmed);
  EXPECT_EQ(inCache, aExpectedInCache);
}

template <typename T>
static void TestCache(const nsCString& aURL, bool aExpectedHas,
                      bool aExpectedConfirmed, bool aExpectedInCache,
                      T* aCache = nullptr) {
  Completion lookupHash;
  lookupHash.FromPlaintext(aURL);

  TestCache<T>(lookupHash, aExpectedHas, aExpectedConfirmed, aExpectedInCache,
               aCache);
}

// This testcase check the returned result of |Has| API if fullhash cannot match
// any prefix in the local database.
TEST(UrlClassifierCaching, NotFound)
{
  TestCache<LookupCacheV2>("nomatch.com/"_ns, false, false, false);
  TestCache<LookupCacheV4>("nomatch.com/"_ns, false, false, false);
}

// This testcase check the returned result of |Has| API if fullhash find a match
// in the local database but not in the cache.
TEST(UrlClassifierCaching, NotInCache)
{
  TestCache<LookupCacheV2>("gound.com/"_ns, true, false, false);
  TestCache<LookupCacheV4>("gound.com/"_ns, true, false, false);
}

// This testcase check the returned result of |Has| API if fullhash matches
// a cache entry in positive cache.
TEST(UrlClassifierCaching, InPositiveCacheNotExpired)
{
  TestCache<LookupCacheV2>("cache.notexpired.com/"_ns, true, true, true);
  TestCache<LookupCacheV4>("cache.notexpired.com/"_ns, true, true, true);
}

// This testcase check the returned result of |Has| API if fullhash matches
// a cache entry in positive cache but that it is expired.
TEST(UrlClassifierCaching, InPositiveCacheExpired)
{
  TestCache<LookupCacheV2>("cache.expired.com/"_ns, true, false, true);
  TestCache<LookupCacheV4>("cache.expired.com/"_ns, true, false, true);
}

// This testcase check the returned result of |Has| API if fullhash matches
// a cache entry in negative cache.
TEST(UrlClassifierCaching, InNegativeCacheNotExpired)
{
  // Create a fullhash whose prefix matches the prefix in negative cache
  // but completion doesn't match any fullhash in positive cache.

  Completion prefix;
  prefix.FromPlaintext("cache.notexpired.com/"_ns);

  Completion fullhash;
  fullhash.FromPlaintext("firefox.com/"_ns);

  // Overwrite the 4-byte prefix of `fullhash` so that it conflicts with
  // `prefix`. Since "cache.notexpired.com" is added to database in TestCache as
  // a 10-byte prefix, we should copy more than 10 bytes to fullhash to ensure
  // it can match the prefix in database.
  memcpy(fullhash.buf, prefix.buf, 10);

  TestCache<LookupCacheV2>(fullhash, false, false, true);
  TestCache<LookupCacheV4>(fullhash, false, false, true);
}

// This testcase check the returned result of |Has| API if fullhash matches
// a cache entry in negative cache but that entry is expired.
TEST(UrlClassifierCaching, InNegativeCacheExpired)
{
  // Create a fullhash whose prefix is in the cache.

  Completion prefix;
  prefix.FromPlaintext("cache.expired.com/"_ns);

  Completion fullhash;
  fullhash.FromPlaintext("firefox.com/"_ns);

  memcpy(fullhash.buf, prefix.buf, 10);

  TestCache<LookupCacheV2>(fullhash, true, false, true);
  TestCache<LookupCacheV4>(fullhash, true, false, true);
}

// This testcase create 4 cache entries.
// 1. unexpired entry.
// 2. an entry whose negative cache time is expired but whose positive cache
//    is not expired.
// 3. an entry whose positive cache time is expired
// 4. an entry whose negative cache time and positive cache time are expired
// After calling |InvalidateExpiredCacheEntry| API, entries with expired
// negative time should be removed from cache(2 & 4)
template <typename T>
void TestInvalidateExpiredCacheEntry() {
  _PrefixArray array = {CreatePrefixFromURL(CACHED_URL, 10),
                        CreatePrefixFromURL(NEG_CACHE_EXPIRED_URL, 8),
                        CreatePrefixFromURL(POS_CACHE_EXPIRED_URL, 5),
                        CreatePrefixFromURL(BOTH_CACHE_EXPIRED_URL, 4)};
  RefPtr<T> cache = SetupLookupCache<T>(array);

  SetupCacheEntry(cache, CACHED_URL, false, false);
  SetupCacheEntry(cache, NEG_CACHE_EXPIRED_URL, true, false);
  SetupCacheEntry(cache, POS_CACHE_EXPIRED_URL, false, true);
  SetupCacheEntry(cache, BOTH_CACHE_EXPIRED_URL, true, true);

  // Before invalidate
  TestCache<T>(CACHED_URL, true, true, true, cache.get());
  TestCache<T>(NEG_CACHE_EXPIRED_URL, true, true, true, cache.get());
  TestCache<T>(POS_CACHE_EXPIRED_URL, true, false, true, cache.get());
  TestCache<T>(BOTH_CACHE_EXPIRED_URL, true, false, true, cache.get());

  // Call InvalidateExpiredCacheEntry to remove cache entries whose negative
  // cache time is expired
  cache->InvalidateExpiredCacheEntries();

  // After invalidate, NEG_CACHE_EXPIRED_URL & BOTH_CACHE_EXPIRED_URL should
  // not be found in cache.
  TestCache<T>(NEG_CACHE_EXPIRED_URL, true, false, false, cache.get());
  TestCache<T>(BOTH_CACHE_EXPIRED_URL, true, false, false, cache.get());

  // Other entries should remain the same result.
  TestCache<T>(CACHED_URL, true, true, true, cache.get());
  TestCache<T>(POS_CACHE_EXPIRED_URL, true, false, true, cache.get());
}

TEST(UrlClassifierCaching, InvalidateExpiredCacheEntryV2)
{ TestInvalidateExpiredCacheEntry<LookupCacheV2>(); }

TEST(UrlClassifierCaching, InvalidateExpiredCacheEntryV4)
{ TestInvalidateExpiredCacheEntry<LookupCacheV4>(); }

// This testcase check if an cache entry whose negative cache time is expired
// and it doesn't have any postive cache entries in it, it should be removed
// from cache after calling |Has|.
TEST(UrlClassifierCaching, NegativeCacheExpireV2)
{
  _PrefixArray array = {CreatePrefixFromURL(NEG_CACHE_EXPIRED_URL, 8)};
  RefPtr<LookupCacheV2> cache = SetupLookupCache<LookupCacheV2>(array);

  nsCOMPtr<nsICryptoHash> cryptoHash =
      do_CreateInstance(NS_CRYPTO_HASH_CONTRACTID);

  MissPrefixArray misses;
  Prefix* prefix = misses.AppendElement(fallible);
  prefix->FromPlaintext(NEG_CACHE_EXPIRED_URL);

  AddCompleteArray dummy;
  cache->AddGethashResultToCache(dummy, misses, EXPIRED_TIME_SEC);

  // Ensure it is in cache in the first place.
  EXPECT_EQ(cache->IsInCache(prefix->ToUint32()), true);

  // It should be removed after calling Has API.
  TestCache<LookupCacheV2>(NEG_CACHE_EXPIRED_URL, true, false, false,
                           cache.get());
}

TEST(UrlClassifierCaching, NegativeCacheExpireV4)
{
  _PrefixArray array = {CreatePrefixFromURL(NEG_CACHE_EXPIRED_URL, 8)};
  RefPtr<LookupCacheV4> cache = SetupLookupCache<LookupCacheV4>(array);

  FullHashResponseMap map;
  Prefix prefix;
  nsCOMPtr<nsICryptoHash> cryptoHash =
      do_CreateInstance(NS_CRYPTO_HASH_CONTRACTID);
  prefix.FromPlaintext(NEG_CACHE_EXPIRED_URL);
  CachedFullHashResponse* response = map.LookupOrAdd(prefix.ToUint32());

  response->negativeCacheExpirySec = EXPIRED_TIME_SEC;

  cache->AddFullHashResponseToCache(map);

  // Ensure it is in cache in the first place.
  EXPECT_EQ(cache->IsInCache(prefix.ToUint32()), true);

  // It should be removed after calling Has API.
  TestCache<LookupCacheV4>(NEG_CACHE_EXPIRED_URL, true, false, false,
                           cache.get());
}
