/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsShellService.h"
#include "nsString.h"

#include "FennecJNIWrappers.h"

using namespace mozilla;

NS_IMPL_ISUPPORTS(nsShellService, nsIShellService)

NS_IMETHODIMP
nsShellService::SwitchTask()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsShellService::CreateShortcut(const nsAString& aTitle, const nsAString& aURI,
                                const nsAString& aIcondata, const nsAString& aIntent)
{
  if (!aTitle.Length() || !aURI.Length() || !jni::IsFennec())
    return NS_ERROR_FAILURE;

  java::GeckoApplication::CreateShortcut(aTitle, aURI);
  return NS_OK;
}
