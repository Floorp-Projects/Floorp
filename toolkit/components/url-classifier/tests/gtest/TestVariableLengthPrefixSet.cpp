/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <mozilla/RefPtr.h>
#include "nsString.h"
#include "nsTArray.h"
#include "nsClassHashtable.h"
#include "VariableLengthPrefixSet.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsIFile.h"
#include "gtest/gtest.h"

using namespace mozilla::safebrowsing;

typedef nsCString _Prefix;
typedef nsTArray<_Prefix> _PrefixArray;

// Create fullhash by appending random characters.
static nsCString* CreateFullHash(const nsACString& in)
{
  nsCString* out = new nsCString(in);
  out->SetLength(32);
  for (size_t i = in.Length(); i < 32; i++) {
    out->SetCharAt(char(rand() % 256), i);
  }

  return out;
}

// This function generate N prefixes with size between MIN and MAX.
// The output array will not be cleared, random result will append to it
static void RandomPrefixes(uint32_t N, uint32_t MIN, uint32_t MAX, _PrefixArray& array)
{
  array.SetCapacity(array.Length() + N);

  uint32_t range = (MAX - MIN + 1);

  for (uint32_t i = 0; i < N; i++) {
    uint32_t prefixSize = (rand() % range) + MIN;
    _Prefix prefix;
    prefix.SetLength(prefixSize);

    bool added = false;
    while(!added) {
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

static void CheckContent(VariableLengthPrefixSet* pset,
                         PrefixStringMap& expected)
{
  PrefixStringMap vlPSetMap;
  pset->GetPrefixes(vlPSetMap);

  for (auto iter = vlPSetMap.Iter(); !iter.Done(); iter.Next()) {
    nsCString* expectedPrefix = expected.Get(iter.Key());
    nsCString* resultPrefix = iter.Data();

    ASSERT_TRUE(resultPrefix->Equals(*expectedPrefix));
  }
}

// This test loops through all the prefixes and converts each prefix to
// fullhash by appending random characters, each converted fullhash
// should at least match its original length in the prefixSet.
static void DoExpectedLookup(VariableLengthPrefixSet* pset,
                             _PrefixArray& array)
{
  uint32_t matchLength = 0;
  for (uint32_t i = 0; i < array.Length(); i++) {
    const nsCString& prefix = array[i];
    UniquePtr<nsCString> fullhash(CreateFullHash(prefix));

    // Find match for prefix-generated full hash
    pset->Matches(*fullhash, &matchLength);
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

        if (0 == memcmp(fullhash->BeginReading(),
                        array[j].BeginReading(),
                        matchLength)) {
          found = true;
          break;
        }
      }
      ASSERT_TRUE(found);
    }
  }
}

static void DoRandomLookup(VariableLengthPrefixSet* pset,
                           uint32_t N,
                           _PrefixArray& array)
{
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

    uint32_t matchLength = 0;
    pset->Matches(nsDependentCSubstring(buf, 32), &matchLength);

    ASSERT_TRUE(expected.IsEmpty() ? !matchLength : expected.Contains(matchLength));
  }
}

static void SetupPrefixMap(const _PrefixArray& array,
                             PrefixStringMap& map)
{
  map.Clear();

  // Buckets are keyed by prefix length and contain an array of
  // all prefixes of that length.
  nsClassHashtable<nsUint32HashKey, _PrefixArray> table;

  for (uint32_t i = 0; i < array.Length(); i++) {
    _PrefixArray* prefixes = table.Get(array[i].Length());
    if (!prefixes) {
      prefixes = new _PrefixArray();
      table.Put(array[i].Length(), prefixes);
    }

    prefixes->AppendElement(array[i]);
  }

  // The resulting map entries will be a concatenation of all
  // prefix data for the prefixes of a given size.
  for (auto iter = table.Iter(); !iter.Done(); iter.Next()) {
    uint32_t size = iter.Key();
    uint32_t count = iter.Data()->Length();

    _Prefix* str = new _Prefix();
    str->SetLength(size * count);

    char* dst = str->BeginWriting();

    iter.Data()->Sort();
    for (uint32_t i = 0; i < count; i++) {
      memcpy(dst, iter.Data()->ElementAt(i).get(), size);
      dst += size;
    }

    map.Put(size, str);
  }
}


