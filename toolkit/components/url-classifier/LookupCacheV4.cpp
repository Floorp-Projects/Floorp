//* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "LookupCacheV4.h"
#include "HashStore.h"

// MOZ_LOG=UrlClassifierDbService:5
extern mozilla::LazyLogModule gUrlClassifierDbServiceLog;
#define LOG(args) MOZ_LOG(gUrlClassifierDbServiceLog, mozilla::LogLevel::Debug, args)
#define LOG_ENABLED() MOZ_LOG_TEST(gUrlClassifierDbServiceLog, mozilla::LogLevel::Debug)

namespace mozilla {
namespace safebrowsing {

const int LookupCacheV4::VER = 4;

// Prefixes coming from updates and VLPrefixSet are both stored in the HashTable
// where the (key, value) pair is a prefix size and a lexicographic-sorted string.
// The difference is prefixes from updates use std:string(to avoid additional copies)
// and prefixes from VLPrefixSet use nsCString.
// This class provides a common interface for the partial update algorithm to make it
// easier to operate on two different kind prefix string map..
class VLPrefixSet
{
public:
  explicit VLPrefixSet(const PrefixStringMap& aMap);
  explicit VLPrefixSet(const TableUpdateV4::PrefixStdStringMap& aMap);

  // This function will merge the prefix map in VLPrefixSet to aPrefixMap.
  void Merge(PrefixStringMap& aPrefixMap);

  // Find the smallest string from the map in VLPrefixSet.
  bool GetSmallestPrefix(nsDependentCSubstring& aOutString);

  // Return the number of prefixes in the map
  uint32_t Count() const { return mCount; }

private:
  // PrefixString structure contains a lexicographic-sorted string with
  // a |pos| variable to indicate which substring we are pointing to right now.
  // |pos| increases each time GetSmallestPrefix finds the smallest string.
  struct PrefixString {
    PrefixString(const nsACString& aStr, uint32_t aSize)
     : pos(0)
     , size(aSize)
    {
      data.Rebind(aStr.BeginReading(), aStr.Length());
    }

    const char* get() {
      return pos < data.Length() ? data.BeginReading() + pos : nullptr;
    }
    void next() { pos += size; }
    uint32_t remaining() { return data.Length() - pos; }

    nsDependentCSubstring data;
    uint32_t pos;
    uint32_t size;
  };

