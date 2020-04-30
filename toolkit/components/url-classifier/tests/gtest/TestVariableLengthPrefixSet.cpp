/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "LookupCacheV4.h"
#include "mozilla/Preferences.h"
#include <mozilla/RefPtr.h>
#include "nsAppDirectoryServiceDefs.h"
#include "nsClassHashtable.h"
#include "nsString.h"
#include "VariableLengthPrefixSet.h"

#include "Common.h"

// Create fullhash by appending random characters.
static nsCString CreateFullHash(const nsACString& in) {
  nsCString out(in);
  out.SetLength(32);
  for (size_t i = in.Length(); i < 32; i++) {
    out.SetCharAt(char(rand() % 256), i);
  }

  return out;
}

// This function generate N prefixes with size between MIN and MAX.
// The output array will not be cleared, random result will append to it
static void RandomPrefixes(uint32_t N, uint32_t MIN, uint32_t MAX,
                           _PrefixArray& array) {
  array.SetCapacity(array.Length() + N);

  uint32_t range = (MAX - MIN + 1);

  for (uint32_t i = 0; i < N; i++) {
    uint32_t prefixSize = (rand() % range) + MIN;
    _Prefix prefix;
    prefix.SetLength(prefixSize);

    bool added = false;
    while (!added) {
      char* dst = prefix.BeginWriting();
      for (uint32_t j = 0; j < prefixSize; j++) {
        dst[j] = rand() % 256;
      }

      if (!array.Contains(prefix)) {
        array.AppendElement(prefix);
        added = true;
      }
    }
  }
}

// This test loops through all the prefixes and converts each prefix to
// fullhash by appending random characters, each converted fullhash
// should at least match its original length in the prefixSet.
static void DoExpectedLookup(LookupCacheV4* cache, _PrefixArray& array) {
  uint32_t matchLength = 0;
  for (uint32_t i = 0; i < array.Length(); i++) {
    const nsCString& prefix = array[i];
    Completion complete;
    complete.Assign(CreateFullHash(prefix));

    // Find match for prefix-generated full hash
    bool has, confirmed;
    cache->Has(complete, &has, &matchLength, &confirmed);
    MOZ_ASSERT(matchLength != 0);

    if (matchLength != prefix.Length()) {
      // Return match size is not the same as prefix size.
      // In this case it could be because the generated fullhash match other
      // prefixes, check if this prefix exist.
      bool found = false;

      for (uint32_t j = 0; j < array.Length(); j++) {
        if (array[j].Length() != matchLength) {
          continue;
        }

        if (0 == memcmp(complete.buf, array[j].BeginReading(), matchLength)) {
          found = true;
          break;
        }
      }
      ASSERT_TRUE(found);
    }
  }
}

static void DoRandomLookup(LookupCacheV4* cache, uint32_t N,
                           _PrefixArray& array) {
  for (uint32_t i = 0; i < N; i++) {
    // Random 32-bytes test fullhash
    char buf[32];
    for (uint32_t j = 0; j < 32; j++) {
      buf[j] = (char)(rand() % 256);
    }

    // Get the expected result.
    nsTArray<uint32_t> expected;
    for (uint32_t j = 0; j < array.Length(); j++) {
      const nsACString& str = array[j];
      if (0 == memcmp(buf, str.BeginReading(), str.Length())) {
        expected.AppendElement(str.Length());
      }
    }

    Completion complete;
    complete.Assign(nsDependentCSubstring(buf, 32));
    bool has, confirmed;
    uint32_t matchLength = 0;
    cache->Has(complete, &has, &matchLength, &confirmed);

    ASSERT_TRUE(expected.IsEmpty() ? !matchLength
                                   : expected.Contains(matchLength));
  }
}

static already_AddRefed<LookupCacheV4> SetupLookupCache(
    const nsACString& aName) {
  nsCOMPtr<nsIFile> rootDir;
  NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR, getter_AddRefs(rootDir));

  nsAutoCString provider("test");
  RefPtr<LookupCacheV4> lookup = new LookupCacheV4(aName, provider, rootDir);
  lookup->Init();

  return lookup.forget();
}

class UrlClassifierPrefixSetTest : public ::testing::TestWithParam<uint32_t> {
 protected:
  void SetUp() override {
    // max_array_size to 0 means we are testing delta algorithm here.
    static const char prefKey[] =
        "browser.safebrowsing.prefixset.max_array_size";
    mozilla::Preferences::SetUint(prefKey, GetParam());

    mCache = SetupLookupCache(NS_LITERAL_CSTRING("test"));
  }

