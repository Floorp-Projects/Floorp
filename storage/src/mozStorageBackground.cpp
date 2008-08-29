/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 sts=2 expandtab
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Shawn Wilsher <me@shawnwilsher.com> (Original Author)
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

#include "nsIThreadPool.h"
#include "nsXPCOMCIDInternal.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsServiceManagerUtils.h"

#include "mozStorageCID.h"
#include "mozStorageBackground.h"

namespace {
  class ThreadShutdownObserver : public nsIObserver
  {
  public:
    NS_DECL_ISUPPORTS

    ThreadShutdownObserver(nsIThreadPool *aThreadPool) :
      mThreadPool(aThreadPool)
    {
    }

    NS_IMETHOD Observe(nsISupports *, const char *aTopic, const PRUnichar *)
    {
      if (!strcmp(aTopic, "xpcom-shutdown-threads")) {
        (void)mThreadPool->Shutdown();
        mThreadPool = nsnull;
      }
      return NS_OK;
    }
  private:
    ThreadShutdownObserver() { }
    nsCOMPtr<nsIThreadPool> mThreadPool;
  };
  NS_IMPL_ISUPPORTS1(ThreadShutdownObserver, nsIObserver)
}

////////////////////////////////////////////////////////////////////////////////
//// Public Methods

mozStorageBackground *mozStorageBackground::mSingleton = nsnull;

mozStorageBackground *
mozStorageBackground::getService()
{
  return mozStorageBackground::mSingleton;
}

mozStorageBackground::mozStorageBackground()
{
  mozStorageBackground::mSingleton = this;
}

mozStorageBackground::~mozStorageBackground()
{
  (void)mThreadPool->Shutdown();
  mozStorageBackground::mSingleton = nsnull;
}

nsIEventTarget *
mozStorageBackground::target()
{
  return mThreadPool;
}

nsresult
mozStorageBackground::initialize()
{
  // Create the thread pool
  mThreadPool = do_CreateInstance(NS_THREADPOOL_CONTRACTID);
  NS_ENSURE_TRUE(mThreadPool, NS_ERROR_OUT_OF_MEMORY);

  // Create the observer, and register it with the observer service
  mObserver = new ThreadShutdownObserver(mThreadPool);
  NS_ENSURE_TRUE(mObserver, NS_ERROR_OUT_OF_MEMORY);

  nsCOMPtr<nsIObserverService> os =
    do_GetService("@mozilla.org/observer-service;1");
  NS_ENSURE_TRUE(os, NS_ERROR_UNEXPECTED);

  nsresult rv = os->AddObserver(mObserver, "xpcom-shutdown-threads", PR_FALSE);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}
