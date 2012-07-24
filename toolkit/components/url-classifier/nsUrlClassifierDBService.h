//* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsUrlClassifierDBService_h_
#define nsUrlClassifierDBService_h_

#include <nsISupportsUtils.h>

#include "nsID.h"
#include "nsInterfaceHashtable.h"
#include "nsIObserver.h"
#include "nsUrlClassifierPrefixSet.h"
#include "nsIUrlClassifierHashCompleter.h"
#include "nsIUrlClassifierDBService.h"
#include "nsIURIClassifier.h"
#include "nsToolkitCompsCID.h"
#include "nsICryptoHash.h"
#include "nsICryptoHMAC.h"
#include "mozilla/Attributes.h"

// The hash length for a domain key.
#define DOMAIN_LENGTH 4

// The hash length of a partial hash entry.
#define PARTIAL_LENGTH 4

// The hash length of a complete hash entry.
#define COMPLETE_LENGTH 32

class nsUrlClassifierDBServiceWorker;
class nsIThread;
class nsIURI;

// This is a proxy class that just creates a background thread and delagates
// calls to the background thread.
class nsUrlClassifierDBService MOZ_FINAL : public nsIUrlClassifierDBService,
                                           public nsIURIClassifier,
                                           public nsIObserver
{
public:
  // This is thread safe. It throws an exception if the thread is busy.
  nsUrlClassifierDBService();

  nsresult Init();

  static nsUrlClassifierDBService* GetInstance(nsresult *result);

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_URLCLASSIFIERDBSERVICE_CID)

  NS_DECL_ISUPPORTS
  NS_DECL_NSIURLCLASSIFIERDBSERVICE
  NS_DECL_NSIURICLASSIFIER
  NS_DECL_NSIOBSERVER

  bool GetCompleter(const nsACString& tableName,
                      nsIUrlClassifierHashCompleter** completer);
  nsresult CacheCompletions(nsTArray<nsUrlClassifierLookupResult> *results);

  static nsIThread* BackgroundThread();

private:
  // No subclassing
  ~nsUrlClassifierDBService();

  // Disallow copy constructor
  nsUrlClassifierDBService(nsUrlClassifierDBService&);

  nsresult LookupURI(nsIPrincipal* aPrincipal, nsIUrlClassifierCallback* c,
                     bool forceCheck, bool *didCheck);

  // Close db connection and join the background thread if it exists.
  nsresult Shutdown();

  // Check if the key is on a known-clean host.
  nsresult CheckClean(const nsACString &lookupKey,
                      bool *clean);

  nsCOMPtr<nsUrlClassifierDBServiceWorker> mWorker;
  nsCOMPtr<nsIUrlClassifierDBServiceWorker> mWorkerProxy;

  nsInterfaceHashtable<nsCStringHashKey, nsIUrlClassifierHashCompleter> mCompleters;

  // TRUE if the nsURIClassifier implementation should check for malware
  // uris on document loads.
  bool mCheckMalware;

  // TRUE if the nsURIClassifier implementation should check for phishing
  // uris on document loads.
  bool mCheckPhishing;

  // TRUE if a BeginUpdate() has been called without an accompanying
  // CancelUpdate()/FinishUpdate().  This is used to prevent competing
  // updates, not to determine whether an update is still being
  // processed.
  bool mInUpdate;

  // The list of tables that can use the default hash completer object.
  nsTArray<nsCString> mGethashWhitelist;

  // Set of prefixes known to be in the database
  nsRefPtr<nsUrlClassifierPrefixSet> mPrefixSet;
  nsCOMPtr<nsICryptoHash> mHash;

  // Thread that we do the updates on.
  static nsIThread* gDbBackgroundThread;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsUrlClassifierDBService, NS_URLCLASSIFIERDBSERVICE_CID)

#endif // nsUrlClassifierDBService_h_
