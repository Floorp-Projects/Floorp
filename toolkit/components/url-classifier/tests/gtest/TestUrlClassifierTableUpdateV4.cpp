/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

#include "Common.h"
#include "Classifier.h"
#include "HashStore.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsIFile.h"
#include "nsIThread.h"
#include "string.h"
#include "gtest/gtest.h"
#include "nsThreadUtils.h"

using namespace mozilla;
using namespace mozilla::safebrowsing;

typedef nsCString _Prefix;
typedef nsTArray<_Prefix> _PrefixArray;

#define GTEST_SAFEBROWSING_DIR NS_LITERAL_CSTRING("safebrowsing")
#define GTEST_TABLE NS_LITERAL_CSTRING("gtest-malware-proto")
#define GTEST_PREFIXFILE NS_LITERAL_CSTRING("gtest-malware-proto.pset")

// This function removes common elements of inArray and outArray from
// outArray. This is used by partial update testcase to ensure partial update
// data won't contain prefixes we already have.
static void
RemoveIntersection(const _PrefixArray& inArray, _PrefixArray& outArray)
{
  for (uint32_t i = 0; i < inArray.Length(); i++) {
    int32_t idx = outArray.BinaryIndexOf(inArray[i]);
    if (idx >= 0) {
      outArray.RemoveElementAt(idx);
    }
  }
}

// This fucntion removes elements from outArray by index specified in
// removal array.
static void
RemoveElements(const nsTArray<uint32_t>& removal, _PrefixArray& outArray)
{
  for (int32_t i = removal.Length() - 1; i >= 0; i--) {
    outArray.RemoveElementAt(removal[i]);
  }
}

static void
MergeAndSortArray(const _PrefixArray& array1,
                  const _PrefixArray& array2,
                  _PrefixArray& output)
{
  output.Clear();
  output.AppendElements(array1);
  output.AppendElements(array2);
  output.Sort();
}

static void
CalculateCheckSum(_PrefixArray& prefixArray, nsCString& checksum)
{
  prefixArray.Sort();

  nsresult rv;
  nsCOMPtr<nsICryptoHash> cryptoHash =
    do_CreateInstance(NS_CRYPTO_HASH_CONTRACTID, &rv);

  cryptoHash->Init(nsICryptoHash::SHA256);
  for (uint32_t i = 0; i < prefixArray.Length(); i++) {
    const _Prefix& prefix = prefixArray[i];
    cryptoHash->Update(reinterpret_cast<uint8_t*>(
                       const_cast<char*>(prefix.get())), prefix.Length());
  }
  cryptoHash->Finish(false, checksum);
}

// N: Number of prefixes, MIN/MAX: minimum/maximum prefix size
// This function will append generated prefixes to outArray.
static void
CreateRandomSortedPrefixArray(uint32_t N,
                              uint32_t MIN,
                              uint32_t MAX,
                              _PrefixArray& outArray)
{
  outArray.SetCapacity(outArray.Length() + N);

  const uint32_t range = (MAX - MIN + 1);

  for (uint32_t i = 0; i < N; i++) {
    uint32_t prefixSize = (rand() % range) + MIN;
    _Prefix prefix;
    prefix.SetLength(prefixSize);

    while (true) {
      char* dst = prefix.BeginWriting();
      for (uint32_t j = 0; j < prefixSize; j++) {
        dst[j] = rand() % 256;
      }

      if (!outArray.Contains(prefix)) {
        outArray.AppendElement(prefix);
        break;
      }
    }
  }

  outArray.Sort();
}

// N: Number of removal indices, MAX: maximum index
static void
CreateRandomRemovalIndices(uint32_t N,
                           uint32_t MAX,
                           nsTArray<uint32_t>& outArray)
{
  for (uint32_t i = 0; i < N; i++) {
    uint32_t idx = rand() % MAX;
    if (!outArray.Contains(idx)) {
      outArray.InsertElementSorted(idx);
    }
  }
}

