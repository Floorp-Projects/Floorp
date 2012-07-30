/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <string>
#include <sstream>
#ifdef MOZ_INSTRUMENT_EVENT_LOOP
#include "EventTracer.h"
#endif
#include "sampler.h"
#include "nsProfiler.h"
#include "nsMemory.h"
#include "shared-libraries.h"
#include "nsString.h"
#include "jsapi.h"

using std::string;

NS_IMPL_ISUPPORTS1(nsProfiler, nsIProfiler)


nsProfiler::nsProfiler()
{
}


NS_IMETHODIMP
nsProfiler::StartProfiler(PRUint32 aEntries, PRUint32 aInterval,
                          const char** aFeatures, PRUint32 aFeatureCount)
{
  SAMPLER_START(aEntries, aInterval, aFeatures, aFeatureCount);
#ifdef MOZ_INSTRUMENT_EVENT_LOOP
  mozilla::InitEventTracing();
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsProfiler::StopProfiler()
{
  SAMPLER_STOP();
  return NS_OK;
}

NS_IMETHODIMP
nsProfiler::GetProfile(char **aProfile)
{
  char *profile = SAMPLER_GET_PROFILE();
  if (profile) {
    PRUint32 len = strlen(profile);
    char *profileStr = static_cast<char *>
                         (nsMemory::Clone(profile, (len + 1) * sizeof(char)));
    profileStr[len] = '\0';
    *aProfile = profileStr;
    free(profile);
  }
  return NS_OK;
}

static void
AddSharedLibraryInfoToStream(std::ostream& aStream, SharedLibrary& aLib)
{
  aStream << "{";
  aStream << "\"start\":" << aLib.GetStart();
  aStream << ",\"end\":" << aLib.GetEnd();
  aStream << ",\"offset\":" << aLib.GetOffset();
  aStream << ",\"name\":\"" << aLib.GetName() << "\"";
#ifdef XP_WIN
  aStream << ",\"pdbSignature\":\"" << aLib.GetPdbSignature().ToString() << "\"";
  aStream << ",\"pdbAge\":" << aLib.GetPdbAge();
  aStream << ",\"pdbName\":\"" << aLib.GetPdbName() << "\"";
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



NS_IMETHODIMP nsProfiler::GetProfileData(JSContext* aCx, jsval* aResult)
{
  JSObject *obj = SAMPLER_GET_PROFILE_DATA(aCx);
  if (!obj)
    return NS_ERROR_FAILURE;

  *aResult = OBJECT_TO_JSVAL(obj);
  return NS_OK;
}

NS_IMETHODIMP
nsProfiler::IsActive(bool *aIsActive)
{
  *aIsActive = SAMPLER_IS_ACTIVE();
  return NS_OK;
}

NS_IMETHODIMP
nsProfiler::GetResponsivenessTimes(PRUint32 *aCount, double **aResult)
{
  unsigned int len = 100;
  const double* times = SAMPLER_GET_RESPONSIVENESS();
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
nsProfiler::GetFeatures(PRUint32 *aCount, char ***aFeatures)
{
  PRUint32 len = 0;

  const char **features = SAMPLER_GET_FEATURES();
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
    PRUint32 strLen = strlen(features[i]);
    featureList[i] = static_cast<char *>
                         (nsMemory::Clone(features[i], (strLen + 1) * sizeof(char)));
  }

  *aFeatures = featureList;
  *aCount = len;
  return NS_OK;
}
