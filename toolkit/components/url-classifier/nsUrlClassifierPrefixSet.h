//* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsUrlClassifierPrefixSet_h_
#define nsUrlClassifierPrefixSet_h_

#include "nsISupportsUtils.h"
#include "nsID.h"
#include "nsIFile.h"
#include "nsIMemoryReporter.h"
#include "nsIMutableArray.h"
#include "nsIUrlClassifierPrefixSet.h"
#include "nsTArray.h"
#include "nsToolkitCompsCID.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/FileUtils.h"
#include "mozilla/Atomics.h"

class nsUrlClassifierPrefixSet final
  : public nsIUrlClassifierPrefixSet
  , public nsIMemoryReporter
{
public:
  nsUrlClassifierPrefixSet();

  NS_IMETHOD Init(const nsACString& aName) override;
  NS_IMETHOD SetPrefixes(const uint32_t* aArray, uint32_t aLength) override;
  NS_IMETHOD GetPrefixes(uint32_t* aCount, uint32_t** aPrefixes) override;
  NS_IMETHOD Contains(uint32_t aPrefix, bool* aFound) override;
  NS_IMETHOD IsEmpty(bool* aEmpty) override;
  NS_IMETHOD LoadFromFile(nsIFile* aFile) override;
  NS_IMETHOD StoreToFile(nsIFile* aFile) override;

  nsresult GetPrefixesNative(FallibleTArray<uint32_t>& outArray);
  size_t SizeInMemory() { return mMemoryInUse; };

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIMEMORYREPORTER

protected:
  virtual ~nsUrlClassifierPrefixSet();

  static const uint32_t BUFFER_SIZE = 64 * 1024;
  static const uint32_t DELTAS_LIMIT = 120;
  static const uint32_t MAX_INDEX_DIFF = (1 << 16);
  static const uint32_t PREFIXSET_VERSION_MAGIC = 1;

  nsresult MakePrefixSet(const uint32_t* aArray, uint32_t aLength);
  uint32_t BinSearch(uint32_t start, uint32_t end, uint32_t target);

  // Return the estimated size of the set on disk and in memory, in bytes.
  size_t SizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf);

  // list of fully stored prefixes, that also form the
  // start of a run of deltas in mIndexDeltas.
  nsTArray<uint32_t> mIndexPrefixes;
  // array containing arrays of deltas from indices.
  // Index to the place that matches the closest lower
  // prefix from mIndexPrefix. Then every "delta" corresponds
  // to a prefix in the PrefixSet.
  nsTArray<nsTArray<uint16_t> > mIndexDeltas;
  // how many prefixes we have.
  uint32_t mTotalPrefixes;

  // memory report collection might happen while we're updating the prefixset
  // on another thread, so pre-compute and remember the size (bug 1050108).
  mozilla::Atomic<size_t> mMemoryInUse;
  nsCString mMemoryReportPath;
};

#endif