// Function to generate TableUpdateV4.
static void
GenerateUpdateData(bool fullUpdate,
                   PrefixStringMap& add,
                   nsTArray<uint32_t>* removal,
                   nsCString* checksum,
                   nsTArray<TableUpdate*>& tableUpdates)
{
  TableUpdateV4* tableUpdate = new TableUpdateV4(GTEST_TABLE);
  tableUpdate->SetFullUpdate(fullUpdate);

  for (auto iter = add.ConstIter(); !iter.Done(); iter.Next()) {
    nsCString* pstring = iter.Data();
    std::string str(pstring->BeginReading(), pstring->Length());

    tableUpdate->NewPrefixes(iter.Key(), str);
  }

  if (removal) {
    tableUpdate->NewRemovalIndices(removal->Elements(), removal->Length());
  }

  if (checksum) {
    std::string stdChecksum;
    stdChecksum.assign(const_cast<char*>(checksum->BeginReading()), checksum->Length());

    tableUpdate->NewChecksum(stdChecksum);
  }

  tableUpdates.AppendElement(tableUpdate);
}

static void
VerifyPrefixSet(PrefixStringMap& expected)
{
  // Verify the prefix set is written to disk.
  nsCOMPtr<nsIFile> file;
  NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR, getter_AddRefs(file));

  file->AppendNative(GTEST_SAFEBROWSING_DIR);
  file->AppendNative(GTEST_PREFIXFILE);

  RefPtr<VariableLengthPrefixSet> load = new VariableLengthPrefixSet;
  load->Init(GTEST_TABLE);

  PrefixStringMap prefixesInFile;
  load->LoadFromFile(file);
  load->GetPrefixes(prefixesInFile);

  for (auto iter = expected.ConstIter(); !iter.Done(); iter.Next()) {
    nsCString* expectedPrefix = iter.Data();
    nsCString* resultPrefix = prefixesInFile.Get(iter.Key());

    ASSERT_TRUE(*resultPrefix == *expectedPrefix);
  }
}

static void
Clear()
{
  nsCOMPtr<nsIFile> file;
  NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR, getter_AddRefs(file));

  UniquePtr<Classifier> classifier(new Classifier());
  classifier->Open(*file);
  classifier->Reset();
}

static void
testUpdateFail(nsTArray<TableUpdate*>& tableUpdates)
{
  nsCOMPtr<nsIFile> file;
  NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR, getter_AddRefs(file));

  UniquePtr<Classifier> classifier(new Classifier());
  classifier->Open(*file);

  RunTestInNewThread([&] () -> void {
    nsresult rv = classifier->ApplyUpdates(&tableUpdates);
    ASSERT_TRUE(NS_FAILED(rv));
  });
}

static void
testUpdate(nsTArray<TableUpdate*>& tableUpdates,
           PrefixStringMap& expected)
{
  nsCOMPtr<nsIFile> file;
  NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR, getter_AddRefs(file));

  UniquePtr<Classifier> classifier(new Classifier());
  classifier->Open(*file);

  RunTestInNewThread([&] () -> void {
    nsresult rv = classifier->ApplyUpdates(&tableUpdates);
    ASSERT_TRUE(rv == NS_OK);

    VerifyPrefixSet(expected);
  });
}

static void
testFullUpdate(PrefixStringMap& add, nsCString* checksum)
{
  nsTArray<TableUpdate*> tableUpdates;

  GenerateUpdateData(true, add, nullptr, checksum, tableUpdates);

  testUpdate(tableUpdates, add);
}

static void
testPartialUpdate(PrefixStringMap& add,
                  nsTArray<uint32_t>* removal,
                  nsCString* checksum,
                  PrefixStringMap& expected)
{
  nsTArray<TableUpdate*> tableUpdates;
  GenerateUpdateData(false, add, removal, checksum, tableUpdates);

  testUpdate(tableUpdates, expected);
}

static void
testOpenLookupCache()
{
  nsCOMPtr<nsIFile> file;
  NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR, getter_AddRefs(file));
  file->AppendNative(GTEST_SAFEBROWSING_DIR);

  RunTestInNewThread([&] () -> void {
    LookupCacheV4 cache(nsCString(GTEST_TABLE), file);
    nsresult rv = cache.Init();
    ASSERT_EQ(rv, NS_OK);

    rv = cache.Open();
    ASSERT_EQ(rv, NS_OK);
  });
}

