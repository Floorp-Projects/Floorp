/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Classifier.h"
#include "LookupCacheV4.h"

#include "Common.h"

static void TestReadNoiseEntries(RefPtr<Classifier> classifier,
                                 const nsCString& aTable, const nsCString& aURL,
                                 const _PrefixArray& aPrefixArray) {
  Completion lookupHash;
  lookupHash.FromPlaintext(aURL);
  RefPtr<LookupResult> result = new LookupResult;
  result->hash.complete = lookupHash;

  PrefixArray noiseEntries;
  uint32_t noiseCount = 3;
  nsresult rv;
  rv = classifier->ReadNoiseEntries(result->hash.fixedLengthPrefix, aTable,
                                    noiseCount, noiseEntries);
  ASSERT_TRUE(rv == NS_OK)
  << "ReadNoiseEntries returns an error";
  EXPECT_TRUE(noiseEntries.Length() > 0)
      << "Number of noise entries is less than 0";

  for (uint32_t i = 0; i < noiseEntries.Length(); i++) {
    // Test the noise entry should not equal the "real" hash request
    EXPECT_NE(noiseEntries[i], result->hash.fixedLengthPrefix)
        << "Noise entry is the same as real hash request";
    // Test the noise entry should exist in the cached prefix array
    nsAutoCString partialHash;
    partialHash.Assign(reinterpret_cast<char*>(&noiseEntries[i]), PREFIX_SIZE);
    EXPECT_TRUE(aPrefixArray.Contains(partialHash))
        << "Noise entry is not in the cached prefix array";
  }
}

TEST(UrlClassifierClassifier, ReadNoiseEntriesV4)
{
  RefPtr<Classifier> classifier = GetClassifier();
  _PrefixArray array = {
      CreatePrefixFromURL("bravo.com/", 5),
      CreatePrefixFromURL("browsing.com/", 9),
      CreatePrefixFromURL("gound.com/", 4),
      CreatePrefixFromURL("small.com/", 4),
      CreatePrefixFromURL("gdfad.com/", 4),
      CreatePrefixFromURL("afdfound.com/", 4),
      CreatePrefixFromURL("dffa.com/", 4),
  };

  nsresult rv;
  rv = BuildLookupCache(classifier, GTEST_TABLE_V4, array);
  ASSERT_TRUE(rv == NS_OK)
  << "Fail to build LookupCache";

  TestReadNoiseEntries(classifier, GTEST_TABLE_V4, "gound.com/"_ns, array);
}

TEST(UrlClassifierClassifier, ReadNoiseEntriesV2)
{
  RefPtr<Classifier> classifier = GetClassifier();
  _PrefixArray array = {
      CreatePrefixFromURL("helloworld.com/", 4),
      CreatePrefixFromURL("firefox.com/", 4),
      CreatePrefixFromURL("chrome.com/", 4),
      CreatePrefixFromURL("safebrowsing.com/", 4),
      CreatePrefixFromURL("opera.com/", 4),
      CreatePrefixFromURL("torbrowser.com/", 4),
      CreatePrefixFromURL("gfaads.com/", 4),
      CreatePrefixFromURL("qggdsas.com/", 4),
      CreatePrefixFromURL("nqtewq.com/", 4),
  };

  nsresult rv;
  rv = BuildLookupCache(classifier, GTEST_TABLE_V2, array);
  ASSERT_TRUE(rv == NS_OK)
  << "Fail to build LookupCache";

  TestReadNoiseEntries(classifier, GTEST_TABLE_V2, "helloworld.com/"_ns, array);
}