  void TearDown() override {
    mCache = nullptr;
    mArray.Clear();
    mMap.Clear();
  }

  nsresult SetupPrefixes(_PrefixArray&& aArray) {
    mArray = std::move(aArray);
    PrefixArrayToPrefixStringMap(mArray, mMap);
    return mCache->Build(mMap);
  }

  void SetupPrefixesAndVerify(_PrefixArray& aArray) {
    mArray = aArray.Clone();
    PrefixArrayToPrefixStringMap(mArray, mMap);

    ASSERT_TRUE(NS_SUCCEEDED(mCache->Build(mMap)));
    Verify();
  }

  void SetupPrefixesAndVerify(_PrefixArray&& aArray) {
    nsresult rv = SetupPrefixes(std::move(aArray));
    ASSERT_TRUE(NS_SUCCEEDED(rv));
    Verify();
  }

  void SetupRandomPrefixesAndVerify(uint32_t N, uint32_t MIN, uint32_t MAX) {
    srand(time(nullptr));
    RandomPrefixes(N, MIN, MAX, mArray);
    PrefixArrayToPrefixStringMap(mArray, mMap);

    ASSERT_TRUE(NS_SUCCEEDED(mCache->Build(mMap)));
    Verify();
  }

  void Verify() {
    DoExpectedLookup(mCache, mArray);
    DoRandomLookup(mCache, 1000, mArray);
    CheckContent(mCache, mArray);
  }

  RefPtr<LookupCacheV4> mCache;
  _PrefixArray mArray;
  PrefixStringMap mMap;
};

// Test setting prefix set with only 4-bytes prefixes
TEST_P(UrlClassifierPrefixSetTest, FixedLengthSet) {
  SetupPrefixesAndVerify({
      _Prefix("alph"),
      _Prefix("brav"),
      _Prefix("char"),
      _Prefix("delt"),
      _Prefix("echo"),
      _Prefix("foxt"),
  });
}

TEST_P(UrlClassifierPrefixSetTest, FixedLengthRandomSet) {
  SetupRandomPrefixesAndVerify(1500, 4, 4);
}

TEST_P(UrlClassifierPrefixSetTest, FixedLengthRandomLargeSet) {
  SetupRandomPrefixesAndVerify(15000, 4, 4);
}

TEST_P(UrlClassifierPrefixSetTest, FixedLengthTinySet) {
  SetupPrefixesAndVerify({
      _Prefix("tiny"),
  });
}

// Test setting prefix set with only 5~32 bytes prefixes
TEST_P(UrlClassifierPrefixSetTest, VariableLengthSet) {
  SetupPrefixesAndVerify(
      {_Prefix("bravo"), _Prefix("charlie"), _Prefix("delta"),
       _Prefix("EchoEchoEchoEchoEcho"), _Prefix("foxtrot"),
       _Prefix("GolfGolfGolfGolfGolfGolfGolfGolf"), _Prefix("hotel"),
       _Prefix("november"), _Prefix("oscar"), _Prefix("quebec"),
       _Prefix("romeo"), _Prefix("sierrasierrasierrasierrasierra"),
       _Prefix("Tango"), _Prefix("whiskey"), _Prefix("yankee"),
       _Prefix("ZuluZuluZuluZulu")});
}

TEST_P(UrlClassifierPrefixSetTest, VariableLengthRandomSet) {
  SetupRandomPrefixesAndVerify(1500, 5, 32);
}

// Test setting prefix set with both 4-bytes prefixes and 5~32 bytes prefixes
TEST_P(UrlClassifierPrefixSetTest, MixedPrefixSet) {
  SetupPrefixesAndVerify(
      {_Prefix("enus"), _Prefix("apollo"), _Prefix("mars"),
       _Prefix("Hecatonchires cyclopes"), _Prefix("vesta"), _Prefix("neptunus"),
       _Prefix("jupiter"), _Prefix("diana"), _Prefix("minerva"),
       _Prefix("ceres"), _Prefix("Aidos,Adephagia,Adikia,Aletheia"),
       _Prefix("hecatonchires"), _Prefix("alcyoneus"), _Prefix("hades"),
       _Prefix("vulcanus"), _Prefix("juno"), _Prefix("mercury"),
       _Prefix("Stheno, Euryale and Medusa")});
}

TEST_P(UrlClassifierPrefixSetTest, MixedRandomPrefixSet) {
  SetupRandomPrefixesAndVerify(1500, 4, 32);
}