// Test setting prefix set with only 4-bytes prefixes
TEST(VariableLengthPrefixSet, FixedLengthSet)
{
  srand(time(nullptr));

  RefPtr<VariableLengthPrefixSet> pset = new VariableLengthPrefixSet;
  pset->Init(NS_LITERAL_CSTRING("test"));

  PrefixStringMap map;
  _PrefixArray array = { _Prefix("alph"), _Prefix("brav"), _Prefix("char"),
                         _Prefix("delt"), _Prefix("echo"), _Prefix("foxt"),
                       };

  SetupPrefixMap(array, map);
  pset->SetPrefixes(map);

  DoExpectedLookup(pset, array);

  DoRandomLookup(pset, 1000, array);

  CheckContent(pset, map);

  // Run random test
  array.Clear();
  map.Clear();

  RandomPrefixes(1500, 4, 4, array);

  SetupPrefixMap(array, map);
  pset->SetPrefixes(map);

  DoExpectedLookup(pset, array);

  DoRandomLookup(pset, 1000, array);

  CheckContent(pset, map);
}

// Test setting prefix set with only 5~32 bytes prefixes
TEST(VariableLengthPrefixSet, VariableLengthSet)
{
  RefPtr<VariableLengthPrefixSet> pset = new VariableLengthPrefixSet;
  pset->Init(NS_LITERAL_CSTRING("test"));

  PrefixStringMap map;
  _PrefixArray array = { _Prefix("bravo"), _Prefix("charlie"), _Prefix("delta"),
                         _Prefix("EchoEchoEchoEchoEcho"), _Prefix("foxtrot"),
                         _Prefix("GolfGolfGolfGolfGolfGolfGolfGolf"),
                         _Prefix("hotel"), _Prefix("november"),
                         _Prefix("oscar"), _Prefix("quebec"), _Prefix("romeo"),
                         _Prefix("sierrasierrasierrasierrasierra"),
                         _Prefix("Tango"), _Prefix("whiskey"), _Prefix("yankee"),
                         _Prefix("ZuluZuluZuluZulu")
                       };

  SetupPrefixMap(array, map);
  pset->SetPrefixes(map);

  DoExpectedLookup(pset, array);

  DoRandomLookup(pset, 1000, array);

  CheckContent(pset, map);

  // Run random test
  array.Clear();
  map.Clear();

  RandomPrefixes(1500, 5, 32, array);

  SetupPrefixMap(array, map);
  pset->SetPrefixes(map);

  DoExpectedLookup(pset, array);

  DoRandomLookup(pset, 1000, array);

  CheckContent(pset, map);

}

// Test setting prefix set with both 4-bytes prefixes and 5~32 bytes prefixes
TEST(VariableLengthPrefixSet, MixedPrefixSet)
{
  RefPtr<VariableLengthPrefixSet> pset = new VariableLengthPrefixSet;
  pset->Init(NS_LITERAL_CSTRING("test"));

  PrefixStringMap map;
  _PrefixArray array = { _Prefix("enus"), _Prefix("apollo"), _Prefix("mars"),
                         _Prefix("Hecatonchires cyclopes"),
                         _Prefix("vesta"), _Prefix("neptunus"), _Prefix("jupiter"),
                         _Prefix("diana"), _Prefix("minerva"), _Prefix("ceres"),
                         _Prefix("Aidos,Adephagia,Adikia,Aletheia"),
                         _Prefix("hecatonchires"), _Prefix("alcyoneus"), _Prefix("hades"),
                         _Prefix("vulcanus"), _Prefix("juno"), _Prefix("mercury"),
                         _Prefix("Stheno, Euryale and Medusa")
                       };

  SetupPrefixMap(array, map);
  pset->SetPrefixes(map);

  DoExpectedLookup(pset, array);

  DoRandomLookup(pset, 1000, array);

  CheckContent(pset, map);

  // Run random test
  array.Clear();
  map.Clear();

  RandomPrefixes(1500, 4, 32, array);

  SetupPrefixMap(array, map);
  pset->SetPrefixes(map);

  DoExpectedLookup(pset, array);

  DoRandomLookup(pset, 1000, array);

  CheckContent(pset, map);
}