// Tests start from here.
TEST(UrlClassifierTableUpdateV4, FixLenghtPSetFullUpdate)
{
  srand(time(NULL));

   _PrefixArray array;
  PrefixStringMap map;
  nsCString checksum;

  CreateRandomSortedPrefixArray(5000, 4, 4, array);
  PrefixArrayToPrefixStringMap(array, map);
  CalculateCheckSum(array, checksum);

  testFullUpdate(map, &checksum);

  Clear();
}

TEST(UrlClassifierTableUpdateV4, VariableLenghtPSetFullUpdate)
{
   _PrefixArray array;
  PrefixStringMap map;
  nsCString checksum;

  CreateRandomSortedPrefixArray(5000, 5, 32, array);
  PrefixArrayToPrefixStringMap(array, map);
  CalculateCheckSum(array, checksum);

  testFullUpdate(map, &checksum);

  Clear();
}

// This test contain both variable length prefix set and fixed-length prefix set
TEST(UrlClassifierTableUpdateV4, MixedPSetFullUpdate)
{
   _PrefixArray array;
  PrefixStringMap map;
  nsCString checksum;

  CreateRandomSortedPrefixArray(5000, 4, 4, array);
  CreateRandomSortedPrefixArray(1000, 5, 32, array);
  PrefixArrayToPrefixStringMap(array, map);
  CalculateCheckSum(array, checksum);

  testFullUpdate(map, &checksum);

  Clear();
}

TEST(UrlClassifierTableUpdateV4, PartialUpdateWithRemoval)
{
  _PrefixArray fArray;

  // Apply a full update first.
  {
    PrefixStringMap fMap;
    nsCString checksum;

    CreateRandomSortedPrefixArray(10000, 4, 4, fArray);
    CreateRandomSortedPrefixArray(2000, 5, 32, fArray);
    PrefixArrayToPrefixStringMap(fArray, fMap);
    CalculateCheckSum(fArray, checksum);

    testFullUpdate(fMap, &checksum);
  }

  // Apply a partial update with removal.
  {
    _PrefixArray pArray, mergedArray;
    PrefixStringMap pMap, mergedMap;
    nsCString checksum;

    CreateRandomSortedPrefixArray(5000, 4, 4, pArray);
    CreateRandomSortedPrefixArray(1000, 5, 32, pArray);
    RemoveIntersection(fArray, pArray);
    PrefixArrayToPrefixStringMap(pArray, pMap);

    // Remove 1/5 of elements of original prefix set.
    nsTArray<uint32_t> removal;
    CreateRandomRemovalIndices(fArray.Length() / 5, fArray.Length(), removal);
    RemoveElements(removal, fArray);

    // Calculate the expected prefix map.
    MergeAndSortArray(fArray, pArray, mergedArray);
    PrefixArrayToPrefixStringMap(mergedArray, mergedMap);
    CalculateCheckSum(mergedArray, checksum);

    testPartialUpdate(pMap, &removal, &checksum, mergedMap);
  }

  Clear();
}

TEST(UrlClassifierTableUpdateV4, PartialUpdateWithoutRemoval)
{
  _PrefixArray fArray;

  // Apply a full update first.
  {
    PrefixStringMap fMap;
    nsCString checksum;

    CreateRandomSortedPrefixArray(10000, 4, 4, fArray);
    CreateRandomSortedPrefixArray(2000, 5, 32, fArray);
    PrefixArrayToPrefixStringMap(fArray, fMap);
    CalculateCheckSum(fArray, checksum);

    testFullUpdate(fMap, &checksum);
  }

  // Apply a partial update without removal
  {
    _PrefixArray pArray, mergedArray;
    PrefixStringMap pMap, mergedMap;
    nsCString checksum;

    CreateRandomSortedPrefixArray(5000, 4, 4, pArray);
    CreateRandomSortedPrefixArray(1000, 5, 32, pArray);
    RemoveIntersection(fArray, pArray);
    PrefixArrayToPrefixStringMap(pArray, pMap);

    // Calculate the expected prefix map.
    MergeAndSortArray(fArray, pArray, mergedArray);
    PrefixArrayToPrefixStringMap(mergedArray, mergedMap);
    CalculateCheckSum(mergedArray, checksum);

    testPartialUpdate(pMap, nullptr, &checksum, mergedMap);
  }

  Clear();
}

