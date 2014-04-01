/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsShellService.h"
#include "nsString.h"

#include "AndroidBridge.h"

using namespace mozilla::widget::android;

NS_IMPL_ISUPPORTS1(nsShellService, nsIShellService)

NS_IMETHODIMP
nsShellService::SwitchTask()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsShellService::CreateShortcut(const nsAString& aTitle, const nsAString& aURI,
                                const nsAString& aIconData, const nsAString& aIntent)
{
  if (!aTitle.Length() || !aURI.Length() || !aIconData.Length())
    return NS_ERROR_FAILURE;

  mozilla::widget::android::GeckoAppShell::CreateShortcut(aTitle, aURI, aIconData, aIntent);
  return NS_OK;
}
