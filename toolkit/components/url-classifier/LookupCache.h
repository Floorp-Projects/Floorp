//* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef LookupCache_h__
#define LookupCache_h__

#include "Entries.h"
#include "nsString.h"
#include "nsTArray.h"
#include "nsCOMPtr.h"
#include "nsIFile.h"
#include "nsIFileStreams.h"
#include "nsRefPtr.h"
#include "nsUrlClassifierPrefixSet.h"
#include "mozilla/Logging.h"

namespace mozilla {
namespace safebrowsing {

#define MAX_HOST_COMPONENTS 5
#define MAX_PATH_COMPONENTS 4

class LookupResult {
public:
  LookupResult() : mComplete(false), mNoise(false), mFresh(false), mProtocolConfirmed(false) {}

  // The fragment that matched in the LookupCache
  union {
    Prefix prefix;
    Completion complete;
  } hash;

  const Prefix &PrefixHash() { return hash.prefix; }
  const Completion &CompleteHash() { return hash.complete; }

  bool Confirmed() const { return (mComplete && mFresh) || mProtocolConfirmed; }
  bool Complete() const { return mComplete; }

  // True if we have a complete match for this hash in the table.
  bool mComplete;

  // True if this is a noise entry, i.e. an extra entry
  // that is inserted to mask the true URL we are requesting
  bool mNoise;

  // True if we've updated this table recently-enough.
  bool mFresh;

  bool mProtocolConfirmed;

  nsCString mTableName;
};

typedef nsTArray<LookupResult> LookupResultArray;

struct CacheResult {
  AddComplete entry;
  nsCString table;
};
typedef nsTArray<CacheResult> CacheResultArray;

class LookupCache {
public:
  // Check for a canonicalized IP address.
  static bool IsCanonicalizedIP(const nsACString& aHost);

  // take a lookup string (www.hostname.com/path/to/resource.html) and
  // expand it into the set of fragments that should be searched for in an
  // entry
  static nsresult GetLookupFragments(const nsACString& aSpec,
                                     nsTArray<nsCString>* aFragments);
  // Similar to GetKey(), but if the domain contains three or more components,
  // two keys will be returned:
  //  hostname.com/foo/bar -> [hostname.com]
  //  mail.hostname.com/foo/bar -> [hostname.com, mail.hostname.com]
  //  www.mail.hostname.com/foo/bar -> [hostname.com, mail.hostname.com]
  static nsresult GetHostKeys(const nsACString& aSpec,
                              nsTArray<nsCString>* aHostKeys);
  // Get the database key for a given URI.  This is the top three
  // domain components if they exist, otherwise the top two.
  //  hostname.com/foo/bar -> hostname.com
  //  mail.hostname.com/foo/bar -> mail.hostname.com
  //  www.mail.hostname.com/foo/bar -> mail.hostname.com
  static nsresult GetKey(const nsACString& aSpec, Completion* aHash,
                         nsCOMPtr<nsICryptoHash>& aCryptoHash);

  LookupCache(const nsACString& aTableName, nsIFile* aStoreFile);
  ~LookupCache();

  const nsCString &TableName() const { return mTableName; }

  nsresult Init();
  nsresult Open();
  // The directory handle where we operate will
  // be moved away when a backup is made.
  nsresult UpdateDirHandle(nsIFile* aStoreDirectory);
  // This will Clear() the passed arrays when done.
  nsresult Build(AddPrefixArray& aAddPrefixes,
                 AddCompleteArray& aAddCompletes);
  nsresult GetPrefixes(FallibleTArray<uint32_t>& aAddPrefixes);
  void ClearCompleteCache();

#if DEBUG
  void Dump();
#endif
  nsresult WriteFile();
  nsresult Has(const Completion& aCompletion,
               bool* aHas, bool* aComplete);
  bool IsPrimed();

private:
  void ClearAll();
  nsresult Reset();
  void UpdateHeader();
  nsresult ReadHeader(nsIInputStream* aInputStream);
  nsresult ReadCompletions(nsIInputStream* aInputStream);
  nsresult EnsureSizeConsistent();
  nsresult LoadPrefixSet();
  // Construct a Prefix Set with known prefixes.
  // This will Clear() aAddPrefixes when done.
  nsresult ConstructPrefixSet(AddPrefixArray& aAddPrefixes);

  struct Header {
    uint32_t magic;
    uint32_t version;
    uint32_t numCompletions;
  };
  Header mHeader;

  bool mPrimed;
  nsCString mTableName;
  nsCOMPtr<nsIFile> mStoreDirectory;
  CompletionArray mCompletions;
  // Set of prefixes known to be in the database
  nsRefPtr<nsUrlClassifierPrefixSet> mPrefixSet;
};

}
}

#endif
