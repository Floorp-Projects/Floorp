/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Initial Developer of the Original Code is
 *   Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Benoit Girard <bgirard@mozilla.com>
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
nsProfiler::StartProfiler(PRUint32 aInterval, PRUint32 aEntries,
                          const char** aFeatures, PRUint32 aFeatureCount)
{
  SAMPLER_START(aInterval, aEntries, aFeatures, aFeatureCount);
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
  aStream << ",\"name\":\"" << aLib.GetName() << "\"";
#ifdef XP_WIN
  aStream << ",\"pdbSignature\":\"" << aLib.GetPdbSignature().ToString() << "\"";
  aStream << ",\"pdbAge\":" << aLib.GetPdbAge();
  aStream << ",\"pdbName\":" << aLib.GetPdbName();
#endif
  aStream << "}";
}

static std::string
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
    *aResult = nsnull;
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
    *aFeatures = nsnull;
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
