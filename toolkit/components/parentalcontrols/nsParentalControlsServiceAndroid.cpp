/* -*- Mode: C++; tab-width: 2; indent-tabs-mode:nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsParentalControlsService.h"
#include "nsString.h"
#include "nsIFile.h"
#include "FennecJNIWrappers.h"

namespace java = mozilla::java;

NS_IMPL_ISUPPORTS(nsParentalControlsService, nsIParentalControlsService)

nsParentalControlsService::nsParentalControlsService() :
  mEnabled(false)
{
  if (mozilla::jni::IsAvailable()) {
    mEnabled = java::Restrictions::IsUserRestricted();
  }
}

nsParentalControlsService::~nsParentalControlsService()
{
}

NS_IMETHODIMP
nsParentalControlsService::GetParentalControlsEnabled(bool *aResult)
{
  *aResult = mEnabled;
  return NS_OK;
}

NS_IMETHODIMP
nsParentalControlsService::GetBlockFileDownloadsEnabled(bool *aResult)
{
  // NOTE: isAllowed returns the opposite intention, so we need to flip it
  bool res;
  IsAllowed(nsIParentalControlsService::DOWNLOAD, NULL, &res);
  *aResult = !res;

  return NS_OK;
}

NS_IMETHODIMP
nsParentalControlsService::GetLoggingEnabled(bool *aResult)
{
  // Android doesn't currently have any method of logging restricted actions.
  *aResult = false;
  return NS_OK;
}

NS_IMETHODIMP
nsParentalControlsService::Log(int16_t aEntryType,
                               bool aBlocked,
                               nsIURI *aSource,
                               nsIFile *aTarget)
{
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
nsParentalControlsService::RequestURIOverride(nsIURI *aTarget,
                                              nsIInterfaceRequestor *aWindowContext,
                                              bool *_retval)
{
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
nsParentalControlsService::RequestURIOverrides(nsIArray *aTargets,
                                               nsIInterfaceRequestor *aWindowContext,
                                               bool *_retval)
{
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
nsParentalControlsService::IsAllowed(int16_t aAction,
                                     nsIURI *aUri,
                                     bool *_retval)
{
  nsresult rv = NS_OK;
  *_retval = true;

  if (!mEnabled) {
    return rv;
  }

  if (mozilla::jni::IsAvailable()) {
    nsAutoCString url;
    if (aUri) {
      rv = aUri->GetSpec(url);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    *_retval = java::Restrictions::IsAllowed(aAction,
                                                    NS_ConvertUTF8toUTF16(url));
    return rv;
  }

  return NS_ERROR_NOT_AVAILABLE;
}
