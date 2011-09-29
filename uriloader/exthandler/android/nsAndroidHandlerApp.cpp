/* -*- Mode: c++; c-basic-offset: 2; tab-width: 20; indent-tabs-mode: nil; -*-
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla Android code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Brad Lassey <blassey@mozilla.com>
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

#include "nsAndroidHandlerApp.h"
#include "AndroidBridge.h"


NS_IMPL_ISUPPORTS2(nsAndroidHandlerApp, nsIHandlerApp, nsISharingHandlerApp)

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
  nsCOMPtr<nsAndroidHandlerApp> aApp = do_QueryInterface(aHandlerApp);
  *aRetval = aApp && aApp->mName.Equals(mName) &&
    aApp->mDescription.Equals(mDescription);
  return NS_OK;
}

NS_IMETHODIMP
nsAndroidHandlerApp::LaunchWithURI(nsIURI *aURI, nsIInterfaceRequestor *aWindowContext)
{
  if (!mozilla::AndroidBridge::Bridge())
    return NS_ERROR_FAILURE;

  nsCString uriSpec;
  aURI->GetSpec(uriSpec);
  return mozilla::AndroidBridge::Bridge()->
    OpenUriExternal(uriSpec, mMimeType, mPackageName, mClassName, mAction) ? 
    NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsAndroidHandlerApp::Share(const nsAString & data, const nsAString & title)
{
  if (!mozilla::AndroidBridge::Bridge())
    return NS_ERROR_FAILURE;

  return mozilla::AndroidBridge::Bridge()->
    OpenUriExternal(NS_ConvertUTF16toUTF8(data), mMimeType, mPackageName, 
                    mClassName, mAction) ? NS_OK : NS_ERROR_FAILURE;
}