// Test resetting prefix set
TEST(VariableLengthPrefixSet, ResetPrefix)
{
  RefPtr<VariableLengthPrefixSet> pset = new VariableLengthPrefixSet;
  pset->Init(NS_LITERAL_CSTRING("test"));

  // First prefix set
  _PrefixArray array1 = { _Prefix("Iceland"), _Prefix("Peru"), _Prefix("Mexico"),
                          _Prefix("Australia"), _Prefix("Japan"), _Prefix("Egypt"),
                          _Prefix("America"), _Prefix("Finland"), _Prefix("Germany"),
                          _Prefix("Italy"), _Prefix("France"), _Prefix("Taiwan"),
                        };
  {
    PrefixStringMap map;

    SetupPrefixMap(array1, map);
    pset->SetPrefixes(map);

    DoExpectedLookup(pset, array1);
  }

  // Second
  _PrefixArray array2 = { _Prefix("Pikachu"), _Prefix("Bulbasaur"), _Prefix("Charmander"),
                          _Prefix("Blastoise"), _Prefix("Pidgey"), _Prefix("Mewtwo"),
                          _Prefix("Jigglypuff"), _Prefix("Persian"), _Prefix("Tentacool"),
                          _Prefix("Onix"), _Prefix("Eevee"), _Prefix("Jynx"),
                        };
  {
    PrefixStringMap map;

    SetupPrefixMap(array2, map);
    pset->SetPrefixes(map);

    DoExpectedLookup(pset, array2);
  }

  // Should not match any of the first prefix set
  uint32_t matchLength = 0;
  for (uint32_t i = 0; i < array1.Length(); i++) {
    UniquePtr<nsACString> fullhash(CreateFullHash(array1[i]));

    pset->Matches(*fullhash, &matchLength);
    ASSERT_TRUE(matchLength == 0);
  }
}

// Test only set one 4-bytes prefix and one full-length prefix
TEST(VariableLengthPrefixSet, TinyPrefixSet)
{
  RefPtr<VariableLengthPrefixSet> pset = new VariableLengthPrefixSet;
  pset->Init(NS_LITERAL_CSTRING("test"));

  PrefixStringMap map;
  _PrefixArray array = { _Prefix("AAAA"),
                         _Prefix("11112222333344445555666677778888"),
                       };

  SetupPrefixMap(array, map);
  pset->SetPrefixes(map);

  DoExpectedLookup(pset, array);

  DoRandomLookup(pset, 1000, array);

  CheckContent(pset, map);
}

// Test empty prefix set and IsEmpty function
TEST(VariableLengthPrefixSet, EmptyPrefixSet)
{
  RefPtr<VariableLengthPrefixSet> pset = new VariableLengthPrefixSet;
  pset->Init(NS_LITERAL_CSTRING("test"));

  bool empty;
  pset->IsEmpty(&empty);
  ASSERT_TRUE(empty);

  PrefixStringMap map;
  _PrefixArray array1;

  // Lookup an empty array should never match
  DoRandomLookup(pset, 100, array1);

  // Insert an 4-bytes prefix, then IsEmpty should return false
  _PrefixArray array2 = { _Prefix("test") };
  SetupPrefixMap(array2, map);
  pset->SetPrefixes(map);

  pset->IsEmpty(&empty);
  ASSERT_TRUE(!empty);

  _PrefixArray array3 = { _Prefix("test variable length") };

  // Insert an 5~32 bytes prefix, then IsEmpty should return false
  SetupPrefixMap(array3, map);
  pset->SetPrefixes(map);

  pset->IsEmpty(&empty);
  ASSERT_TRUE(!empty);
}

// Test prefix size should only between 4~32 bytes
TEST(VariableLengthPrefixSet, MinMaxPrefixSet)
{
  RefPtr<VariableLengthPrefixSet> pset = new VariableLengthPrefixSet;
  pset->Init(NS_LITERAL_CSTRING("test"));

  PrefixStringMap map;
  {
    _PrefixArray array = { _Prefix("1234"),
                           _Prefix("ABCDEFGHIJKKMNOP"),
                           _Prefix("1aaa2bbb3ccc4ddd5eee6fff7ggg8hhh") };

    SetupPrefixMap(array, map);
    nsresult rv = pset->SetPrefixes(map);
    ASSERT_TRUE(rv == NS_OK);
  }

  // Prefix size less than 4-bytes should fail
  {
    _PrefixArray array = { _Prefix("123") };

    SetupPrefixMap(array, map);
    nsresult rv = pset->SetPrefixes(map);
    ASSERT_TRUE(NS_FAILED(rv));
  }

  // Prefix size greater than 32-bytes should fail
  {
    _PrefixArray array = { _Prefix("1aaa2bbb3ccc4ddd5eee6fff7ggg8hhh9") };

    SetupPrefixMap(array, map);
    nsresult rv = pset->SetPrefixes(map);
    ASSERT_TRUE(NS_FAILED(rv));
  }
}

