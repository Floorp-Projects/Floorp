/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsUrlClassifierGTestCommon_h__
#define nsUrlClassifierGTestCommon_h__

#include "Entries.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsIFile.h"
#include "nsTArray.h"
#include "nsIThread.h"
#include "nsThreadUtils.h"
#include "HashStore.h"

#include "gtest/gtest.h"
#include "mozilla/gtest/MozAssertions.h"
#include "LookupCacheV4.h"

using namespace mozilla::safebrowsing;

namespace mozilla {
namespace safebrowsing {
class Classifier;
class LookupCacheV4;
class TableUpdate;
}  // namespace safebrowsing
}  // namespace mozilla

#define GTEST_SAFEBROWSING_DIR "safebrowsing"_ns
#define GTEST_TABLE_V4 "gtest-malware-proto"_ns
#define GTEST_TABLE_V2 "gtest-malware-simple"_ns

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

// Synchronously apply updates by calling Classifier::AsyncApplyUpdates.
nsresult SyncApplyUpdates(Classifier* aClassifier,
                          nsTArray<TableUpdate*>* aUpdates);
nsresult SyncApplyUpdates(TableUpdateArray& aUpdates);

// Return nsIFile with root directory - NS_APP_USER_PROFILE_50_DIR
// Sub-directories are passed in path argument.
already_AddRefed<nsIFile> GetFile(const nsTArray<nsString>& aPath);

// ApplyUpdate will call |ApplyUpdates| of Classifier within a new thread
void ApplyUpdate(nsTArray<TableUpdate*>& aUpdates);

void ApplyUpdate(TableUpdate* aUpdate);

/**
 * Prefix processing utility functions
 */

typedef nsCString _Prefix;
typedef nsTArray<nsCString> _PrefixArray;

// This function converts a lexigraphic-sorted prefixes array
// to a hash table keyed by prefix size(PrefixStringMap is defined in Entries.h)
nsresult PrefixArrayToPrefixStringMap(const _PrefixArray& aPrefixArray,
                                      PrefixStringMap& aOut);

// This function converts a lexigraphic-sorted prefixes array
// to an array containing AddPrefix(AddPrefix is defined in Entries.h)
nsresult PrefixArrayToAddPrefixArray(const _PrefixArray& aPrefixArray,
                                     AddPrefixArray& aOut);

_Prefix CreatePrefixFromURL(const char* aURL, uint8_t aPrefixSize);

_Prefix CreatePrefixFromURL(const nsCString& aURL, uint8_t aPrefixSize);

// To test if the content is equal
void CheckContent(LookupCacheV4* cache, const _PrefixArray& aPrefixArray);

/**
 * Utility function to generate safebrowsing internal structure
 */

static inline nsresult BuildCache(LookupCacheV2* cache,
                                  const _PrefixArray& aPrefixArray) {
  AddPrefixArray prefixes;
  AddCompleteArray completions;
  nsresult rv = PrefixArrayToAddPrefixArray(aPrefixArray, prefixes);
  if (NS_FAILED(rv)) {
    return rv;
  }

  return cache->Build(prefixes, completions);
}

static inline nsresult BuildCache(LookupCacheV4* cache,
                                  const _PrefixArray& aPrefixArray) {
  PrefixStringMap map;
  PrefixArrayToPrefixStringMap(aPrefixArray, map);
  return cache->Build(map);
}

// Create a LookupCacheV4 object with sepecified prefix array.
template <typename T>
RefPtr<T> SetupLookupCache(const _PrefixArray& aPrefixArray,
                           nsCOMPtr<nsIFile>& aFile) {
  RefPtr<T> cache = new T(GTEST_TABLE_V4, ""_ns, aFile);

  nsresult rv = cache->Init();
  EXPECT_EQ(rv, NS_OK);

  rv = BuildCache(cache, aPrefixArray);
  EXPECT_EQ(rv, NS_OK);

  return cache;
}

template <typename T>
RefPtr<T> SetupLookupCache(const _PrefixArray& aPrefixArray) {
  nsCOMPtr<nsIFile> file;
  NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR, getter_AddRefs(file));

  file->AppendNative(GTEST_SAFEBROWSING_DIR);

  RefPtr<T> cache = new T(GTEST_TABLE_V4, ""_ns, file);
  nsresult rv = cache->Init();
  EXPECT_EQ(rv, NS_OK);

  rv = BuildCache(cache, aPrefixArray);
  EXPECT_EQ(rv, NS_OK);

  return cache;
}

/**
 * Retrieve Classifer class
 */
RefPtr<Classifier> GetClassifier();

nsresult BuildLookupCache(const RefPtr<Classifier>& aClassifier,
                          const nsACString& aTable, _PrefixArray& aPrefixArray);

#endif  // nsUrlClassifierGTestCommon_h__
