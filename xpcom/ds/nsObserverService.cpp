/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Logging.h"
#include "nsAutoPtr.h"
#include "nsIConsoleService.h"
#include "nsIObserverService.h"
#include "nsIObserver.h"
#include "nsIScriptError.h"
#include "nsObserverService.h"
#include "nsObserverList.h"
#include "nsServiceManagerUtils.h"
#include "nsThreadUtils.h"
#include "nsEnumeratorUtils.h"
#include "xpcpublic.h"
#include "mozilla/net/NeckoCommon.h"
#include "mozilla/Services.h"

#define NOTIFY_GLOBAL_OBSERVERS

// Log module for nsObserverService logging...
//
// To enable logging (see prlog.h for full details):
//
//    set NSPR_LOG_MODULES=ObserverService:5
//    set NSPR_LOG_FILE=nspr.log
//
// this enables LogLevel::Debug level information and places all output in
// the file nspr.log
static PRLogModuleInfo*
GetObserverServiceLog()
{
  static PRLogModuleInfo* sLog;
  if (!sLog) {
    sLog = PR_NewLogModule("ObserverService");
  }
  return sLog;
}
#define LOG(x)  MOZ_LOG(GetObserverServiceLog(), mozilla::LogLevel::Debug, x)

using namespace mozilla;

