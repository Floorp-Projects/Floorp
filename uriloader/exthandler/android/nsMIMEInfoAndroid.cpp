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
 *   Michael Wu <mwu@mozilla.com>
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
#include "nsMIMEInfoAndroid.h"
#include "AndroidBridge.h"
#include "nsAndroidHandlerApp.h"
#include "nsArrayUtils.h"
#include "nsISupportsUtils.h"
#include "nsStringEnumerator.h"
#include "nsNetUtil.h"

NS_IMPL_ISUPPORTS2(nsMIMEInfoAndroid, nsIMIMEInfo, nsIHandlerInfo)

NS_IMETHODIMP
nsMIMEInfoAndroid::LaunchDefaultWithFile(nsIFile* aFile)
{
  LaunchWithFile(aFile);
  return NS_OK;
}

NS_IMETHODIMP
nsMIMEInfoAndroid::LoadUriInternal(nsIURI * aURI)
{
  nsCString uriSpec;
  aURI->GetSpec(uriSpec);
  if (mozilla::AndroidBridge::Bridge())
    return mozilla::AndroidBridge::Bridge()->
      OpenUriExternal(uriSpec, mMimeType) ? NS_OK : NS_ERROR_FAILURE;

  return NS_ERROR_FAILURE;
}


PRBool
nsMIMEInfoAndroid::GetMimeInfoForMimeType(const nsACString& aMimeType,
                                          nsMIMEInfoAndroid** aMimeInfo)
{
  nsRefPtr<nsMIMEInfoAndroid> info = new nsMIMEInfoAndroid(aMimeType);
  mozilla::AndroidBridge* bridge = mozilla::AndroidBridge::Bridge();
  // we don't have access to the bridge, so just assume we can handle
  // the mime type for now and let the system deal with it
  if (!bridge){
    info.forget(aMimeInfo);
    return PR_TRUE;
  }

  nsIHandlerApp* systemDefault = nsnull;
  bridge->GetHandlersForMimeType(nsCAutoString(aMimeType).get(), 
                                 info->mHandlerApps, &systemDefault);
  
  if (systemDefault)
    info->mPrefApp = systemDefault;

  PRUint32 len;
  info->mHandlerApps->GetLength(&len);
  if (len == 1) {
    info.forget(aMimeInfo);
    return PR_FALSE;
  }
  
  info.forget(aMimeInfo);
  return PR_TRUE;
}
  
PRBool
nsMIMEInfoAndroid::GetMimeInfoForFileExt(const nsACString& aFileExt,
                                         nsMIMEInfoAndroid **aMimeInfo)
{
  nsCString mimeType;
  if (mozilla::AndroidBridge::Bridge())
    mozilla::AndroidBridge::Bridge()->
      GetMimeTypeFromExtension(aFileExt, mimeType);
  return GetMimeInfoForMimeType(mimeType, aMimeInfo);
}

nsresult
nsMIMEInfoAndroid::GetMimeInfoForProtocol(const nsACString &aScheme,
                                          PRBool *found,
                                          nsIHandlerInfo **info)
{
  const nsCString &emptyC = EmptyCString();
  mozilla::AndroidBridge* bridge = mozilla::AndroidBridge::Bridge();
  nsMIMEInfoAndroid *mimeinfo = new nsMIMEInfoAndroid(emptyC);
  NS_ADDREF(*info = mimeinfo);
  *found = PR_TRUE;
  
  if (!bridge) {
    // we don't have access to the bridge, so just assume we can handle
    // the protocol for now and let the system deal with it
    return NS_OK;
  }

  nsIHandlerApp* systemDefault = nsnull;
  bridge->GetHandlersForProtocol(nsCAutoString(aScheme).get(), 
                                 mimeinfo->mHandlerApps, &systemDefault);
  
  if (systemDefault)
    mimeinfo->mPrefApp = systemDefault;


  PRUint32 len;
  mimeinfo->mHandlerApps->GetLength(&len);
  if (len == 1) {
    // Code that calls this requires an object regardless if the OS has
    // something for us, so we return the empty object.
    *found = PR_FALSE;
    return NS_OK;
  }
  
  return NS_OK;
}

NS_IMETHODIMP
nsMIMEInfoAndroid::GetType(nsACString& aType)
{
  aType.Assign(mMimeType);
  return NS_OK;
}