// Test resetting prefix set
TEST_P(UrlClassifierPrefixSetTest, ResetPrefix) {
  // Base prefix set
  _PrefixArray oldArray = {
      _Prefix("Iceland"),   _Prefix("Peru"),    _Prefix("Mexico"),
      _Prefix("Australia"), _Prefix("Japan"),   _Prefix("Egypt"),
      _Prefix("America"),   _Prefix("Finland"), _Prefix("Germany"),
      _Prefix("Italy"),     _Prefix("France"),  _Prefix("Taiwan"),
  };
  SetupPrefixesAndVerify(oldArray);

  // New prefix set
  _PrefixArray newArray = {
      _Prefix("Pikachu"),    _Prefix("Bulbasaur"), _Prefix("Charmander"),
      _Prefix("Blastoise"),  _Prefix("Pidgey"),    _Prefix("Mewtwo"),
      _Prefix("Jigglypuff"), _Prefix("Persian"),   _Prefix("Tentacool"),
      _Prefix("Onix"),       _Prefix("Eevee"),     _Prefix("Jynx"),
  };
  SetupPrefixesAndVerify(newArray);

  // Should not match any of the first prefix set
  uint32_t matchLength = 0;
  for (uint32_t i = 0; i < oldArray.Length(); i++) {
    Completion complete;
    complete.Assign(CreateFullHash(oldArray[i]));

    // Find match for prefix-generated full hash
    bool has, confirmed;
    mCache->Has(complete, &has, &matchLength, &confirmed);

    ASSERT_TRUE(matchLength == 0);
  }
}

// Test only set one 4-bytes prefix and one full-length prefix
TEST_P(UrlClassifierPrefixSetTest, TinyPrefixSet) {
  SetupPrefixesAndVerify({
      _Prefix("AAAA"),
      _Prefix("11112222333344445555666677778888"),
  });
}

// Test empty prefix set and IsEmpty function
TEST_P(UrlClassifierPrefixSetTest, EmptyFixedPrefixSet) {
  ASSERT_TRUE(mCache->IsEmpty());

  SetupPrefixesAndVerify({});

  // Insert an 4-bytes prefix, then IsEmpty should return false
  SetupPrefixesAndVerify({_Prefix("test")});

  ASSERT_TRUE(!mCache->IsEmpty());
}

TEST_P(UrlClassifierPrefixSetTest, EmptyVariableLengthPrefixSet) {
  ASSERT_TRUE(mCache->IsEmpty());

  SetupPrefixesAndVerify({});

  // Insert an 5~32 bytes prefix, then IsEmpty should return false
  SetupPrefixesAndVerify({_Prefix("test variable length")});

  ASSERT_TRUE(!mCache->IsEmpty());
}

// Test prefix size should only between 4~32 bytes
TEST_P(UrlClassifierPrefixSetTest, MinMaxPrefixSet) {
  // Test prefix set between 4-32 bytes, should success
  SetupPrefixesAndVerify({_Prefix("1234"), _Prefix("ABCDEFGHIJKKMNOP"),
                          _Prefix("1aaa2bbb3ccc4ddd5eee6fff7ggg8hhh")});

  // Prefix size less than 4-bytes should fail
  nsresult rv = SetupPrefixes({_Prefix("123")});
  ASSERT_TRUE(NS_FAILED(rv));

  // Prefix size greater than 32-bytes should fail
  rv = SetupPrefixes({_Prefix("1aaa2bbb3ccc4ddd5eee6fff7ggg8hhh9")});
  ASSERT_TRUE(NS_FAILED(rv));
}

// Test save then load prefix set with only 4-bytes prefixes
TEST_P(UrlClassifierPrefixSetTest, LoadSaveFixedLengthPrefixSet) {
  nsCOMPtr<nsIFile> file;
  _PrefixArray array;
  PrefixStringMap map;

  // Save
  {
    RefPtr<LookupCacheV4> save =
        SetupLookupCache(NS_LITERAL_CSTRING("test-save"));

    RandomPrefixes(10000, 4, 4, array);

    PrefixArrayToPrefixStringMap(array, map);
    save->Build(map);

    DoExpectedLookup(save, array);
    DoRandomLookup(save, 1000, array);
    CheckContent(save, array);

    NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR, getter_AddRefs(file));
    file->Append(NS_LITERAL_STRING("test.vlpset"));
    save->StoreToFile(file);
  }

  // Load
  {
    RefPtr<LookupCacheV4> load =
        SetupLookupCache(NS_LITERAL_CSTRING("test-load"));
    load->LoadFromFile(file);

    DoExpectedLookup(load, array);
    DoRandomLookup(load, 1000, array);
    CheckContent(load, array);
  }

  file->Remove(false);
}

