/* -*- Mode: c++; c-basic-offset: 2; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsAndroidHandlerApp.h"
#include "GeneratedJNIWrappers.h"

using namespace mozilla;


NS_IMPL_ISUPPORTS(nsAndroidHandlerApp, nsIHandlerApp, nsISharingHandlerApp)

nsAndroidHandlerApp::nsAndroidHandlerApp(const nsAString& aName,
                                         const nsAString& aDescription,
                                         const nsAString& aPackageName,
                                         const nsAString& aClassName,
                                         const nsACString& aMimeType,
                                         const nsAString& aAction) :
mName(aName), mDescription(aDescription), mPackageName(aPackageName),
  mClassName(aClassName), mMimeType(aMimeType), mAction(aAction)
{
}

nsAndroidHandlerApp::~nsAndroidHandlerApp()
{
}

NS_IMETHODIMP
nsAndroidHandlerApp::GetName(nsAString & aName)
{
  aName.Assign(mName);
  return NS_OK;
}

NS_IMETHODIMP
nsAndroidHandlerApp::SetName(const nsAString & aName)
{
  mName.Assign(aName);
  return NS_OK;
}

NS_IMETHODIMP
nsAndroidHandlerApp::GetDetailedDescription(nsAString & aDescription)
{
  aDescription.Assign(mDescription);
  return NS_OK;
}

NS_IMETHODIMP
nsAndroidHandlerApp::SetDetailedDescription(const nsAString & aDescription)
{
  mDescription.Assign(aDescription);

  return NS_OK;
}

NS_IMETHODIMP
nsAndroidHandlerApp::Equals(nsIHandlerApp *aHandlerApp, bool *aRetval)
{
  *aRetval = false;
  if (!aHandlerApp) {
    return NS_OK;
  }

  nsAutoString name;
  nsAutoString detailedDescription;
  aHandlerApp->GetName(name);
  aHandlerApp->GetDetailedDescription(detailedDescription);

  *aRetval = name.Equals(mName) && detailedDescription.Equals(mDescription);
  return NS_OK;
}

NS_IMETHODIMP
nsAndroidHandlerApp::LaunchWithURI(nsIURI *aURI, nsIInterfaceRequestor *aWindowContext)
{
  nsCString uriSpec;
  aURI->GetSpec(uriSpec);
  return java::GeckoAppShell::OpenUriExternal(
          uriSpec, mMimeType, mPackageName, mClassName,
          mAction, EmptyString()) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsAndroidHandlerApp::Share(const nsAString & data, const nsAString & title)
{
  return java::GeckoAppShell::OpenUriExternal(
          data, mMimeType, mPackageName, mClassName,
          mAction, EmptyString()) ? NS_OK : NS_ERROR_FAILURE;
}