// Expect failure because partial update contains prefix already
// in old prefix set.
TEST(UrlClassifierTableUpdateV4, PartialUpdatePrefixAlreadyExist)
{
  _PrefixArray fArray;

  // Apply a full update fist.
  {
    PrefixStringMap fMap;
    nsCString checksum;

    CreateRandomSortedPrefixArray(1000, 4, 32, fArray);
    PrefixArrayToPrefixStringMap(fArray, fMap);
    CalculateCheckSum(fArray, checksum);

    testFullUpdate(fMap, &checksum);
  }

  // Apply a partial update which contains a prefix in previous full update.
  // This should cause an update error.
  {
    _PrefixArray pArray;
    PrefixStringMap pMap;
    nsTArray<TableUpdate*> tableUpdates;

    // Pick one prefix from full update prefix and add it to partial update.
    // This should result a failure when call ApplyUpdates.
    pArray.AppendElement(fArray[rand() % fArray.Length()]);
    CreateRandomSortedPrefixArray(200, 4, 32, pArray);
    PrefixArrayToPrefixStringMap(pArray, pMap);

    GenerateUpdateData(false, pMap, nullptr, nullptr, tableUpdates);
    testUpdateFail(tableUpdates);
  }

  Clear();
}

// Test apply partial update directly without applying an full update first.
TEST(UrlClassifierTableUpdateV4, OnlyPartialUpdate)
{
  _PrefixArray pArray;
  PrefixStringMap pMap;
  nsCString checksum;

  CreateRandomSortedPrefixArray(5000, 4, 4, pArray);
  CreateRandomSortedPrefixArray(1000, 5, 32, pArray);
  PrefixArrayToPrefixStringMap(pArray, pMap);
  CalculateCheckSum(pArray, checksum);

  testPartialUpdate(pMap, nullptr, &checksum, pMap);

  Clear();
}

// Test partial update without any ADD prefixes, only removalIndices.
TEST(UrlClassifierTableUpdateV4, PartialUpdateOnlyRemoval)
{
  _PrefixArray fArray;

  // Apply a full update first.
  {
    PrefixStringMap fMap;
    nsCString checksum;

    CreateRandomSortedPrefixArray(5000, 4, 4, fArray);
    CreateRandomSortedPrefixArray(1000, 5, 32, fArray);
    PrefixArrayToPrefixStringMap(fArray, fMap);
    CalculateCheckSum(fArray, checksum);

    testFullUpdate(fMap, &checksum);
  }

  // Apply a partial update without add prefix, only contain removal indices.
  {
    _PrefixArray pArray;
    PrefixStringMap pMap, mergedMap;
    nsCString checksum;

    // Remove 1/5 of elements of original prefix set.
    nsTArray<uint32_t> removal;
    CreateRandomRemovalIndices(fArray.Length() / 5, fArray.Length(), removal);
    RemoveElements(removal, fArray);

    PrefixArrayToPrefixStringMap(fArray, mergedMap);
    CalculateCheckSum(fArray, checksum);

    testPartialUpdate(pMap, &removal, &checksum, mergedMap);
  }

  Clear();
}

// Test one tableupdate array contains full update and multiple partial updates.
TEST(UrlClassifierTableUpdateV4, MultipleTableUpdates)
{
  _PrefixArray fArray, pArray, mergedArray;
  PrefixStringMap fMap, pMap, mergedMap;
  nsCString checksum;

  nsTArray<TableUpdate*> tableUpdates;

  // Generate first full udpate
  CreateRandomSortedPrefixArray(10000, 4, 4, fArray);
  CreateRandomSortedPrefixArray(2000, 5, 32, fArray);
  PrefixArrayToPrefixStringMap(fArray, fMap);
  CalculateCheckSum(fArray, checksum);

  GenerateUpdateData(true, fMap, nullptr, &checksum, tableUpdates);

  // Generate second partial update
  CreateRandomSortedPrefixArray(3000, 4, 4, pArray);
  CreateRandomSortedPrefixArray(1000, 5, 32, pArray);
  RemoveIntersection(fArray, pArray);
  PrefixArrayToPrefixStringMap(pArray, pMap);

  MergeAndSortArray(fArray, pArray, mergedArray);
  CalculateCheckSum(mergedArray, checksum);

  GenerateUpdateData(false, pMap, nullptr, &checksum, tableUpdates);

  // Generate thrid partial update
  fArray.AppendElements(pArray);
  fArray.Sort();
  pArray.Clear();
  CreateRandomSortedPrefixArray(3000, 4, 4, pArray);
  CreateRandomSortedPrefixArray(1000, 5, 32, pArray);
  RemoveIntersection(fArray, pArray);
  PrefixArrayToPrefixStringMap(pArray, pMap);

  // Remove 1/5 of elements of original prefix set.
  nsTArray<uint32_t> removal;
  CreateRandomRemovalIndices(fArray.Length() / 5, fArray.Length(), removal);
  RemoveElements(removal, fArray);

  MergeAndSortArray(fArray, pArray, mergedArray);
  PrefixArrayToPrefixStringMap(mergedArray, mergedMap);
  CalculateCheckSum(mergedArray, checksum);

  GenerateUpdateData(false, pMap, &removal, &checksum, tableUpdates);

  testUpdate(tableUpdates, mergedMap);

  Clear();
}

