/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Common.h"

#include "Classifier.h"
#include "HashStore.h"
#include "LookupCacheV4.h"
#include "mozilla/Components.h"
#include "mozilla/Unused.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsIThread.h"
#include "nsTArray.h"
#include "nsThreadUtils.h"
#include "nsUrlClassifierUtils.h"

using namespace mozilla;
using namespace mozilla::safebrowsing;

#define GTEST_SAFEBROWSING_DIR NS_LITERAL_CSTRING("safebrowsing")

template <typename Function>
void RunTestInNewThread(Function&& aFunction) {
  nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction(
      "RunTestInNewThread", std::forward<Function>(aFunction));
  nsCOMPtr<nsIThread> testingThread;
  nsresult rv =
      NS_NewNamedThread("Testing Thread", getter_AddRefs(testingThread), r);
  ASSERT_EQ(rv, NS_OK);
  testingThread->Shutdown();
}

nsresult SyncApplyUpdates(RefPtr<Classifier> aClassifier,
                          TableUpdateArray& aUpdates) {
  // We need to spin a new thread specifically because the callback
  // will be on the caller thread. If we call Classifier::AsyncApplyUpdates
  // and wait on the same thread, this function will never return.

  nsresult ret = NS_ERROR_FAILURE;
  bool done = false;
  auto onUpdateComplete = [&done, &ret](nsresult rv) {
    // We are on the "ApplyUpdate" thread. Post an event to main thread
    // so that we can avoid busy waiting on the main thread.
    nsCOMPtr<nsIRunnable> r =
        NS_NewRunnableFunction("SyncApplyUpdates", [&done, &ret, rv] {
          ret = rv;
          done = true;
        });
    NS_DispatchToMainThread(r);
  };

  nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction("SyncApplyUpdates", [&]() {
    nsresult rv = aClassifier->AsyncApplyUpdates(aUpdates, onUpdateComplete);
    if (NS_FAILED(rv)) {
      onUpdateComplete(rv);
    }
  });

  nsCOMPtr<nsIThread> testingThread;
  NS_NewNamedThread("ApplyUpdates", getter_AddRefs(testingThread));
  if (!testingThread) {
    return NS_ERROR_FAILURE;
  }

  testingThread->Dispatch(r, NS_DISPATCH_NORMAL);

  // NS_NewCheckSummedOutputStream in HashStore::WriteFile
  // will synchronously init NS_CRYPTO_HASH_CONTRACTID on
  // the main thread. As a result we have to keep processing
  // pending event until |done| becomes true. If there's no
  // more pending event, what we only can do is wait.
  MOZ_ALWAYS_TRUE(SpinEventLoopUntil([&]() { return done; }));

  return ret;
}

already_AddRefed<nsIFile> GetFile(const nsTArray<nsString>& path) {
  nsCOMPtr<nsIFile> file;
  nsresult rv =
      NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR, getter_AddRefs(file));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return nullptr;
  }

  for (uint32_t i = 0; i < path.Length(); i++) {
    file->Append(path[i]);
  }
  return file.forget();
}

void ApplyUpdate(TableUpdateArray& updates) {
  nsCOMPtr<nsIFile> file;
  NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR, getter_AddRefs(file));

  RefPtr<Classifier> classifier = new Classifier();
  classifier->Open(*file);

  // Force nsUrlClassifierUtils loading on main thread
  // because nsIUrlClassifierDBService will not run in advance
  // in gtest.
  nsUrlClassifierUtils::GetInstance();

  SyncApplyUpdates(classifier, updates);
}

void ApplyUpdate(TableUpdate* update) {
  TableUpdateArray updates = {update};
  ApplyUpdate(updates);
}

nsresult PrefixArrayToPrefixStringMap(const _PrefixArray& aPrefixArray,
                                      PrefixStringMap& aOut) {
  aOut.Clear();

  // Buckets are keyed by prefix length and contain an array of
  // all prefixes of that length.
  nsClassHashtable<nsUint32HashKey, _PrefixArray> table;
  for (const auto& prefix : aPrefixArray) {
    _PrefixArray* array = table.LookupOrAdd(prefix.Length());
    array->AppendElement(prefix);
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

    aOut.Put(size, str);
  }

  return NS_OK;
}

