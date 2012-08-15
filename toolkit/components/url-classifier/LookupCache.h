//* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Url Classifier code
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Dave Camp <dcamp@mozilla.com>
 *   Gian-Carlo Pascutto <gpascutto@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef LookupCache_h__
#define LookupCache_h__

#include "Entries.h"
#include "nsString.h"
#include "nsTArray.h"
#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "nsIFile.h"
#include "nsUrlClassifierPrefixSet.h"
#include "prlog.h"

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

  // Value of actual key looked up in the prefixset (coded with client key)
  Prefix mCodedPrefix;

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

  /* We have both a prefix and a domain. Drop the domain, but
     hash the domain, the prefix and a random value together,
     ensuring any collisions happens at a different points for
     different users. If aPassthrough is set, we ignore the
     random value and copy prefix directly into output.
  */
  static nsresult KeyedHash(PRUint32 aPref, PRUint32 aDomain,
                            PRUint32 aKey, PRUint32* aOut,
                            bool aPassthrough);

  LookupCache(const nsACString& aTableName, nsIFile* aStoreFile,
              bool aPerClientRandomize);
  ~LookupCache();

  const nsCString &TableName() const { return mTableName; }

  nsresult Init();
  nsresult Open();
  // This will Clear() the passed arrays when done.
  nsresult Build(AddPrefixArray& aAddPrefixes,
                 AddCompleteArray& aAddCompletes);
  nsresult GetPrefixes(nsTArray<PRUint32>* aAddPrefixes);

#if DEBUG && defined(PR_LOGGING)
  void Dump();
#endif
  nsresult WriteFile();
  nsresult Has(const Completion& aCompletion,
               const Completion& aHostkey,
               PRUint32 aHashKey,
               bool* aHas, bool* aComplete,
               Prefix* aOrigPrefix);
  bool IsPrimed();

private:

  void Clear();
  nsresult Reset();
  void UpdateHeader();
  nsresult ReadHeader();
  nsresult EnsureSizeConsistent();
  nsresult ReadCompletions();
  nsresult LoadPrefixSet();
  // Construct a Prefix Set with known prefixes.
  // This will Clear() aAddPrefixes when done.
  nsresult ConstructPrefixSet(AddPrefixArray& aAddPrefixes);

  struct Header {
    uint32 magic;
    uint32 version;
    uint32 numCompletions;
  };
  Header mHeader;

  bool mPrimed;
  bool mPerClientRandomize;
  nsCString mTableName;
  nsCOMPtr<nsIFile> mStoreDirectory;
  nsCOMPtr<nsIInputStream> mInputStream;
  CompletionArray mCompletions;
  // Set of prefixes known to be in the database
  nsRefPtr<nsUrlClassifierPrefixSet> mPrefixSet;
};

}
}

#endif