// Test apply full update first, and then apply multiple partial updates
// in one tableupdate array.
TEST(UrlClassifierTableUpdateV4, MultiplePartialUpdateTableUpdates)
{
  _PrefixArray fArray;

  // Apply a full update first
  {
    PrefixStringMap fMap;
    nsCString checksum;

    // Generate first full udpate
    CreateRandomSortedPrefixArray(10000, 4, 4, fArray);
    CreateRandomSortedPrefixArray(3000, 5, 32, fArray);
    PrefixArrayToPrefixStringMap(fArray, fMap);
    CalculateCheckSum(fArray, checksum);

    testFullUpdate(fMap, &checksum);
  }

  // Apply multiple partial updates in one table update
  {
    _PrefixArray pArray, mergedArray;
    PrefixStringMap pMap, mergedMap;
    nsCString checksum;
    nsTArray<uint32_t> removal;
    nsTArray<TableUpdate*> tableUpdates;

    // Generate first partial update
    CreateRandomSortedPrefixArray(3000, 4, 4, pArray);
    CreateRandomSortedPrefixArray(1000, 5, 32, pArray);
    RemoveIntersection(fArray, pArray);
    PrefixArrayToPrefixStringMap(pArray, pMap);

    // Remove 1/5 of elements of original prefix set.
    CreateRandomRemovalIndices(fArray.Length() / 5, fArray.Length(), removal);
    RemoveElements(removal, fArray);

    MergeAndSortArray(fArray, pArray, mergedArray);
    CalculateCheckSum(mergedArray, checksum);

    GenerateUpdateData(false, pMap, &removal, &checksum, tableUpdates);

    fArray.AppendElements(pArray);
    fArray.Sort();
    pArray.Clear();
    removal.Clear();

    // Generate second partial update.
    CreateRandomSortedPrefixArray(2000, 4, 4, pArray);
    CreateRandomSortedPrefixArray(1000, 5, 32, pArray);
    RemoveIntersection(fArray, pArray);
    PrefixArrayToPrefixStringMap(pArray, pMap);

    // Remove 1/5 of elements of original prefix set.
    CreateRandomRemovalIndices(fArray.Length() / 5, fArray.Length(), removal);
    RemoveElements(removal, fArray);

    MergeAndSortArray(fArray, pArray, mergedArray);
    PrefixArrayToPrefixStringMap(mergedArray, mergedMap);
    CalculateCheckSum(mergedArray, checksum);

    GenerateUpdateData(false, pMap, &removal, &checksum, tableUpdates);

    testUpdate(tableUpdates, mergedMap);
  }

  Clear();
}