// Test save then load prefix set with only 5~32 bytes prefixes
TEST_P(UrlClassifierPrefixSetTest, LoadSaveVariableLengthPrefixSet) {
  nsCOMPtr<nsIFile> file;
  _PrefixArray array;
  PrefixStringMap map;

  // Save
  {
    RefPtr<LookupCacheV4> save =
        SetupLookupCache(NS_LITERAL_CSTRING("test-save"));

    RandomPrefixes(10000, 5, 32, array);

    PrefixArrayToPrefixStringMap(array, map);
    save->Build(map);

    DoExpectedLookup(save, array);
    DoRandomLookup(save, 1000, array);
    CheckContent(save, array);

    NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR, getter_AddRefs(file));
    file->Append(NS_LITERAL_STRING("test.vlpset"));
    save->StoreToFile(file);
  }

  // Load
  {
    RefPtr<LookupCacheV4> load =
        SetupLookupCache(NS_LITERAL_CSTRING("test-load"));
    load->LoadFromFile(file);

    DoExpectedLookup(load, array);
    DoRandomLookup(load, 1000, array);
    CheckContent(load, array);
  }

  file->Remove(false);
}

// Test save then load prefix with both 4 bytes prefixes and 5~32 bytes prefixes
TEST_P(UrlClassifierPrefixSetTest, LoadSavePrefixSet) {
  nsCOMPtr<nsIFile> file;
  _PrefixArray array;
  PrefixStringMap map;

  // Save
  {
    RefPtr<LookupCacheV4> save =
        SetupLookupCache(NS_LITERAL_CSTRING("test-save"));

    // Try to simulate the real case that most prefixes are 4bytes
    RandomPrefixes(20000, 4, 4, array);
    RandomPrefixes(1000, 5, 32, array);

    PrefixArrayToPrefixStringMap(array, map);
    save->Build(map);

    DoExpectedLookup(save, array);
    DoRandomLookup(save, 1000, array);
    CheckContent(save, array);

    NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR, getter_AddRefs(file));
    file->Append(NS_LITERAL_STRING("test.vlpset"));
    save->StoreToFile(file);
  }

  // Load
  {
    RefPtr<LookupCacheV4> load =
        SetupLookupCache(NS_LITERAL_CSTRING("test-load"));
    load->LoadFromFile(file);

    DoExpectedLookup(load, array);
    DoRandomLookup(load, 1000, array);
    CheckContent(load, array);
  }

  file->Remove(false);
}

// This is for fixed-length prefixset
TEST_P(UrlClassifierPrefixSetTest, LoadSaveNoDelta) {
  nsCOMPtr<nsIFile> file;
  _PrefixArray array;
  PrefixStringMap map;

  for (uint32_t i = 0; i < 100; i++) {
    // construct a tree without deltas by making the distance
    // between entries larger than 16 bits
    uint32_t v = ((1 << 16) + 1) * i;
    nsCString* ele = array.AppendElement();
    ele->AppendASCII(reinterpret_cast<const char*>(&v), 4);
  }

  // Save
  {
    RefPtr<LookupCacheV4> save =
        SetupLookupCache(NS_LITERAL_CSTRING("test-save"));

    PrefixArrayToPrefixStringMap(array, map);
    save->Build(map);

    DoExpectedLookup(save, array);
    DoRandomLookup(save, 1000, array);
    CheckContent(save, array);

    NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR, getter_AddRefs(file));
    file->Append(NS_LITERAL_STRING("test.vlpset"));
    save->StoreToFile(file);
  }

  // Load
  {
    RefPtr<LookupCacheV4> load =
        SetupLookupCache(NS_LITERAL_CSTRING("test-load"));
    load->LoadFromFile(file);

    DoExpectedLookup(load, array);
    DoRandomLookup(load, 1000, array);
    CheckContent(load, array);
  }

  file->Remove(false);
}

// To run the same test for different configurations of
// "browser_safebrowsing_prefixset_max_array_size"
INSTANTIATE_TEST_CASE_P(UrlClassifierPrefixSetTest, UrlClassifierPrefixSetTest,
                        ::testing::Values(0, UINT32_MAX));
