/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
#include "mozilla/net/NeckoCommon.h"

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
static PRLogModuleInfo*
GetObserverServiceLog()
{
    static PRLogModuleInfo *sLog;
    if (!sLog)
        sLog = PR_NewLogModule("ObserverService");
    return sLog;
}
  #define LOG(x)  PR_LOG(GetObserverServiceLog(), PR_LOG_DEBUG, x)
#else
  #define LOG(x)
#endif /* PR_LOGGING */

////////////////////////////////////////////////////////////////////////////////
// nsObserverService Implementation


NS_IMPL_THREADSAFE_ISUPPORTS2(nsObserverService, nsIObserverService, nsObserverService)

nsObserverService::nsObserverService() :
    mShuttingDown(false)
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
    mShuttingDown = true;

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

    if (mozilla::net::IsNeckoChild() && !strncmp(aTopic, "http-on-", 8)) {
      return NS_ERROR_NOT_IMPLEMENTED;
    }

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

static PLDHashOperator
UnmarkGrayObserverEntry(nsObserverList* aObserverList, void* aClosure)
{
    if (aObserverList) {
        aObserverList->UnmarkGrayStrongObservers();
    }
    return PL_DHASH_NEXT;
}

NS_IMETHODIMP
nsObserverService::UnmarkGrayStrongObservers()
{
    NS_ENSURE_VALIDCALL

    mObserverTopicTable.EnumerateEntries(UnmarkGrayObserverEntry, nullptr);

    return NS_OK;
}
