/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SaveProfileTask.h"
#include "GeckoProfiler.h"

nsresult
SaveProfileTask::Run() {
  // Get file path
#if defined(SPS_PLAT_arm_android) && !defined(MOZ_WIDGET_GONK)
  nsCString tmpPath;
  tmpPath.AppendPrintf("/sdcard/profile_%i_%i.txt", XRE_GetProcessType(), getpid());
#else
  nsCOMPtr<nsIFile> tmpFile;
  nsAutoCString tmpPath;
  if (NS_FAILED(NS_GetSpecialDirectory(NS_OS_TEMP_DIR, getter_AddRefs(tmpFile)))) {
    LOG("Failed to find temporary directory.");
    return NS_ERROR_FAILURE;
  }
  tmpPath.AppendPrintf("profile_%i_%i.txt", XRE_GetProcessType(), getpid());

  nsresult rv = tmpFile->AppendNative(tmpPath);
  if (NS_FAILED(rv))
    return rv;

  rv = tmpFile->GetNativePath(tmpPath);
  if (NS_FAILED(rv))
    return rv;
#endif

  profiler_save_profile_to_file(tmpPath.get());

  return NS_OK;
}

NS_IMPL_ISUPPORTS(ProfileSaveEvent, nsIProfileSaveEvent)

nsresult
ProfileSaveEvent::AddSubProfile(const char* aProfile) {
  mFunc(aProfile, mClosure);
  return NS_OK;
}

