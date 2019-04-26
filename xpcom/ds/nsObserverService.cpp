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
#include "mozilla/ResultExtensions.h"
#include "mozilla/Telemetry.h"
#include "mozilla/TimeStamp.h"
#include "nsString.h"
#include "GeckoProfiler.h"

static const uint32_t kMinTelemetryNotifyObserversLatencyMs = 1;

// Log module for nsObserverService logging...
//
// To enable logging (see prlog.h for full details):
//
//    set MOZ_LOG=ObserverService:5
//    set MOZ_LOG_FILE=service.log
//
// This enables LogLevel::Debug level information and places all output in
// the file service.log.
static mozilla::LazyLogModule sObserverServiceLog("ObserverService");
#define LOG(x) MOZ_LOG(sObserverServiceLog, mozilla::LogLevel::Debug, x)

using namespace mozilla;

NS_IMETHODIMP
nsObserverService::CollectReports(nsIHandleReportCallback* aHandleReport,
                                  nsISupports* aData, bool aAnonymize) {
  struct SuspectObserver {
    SuspectObserver(const char* aTopic, size_t aReferentCount)
        : mTopic(aTopic), mReferentCount(aReferentCount) {}
    const char* mTopic;
    size_t mReferentCount;
  };

  size_t totalNumStrong = 0;
  size_t totalNumWeakAlive = 0;
  size_t totalNumWeakDead = 0;
  nsTArray<SuspectObserver> suspectObservers;

  for (auto iter = mObserverTopicTable.Iter(); !iter.Done(); iter.Next()) {
    nsObserverList* observerList = iter.Get();
    if (!observerList) {
      continue;
    }

    size_t topicNumStrong = 0;
    size_t topicNumWeakAlive = 0;
    size_t topicNumWeakDead = 0;

    nsMaybeWeakPtrArray<nsIObserver>& observers = observerList->mObservers;
    for (uint32_t i = 0; i < observers.Length(); i++) {
      if (observers[i].IsWeak()) {
        nsCOMPtr<nsIObserver> ref = observers[i].GetValue();
        if (ref) {
          topicNumWeakAlive++;
        } else {
          topicNumWeakDead++;
        }
      } else {
        topicNumStrong++;
      }
    }

    totalNumStrong += topicNumStrong;
    totalNumWeakAlive += topicNumWeakAlive;
    totalNumWeakDead += topicNumWeakDead;

    // Keep track of topics that have a suspiciously large number
    // of referents (symptom of leaks).
    size_t topicTotal = topicNumStrong + topicNumWeakAlive + topicNumWeakDead;
    if (topicTotal > kSuspectReferentCount) {
      SuspectObserver suspect(observerList->GetKey(), topicTotal);
      suspectObservers.AppendElement(suspect);
    }
  }

  // These aren't privacy-sensitive and so don't need anonymizing.
  for (uint32_t i = 0; i < suspectObservers.Length(); i++) {
    SuspectObserver& suspect = suspectObservers[i];
    nsPrintfCString suspectPath("observer-service-suspect/referent(topic=%s)",
                                suspect.mTopic);
    aHandleReport->Callback(
        /* process */ EmptyCString(), suspectPath, KIND_OTHER, UNITS_COUNT,
        suspect.mReferentCount,
        NS_LITERAL_CSTRING("A topic with a suspiciously large number of "
                           "referents.  This may be symptomatic of a leak "
                           "if the number of referents is high with "
                           "respect to the number of windows."),
        aData);
  }

  MOZ_COLLECT_REPORT(
      "observer-service/referent/strong", KIND_OTHER, UNITS_COUNT,
      totalNumStrong,
      "The number of strong references held by the observer service.");

  MOZ_COLLECT_REPORT(
      "observer-service/referent/weak/alive", KIND_OTHER, UNITS_COUNT,
      totalNumWeakAlive,
      "The number of weak references held by the observer service that are "
      "still alive.");

  MOZ_COLLECT_REPORT(
      "observer-service/referent/weak/dead", KIND_OTHER, UNITS_COUNT,
      totalNumWeakDead,
      "The number of weak references held by the observer service that are "
      "dead.");

  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsObserverService Implementation

NS_IMPL_ISUPPORTS(nsObserverService, nsIObserverService, nsObserverService,
                  nsIMemoryReporter)

nsObserverService::nsObserverService() : mShuttingDown(false) {}

nsObserverService::~nsObserverService(void) { Shutdown(); }

void nsObserverService::RegisterReporter() { RegisterWeakMemoryReporter(this); }

void nsObserverService::Shutdown() {
  if (mShuttingDown) {
    return;
  }

  mShuttingDown = true;
  UnregisterWeakMemoryReporter(this);
  mObserverTopicTable.Clear();
}

nsresult nsObserverService::Create(nsISupports* aOuter, const nsIID& aIID,
                                   void** aInstancePtr) {
  LOG(("nsObserverService::Create()"));

  RefPtr<nsObserverService> os = new nsObserverService();

  if (!os) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // The memory reporter can not be immediately registered here because
  // the nsMemoryReporterManager may attempt to get the nsObserverService
  // during initialization, causing a recursive GetService.
  NS_DispatchToCurrentThread(
      NewRunnableMethod("nsObserverService::RegisterReporter", os,
                        &nsObserverService::RegisterReporter));

  return os->QueryInterface(aIID, aInstancePtr);
}

nsresult nsObserverService::EnsureValidCall() const {
  if (!NS_IsMainThread()) {
    MOZ_CRASH("Using observer service off the main thread!");
    return NS_ERROR_UNEXPECTED;
  }

  if (mShuttingDown) {
    NS_ERROR("Using observer service after XPCOM shutdown!");
    return NS_ERROR_ILLEGAL_DURING_SHUTDOWN;
  }

  return NS_OK;
}

nsresult nsObserverService::FilterHttpOnTopics(const char* aTopic) {
  // Specifically allow http-on-opening-request and http-on-stop-request in the
  // child process; see bug 1269765.
  if (mozilla::net::IsNeckoChild() && !strncmp(aTopic, "http-on-", 8) &&
      strcmp(aTopic, "http-on-failed-opening-request") &&
      strcmp(aTopic, "http-on-opening-request") &&
      strcmp(aTopic, "http-on-stop-request")) {
    nsCOMPtr<nsIConsoleService> console(
        do_GetService(NS_CONSOLESERVICE_CONTRACTID));
    nsCOMPtr<nsIScriptError> error(
        do_CreateInstance(NS_SCRIPTERROR_CONTRACTID));
    error->Init(NS_LITERAL_STRING(
                    "http-on-* observers only work in the parent process"),
                EmptyString(), EmptyString(), 0, 0, nsIScriptError::warningFlag,
                "chrome javascript", false /* from private window */,
                true /* from chrome context */);
    console->LogMessage(error);

    return NS_ERROR_NOT_IMPLEMENTED;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsObserverService::AddObserver(nsIObserver* aObserver, const char* aTopic,
                               bool aOwnsWeak) {
  LOG(("nsObserverService::AddObserver(%p: %s, %s)", (void*)aObserver, aTopic,
       aOwnsWeak ? "weak" : "strong"));

  MOZ_TRY(EnsureValidCall());
  if (NS_WARN_IF(!aObserver) || NS_WARN_IF(!aTopic)) {
    return NS_ERROR_INVALID_ARG;
  }

  MOZ_TRY(FilterHttpOnTopics(aTopic));

  nsObserverList* observerList = mObserverTopicTable.PutEntry(aTopic);
  if (!observerList) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return observerList->AddObserver(aObserver, aOwnsWeak);
}

NS_IMETHODIMP
nsObserverService::RemoveObserver(nsIObserver* aObserver, const char* aTopic) {
  LOG(("nsObserverService::RemoveObserver(%p: %s)", (void*)aObserver, aTopic));

  if (mShuttingDown) {
    // The service is shutting down. Let's ignore this call.
    return NS_OK;
  }

  MOZ_TRY(EnsureValidCall());
  if (NS_WARN_IF(!aObserver) || NS_WARN_IF(!aTopic)) {
    return NS_ERROR_INVALID_ARG;
  }

  nsObserverList* observerList = mObserverTopicTable.GetEntry(aTopic);
  if (!observerList) {
    return NS_ERROR_FAILURE;
  }

  /* This death grip is needed to protect against consumers who call
   * RemoveObserver from their Destructor thus potentially thus causing
   * infinite recursion, see bug 485834/bug 325392. */
  nsCOMPtr<nsIObserver> kungFuDeathGrip(aObserver);
  return observerList->RemoveObserver(aObserver);
}

NS_IMETHODIMP
nsObserverService::EnumerateObservers(const char* aTopic,
                                      nsISimpleEnumerator** anEnumerator) {
  LOG(("nsObserverService::EnumerateObservers(%s)", aTopic));

  MOZ_TRY(EnsureValidCall());
  if (NS_WARN_IF(!anEnumerator) || NS_WARN_IF(!aTopic)) {
    return NS_ERROR_INVALID_ARG;
  }

  nsObserverList* observerList = mObserverTopicTable.GetEntry(aTopic);
  if (!observerList) {
    return NS_NewEmptyEnumerator(anEnumerator);
  }

  observerList->GetObserverList(anEnumerator);
  return NS_OK;
}

// Enumerate observers of aTopic and call Observe on each.
NS_IMETHODIMP nsObserverService::NotifyObservers(nsISupports* aSubject,
                                                 const char* aTopic,
                                                 const char16_t* aSomeData) {
  LOG(("nsObserverService::NotifyObservers(%s)", aTopic));

  MOZ_TRY(EnsureValidCall());
  if (NS_WARN_IF(!aTopic)) {
    return NS_ERROR_INVALID_ARG;
  }

  mozilla::TimeStamp start = TimeStamp::Now();

  AUTO_PROFILER_TEXT_MARKER_CAUSE("NotifyObservers", nsDependentCString(aTopic),
                                  OTHER, profiler_get_backtrace());
  AUTO_PROFILER_LABEL_DYNAMIC_CSTR("nsObserverService::NotifyObservers", OTHER,
                                   aTopic);

  nsObserverList* observerList = mObserverTopicTable.GetEntry(aTopic);
  if (observerList) {
    observerList->NotifyObservers(aSubject, aTopic, aSomeData);
  }

  uint32_t latencyMs = round((TimeStamp::Now() - start).ToMilliseconds());
  if (latencyMs >= kMinTelemetryNotifyObserversLatencyMs) {
    Telemetry::Accumulate(Telemetry::NOTIFY_OBSERVERS_LATENCY_MS,
                          nsDependentCString(aTopic), latencyMs);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsObserverService::UnmarkGrayStrongObservers() {
  MOZ_TRY(EnsureValidCall());

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

  return NS_OK;
}
