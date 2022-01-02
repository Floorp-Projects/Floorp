/* -*- Mode: C++; tab-width: 2; indent-tabs-mode:nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsParentalControlsService.h"
#include "nsString.h"
#include "nsIFile.h"
#include "mozilla/Unused.h"

NS_IMPL_ISUPPORTS(nsParentalControlsService, nsIParentalControlsService)

nsParentalControlsService::nsParentalControlsService() : mEnabled(false) {
  mozilla::Unused << mEnabled;
}

nsParentalControlsService::~nsParentalControlsService() = default;

NS_IMETHODIMP
nsParentalControlsService::GetParentalControlsEnabled(bool* aResult) {
  *aResult = false;
  return NS_OK;
}

NS_IMETHODIMP
nsParentalControlsService::GetBlockFileDownloadsEnabled(bool* aResult) {
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
nsParentalControlsService::GetLoggingEnabled(bool* aResult) {
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
nsParentalControlsService::Log(int16_t aEntryType, bool blocked,
                               nsIURI* aSource, nsIFile* aTarget) {
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
nsParentalControlsService::IsAllowed(int16_t aAction, nsIURI* aUri,
                                     bool* _retval) {
  return NS_ERROR_NOT_AVAILABLE;
}