NS_IMETHODIMP
nsMIMEInfoAndroid::GetDescription(nsAString& aDesc)
{
  aDesc.Assign(mDescription);
  return NS_OK;
}

NS_IMETHODIMP
nsMIMEInfoAndroid::SetDescription(const nsAString& aDesc)
{
  mDescription.Assign(aDesc);
  return NS_OK;
}

NS_IMETHODIMP
nsMIMEInfoAndroid::GetPreferredApplicationHandler(nsIHandlerApp** aApp)
{
  *aApp = mPrefApp;
  NS_IF_ADDREF(*aApp);
  return NS_OK;
}

NS_IMETHODIMP
nsMIMEInfoAndroid::SetPreferredApplicationHandler(nsIHandlerApp* aApp)
{
  mPrefApp = aApp;
  return NS_OK;
}

NS_IMETHODIMP
nsMIMEInfoAndroid::GetPossibleApplicationHandlers(nsIMutableArray **aHandlerApps)
{
  if (!mHandlerApps)
    mHandlerApps = do_CreateInstance(NS_ARRAY_CONTRACTID);

  if (!mHandlerApps)
    return NS_ERROR_OUT_OF_MEMORY;

  *aHandlerApps = mHandlerApps;
  NS_IF_ADDREF(*aHandlerApps);
  return NS_OK;
}