// Test save then load prefix set with only 4-bytes prefixes
TEST(VariableLengthPrefixSet, LoadSaveFixedLengthPrefixSet)
{
  RefPtr<VariableLengthPrefixSet> save = new VariableLengthPrefixSet;
  save->Init(NS_LITERAL_CSTRING("test-save"));

  _PrefixArray array;
  RandomPrefixes(10000, 4, 4, array);

  PrefixStringMap map;
  SetupPrefixMap(array, map);
  save->SetPrefixes(map);

  DoExpectedLookup(save, array);

  DoRandomLookup(save, 1000, array);

  CheckContent(save, map);

  nsCOMPtr<nsIFile> file;
  NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR,
                         getter_AddRefs(file));
  file->Append(NS_LITERAL_STRING("test.vlpset"));

  save->StoreToFile(file);

  RefPtr<VariableLengthPrefixSet> load = new VariableLengthPrefixSet;
  load->Init(NS_LITERAL_CSTRING("test-load"));

  load->LoadFromFile(file);

  DoExpectedLookup(load, array);

  DoRandomLookup(load, 1000, array);

  CheckContent(load, map);

  file->Remove(false);
}

// Test save then load prefix set with only 5~32 bytes prefixes
TEST(VariableLengthPrefixSet, LoadSaveVariableLengthPrefixSet)
{
  RefPtr<VariableLengthPrefixSet> save = new VariableLengthPrefixSet;
  save->Init(NS_LITERAL_CSTRING("test-save"));

  _PrefixArray array;
  RandomPrefixes(10000, 5, 32, array);

  PrefixStringMap map;
  SetupPrefixMap(array, map);
  save->SetPrefixes(map);

  DoExpectedLookup(save, array);

  DoRandomLookup(save, 1000, array);

  CheckContent(save, map);

  nsCOMPtr<nsIFile> file;
  NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR,
                         getter_AddRefs(file));
  file->Append(NS_LITERAL_STRING("test.vlpset"));

  save->StoreToFile(file);

  RefPtr<VariableLengthPrefixSet> load = new VariableLengthPrefixSet;
  load->Init(NS_LITERAL_CSTRING("test-load"));

  load->LoadFromFile(file);

  DoExpectedLookup(load, array);

  DoRandomLookup(load, 1000, array);

  CheckContent(load, map);

  file->Remove(false);
}

// Test save then load prefix with both 4 bytes prefixes and 5~32 bytes prefixes
TEST(VariableLengthPrefixSet, LoadSavePrefixSet)
{
  RefPtr<VariableLengthPrefixSet> save = new VariableLengthPrefixSet;
  save->Init(NS_LITERAL_CSTRING("test-save"));

  // Try to simulate the real case that most prefixes are 4bytes
  _PrefixArray array;
  RandomPrefixes(20000, 4, 4, array);
  RandomPrefixes(1000, 5, 32, array);

  PrefixStringMap map;
  SetupPrefixMap(array, map);
  save->SetPrefixes(map);

  DoExpectedLookup(save, array);

  DoRandomLookup(save, 1000, array);

  CheckContent(save, map);

  nsCOMPtr<nsIFile> file;
  NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR,
                         getter_AddRefs(file));
  file->Append(NS_LITERAL_STRING("test.vlpset"));

  save->StoreToFile(file);

  RefPtr<VariableLengthPrefixSet> load = new VariableLengthPrefixSet;
  load->Init(NS_LITERAL_CSTRING("test-load"));

  load->LoadFromFile(file);

  DoExpectedLookup(load, array);

  DoRandomLookup(load, 1000, array);

  CheckContent(load, map);

  file->Remove(false);
}
