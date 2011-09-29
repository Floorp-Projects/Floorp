/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "prlog.h"
#include "nsAutoPtr.h"
#include "nsIFactory.h"
#include "nsIServiceManager.h"
#include "nsIComponentManager.h"
#include "nsIObserverService.h"
#include "nsIObserver.h"
#include "nsISimpleEnumerator.h"
#include "nsObserverService.h"
#include "nsObserverList.h"
#include "nsHashtable.h"
#include "nsThreadUtils.h"
#include "nsIWeakReference.h"
#include "nsEnumeratorUtils.h"

#define NOTIFY_GLOBAL_OBSERVERS

#if defined(PR_LOGGING)
// Log module for nsObserverService logging...
//
// To enable logging (see prlog.h for full details):
//
//    set NSPR_LOG_MODULES=ObserverService:5
//    set NSPR_LOG_FILE=nspr.log
//
// this enables PR_LOG_DEBUG level information and places all output in
// the file nspr.log
  PRLogModuleInfo* observerServiceLog = PR_NewLogModule("ObserverService");
  #define LOG(x)  PR_LOG(observerServiceLog, PR_LOG_DEBUG, x)
#else
  #define LOG(x)
#endif /* PR_LOGGING */

////////////////////////////////////////////////////////////////////////////////
// nsObserverService Implementation


NS_IMPL_THREADSAFE_ISUPPORTS2(nsObserverService, nsIObserverService, nsObserverService)

nsObserverService::nsObserverService() :
    mShuttingDown(PR_FALSE)
{
    mObserverTopicTable.Init();
}

nsObserverService::~nsObserverService(void)
{
    Shutdown();
}

void
nsObserverService::Shutdown()
{
    mShuttingDown = PR_TRUE;

    if (mObserverTopicTable.IsInitialized())
        mObserverTopicTable.Clear();
}

nsresult
nsObserverService::Create(nsISupports* outer, const nsIID& aIID, void* *aInstancePtr)
{
    LOG(("nsObserverService::Create()"));

    nsRefPtr<nsObserverService> os = new nsObserverService();

    if (!os || !os->mObserverTopicTable.IsInitialized())
        return NS_ERROR_OUT_OF_MEMORY;

    return os->QueryInterface(aIID, aInstancePtr);
}

#define NS_ENSURE_VALIDCALL \
    if (!NS_IsMainThread()) {                                     \
        NS_ERROR("Using observer service off the main thread!");  \
        return NS_ERROR_UNEXPECTED;                               \
    }                                                             \
    if (mShuttingDown) {                                          \
        NS_ERROR("Using observer service after XPCOM shutdown!"); \
        return NS_ERROR_ILLEGAL_DURING_SHUTDOWN;                  \
    }

NS_IMETHODIMP
nsObserverService::AddObserver(nsIObserver* anObserver, const char* aTopic,
                               bool ownsWeak)
{
    LOG(("nsObserverService::AddObserver(%p: %s)",
         (void*) anObserver, aTopic));

    NS_ENSURE_VALIDCALL
    NS_ENSURE_ARG(anObserver && aTopic);

    nsObserverList *observerList = mObserverTopicTable.PutEntry(aTopic);
    if (!observerList)
        return NS_ERROR_OUT_OF_MEMORY;

    return observerList->AddObserver(anObserver, ownsWeak);
}

NS_IMETHODIMP
nsObserverService::RemoveObserver(nsIObserver* anObserver, const char* aTopic)
{
    LOG(("nsObserverService::RemoveObserver(%p: %s)",
         (void*) anObserver, aTopic));
    NS_ENSURE_VALIDCALL
    NS_ENSURE_ARG(anObserver && aTopic);

    nsObserverList *observerList = mObserverTopicTable.GetEntry(aTopic);
    if (!observerList)
        return NS_ERROR_FAILURE;

    /* This death grip is to protect against stupid consumers who call
       RemoveObserver from their Destructor, see bug 485834/bug 325392. */
    nsCOMPtr<nsIObserver> kungFuDeathGrip(anObserver);
    return observerList->RemoveObserver(anObserver);
}

NS_IMETHODIMP
nsObserverService::EnumerateObservers(const char* aTopic,
                                      nsISimpleEnumerator** anEnumerator)
{
    NS_ENSURE_VALIDCALL
    NS_ENSURE_ARG(aTopic && anEnumerator);

    nsObserverList *observerList = mObserverTopicTable.GetEntry(aTopic);
    if (!observerList)
        return NS_NewEmptyEnumerator(anEnumerator);

    return observerList->GetObserverList(anEnumerator);
}

// Enumerate observers of aTopic and call Observe on each.
NS_IMETHODIMP nsObserverService::NotifyObservers(nsISupports *aSubject,
                                                 const char *aTopic,
                                                 const PRUnichar *someData)
{
    LOG(("nsObserverService::NotifyObservers(%s)", aTopic));

    NS_ENSURE_VALIDCALL
    NS_ENSURE_ARG(aTopic);

    nsObserverList *observerList = mObserverTopicTable.GetEntry(aTopic);
    if (observerList)
        observerList->NotifyObservers(aSubject, aTopic, someData);

#ifdef NOTIFY_GLOBAL_OBSERVERS
    observerList = mObserverTopicTable.GetEntry("*");
    if (observerList)
        observerList->NotifyObservers(aSubject, aTopic, someData);
#endif

    return NS_OK;
}

