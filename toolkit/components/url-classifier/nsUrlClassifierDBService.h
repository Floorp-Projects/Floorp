//* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-/
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
 * Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Tony Chang <tony@ponderer.org> (original author)
 *   Brett Wilson <brettw@gmail.com>
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

#ifndef nsUrlClassifierDBService_h_
#define nsUrlClassifierDBService_h_

#include <nsISupportsUtils.h>

#include "nsID.h"
#include "nsInterfaceHashtable.h"
#include "nsIObserver.h"
#include "nsIUrlClassifierHashCompleter.h"
#include "nsIUrlClassifierDBService.h"
#include "nsIURIClassifier.h"
#include "nsToolkitCompsCID.h"

// The hash length for a domain key.
#define DOMAIN_LENGTH 4

// The hash length of a partial hash entry.
#define PARTIAL_LENGTH 4

// The hash length of a complete hash entry.
#define COMPLETE_LENGTH 32

class nsUrlClassifierDBServiceWorker;
class nsIThread;

// This is a proxy class that just creates a background thread and delagates
// calls to the background thread.
class nsUrlClassifierDBService : public nsIUrlClassifierDBService,
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

  PRBool GetCompleter(const nsACString& tableName,
                      nsIUrlClassifierHashCompleter** completer);
  nsresult CacheCompletions(nsTArray<nsUrlClassifierLookupResult> *results);

  static nsIThread* BackgroundThread();

private:
  // No subclassing
  ~nsUrlClassifierDBService();

  // Disallow copy constructor
  nsUrlClassifierDBService(nsUrlClassifierDBService&);

  nsresult LookupURI(nsIURI* uri, nsIUrlClassifierCallback* c,
                     PRBool forceCheck, PRBool *didCheck);

  // Close db connection and join the background thread if it exists. 
  nsresult Shutdown();
  
  nsCOMPtr<nsUrlClassifierDBServiceWorker> mWorker;
  nsCOMPtr<nsIUrlClassifierDBServiceWorker> mWorkerProxy;

  nsInterfaceHashtable<nsCStringHashKey, nsIUrlClassifierHashCompleter> mCompleters;

  // TRUE if the nsURIClassifier implementation should check for malware
  // uris on document loads.
  PRBool mCheckMalware;

  // TRUE if the nsURIClassifier implementation should check for phishing
  // uris on document loads.
  PRBool mCheckPhishing;

  // TRUE if a BeginUpdate() has been called without an accompanying
  // CancelUpdate()/FinishUpdate().  This is used to prevent competing
  // updates, not to determine whether an update is still being
  // processed.
  PRBool mInUpdate;

  // The list of tables that can use the default hash completer object.
  nsTArray<nsCString> mGethashWhitelist;

  // Thread that we do the updates on.
  static nsIThread* gDbBackgroundThread;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsUrlClassifierDBService, NS_URLCLASSIFIERDBSERVICE_CID)

#endif // nsUrlClassifierDBService_h_
