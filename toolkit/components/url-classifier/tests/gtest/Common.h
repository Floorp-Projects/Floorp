/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Entries.h"
#include "nsIFile.h"
#include "nsTArray.h"
#include "gtest/gtest.h"

using namespace mozilla::safebrowsing;

namespace mozilla {
namespace safebrowsing {
class Classifier;
class LookupCacheV4;
class TableUpdate;
}  // namespace safebrowsing
}  // namespace mozilla

#define GTEST_TABLE_V4 NS_LITERAL_CSTRING("gtest-malware-proto")
#define GTEST_TABLE_V2 NS_LITERAL_CSTRING("gtest-malware-simple")

template <typename Function>
void RunTestInNewThread(Function&& aFunction);

// Synchronously apply updates by calling Classifier::AsyncApplyUpdates.
nsresult SyncApplyUpdates(Classifier* aClassifier,
                          nsTArray<TableUpdate*>* aUpdates);

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
void CheckContent(LookupCacheV4* aCache, PrefixStringMap& aExpected);

/**
 * Utility function to generate safebrowsing internal structure
 */

// Create a LookupCacheV4 object with sepecified prefix array.
template <typename T>
RefPtr<T> SetupLookupCache(const _PrefixArray& aPrefixArray);

/**
 * Retrieve Classifer class
 */
RefPtr<Classifier> GetClassifier();

nsresult BuildLookupCache(const RefPtr<Classifier>& aClassifier,
                          const nsACString& aTable, _PrefixArray& aPrefixArray);
