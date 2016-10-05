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

// This function converts lexigraphic-sorted prefixes to a hashtable
// which key is prefix size and value is concatenated prefix string.
static void
PrefixArrayToPrefixStringMap(const _PrefixArray& prefixArray,
                             PrefixStringMap& outMap)
{
  outMap.Clear();

  for (uint32_t i = 0; i < prefixArray.Length(); i++) {
    const _Prefix& prefix = prefixArray[i];
    nsCString* prefixString = outMap.LookupOrAdd(prefix.Length());
    prefixString->Append(prefix.BeginReading(), prefix.Length());
  }
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
                   nsTArray<uint32_t>& removal,
                   nsTArray<TableUpdate*>& tableUpdates)
{
  TableUpdateV4* tableUpdate = new TableUpdateV4(NS_LITERAL_CSTRING("gtest-malware-proto"));
  tableUpdate->SetFullUpdate(fullUpdate);

  for (auto iter = add.ConstIter(); !iter.Done(); iter.Next()) {
    nsCString* pstring = iter.Data();
    std::string str(pstring->BeginReading(), pstring->Length());

    tableUpdate->NewPrefixes(iter.Key(), str);
  }

  tableUpdate->NewRemovalIndices(removal.Elements(), removal.Length());

  tableUpdates.AppendElement(tableUpdate);
}

static void
VerifyPrefixSet(PrefixStringMap& expected)
{
  // Verify the prefix set is written to disk.
  nsCOMPtr<nsIFile> file;
  NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR,
                         getter_AddRefs(file));

  file->AppendNative(NS_LITERAL_CSTRING("safebrowsing"));
  file->AppendNative(NS_LITERAL_CSTRING("gtest-malware-proto.pset"));

  RefPtr<VariableLengthPrefixSet> load = new VariableLengthPrefixSet;
  load->Init(NS_LITERAL_CSTRING("gtest-malware-proto"));

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
  NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR,
                         getter_AddRefs(file));

  UniquePtr<Classifier> classifier(new Classifier());
  classifier->Open(*file);
  classifier->Reset();
}

static void
testUpdateFail(nsTArray<TableUpdate*>& tableUpdates)
{
  nsCOMPtr<nsIFile> file;
  NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR,
                         getter_AddRefs(file));

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
  NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR,
                         getter_AddRefs(file));

  UniquePtr<Classifier> classifier(new Classifier());
  classifier->Open(*file);

  RunTestInNewThread([&] () -> void {
    nsresult rv = classifier->ApplyUpdates(&tableUpdates);
    ASSERT_TRUE(rv == NS_OK);

    VerifyPrefixSet(expected);
  });
}

static void
testFullUpdate(PrefixStringMap& add)
{
  nsTArray<uint32_t> empty;
  nsTArray<TableUpdate*> tableUpdates;
  GenerateUpdateData(true, add, empty, tableUpdates);

  testUpdate(tableUpdates, add);
}

static void
testPartialUpdate(PrefixStringMap& add,
                  nsTArray<uint32_t>& removal,
                  PrefixStringMap& expected)
{
  nsTArray<TableUpdate*> tableUpdates;
  GenerateUpdateData(false, add, removal, tableUpdates);

  testUpdate(tableUpdates, expected);
}


TEST(UrlClassifierTableUpdateV4, FixLenghtPSetFullUpdate)
{
  srand(time(NULL));

   _PrefixArray array;
  PrefixStringMap map;

  CreateRandomSortedPrefixArray(5000, 4, 4, array);
  PrefixArrayToPrefixStringMap(array, map);

  testFullUpdate(map);

  Clear();
}


TEST(UrlClassifierTableUpdateV4, VariableLenghtPSetFullUpdate)
{
   _PrefixArray array;
  PrefixStringMap map;

  CreateRandomSortedPrefixArray(5000, 5, 32, array);
  PrefixArrayToPrefixStringMap(array, map);

  testFullUpdate(map);

  Clear();
}

// This test contain both variable length prefix set and fixed-length prefix set
TEST(UrlClassifierTableUpdateV4, MixedPSetFullUpdate)
{
   _PrefixArray array;
  PrefixStringMap map;

  CreateRandomSortedPrefixArray(5000, 4, 4, array);
  CreateRandomSortedPrefixArray(1000, 5, 32, array);
  PrefixArrayToPrefixStringMap(array, map);

  testFullUpdate(map);

  Clear();
}