nsresult PrefixArrayToAddPrefixArray(const _PrefixArray& aPrefixArray,
                                     AddPrefixArray& aOut) {
  aOut.Clear();

  for (const auto& prefix : aPrefixArray) {
    // Create prefix hash from string
    AddPrefix* add = aOut.AppendElement(fallible);
    if (!add) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    add->addChunk = 1;
    add->prefix.Assign(prefix);
  }

  EntrySort(aOut);

  return NS_OK;
}

_Prefix CreatePrefixFromURL(const char* aURL, uint8_t aPrefixSize) {
  return CreatePrefixFromURL(nsCString(aURL), aPrefixSize);
}

_Prefix CreatePrefixFromURL(const nsCString& aURL, uint8_t aPrefixSize) {
  Completion complete;
  complete.FromPlaintext(aURL);

  _Prefix prefix;
  prefix.Assign((const char*)complete.buf, aPrefixSize);
  return prefix;
}

void CheckContent(LookupCacheV4* cache, const _PrefixArray& array) {
  PrefixStringMap vlPSetMap;
  cache->GetPrefixes(vlPSetMap);

  PrefixStringMap expected;
  PrefixArrayToPrefixStringMap(array, expected);

  for (auto iter = vlPSetMap.Iter(); !iter.Done(); iter.Next()) {
    nsCString* expectedPrefix = expected.Get(iter.Key());
    nsCString* resultPrefix = iter.Data();

    ASSERT_TRUE(resultPrefix->Equals(*expectedPrefix));
  }
}

static nsresult BuildCache(LookupCacheV2* cache,
                           const _PrefixArray& aPrefixArray) {
  AddPrefixArray prefixes;
  AddCompleteArray completions;
  nsresult rv = PrefixArrayToAddPrefixArray(aPrefixArray, prefixes);
  if (NS_FAILED(rv)) {
    return rv;
  }

  return cache->Build(prefixes, completions);
}

static nsresult BuildCache(LookupCacheV4* cache,
                           const _PrefixArray& aPrefixArray) {
  PrefixStringMap map;
  PrefixArrayToPrefixStringMap(aPrefixArray, map);
  return cache->Build(map);
}

template <typename T>
RefPtr<T> SetupLookupCache(const _PrefixArray& aPrefixArray) {
  nsCOMPtr<nsIFile> file;
  NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR, getter_AddRefs(file));

  file->AppendNative(GTEST_SAFEBROWSING_DIR);

  RefPtr<T> cache = new T(GTEST_TABLE_V4, EmptyCString(), file);
  nsresult rv = cache->Init();
  EXPECT_EQ(rv, NS_OK);

  rv = BuildCache(cache, aPrefixArray);
  EXPECT_EQ(rv, NS_OK);

  return cache;
}

nsresult BuildLookupCache(const RefPtr<Classifier>& classifier,
                          const nsACString& aTable,
                          _PrefixArray& aPrefixArray) {
  RefPtr<LookupCache> cache = classifier->GetLookupCache(aTable, false);
  if (!cache) {
    return NS_ERROR_FAILURE;
  }

  if (LookupCache::Cast<LookupCacheV4>(cache)) {
    // V4
    RefPtr<LookupCacheV4> cacheV4 = LookupCache::Cast<LookupCacheV4>(cache);

    PrefixStringMap map;
    PrefixArrayToPrefixStringMap(aPrefixArray, map);
    return cacheV4->Build(map);
  } else {
    // V2
    RefPtr<LookupCacheV2> cacheV2 = LookupCache::Cast<LookupCacheV2>(cache);

    AddPrefixArray addPrefixes;
    AddCompleteArray addComples;

    PrefixArrayToAddPrefixArray(aPrefixArray, addPrefixes);
    return cacheV2->Build(addPrefixes, addComples);
  }
}

RefPtr<Classifier> GetClassifier() {
  nsCOMPtr<nsIFile> file;
  NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR, getter_AddRefs(file));

  RefPtr<Classifier> classifier = new Classifier();
  nsresult rv = classifier->Open(*file);
  EXPECT_TRUE(rv == NS_OK);

  return classifier;
}