  nsClassHashtable<nsUint32HashKey, PrefixString> mMap;
  uint32_t mCount;
};

nsresult
LookupCacheV4::Init()
{
  mVLPrefixSet = new VariableLengthPrefixSet();
  nsresult rv = mVLPrefixSet->Init(mTableName);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

// TODO : Bug 1298257, Implement url matching for variable-length prefix set
nsresult
LookupCacheV4::Has(const Completion& aCompletion,
                   bool* aHas, bool* aComplete)
{
  *aHas = false;
  return NS_OK;
}

nsresult
LookupCacheV4::Build(PrefixStringMap& aPrefixMap)
{
  return mVLPrefixSet->SetPrefixes(aPrefixMap);
}

nsresult
LookupCacheV4::GetPrefixes(PrefixStringMap& aPrefixMap)
{
  return mVLPrefixSet->GetPrefixes(aPrefixMap);
}

nsresult
LookupCacheV4::ClearPrefixes()
{
  // Clear by seting a empty map
  PrefixStringMap map;
  return mVLPrefixSet->SetPrefixes(map);
}

nsresult
LookupCacheV4::StoreToFile(nsIFile* aFile)
{
  return mVLPrefixSet->StoreToFile(aFile);
}

nsresult
LookupCacheV4::LoadFromFile(nsIFile* aFile)
{
  return mVLPrefixSet->LoadFromFile(aFile);
}

size_t
LookupCacheV4::SizeOfPrefixSet()
{
  return mVLPrefixSet->SizeOfIncludingThis(moz_malloc_size_of);
}

static void
AppendPrefixToMap(PrefixStringMap& prefixes, nsDependentCSubstring& prefix)
{
  if (!prefix.Length()) {
    return;
  }

  nsCString* prefixString = prefixes.LookupOrAdd(prefix.Length());
  prefixString->Append(prefix.BeginReading(), prefix.Length());
}

// Please see https://bug1287058.bmoattachments.org/attachment.cgi?id=8795366
// for detail about partial update algorithm.
nsresult
LookupCacheV4::ApplyPartialUpdate(TableUpdateV4* aTableUpdate,
                                  PrefixStringMap& aInputMap,
                                  PrefixStringMap& aOutputMap)
{
  MOZ_ASSERT(aOutputMap.IsEmpty());

  // oldPSet contains prefixes we already have or we just merged last round.
  // addPSet contains prefixes stored in tableUpdate which should be merged with oldPSet.
  VLPrefixSet oldPSet(aInputMap);
  VLPrefixSet addPSet(aTableUpdate->Prefixes());

  // RemovalIndiceArray is a sorted integer array indicating the index of prefix we should
  // remove from the old prefix set(according to lexigraphic order).
  // |removalIndex| is the current index of RemovalIndiceArray.
  // |numOldPrefixPicked| is used to record how many prefixes we picked from the old map.
  TableUpdateV4::RemovalIndiceArray& removalArray = aTableUpdate->RemovalIndices();
  uint32_t removalIndex = 0;
  int32_t numOldPrefixPicked = -1;

  nsDependentCSubstring smallestOldPrefix;
  nsDependentCSubstring smallestAddPrefix;

  // This is used to avoid infinite loop for partial update algorithm.
  // The maximum loops will be the number of old prefixes plus the number of add prefixes.
  uint32_t index = oldPSet.Count() + addPSet.Count() + 1;
  for(;index > 0; index--) {
    // Get smallest prefix from the old prefix set if we don't have one
    if (smallestOldPrefix.IsEmpty()) {
      // If prefixes from the old prefix set are all merged,
      // then we can merge the entire add prefix set directly.
      if (!oldPSet.GetSmallestPrefix(smallestOldPrefix)) {
        AppendPrefixToMap(aOutputMap, smallestAddPrefix);
        addPSet.Merge(aOutputMap);
        break;
      }
    }

    // Get smallest prefix from add prefix set if we don't have one
    if (smallestAddPrefix.IsEmpty()) {
      // If add prefixes are all merged and there is no removalIndices left,
      // then merge the entire old prefix set directly. If there are still
      // removalIndices left, we should still merge prefixes one by one
      // to know which prefix from old prefix set should be removed.
      if (!addPSet.GetSmallestPrefix(smallestAddPrefix) &&
        removalIndex >= removalArray.Length()) {
        AppendPrefixToMap(aOutputMap, smallestOldPrefix);
        oldPSet.Merge(aOutputMap);
        break;
      }
    }

    // Compare the smallest string in old prefix set and add prefix set, merge the
    // smaller one into new map to ensure merged string still follows
    // lexigraphic order.
    if (smallestOldPrefix < smallestAddPrefix ||
        smallestAddPrefix.IsEmpty()) {
      numOldPrefixPicked++;

      // If the number of picks from old map matches the removalIndex, then this prefix
      // will be removed by not merging it to new map.
      if (removalIndex < removalArray.Length() &&
          numOldPrefixPicked == removalArray[removalIndex]) {
        removalIndex++;
      } else {
        AppendPrefixToMap(aOutputMap, smallestOldPrefix);
      }
      smallestOldPrefix.SetLength(0);
    } else if (smallestOldPrefix > smallestAddPrefix ||
               smallestOldPrefix.IsEmpty()){
      AppendPrefixToMap(aOutputMap, smallestAddPrefix);
      smallestAddPrefix.SetLength(0);
    } else {
      NS_WARNING("Add prefix should not exist in the original prefix set.");
      Telemetry::Accumulate(Telemetry::URLCLASSIFIER_UPDATE_ERROR_TYPE,
                            DUPLICATE_PREFIX);
      return NS_ERROR_FAILURE;
    }
  }

  // We expect index will be greater to 0 because max number of runs will be
  // the number of original prefix plus add prefix.
  if (index <= 0) {
    NS_WARNING("There are still prefixes remaining after reaching maximum runs.");
    Telemetry::Accumulate(Telemetry::URLCLASSIFIER_UPDATE_ERROR_TYPE,
                          INFINITE_LOOP);
    return NS_ERROR_FAILURE;
  }

  if (removalIndex < removalArray.Length()) {
    NS_WARNING("There are still prefixes to remove after exhausting the old PrefixSet.");
    Telemetry::Accumulate(Telemetry::URLCLASSIFIER_UPDATE_ERROR_TYPE,
                          WRONG_REMOVAL_INDICES);
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

VLPrefixSet::VLPrefixSet(const PrefixStringMap& aMap)
  : mCount(0)
{
  for (auto iter = aMap.ConstIter(); !iter.Done(); iter.Next()) {
    uint32_t size = iter.Key();
    mMap.Put(size, new PrefixString(*iter.Data(), size));
    mCount += iter.Data()->Length() / size;
  }
}

VLPrefixSet::VLPrefixSet(const TableUpdateV4::PrefixStdStringMap& aMap)
  : mCount(0)
{
  for (auto iter = aMap.ConstIter(); !iter.Done(); iter.Next()) {
    uint32_t size = iter.Key();
    mMap.Put(size, new PrefixString(iter.Data()->GetPrefixString(), size));
    mCount += iter.Data()->GetPrefixString().Length() / size;
  }
}

void
VLPrefixSet::Merge(PrefixStringMap& aPrefixMap) {
  for (auto iter = mMap.ConstIter(); !iter.Done(); iter.Next()) {
    nsCString* prefixString = aPrefixMap.LookupOrAdd(iter.Key());
    PrefixString* str = iter.Data();

    if (str->get()) {
      prefixString->Append(str->get(), str->remaining());
    }
  }
}

bool
VLPrefixSet::GetSmallestPrefix(nsDependentCSubstring& aOutString) {
  PrefixString* pick = nullptr;
  for (auto iter = mMap.ConstIter(); !iter.Done(); iter.Next()) {
    PrefixString* str = iter.Data();

    if (!str->get()) {
      continue;
    }

    if (aOutString.IsEmpty()) {
      aOutString.Rebind(str->get(), iter.Key());
      pick = str;
      continue;
    }

    nsDependentCSubstring cur(str->get(), iter.Key());
    if (cur < aOutString) {
      aOutString.Rebind(str->get(), iter.Key());
      pick = str;
    }
  }

  if (pick) {
    pick->next();
  }

  return pick != nullptr;
}

} // namespace safebrowsing
} // namespace mozilla