TEST(UrlClassifierTableUpdateV4, PartialUpdateWithRemoval)
{
  _PrefixArray fArray, pArray, mergedArray;
  PrefixStringMap fMap, pMap, mergedMap;

  {
    CreateRandomSortedPrefixArray(10000, 4, 4, fArray);
    CreateRandomSortedPrefixArray(2000, 5, 32, fArray);
    PrefixArrayToPrefixStringMap(fArray, fMap);

    testFullUpdate(fMap);
  }

  {
    CreateRandomSortedPrefixArray(5000, 4, 4, pArray);
    CreateRandomSortedPrefixArray(1000, 5, 32, pArray);
    RemoveIntersection(fArray, pArray);
    PrefixArrayToPrefixStringMap(pArray, pMap);

    // Remove 1/5 of elements of original prefix set.
    nsTArray<uint32_t> removal;
    CreateRandomRemovalIndices(fArray.Length() / 5, fArray.Length(), removal);
    RemoveElements(removal, fArray);

    // Calculate the expected prefix map.
    mergedArray.AppendElements(fArray);
    mergedArray.AppendElements(pArray);
    mergedArray.Sort();
    PrefixArrayToPrefixStringMap(mergedArray, mergedMap);

    testPartialUpdate(pMap, removal, mergedMap);
  }

  Clear();
}

TEST(UrlClassifierTableUpdateV4, PartialUpdateWithoutRemoval)
{
  _PrefixArray fArray, pArray, mergedArray;
  PrefixStringMap fMap, pMap, mergedMap;

  {
    CreateRandomSortedPrefixArray(10000, 4, 4, fArray);
    CreateRandomSortedPrefixArray(2000, 5, 32, fArray);
    PrefixArrayToPrefixStringMap(fArray, fMap);

    testFullUpdate(fMap);
  }

  {
    nsTArray<uint32_t> empty;

    CreateRandomSortedPrefixArray(5000, 4, 4, pArray);
    CreateRandomSortedPrefixArray(1000, 5, 32, pArray);
    RemoveIntersection(fArray, pArray);
    PrefixArrayToPrefixStringMap(pArray, pMap);

    // Calculate the expected prefix map.
    mergedArray.AppendElements(fArray);
    mergedArray.AppendElements(pArray);
    mergedArray.Sort();
    PrefixArrayToPrefixStringMap(mergedArray, mergedMap);

    testPartialUpdate(pMap, empty, mergedMap);
  }

  Clear();
}

// Expect failure because partial update contains prefix already
// in old prefix set.
TEST(UrlClassifierTableUpdateV4, PartialUpdatePrefixAlreadyExist)
{
  _PrefixArray fArray, pArray;
  PrefixStringMap fMap, pMap;

  {
    CreateRandomSortedPrefixArray(1000, 4, 32, fArray);
    PrefixArrayToPrefixStringMap(fArray, fMap);

    testFullUpdate(fMap);
  }

  {
    nsTArray<uint32_t> empty;
    nsTArray<TableUpdate*> tableUpdates;

    // Pick one prefix from full update prefix and add it to partial update.
    // This should result a failure when call ApplyUpdates.
    pArray.AppendElement(fArray[rand() % fArray.Length()]);
    CreateRandomSortedPrefixArray(200, 4, 32, pArray);
    PrefixArrayToPrefixStringMap(pArray, pMap);

    GenerateUpdateData(false, pMap, empty, tableUpdates);
    testUpdateFail(tableUpdates);
  }

  Clear();
}

// Test apply partial update directly without applying an full update first.
TEST(UrlClassifierTableUpdateV4, OnlyPartialUpdate)
{
  _PrefixArray pArray;
  PrefixStringMap pMap;
  nsTArray<uint32_t> empty;

  CreateRandomSortedPrefixArray(5000, 4, 4, pArray);
  CreateRandomSortedPrefixArray(1000, 5, 32, pArray);
  PrefixArrayToPrefixStringMap(pArray, pMap);

  testPartialUpdate(pMap, empty, pMap);

  Clear();
}

