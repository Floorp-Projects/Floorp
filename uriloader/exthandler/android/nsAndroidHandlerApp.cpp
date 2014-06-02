/* -*- Mode: c++; c-basic-offset: 2; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsAndroidHandlerApp.h"
#include "AndroidBridge.h"

using namespace mozilla::widget::android;


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

// XXX Workaround for bug 986975 to maintain the existing broken semantics
template<>
struct nsISharingHandlerApp::COMTypeInfo<nsAndroidHandlerApp, void> {
  static const nsIID kIID;
};
const nsIID nsISharingHandlerApp::COMTypeInfo<nsAndroidHandlerApp, void>::kIID = NS_IHANDLERAPP_IID;

NS_IMETHODIMP
nsAndroidHandlerApp::Equals(nsIHandlerApp *aHandlerApp, bool *aRetval)
{
  nsCOMPtr<nsAndroidHandlerApp> aApp = do_QueryInterface(aHandlerApp);
  *aRetval = aApp && aApp->mName.Equals(mName) &&
    aApp->mDescription.Equals(mDescription);
  return NS_OK;
}

NS_IMETHODIMP
nsAndroidHandlerApp::LaunchWithURI(nsIURI *aURI, nsIInterfaceRequestor *aWindowContext)
{
  nsCString uriSpec;
  aURI->GetSpec(uriSpec);
  return mozilla::widget::android::GeckoAppShell::OpenUriExternal
    (NS_ConvertUTF8toUTF16(uriSpec), NS_ConvertUTF8toUTF16(mMimeType), mPackageName, mClassName, mAction) ?
    NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsAndroidHandlerApp::Share(const nsAString & data, const nsAString & title)
{
  return mozilla::widget::android::GeckoAppShell::OpenUriExternal(data, NS_ConvertUTF8toUTF16(mMimeType),
                    mPackageName, mClassName, mAction) ? NS_OK : NS_ERROR_FAILURE;
}