// Test removal indices are larger than the original prefix set.
TEST(UrlClassifierTableUpdateV4, RemovalIndexTooLarge)
{
  _PrefixArray fArray;

  // Apply a full update first
  {
    PrefixStringMap fMap;
    nsCString checksum;

    CreateRandomSortedPrefixArray(1000, 4, 32, fArray);
    PrefixArrayToPrefixStringMap(fArray, fMap);
    CalculateCheckSum(fArray, checksum);

    testFullUpdate(fMap, &checksum);
  }

  // Apply a partial update with removal indice array larger than
  // old prefix set(fArray). This should cause an error.
  {
    _PrefixArray pArray;
    PrefixStringMap pMap;
    nsTArray<uint32_t> removal;
    nsTArray<TableUpdate*> tableUpdates;

    CreateRandomSortedPrefixArray(200, 4, 32, pArray);
    RemoveIntersection(fArray, pArray);
    PrefixArrayToPrefixStringMap(pArray, pMap);

    for (uint32_t i = 0; i < fArray.Length() + 1 ;i++) {
      removal.AppendElement(i);
    }

    GenerateUpdateData(false, pMap, &removal, nullptr, tableUpdates);
    testUpdateFail(tableUpdates);
  }

  Clear();
}

TEST(UrlClassifierTableUpdateV4, ChecksumMismatch)
{
  // Apply a full update first
  {
    _PrefixArray fArray;
    PrefixStringMap fMap;
    nsCString checksum;

    CreateRandomSortedPrefixArray(1000, 4, 32, fArray);
    PrefixArrayToPrefixStringMap(fArray, fMap);
    CalculateCheckSum(fArray, checksum);

    testFullUpdate(fMap, &checksum);
  }

  // Apply a partial update with incorrect checksum
  {
    _PrefixArray pArray;
    PrefixStringMap pMap;
    nsCString checksum;
    nsTArray<TableUpdate*> tableUpdates;

    CreateRandomSortedPrefixArray(200, 4, 32, pArray);
    PrefixArrayToPrefixStringMap(pArray, pMap);

    // Checksum should be calculated with both old prefix set and add prefix set,
    // here we only calculate checksum with add prefix set to check if applyUpdate
    // will return failure.
    CalculateCheckSum(pArray, checksum);

    GenerateUpdateData(false, pMap, nullptr, &checksum, tableUpdates);
    testUpdateFail(tableUpdates);
  }

  Clear();
}

TEST(UrlClassifierTableUpdateV4, ApplyUpdateThenLoad)
{
  // Apply update with checksum
  {
    _PrefixArray fArray;
    PrefixStringMap fMap;
    nsCString checksum;

    CreateRandomSortedPrefixArray(1000, 4, 32, fArray);
    PrefixArrayToPrefixStringMap(fArray, fMap);
    CalculateCheckSum(fArray, checksum);

    testFullUpdate(fMap, &checksum);

    // Open lookup cache will load prefix set and verify the checksum
    testOpenLookupCache();
  }

  Clear();

  // Apply update without checksum
  {
    _PrefixArray fArray;
    PrefixStringMap fMap;

    CreateRandomSortedPrefixArray(1000, 4, 32, fArray);
    PrefixArrayToPrefixStringMap(fArray, fMap);

    testFullUpdate(fMap, nullptr);

    testOpenLookupCache();
  }

  Clear();
}

// This test is used to avoid an eror from nsICryptoHash
TEST(UrlClassifierTableUpdateV4, ApplyUpdateWithFixedChecksum)
{
  _PrefixArray fArray = { _Prefix("enus"), _Prefix("apollo"), _Prefix("mars"),
                          _Prefix("Hecatonchires cyclopes"),
                          _Prefix("vesta"), _Prefix("neptunus"), _Prefix("jupiter"),
                          _Prefix("diana"), _Prefix("minerva"), _Prefix("ceres"),
                          _Prefix("Aidos,Adephagia,Adikia,Aletheia"),
                          _Prefix("hecatonchires"), _Prefix("alcyoneus"), _Prefix("hades"),
                          _Prefix("vulcanus"), _Prefix("juno"), _Prefix("mercury"),
                          _Prefix("Stheno, Euryale and Medusa")
                        };
  fArray.Sort();

  PrefixStringMap fMap;
  PrefixArrayToPrefixStringMap(fArray, fMap);

  nsCString checksum("\xae\x18\x94\xd7\xd0\x83\x5f\xc1"
                     "\x58\x59\x5c\x2c\x72\xb9\x6e\x5e"
                     "\xf4\xe8\x0a\x6b\xff\x5e\x6b\x81"
                     "\x65\x34\x06\x16\x06\x59\xa0\x67");

  testFullUpdate(fMap, &checksum);

  // Open lookup cache will load prefix set and verify the checksum
  testOpenLookupCache();

  Clear();
}

