#include "Classifier.h"
#include "HashStore.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsIFile.h"
#include "nsIThread.h"
#include "string.h"
#include "gtest/gtest.h"

using namespace mozilla;
using namespace mozilla::safebrowsing;

typedef nsCString _Prefix;
typedef nsTArray<_Prefix> _PrefixArray;

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
// All elements in the generated array will be different.
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

  file->AppendRelativeNativePath(NS_LITERAL_CSTRING("safebrowsing/gtest-malware-proto.pset"));

  RefPtr<VariableLengthPrefixSet> load = new VariableLengthPrefixSet;
  load->Init(NS_LITERAL_CSTRING("gtest-malware-proto"));

  PrefixStringMap out;
  load->LoadFromFile(file);
  load->GetPrefixes(out);

  for (auto iter = expected.ConstIter(); !iter.Done(); iter.Next()) {
    nsCString* str1 = iter.Data();
    nsCString* str2 = out.Get(iter.Key());

    ASSERT_TRUE(*str1 == *str2);
  }
}

static void
Clear()
{
  nsCOMPtr<nsIFile> file;
  NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR,
                         getter_AddRefs(file));
  file->AppendRelativeNativePath(NS_LITERAL_CSTRING("safebrowsing/gtest-malware-proto.pset"));
  file->Remove(false);
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
