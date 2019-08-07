/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Common.h"

static void TestHasPrefix(const nsCString& aURL, bool aExpectedHas,
                          bool aExpectedComplete) {
  _PrefixArray array = {CreatePrefixFromURL("bravo.com/", 32),
                        CreatePrefixFromURL("browsing.com/", 8),
                        CreatePrefixFromURL("gound.com/", 5),
                        CreatePrefixFromURL("small.com/", 4)};

  RunTestInNewThread([&]() -> void {
    RefPtr<LookupCache> cache = SetupLookupCache<LookupCacheV4>(array);

    Completion lookupHash;
    lookupHash.FromPlaintext(aURL);

    bool has, confirmed;
    uint32_t matchLength;
    // Freshness is not used in V4 so we just put dummy values here.
    TableFreshnessMap dummy;
    nsresult rv = cache->Has(lookupHash, &has, &matchLength, &confirmed);

    EXPECT_EQ(rv, NS_OK);
    EXPECT_EQ(has, aExpectedHas);
    EXPECT_EQ(matchLength == COMPLETE_SIZE, aExpectedComplete);
    EXPECT_EQ(confirmed, false);

    cache->ClearAll();
  });
}

TEST(UrlClassifierLookupCacheV4, HasComplete)
{ TestHasPrefix(NS_LITERAL_CSTRING("bravo.com/"), true, true); }

TEST(UrlClassifierLookupCacheV4, HasPrefix)
{ TestHasPrefix(NS_LITERAL_CSTRING("browsing.com/"), true, false); }

TEST(UrlClassifierLookupCacheV4, Nomatch)
{ TestHasPrefix(NS_LITERAL_CSTRING("nomatch.com/"), false, false); }

// Test an existing .pset should be removed after .vlpset is written
TEST(UrlClassifierLookupCacheV4, RemoveOldPset)
{
  nsCOMPtr<nsIFile> oldPsetFile;
  NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR,
                         getter_AddRefs(oldPsetFile));
  oldPsetFile->AppendNative(NS_LITERAL_CSTRING("safebrowsing"));
  oldPsetFile->AppendNative(GTEST_TABLE_V4 + NS_LITERAL_CSTRING(".pset"));

  nsCOMPtr<nsIFile> newPsetFile;
  NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR,
                         getter_AddRefs(newPsetFile));
  newPsetFile->AppendNative(NS_LITERAL_CSTRING("safebrowsing"));
  newPsetFile->AppendNative(GTEST_TABLE_V4 + NS_LITERAL_CSTRING(".vlpset"));

  // Create the legacy .pset file
  nsresult rv = oldPsetFile->Create(nsIFile::NORMAL_FILE_TYPE, 0666);
  EXPECT_EQ(rv, NS_OK);

  bool exists;
  rv = oldPsetFile->Exists(&exists);
  EXPECT_EQ(rv, NS_OK);
  EXPECT_EQ(exists, true);

  // Setup the data in lookup cache and write its data to disk
  RefPtr<Classifier> classifier = GetClassifier();
  _PrefixArray array = {CreatePrefixFromURL("entry.com/", 4)};
  rv = BuildLookupCache(classifier, GTEST_TABLE_V4, array);
  EXPECT_EQ(rv, NS_OK);

  RefPtr<LookupCache> cache = classifier->GetLookupCache(GTEST_TABLE_V4, false);
  rv = cache->WriteFile();
  EXPECT_EQ(rv, NS_OK);

  // .vlpset should exist while .pset should be removed
  rv = newPsetFile->Exists(&exists);
  EXPECT_EQ(rv, NS_OK);
  EXPECT_EQ(exists, true);

  rv = oldPsetFile->Exists(&exists);
  EXPECT_EQ(rv, NS_OK);
  EXPECT_EQ(exists, false);

  newPsetFile->Remove(false);
}

// Test the legacy load
TEST(UrlClassifierLookupCacheV4, LoadOldPset)
{
  nsCOMPtr<nsIFile> oldPsetFile;

  _PrefixArray array = {CreatePrefixFromURL("entry.com/", 4)};
  PrefixStringMap map;
  PrefixArrayToPrefixStringMap(array, map);

  // Prepare .pset file on disk
  {
    NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR,
                           getter_AddRefs(oldPsetFile));
    oldPsetFile->AppendNative(NS_LITERAL_CSTRING("safebrowsing"));
    oldPsetFile->AppendNative(GTEST_TABLE_V4 + NS_LITERAL_CSTRING(".pset"));

    RefPtr<VariableLengthPrefixSet> pset = new VariableLengthPrefixSet;
    pset->SetPrefixes(map);

    nsCOMPtr<nsIOutputStream> stream;
    nsresult rv =
        NS_NewLocalFileOutputStream(getter_AddRefs(stream), oldPsetFile);
    EXPECT_EQ(rv, NS_OK);

    rv = pset->WritePrefixes(stream);
    EXPECT_EQ(rv, NS_OK);
  }

  // Load data from disk
  RefPtr<Classifier> classifier = GetClassifier();
  RefPtr<LookupCache> cache = classifier->GetLookupCache(GTEST_TABLE_V4, false);

  RefPtr<LookupCacheV4> cacheV4 = LookupCache::Cast<LookupCacheV4>(cache);
  CheckContent(cacheV4, array);

  oldPsetFile->Remove(false);
}

TEST(UrlClassifierLookupCacheV4, BuildAPI)
{
  _PrefixArray init = {_Prefix("alph")};
  RefPtr<LookupCacheV4> cache = SetupLookupCache<LookupCacheV4>(init);

  _PrefixArray update = {_Prefix("beta")};
  PrefixStringMap map;
  PrefixArrayToPrefixStringMap(update, map);

  cache->Build(map);
  EXPECT_TRUE(map.IsEmpty());

  CheckContent(cache, update);
}