NS_IMETHODIMP
nsObserverService::CollectReports(nsIHandleReportCallback* aHandleReport,
                                  nsISupports* aData, bool aAnonymize)
{
  struct SuspectObserver
  {
    SuspectObserver(const char* aTopic, size_t aReferentCount)
      : mTopic(aTopic)
      , mReferentCount(aReferentCount)
    {}
    const char* mTopic;
    size_t mReferentCount;
  };

  size_t numStrong = 0;
  size_t numWeakAlive = 0;
  size_t numWeakDead = 0;
  nsTArray<SuspectObserver> suspectObservers;

  for (auto iter = mObserverTopicTable.Iter(); !iter.Done(); iter.Next()) {
    nsObserverList* observerList = iter.Get();
    if (!observerList) {
      continue;
    }

    nsTArray<ObserverRef>& observers = observerList->mObservers;
    for (uint32_t i = 0; i < observers.Length(); i++) {
      if (observers[i].isWeakRef) {
        nsCOMPtr<nsIObserver> observerRef(
          do_QueryReferent(observers[i].asWeak()));
        if (observerRef) {
          numWeakAlive++;
        } else {
          numWeakDead++;
        }
      } else {
        numStrong++;
      }
    }

    // Keep track of topics that have a suspiciously large number
    // of referents (symptom of leaks).
    size_t total = numStrong + numWeakAlive + numWeakDead;
    if (total > kSuspectReferentCount) {
      SuspectObserver suspect(observerList->GetKey(), total);
      suspectObservers.AppendElement(suspect);
    }
  }

  // These aren't privacy-sensitive and so don't need anonymizing.
  nsresult rv;
  for (uint32_t i = 0; i < suspectObservers.Length(); i++) {
    SuspectObserver& suspect = suspectObservers[i];
    nsPrintfCString suspectPath("observer-service-suspect/referent(topic=%s)",
                                suspect.mTopic);
    rv = aHandleReport->Callback(
      /* process */ EmptyCString(),
      suspectPath, KIND_OTHER, UNITS_COUNT, suspect.mReferentCount,
      NS_LITERAL_CSTRING("A topic with a suspiciously large number of "
                         "referents.  This may be symptomatic of a leak "
                         "if the number of referents is high with "
                         "respect to the number of windows."),
      aData);

    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  rv = aHandleReport->Callback(
         /* process */ EmptyCString(),
         NS_LITERAL_CSTRING("observer-service/referent/strong"),
         KIND_OTHER, UNITS_COUNT, numStrong,
         NS_LITERAL_CSTRING("The number of strong references held by the "
                            "observer service."),
         aData);

  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aHandleReport->Callback(
         /* process */ EmptyCString(),
         NS_LITERAL_CSTRING("observer-service/referent/weak/alive"),
         KIND_OTHER, UNITS_COUNT, numWeakAlive,
         NS_LITERAL_CSTRING("The number of weak references held by the "
                            "observer service that are still alive."),
         aData);

  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aHandleReport->Callback(
         /* process */ EmptyCString(),
         NS_LITERAL_CSTRING("observer-service/referent/weak/dead"),
         KIND_OTHER, UNITS_COUNT, numWeakDead,
         NS_LITERAL_CSTRING("The number of weak references held by the "
                            "observer service that are dead."),
         aData);

  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsObserverService Implementation

NS_IMPL_ISUPPORTS(nsObserverService,
                  nsIObserverService,
                  nsObserverService,
                  nsIMemoryReporter)

nsObserverService::nsObserverService()
  : mShuttingDown(false)
{
}

nsObserverService::~nsObserverService(void)
{
  Shutdown();
}

void
nsObserverService::RegisterReporter()
{
#if !defined(MOZILLA_XPCOMRT_API)
  RegisterWeakMemoryReporter(this);
#endif // !defined(MOZILLA_XPCOMRT_API)
}

void
nsObserverService::Shutdown()
{
#if !defined(MOZILLA_XPCOMRT_API)
  UnregisterWeakMemoryReporter(this);
#endif // !defined(MOZILLA_XPCOMRT_API)

  mShuttingDown = true;

  mObserverTopicTable.Clear();
}

nsresult
nsObserverService::Create(nsISupports* aOuter, const nsIID& aIID,
                          void** aInstancePtr)
{
  LOG(("nsObserverService::Create()"));

  nsRefPtr<nsObserverService> os = new nsObserverService();

  if (!os) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // The memory reporter can not be immediately registered here because
  // the nsMemoryReporterManager may attempt to get the nsObserverService
  // during initialization, causing a recursive GetService.
  nsRefPtr<nsRunnableMethod<nsObserverService>> registerRunnable =
    NS_NewRunnableMethod(os, &nsObserverService::RegisterReporter);
  NS_DispatchToCurrentThread(registerRunnable);

  return os->QueryInterface(aIID, aInstancePtr);
}

#define NS_ENSURE_VALIDCALL \
    if (!NS_IsMainThread()) {                                     \
        MOZ_CRASH("Using observer service off the main thread!"); \
        return NS_ERROR_UNEXPECTED;                               \
    }                                                             \
    if (mShuttingDown) {                                          \
        NS_ERROR("Using observer service after XPCOM shutdown!"); \
        return NS_ERROR_ILLEGAL_DURING_SHUTDOWN;                  \
    }

NS_IMETHODIMP
nsObserverService::AddObserver(nsIObserver* aObserver, const char* aTopic,
                               bool aOwnsWeak)
{
  LOG(("nsObserverService::AddObserver(%p: %s)",
       (void*)aObserver, aTopic));

  NS_ENSURE_VALIDCALL
  if (NS_WARN_IF(!aObserver) || NS_WARN_IF(!aTopic)) {
    return NS_ERROR_INVALID_ARG;
  }

  if (mozilla::net::IsNeckoChild() && !strncmp(aTopic, "http-on-", 8)) {
    nsCOMPtr<nsIConsoleService> console(do_GetService(NS_CONSOLESERVICE_CONTRACTID));
    nsCOMPtr<nsIScriptError> error(do_CreateInstance(NS_SCRIPTERROR_CONTRACTID));
    error->Init(NS_LITERAL_STRING("http-on-* observers only work in the parent process"),
                EmptyString(), EmptyString(), 0, 0,
                nsIScriptError::warningFlag, "chrome javascript");
    console->LogMessage(error);

    return NS_ERROR_NOT_IMPLEMENTED;
  }

  nsObserverList* observerList = mObserverTopicTable.PutEntry(aTopic);
  if (!observerList) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return observerList->AddObserver(aObserver, aOwnsWeak);
}

NS_IMETHODIMP
nsObserverService::RemoveObserver(nsIObserver* aObserver, const char* aTopic)
{
  LOG(("nsObserverService::RemoveObserver(%p: %s)",
       (void*)aObserver, aTopic));
  NS_ENSURE_VALIDCALL
  if (NS_WARN_IF(!aObserver) || NS_WARN_IF(!aTopic)) {
    return NS_ERROR_INVALID_ARG;
  }

  nsObserverList* observerList = mObserverTopicTable.GetEntry(aTopic);
  if (!observerList) {
    return NS_ERROR_FAILURE;
  }

  /* This death grip is to protect against stupid consumers who call
     RemoveObserver from their Destructor, see bug 485834/bug 325392. */
  nsCOMPtr<nsIObserver> kungFuDeathGrip(aObserver);
  return observerList->RemoveObserver(aObserver);
}

NS_IMETHODIMP
nsObserverService::EnumerateObservers(const char* aTopic,
                                      nsISimpleEnumerator** anEnumerator)
{
  NS_ENSURE_VALIDCALL
  if (NS_WARN_IF(!anEnumerator) || NS_WARN_IF(!aTopic)) {
    return NS_ERROR_INVALID_ARG;
  }

  nsObserverList* observerList = mObserverTopicTable.GetEntry(aTopic);
  if (!observerList) {
    return NS_NewEmptyEnumerator(anEnumerator);
  }

  return observerList->GetObserverList(anEnumerator);
}

// Enumerate observers of aTopic and call Observe on each.
NS_IMETHODIMP nsObserverService::NotifyObservers(nsISupports* aSubject,
                                                 const char* aTopic,
                                                 const char16_t* aSomeData)
{
  LOG(("nsObserverService::NotifyObservers(%s)", aTopic));

  NS_ENSURE_VALIDCALL
  if (NS_WARN_IF(!aTopic)) {
    return NS_ERROR_INVALID_ARG;
  }

  nsObserverList* observerList = mObserverTopicTable.GetEntry(aTopic);
  if (observerList) {
    observerList->NotifyObservers(aSubject, aTopic, aSomeData);
  }

#ifdef NOTIFY_GLOBAL_OBSERVERS
  observerList = mObserverTopicTable.GetEntry("*");
  if (observerList) {
    observerList->NotifyObservers(aSubject, aTopic, aSomeData);
  }
#endif

  return NS_OK;
}

NS_IMETHODIMP
nsObserverService::UnmarkGrayStrongObservers()
{
  NS_ENSURE_VALIDCALL

#if !defined(MOZILLA_XPCOMRT_API)
  nsCOMArray<nsIObserver> strongObservers;
  for (auto iter = mObserverTopicTable.Iter(); !iter.Done(); iter.Next()) {
    nsObserverList* aObserverList = iter.Get();
    if (aObserverList) {
      aObserverList->AppendStrongObservers(strongObservers);
    }
  }

  for (uint32_t i = 0; i < strongObservers.Length(); ++i) {
    xpc_TryUnmarkWrappedGrayObject(strongObservers[i]);
  }
#endif // !defined(MOZILLA_XPCOMRT_API)

  return NS_OK;
}