NS_IMETHODIMP
nsMIMEInfoAndroid::GetHasDefaultHandler(PRBool* aHasDefault)
{
  PRUint32 len;
  *aHasDefault = PR_FALSE;
  if (!mHandlerApps)
    return NS_OK;

  if (NS_FAILED(mHandlerApps->GetLength(&len)))
    return NS_OK;

  if (len == 0)
    return NS_OK;

  *aHasDefault = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP
nsMIMEInfoAndroid::GetDefaultDescription(nsAString& aDesc)
{
  aDesc.Assign(EmptyString());
  return NS_OK;
}

NS_IMETHODIMP
nsMIMEInfoAndroid::LaunchWithURI(nsIURI* aURI, nsIInterfaceRequestor* req)
{
  return mPrefApp->LaunchWithURI(aURI, req);
}

NS_IMETHODIMP
nsMIMEInfoAndroid::GetPreferredAction(nsHandlerInfoAction* aPrefAction)
{
  *aPrefAction = mPrefAction;
  return NS_OK;
}

NS_IMETHODIMP
nsMIMEInfoAndroid::SetPreferredAction(nsHandlerInfoAction aPrefAction)
{
  mPrefAction = aPrefAction;
  return NS_OK;
}

NS_IMETHODIMP
nsMIMEInfoAndroid::GetAlwaysAskBeforeHandling(PRBool* aAlwaysAsk)
{
  *aAlwaysAsk = mAlwaysAsk;
  return NS_OK;
}

NS_IMETHODIMP
nsMIMEInfoAndroid::SetAlwaysAskBeforeHandling(PRBool aAlwaysAsk)
{
  mAlwaysAsk = aAlwaysAsk;
  return NS_OK;
}

NS_IMETHODIMP
nsMIMEInfoAndroid::GetFileExtensions(nsIUTF8StringEnumerator** aResult)
{
  return NS_NewUTF8StringEnumerator(aResult, &mExtensions, this);
}

NS_IMETHODIMP
nsMIMEInfoAndroid::SetFileExtensions(const nsACString & aExtensions)
{
  mExtensions.Clear();
  nsCString extList(aExtensions);

  PRInt32 breakLocation = -1;
  while ( (breakLocation = extList.FindChar(',')) != -1)
  {
    mExtensions.AppendElement(Substring(extList.get(), extList.get() + breakLocation));
    extList.Cut(0, breakLocation + 1);
  }
  if (!extList.IsEmpty())
    mExtensions.AppendElement(extList);
  return NS_OK;
}

NS_IMETHODIMP
nsMIMEInfoAndroid::ExtensionExists(const nsACString & aExtension, PRBool *aRetVal)
{
  NS_ASSERTION(!aExtension.IsEmpty(), "no extension");
  PRBool found = PR_FALSE;
  PRUint32 extCount = mExtensions.Length();
  if (extCount < 1) return NS_OK;

  for (PRUint8 i=0; i < extCount; i++) {
    const nsCString& ext = mExtensions[i];
    if (ext.Equals(aExtension, nsCaseInsensitiveCStringComparator())) {
      found = PR_TRUE;
      break;
    }
  }

  *aRetVal = found;
  return NS_OK;
}

NS_IMETHODIMP
nsMIMEInfoAndroid::AppendExtension(const nsACString & aExtension)
{
  mExtensions.AppendElement(aExtension);
  return NS_OK;
}

NS_IMETHODIMP
nsMIMEInfoAndroid::GetPrimaryExtension(nsACString & aPrimaryExtension)
{
  if (!mExtensions.Length())
    return NS_ERROR_NOT_INITIALIZED;

  aPrimaryExtension = mExtensions[0];
  return NS_OK;
}

NS_IMETHODIMP
nsMIMEInfoAndroid::SetPrimaryExtension(const nsACString & aExtension)
{
  NS_ASSERTION(!aExtension.IsEmpty(), "no extension");
  PRUint32 extCount = mExtensions.Length();
  PRUint8 i;
  PRBool found = PR_FALSE;
  for (i=0; i < extCount; i++) {
    const nsCString& ext = mExtensions[i];
    if (ext.Equals(aExtension, nsCaseInsensitiveCStringComparator())) {
      found = PR_TRUE;
      break;
    }
  }
  if (found) {
    mExtensions.RemoveElementAt(i);
  }

  mExtensions.InsertElementAt(0, aExtension);

  return NS_OK;
}

NS_IMETHODIMP
nsMIMEInfoAndroid::GetMIMEType(nsACString & aMIMEType)
{
  aMIMEType.Assign(mMimeType);
  return NS_OK;
}

NS_IMETHODIMP
nsMIMEInfoAndroid::Equals(nsIMIMEInfo *aMIMEInfo, PRBool *aRetVal)
{
  if (!aMIMEInfo) return NS_ERROR_NULL_POINTER;

  nsCAutoString type;
  nsresult rv = aMIMEInfo->GetMIMEType(type);
  if (NS_FAILED(rv)) return rv;

  *aRetVal = mMimeType.Equals(type);

  return NS_OK;
}

NS_IMETHODIMP
nsMIMEInfoAndroid::GetPossibleLocalHandlers(nsIArray * *aPossibleLocalHandlers)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsMIMEInfoAndroid::LaunchWithFile(nsIFile *aFile)
{
  nsIURI* uri;
  NS_NewFileURI(&uri, aFile);
  LoadUriInternal(uri);
  return NS_OK;
}

nsMIMEInfoAndroid::nsMIMEInfoAndroid(const nsACString& aMIMEType) :
  mMimeType(aMIMEType), mAlwaysAsk(PR_TRUE),
  mPrefAction(nsIMIMEInfo::useHelperApp)
{
  mPrefApp = new nsMIMEInfoAndroid::SystemChooser(this);
  nsresult rv;
  mHandlerApps = do_CreateInstance(NS_ARRAY_CONTRACTID, &rv);
  mHandlerApps->AppendElement(mPrefApp, PR_FALSE);
}

NS_IMPL_ISUPPORTS1(nsMIMEInfoAndroid::SystemChooser, nsIHandlerApp)


nsresult nsMIMEInfoAndroid::SystemChooser::GetName(nsAString & aName) {
  aName.Assign(NS_LITERAL_STRING("Android chooser"));
  return NS_OK;
}

nsresult
nsMIMEInfoAndroid::SystemChooser::SetName(const nsAString&) {
  return NS_OK;
}

nsresult
nsMIMEInfoAndroid::SystemChooser::GetDetailedDescription(nsAString & aDesc) {
  aDesc.Assign(NS_LITERAL_STRING("Android's default handler application chooser"));
  return NS_OK;
}

nsresult
nsMIMEInfoAndroid::SystemChooser::SetDetailedDescription(const nsAString&) {
  return NS_OK;
}

nsresult
nsMIMEInfoAndroid::SystemChooser::Equals(nsIHandlerApp *aHandlerApp, PRBool *aRetVal) {
  nsCOMPtr<nsMIMEInfoAndroid::SystemChooser> info = do_QueryInterface(aHandlerApp);
  if (info)
    return mOuter->Equals(info->mOuter, aRetVal);
  *aRetVal = PR_FALSE;
  return NS_OK;
}

nsresult
nsMIMEInfoAndroid::SystemChooser::LaunchWithURI(nsIURI* aURI, nsIInterfaceRequestor*)
{
  return mOuter->LoadUriInternal(aURI);
}
