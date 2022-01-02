//* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsUrlClassifierPrefixSet_h_
#define nsUrlClassifierPrefixSet_h_

#include "nsISupportsUtils.h"
#include "nsID.h"
#include "nsIUrlClassifierPrefixSet.h"
#include "nsTArray.h"
#include "nsToolkitCompsCID.h"
#include "mozilla/FileUtils.h"
#include "mozilla/Mutex.h"
#include "mozilla/Poison.h"

class nsIInputStream;
class nsIOutputStream;

namespace mozilla {
namespace safebrowsing {

class VariableLengthPrefixSet;

}  // namespace safebrowsing
}  // namespace mozilla

class nsUrlClassifierPrefixSet final : public nsIUrlClassifierPrefixSet {
 public:
  nsUrlClassifierPrefixSet();

  NS_IMETHOD Init(const nsACString& aName) override;
  NS_IMETHOD SetPrefixes(const uint32_t* aArray, uint32_t aLength) override;
  NS_IMETHOD GetPrefixes(uint32_t* aCount, uint32_t** aPrefixes) override;
  NS_IMETHOD Contains(uint32_t aPrefix, bool* aFound) override;
  NS_IMETHOD IsEmpty(bool* aEmpty) override;

  nsresult GetPrefixesNative(FallibleTArray<uint32_t>& outArray);
  nsresult WritePrefixes(nsCOMPtr<nsIOutputStream>& out) const;
  nsresult LoadPrefixes(nsCOMPtr<nsIInputStream>& in);
  uint32_t CalculatePreallocateSize() const;

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf) const;

  NS_DECL_THREADSAFE_ISUPPORTS

  friend class mozilla::safebrowsing::VariableLengthPrefixSet;

 private:
  virtual ~nsUrlClassifierPrefixSet();

  static const uint32_t DELTAS_LIMIT = 120;
  static const uint32_t MAX_INDEX_DIFF = (1 << 16);
  static const uint32_t PREFIXSET_VERSION_MAGIC = 1;

  void Clear();
  nsresult MakePrefixSet(const uint32_t* aArray, uint32_t aLength);
  uint32_t BinSearch(uint32_t start, uint32_t end, uint32_t target) const;
  bool IsEmptyInternal() const;

  // Lock to prevent races between the url-classifier thread (which does most
  // of the operations) and the main thread (which does memory reporting).
  // It should be held for all operations between Init() and destruction that
  // touch this class's data members.
  mutable mozilla::Mutex mLock;
  // list of fully stored prefixes, that also form the
  // start of a run of deltas in mIndexDeltas.
  nsTArray<uint32_t> mIndexPrefixes;
  // array containing arrays of deltas from indices.
  // Index to the place that matches the closest lower
  // prefix from mIndexPrefix. Then every "delta" corresponds
  // to a prefix in the PrefixSet.
  // This array could be empty when we decide to store all the prefixes
  // in mIndexPrefixes.
  nsTArray<nsTArray<uint16_t> > mIndexDeltas;

  // how many prefixes we have.
  uint32_t mTotalPrefixes;

  nsCString mName;
  mozilla::CorruptionCanary mCanary;
};

#endif
