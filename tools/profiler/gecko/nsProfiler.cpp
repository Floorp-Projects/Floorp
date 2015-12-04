/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <string>
#include <sstream>
#include "GeckoProfiler.h"
#include "nsProfiler.h"
#include "nsProfilerStartParams.h"
#include "nsMemory.h"
#include "nsString.h"
#include "mozilla/Services.h"
#include "nsIObserverService.h"
#include "nsIInterfaceRequestor.h"
#include "nsILoadContext.h"
#include "nsIWebNavigation.h"
#include "nsIInterfaceRequestorUtils.h"
#include "shared-libraries.h"
#include "js/Value.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/Promise.h"

using mozilla::ErrorResult;
using mozilla::dom::Promise;
using std::string;

NS_IMPL_ISUPPORTS(nsProfiler, nsIProfiler)

nsProfiler::nsProfiler()
  : mLockedForPrivateBrowsing(false)
{
}

nsProfiler::~nsProfiler()
{
  nsCOMPtr<nsIObserverService> observerService = mozilla::services::GetObserverService();
  if (observerService) {
    observerService->RemoveObserver(this, "chrome-document-global-created");
    observerService->RemoveObserver(this, "last-pb-context-exited");
  }
}

nsresult
nsProfiler::Init() {
  nsCOMPtr<nsIObserverService> observerService = mozilla::services::GetObserverService();
  if (observerService) {
    observerService->AddObserver(this, "chrome-document-global-created", false);
    observerService->AddObserver(this, "last-pb-context-exited", false);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsProfiler::Observe(nsISupports *aSubject,
                    const char *aTopic,
                    const char16_t *aData)
{
  if (strcmp(aTopic, "chrome-document-global-created") == 0) {
    nsCOMPtr<nsIInterfaceRequestor> requestor = do_QueryInterface(aSubject);
    nsCOMPtr<nsIWebNavigation> parentWebNav = do_GetInterface(requestor);
    nsCOMPtr<nsILoadContext> loadContext = do_QueryInterface(parentWebNav);
    if (loadContext && loadContext->UsePrivateBrowsing() && !mLockedForPrivateBrowsing) {
      mLockedForPrivateBrowsing = true;
      profiler_lock();
    }
  } else if (strcmp(aTopic, "last-pb-context-exited") == 0) {
    mLockedForPrivateBrowsing = false;
    profiler_unlock();
  }
  return NS_OK;
}

NS_IMETHODIMP
nsProfiler::CanProfile(bool *aCanProfile)
{
  *aCanProfile = !mLockedForPrivateBrowsing;
  return NS_OK;
}

NS_IMETHODIMP
nsProfiler::StartProfiler(uint32_t aEntries, double aInterval,
                          const char** aFeatures, uint32_t aFeatureCount,
                          const char** aThreadNameFilters, uint32_t aFilterCount)
{
  if (mLockedForPrivateBrowsing) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  profiler_start(aEntries, aInterval,
                 aFeatures, aFeatureCount,
                 aThreadNameFilters, aFilterCount);
  return NS_OK;
}

NS_IMETHODIMP
nsProfiler::StopProfiler()
{
  profiler_stop();
  return NS_OK;
}

NS_IMETHODIMP
nsProfiler::IsPaused(bool *aIsPaused)
{
  *aIsPaused = profiler_is_paused();
  return NS_OK;
}

NS_IMETHODIMP
nsProfiler::PauseSampling()
{
  profiler_pause();
  return NS_OK;
}

NS_IMETHODIMP
nsProfiler::ResumeSampling()
{
  profiler_resume();
  return NS_OK;
}

NS_IMETHODIMP
nsProfiler::AddMarker(const char *aMarker)
{
  PROFILER_MARKER(aMarker);
  return NS_OK;
}

NS_IMETHODIMP
nsProfiler::GetProfile(double aSinceTime, char** aProfile)
{
  mozilla::UniquePtr<char[]> profile = profiler_get_profile(aSinceTime);
  if (profile) {
    size_t len = strlen(profile.get());
    char *profileStr = static_cast<char *>
                         (nsMemory::Clone(profile.get(), (len + 1) * sizeof(char)));
    profileStr[len] = '\0';
    *aProfile = profileStr;
  }
  return NS_OK;
}

std::string GetSharedLibraryInfoStringInternal();

std::string
GetSharedLibraryInfoString()
{
  return GetSharedLibraryInfoStringInternal();
}

NS_IMETHODIMP
nsProfiler::GetSharedLibraryInformation(nsAString& aOutString)
{
  aOutString.Assign(NS_ConvertUTF8toUTF16(GetSharedLibraryInfoString().c_str()));
  return NS_OK;
}

NS_IMETHODIMP
nsProfiler::DumpProfileToFile(const char* aFilename)
{
  profiler_save_profile_to_file(aFilename);
  return NS_OK;
}

NS_IMETHODIMP
nsProfiler::GetProfileData(double aSinceTime, JSContext* aCx,
                           JS::MutableHandle<JS::Value> aResult)
{
  JS::RootedObject obj(aCx, profiler_get_profile_jsobject(aCx, aSinceTime));
  if (!obj) {
    return NS_ERROR_FAILURE;
  }
  aResult.setObject(*obj);
  return NS_OK;
}

NS_IMETHODIMP
nsProfiler::GetProfileDataAsync(double aSinceTime, JSContext* aCx,
                                nsISupports** aPromise)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (NS_WARN_IF(!aCx)) {
    return NS_ERROR_FAILURE;
  }

  nsIGlobalObject* go = xpc::NativeGlobal(JS::CurrentGlobalOrNull(aCx));

  if (NS_WARN_IF(!go)) {
    return NS_ERROR_FAILURE;
  }

  ErrorResult result;
  RefPtr<Promise> promise = Promise::Create(go, result);
  if (NS_WARN_IF(result.Failed())) {
    return result.StealNSResult();
  }

  profiler_get_profile_jsobject_async(aSinceTime, promise);

  promise.forget(aPromise);
  return NS_OK;
}

NS_IMETHODIMP
nsProfiler::GetElapsedTime(double* aElapsedTime)
{
  *aElapsedTime = profiler_time();
  return NS_OK;
}

NS_IMETHODIMP
nsProfiler::IsActive(bool *aIsActive)
{
  *aIsActive = profiler_is_active();
  return NS_OK;
}

NS_IMETHODIMP
nsProfiler::GetFeatures(uint32_t *aCount, char ***aFeatures)
{
  uint32_t len = 0;

  const char **features = profiler_get_features();
  if (!features) {
    *aCount = 0;
    *aFeatures = nullptr;
    return NS_OK;
  }

  while (features[len]) {
    len++;
  }

  char **featureList = static_cast<char **>
                       (moz_xmalloc(len * sizeof(char*)));

  for (size_t i = 0; i < len; i++) {
    size_t strLen = strlen(features[i]);
    featureList[i] = static_cast<char *>
                         (nsMemory::Clone(features[i], (strLen + 1) * sizeof(char)));
  }

  *aFeatures = featureList;
  *aCount = len;
  return NS_OK;
}

NS_IMETHODIMP
nsProfiler::GetStartParams(nsIProfilerStartParams** aRetVal)
{
  if (!profiler_is_active()) {
    *aRetVal = nullptr;
  } else {
    int entrySize = 0;
    double interval = 0;
    mozilla::Vector<const char*> filters;
    mozilla::Vector<const char*> features;
    profiler_get_start_params(&entrySize, &interval, &filters, &features);

    nsTArray<nsCString> filtersArray;
    for (uint32_t i = 0; i < filters.length(); ++i) {
      filtersArray.AppendElement(filters[i]);
    }

    nsTArray<nsCString> featuresArray;
    for (size_t i = 0; i < features.length(); ++i) {
      featuresArray.AppendElement(features[i]);
    }

    nsCOMPtr<nsIProfilerStartParams> startParams =
      new nsProfilerStartParams(entrySize, interval, featuresArray,
                                filtersArray);

    startParams.forget(aRetVal);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsProfiler::GetBufferInfo(uint32_t *aCurrentPosition, uint32_t *aTotalSize, uint32_t *aGeneration)
{
  MOZ_ASSERT(aCurrentPosition);
  MOZ_ASSERT(aTotalSize);
  MOZ_ASSERT(aGeneration);
  profiler_get_buffer_info(aCurrentPosition, aTotalSize, aGeneration);
  return NS_OK;
}

NS_IMETHODIMP
nsProfiler::GetProfileGatherer(nsISupports** aRetVal)
{
  if (!aRetVal) {
    return NS_ERROR_INVALID_POINTER;
  }

  // If we're not profiling, there will be no gatherer.
  if (!profiler_is_active()) {
    *aRetVal = nullptr;
  } else {
    nsCOMPtr<nsISupports> gatherer;
    profiler_get_gatherer(getter_AddRefs(gatherer));
    gatherer.forget(aRetVal);
  }
  return NS_OK;
}