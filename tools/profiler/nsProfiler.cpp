/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <string>
#include <sstream>
#ifdef MOZ_INSTRUMENT_EVENT_LOOP
#include "EventTracer.h"
#endif
#include "GeckoProfiler.h"
#include "nsProfiler.h"
#include "nsMemory.h"
#include "nsString.h"
#include "mozilla/Services.h"
#include "nsIObserverService.h"
#include "nsIInterfaceRequestor.h"
#include "nsILoadContext.h"
#include "nsIWebNavigation.h"
#include "nsIInterfaceRequestorUtils.h"
#include "shared-libraries.h"
#include "jsapi.h"

using std::string;

NS_IMPL_ISUPPORTS1(nsProfiler, nsIProfiler)

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
                    const PRUnichar *aData)
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
nsProfiler::StartProfiler(uint32_t aEntries, uint32_t aInterval,
                          const char** aFeatures, uint32_t aFeatureCount,
                          const char** aThreadNameFilters, uint32_t aFilterCount)
{
  if (mLockedForPrivateBrowsing) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  profiler_start(aEntries, aInterval,
                 aFeatures, aFeatureCount,
                 aThreadNameFilters, aFilterCount);
#ifdef MOZ_INSTRUMENT_EVENT_LOOP
  bool printToConsole = false;
  mozilla::InitEventTracing(printToConsole);
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsProfiler::StopProfiler()
{
  profiler_stop();
  return NS_OK;
}

NS_IMETHODIMP
nsProfiler::AddMarker(const char *aMarker)
{
  PROFILER_MARKER(aMarker);
  return NS_OK;
}

NS_IMETHODIMP
nsProfiler::GetProfile(char **aProfile)
{
  char *profile = profiler_get_profile();
  if (profile) {
    size_t len = strlen(profile);
    char *profileStr = static_cast<char *>
                         (nsMemory::Clone(profile, (len + 1) * sizeof(char)));
    profileStr[len] = '\0';
    *aProfile = profileStr;
    free(profile);
  }
  return NS_OK;
}

static void
AddSharedLibraryInfoToStream(std::ostream& aStream, const SharedLibrary& aLib)
{
  aStream << "{";
  aStream << "\"start\":" << aLib.GetStart();
  aStream << ",\"end\":" << aLib.GetEnd();
  aStream << ",\"offset\":" << aLib.GetOffset();
  aStream << ",\"name\":\"" << aLib.GetName() << "\"";
  const std::string &breakpadId = aLib.GetBreakpadId();
  aStream << ",\"breakpadId\":\"" << breakpadId << "\"";
#ifdef XP_WIN
  // FIXME: remove this XP_WIN code when the profiler plugin has switched to
  // using breakpadId.
  std::string pdbSignature = breakpadId.substr(0, 32);
  std::string pdbAgeStr = breakpadId.substr(32,  breakpadId.size() - 1);

  std::stringstream stream;
  stream << pdbAgeStr;

  unsigned pdbAge;
  stream << std::hex;
  stream >> pdbAge;

#ifdef DEBUG
  std::ostringstream oStream;
  oStream << pdbSignature << std::hex << std::uppercase << pdbAge;
  MOZ_ASSERT(breakpadId == oStream.str());
#endif

  aStream << ",\"pdbSignature\":\"" << pdbSignature << "\"";
  aStream << ",\"pdbAge\":" << pdbAge;
  aStream << ",\"pdbName\":\"" << aLib.GetName() << "\"";
#endif
  aStream << "}";
}

std::string
GetSharedLibraryInfoString()
{
  SharedLibraryInfo info = SharedLibraryInfo::GetInfoForSelf();
  if (info.GetSize() == 0)
    return "[]";

  std::ostringstream os;
  os << "[";
  AddSharedLibraryInfoToStream(os, info.GetEntry(0));

  for (size_t i = 1; i < info.GetSize(); i++) {
    os << ",";
    AddSharedLibraryInfoToStream(os, info.GetEntry(i));
  }

  os << "]";
  return os.str();
}

NS_IMETHODIMP
nsProfiler::GetSharedLibraryInformation(nsAString& aOutString)
{
  aOutString.Assign(NS_ConvertUTF8toUTF16(GetSharedLibraryInfoString().c_str()));
  return NS_OK;
}

NS_IMETHODIMP nsProfiler::GetProfileData(JSContext* aCx, JS::Value* aResult)
{
  JSObject *obj = profiler_get_profile_jsobject(aCx);
  if (!obj)
    return NS_ERROR_FAILURE;

  *aResult = OBJECT_TO_JSVAL(obj);
  return NS_OK;
}

NS_IMETHODIMP
nsProfiler::IsActive(bool *aIsActive)
{
  *aIsActive = profiler_is_active();
  return NS_OK;
}

NS_IMETHODIMP
nsProfiler::GetResponsivenessTimes(uint32_t *aCount, double **aResult)
{
  unsigned int len = 100;
  const double* times = profiler_get_responsiveness();
  if (!times) {
    *aCount = 0;
    *aResult = nullptr;
    return NS_OK;
  }

  double *fs = static_cast<double *>
                       (nsMemory::Clone(times, len * sizeof(double)));

  *aCount = len;
  *aResult = fs;

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
                       (nsMemory::Alloc(len * sizeof(char*)));

  for (size_t i = 0; i < len; i++) {
    size_t strLen = strlen(features[i]);
    featureList[i] = static_cast<char *>
                         (nsMemory::Clone(features[i], (strLen + 1) * sizeof(char)));
  }

  *aFeatures = featureList;
  *aCount = len;
  return NS_OK;
}