// Test partial update without any ADD prefixes, only removalIndices.
TEST(UrlClassifierTableUpdateV4, PartialUpdateOnlyRemoval)
{
  _PrefixArray fArray, pArray;
  PrefixStringMap fMap, pMap, mergedMap;

  {
    CreateRandomSortedPrefixArray(5000, 4, 4, fArray);
    CreateRandomSortedPrefixArray(1000, 5, 32, fArray);
    PrefixArrayToPrefixStringMap(fArray, fMap);

    testFullUpdate(fMap);
  }

  {
    // Remove 1/5 of elements of original prefix set.
    nsTArray<uint32_t> removal;
    CreateRandomRemovalIndices(fArray.Length() / 5, fArray.Length(), removal);
    RemoveElements(removal, fArray);

    PrefixArrayToPrefixStringMap(fArray, mergedMap);

    testPartialUpdate(pMap, removal, mergedMap);
  }

  Clear();
}

// Test one tableupdate array contains full update and multiple partial updates.
TEST(UrlClassifierTableUpdateV4, MultipleTableUpdates)
{
  _PrefixArray fArray, pArray, mergedArray;
  PrefixStringMap fMap, pMap, mergedMap;

  {
    nsTArray<uint32_t> empty;
    nsTArray<TableUpdate*> tableUpdates;

    // Generate first full udpate
    CreateRandomSortedPrefixArray(10000, 4, 4, fArray);
    CreateRandomSortedPrefixArray(2000, 5, 32, fArray);
    PrefixArrayToPrefixStringMap(fArray, fMap);

    GenerateUpdateData(true, fMap, empty, tableUpdates);

    // Generate second partial update
    CreateRandomSortedPrefixArray(3000, 4, 4, pArray);
    CreateRandomSortedPrefixArray(1000, 5, 32, pArray);
    RemoveIntersection(fArray, pArray);
    PrefixArrayToPrefixStringMap(pArray, pMap);

    GenerateUpdateData(false, pMap, empty, tableUpdates);

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

    GenerateUpdateData(false, pMap, removal, tableUpdates);

    mergedArray.AppendElements(fArray);
    mergedArray.AppendElements(pArray);
    mergedArray.Sort();
    PrefixArrayToPrefixStringMap(mergedArray, mergedMap);

    testUpdate(tableUpdates, mergedMap);
  }

  Clear();
}

// Test apply full update first, and then apply multiple partial updates
// in one tableupdate array.
TEST(UrlClassifierTableUpdateV4, MultiplePartialUpdateTableUpdates)
{
  _PrefixArray fArray, pArray, mergedArray;
  PrefixStringMap fMap, pMap, mergedMap;

  {
    // Generate first full udpate
    CreateRandomSortedPrefixArray(10000, 4, 4, fArray);
    CreateRandomSortedPrefixArray(3000, 5, 32, fArray);
    PrefixArrayToPrefixStringMap(fArray, fMap);

    testFullUpdate(fMap);
  }

  {
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

    GenerateUpdateData(false, pMap, removal, tableUpdates);

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

    GenerateUpdateData(false, pMap, removal, tableUpdates);

    mergedArray.AppendElements(fArray);
    mergedArray.AppendElements(pArray);
    mergedArray.Sort();
    PrefixArrayToPrefixStringMap(mergedArray, mergedMap);

    testUpdate(tableUpdates, mergedMap);
  }

  Clear();
}

// Test removal indices are larger than the original prefix set.
TEST(UrlClassifierTableUpdateV4, RemovalIndexTooLarge)
{
  _PrefixArray fArray, pArray;
  PrefixStringMap fMap, pMap;

  {
    CreateRandomSortedPrefixArray(1000, 4, 32, fArray);
    PrefixArrayToPrefixStringMap(fArray, fMap);

    testFullUpdate(fMap);
  }

  {
    nsTArray<uint32_t> removal;
    nsTArray<TableUpdate*> tableUpdates;

    CreateRandomSortedPrefixArray(200, 4, 32, pArray);
    RemoveIntersection(fArray, pArray);
    PrefixArrayToPrefixStringMap(pArray, pMap);

    for (uint32_t i = 0; i < fArray.Length() + 1 ;i++) {
      removal.AppendElement(i);
    }

    GenerateUpdateData(false, pMap, removal, tableUpdates);
    testUpdateFail(tableUpdates);
  }

  Clear();
}
