//* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef VariableLengthPrefixSet_h
#define VariableLengthPrefixSet_h

#include "nsISupports.h"
#include "nsIMemoryReporter.h"
#include "Entries.h"
#include "nsTArray.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/Mutex.h"

class nsUrlClassifierPrefixSet;

namespace mozilla {
namespace safebrowsing {

class VariableLengthPrefixSet final
  : public nsIMemoryReporter
{
public:
  VariableLengthPrefixSet();

  nsresult Init(const nsACString& aName);
  nsresult SetPrefixes(const mozilla::safebrowsing::PrefixStringMap& aPrefixMap);
  nsresult GetPrefixes(mozilla::safebrowsing::PrefixStringMap& aPrefixMap);
  nsresult GetFixedLengthPrefixes(FallibleTArray<uint32_t>& aPrefixes);
  nsresult Matches(const nsACString& aFullHash, uint32_t* aLength) const;
  nsresult IsEmpty(bool* aEmpty) const;
  nsresult LoadFromFile(nsCOMPtr<nsIFile>& aFile);
  nsresult StoreToFile(nsCOMPtr<nsIFile>& aFile) const;

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf) const;

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIMEMORYREPORTER

private:
  virtual ~VariableLengthPrefixSet();

  static const uint32_t MAX_BUFFER_SIZE = 64 * 1024;
  static const uint32_t PREFIXSET_VERSION_MAGIC = 1;

  bool BinarySearch(const nsACString& aFullHash,
                    const nsACString& aPrefixes,
                    uint32_t aPrefixSize) const;

  uint32_t CalculatePreallocateSize() const;
  nsresult WritePrefixes(nsCOMPtr<nsIOutputStream>& out) const;
  nsresult LoadPrefixes(nsCOMPtr<nsIInputStream>& in);

  // Lock to prevent races between the url-classifier thread (which does most
  // of the operations) and the main thread (which does memory reporting).
  // It should be held for all operations between Init() and destruction that
  // touch this class's data members.
  mutable mozilla::Mutex mLock;

  const RefPtr<nsUrlClassifierPrefixSet> mFixedPrefixSet;
  mozilla::safebrowsing::PrefixStringMap mVLPrefixSet;

  nsCString mMemoryReportPath;
};

} // namespace safebrowsing
} // namespace mozilla

#endif
