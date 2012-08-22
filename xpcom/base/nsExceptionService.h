/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsExceptionService_h__
#define nsExceptionService_h__

#include "mozilla/Attributes.h"
#include "mozilla/Mutex.h"

#include "nsIException.h"
#include "nsIExceptionService.h"
#include "nsIObserverService.h"
#include "nsHashtable.h"
#include "nsIObserver.h"

class nsExceptionManager;

/** Exception Service definition **/
class nsExceptionService MOZ_FINAL : public nsIExceptionService, public nsIObserver
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIEXCEPTIONSERVICE
  NS_DECL_NSIEXCEPTIONMANAGER
  NS_DECL_NSIOBSERVER

  nsExceptionService();

  /* additional members */
  nsresult DoGetExceptionFromProvider(nsresult errCode,
                                      nsIException *defaultException,
                                      nsIException **_richError);
  void Shutdown();


  /* thread management and cleanup */
  static void AddThread(nsExceptionManager *);
  static void DropThread(nsExceptionManager *);
  static void DoDropThread(nsExceptionManager *thread);

  static void DropAllThreads();
  static nsExceptionManager *firstThread;

  nsSupportsHashtable mProviders;

  /* single lock protects both providers hashtable
     and thread list */
  static mozilla::Mutex* sLock;

  static unsigned tlsIndex;
  static void ThreadDestruct( void *data );
#ifdef DEBUG
  static int32_t totalInstances;
#endif

private:
  ~nsExceptionService();
};


#endif // nsExceptionService_h__
